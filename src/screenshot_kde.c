// SPDX-License-Identifier: GPL-3.0-only

#if OPENCV_ENABLED && KDE_ENABLED

#include "screenshot_kde.h"

#include "log.h"
#include "state.h"
#include "utils.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef USE_BASU_SDBUS
#include <basu/sd-bus.h>
#else
#include <systemd/sd-bus.h>
#endif

// QImage::Format values returned by ScreenShot2.
#define QIMAGE_FORMAT_RGB32                4
#define QIMAGE_FORMAT_ARGB32               5
#define QIMAGE_FORMAT_ARGB32_PREMULTIPLIED 6

static bool qimage_to_wl_shm(uint32_t qformat, enum wl_shm_format *out) {
    switch (qformat) {
    case QIMAGE_FORMAT_RGB32:
        *out = WL_SHM_FORMAT_XRGB8888;
        return true;
    case QIMAGE_FORMAT_ARGB32:
    case QIMAGE_FORMAT_ARGB32_PREMULTIPLIED:
        *out = WL_SHM_FORMAT_ARGB8888;
        return true;
    default:
        return false;
    }
}

// Reads exactly `size` bytes from fd into buf, looping over short reads.
static bool read_full(int fd, uint8_t *buf, size_t size) {
    size_t off = 0;
    while (off < size) {
        ssize_t n = read(fd, buf + off, size - off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_ERR("Failed reading screenshot pipe: %s", strerror(errno));
            return false;
        }
        if (n == 0) {
            LOG_ERR("Screenshot pipe closed early (%zu/%zu bytes).", off, size);
            return false;
        }
        off += (size_t)n;
    }
    return true;
}

// Reads width/height/stride/format from the a{sv} reply.
static bool read_metadata(
    sd_bus_message *reply, uint32_t *width, uint32_t *height, uint32_t *stride,
    uint32_t *qformat
) {
    bool have_w = false, have_h = false, have_s = false, have_f = false;
    int  r = sd_bus_message_enter_container(reply, 'a', "{sv}");
    if (r < 0) {
        return false;
    }
    while (sd_bus_message_enter_container(reply, 'e', "sv") > 0) {
        const char *key;
        sd_bus_message_read(reply, "s", &key);
        uint32_t value;
        // All four fields we need are u (uint32); skip anything else.
        if (strcmp(key, "width") == 0) {
            sd_bus_message_read(reply, "v", "u", &value);
            *width = value;
            have_w = true;
        } else if (strcmp(key, "height") == 0) {
            sd_bus_message_read(reply, "v", "u", &value);
            *height = value;
            have_h  = true;
        } else if (strcmp(key, "stride") == 0) {
            sd_bus_message_read(reply, "v", "u", &value);
            *stride = value;
            have_s  = true;
        } else if (strcmp(key, "format") == 0) {
            sd_bus_message_read(reply, "v", "u", &value);
            *qformat = value;
            have_f   = true;
        } else {
            sd_bus_message_skip(reply, "v");
        }
        sd_bus_message_exit_container(reply);
    }
    sd_bus_message_exit_container(reply);
    return have_w && have_h && have_s && have_f;
}

struct scrcpy_buffer *
query_screenshot_kde(struct state *state, struct rect region) {
    int32_t gx = state->current_output->x + region.x;
    int32_t gy = state->current_output->y + region.y;

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        LOG_ERR("Failed to create screenshot pipe: %s", strerror(errno));
        return NULL;
    }

    sd_bus               *bus   = NULL;
    sd_bus_error          error = SD_BUS_ERROR_NULL;
    sd_bus_message       *m = NULL, *reply = NULL;
    struct scrcpy_buffer *result = NULL;

    int r = sd_bus_open_user(&bus);
    if (r < 0) {
        LOG_ERR("Failed to connect to session bus: %s", strerror(-r));
        goto out;
    }

    r = sd_bus_message_new_method_call(
        bus, &m, "org.kde.KWin", "/org/kde/KWin/ScreenShot2",
        "org.kde.KWin.ScreenShot2", "CaptureArea"
    );
    if (r < 0) {
        goto out;
    }
    sd_bus_message_append(
        m, "iiuu", (int)gx, (int)gy, (unsigned)region.w, (unsigned)region.h
    );
    sd_bus_message_open_container(m, 'a', "{sv}");
    sd_bus_message_close_container(m);
    sd_bus_message_append(m, "h", pipefd[1]);

    r = sd_bus_call(bus, m, 0, &error, &reply);
    if (r < 0) {
        LOG_ERR(
            "CaptureArea failed: %s",
            error.message ? error.message : strerror(-r)
        );
        goto out;
    }

    // Close our copy of the write end so read() sees EOF after KWin is done.
    close(pipefd[1]);
    pipefd[1] = -1;

    uint32_t width, height, stride, qformat;
    if (!read_metadata(reply, &width, &height, &stride, &qformat)) {
        LOG_ERR("Incomplete screenshot metadata.");
        goto out;
    }

    enum wl_shm_format wl_format;
    if (!qimage_to_wl_shm(qformat, &wl_format)) {
        LOG_ERR("Unsupported screenshot QImage format %u.", qformat);
        goto out;
    }

    size_t   size = (size_t)stride * height;
    uint8_t *data = malloc(size);
    if (data == NULL) {
        LOG_ERR("Failed to allocate screenshot buffer.");
        goto out;
    }
    if (!read_full(pipefd[0], data, size)) {
        free(data);
        goto out;
    }

    result            = malloc(sizeof(*result));
    result->wl_buffer = NULL; // malloc'd, not SHM
    result->data      = data;
    result->format    = wl_format;
    result->width     = (int32_t)width;
    result->height    = (int32_t)height;
    result->stride    = (int32_t)stride;

out:
    if (pipefd[1] >= 0) {
        close(pipefd[1]);
    }
    close(pipefd[0]);
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
    return result;
}

#endif
