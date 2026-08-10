// SPDX-License-Identifier: GPL-3.0-only

#if OPENCV_ENABLED && KWIN_ENABLED

#include "screenshot_kwin.h"

#include "log.h"
#include "sdbus.h"
#include "state.h"
#include "utils.h"

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// QImage::Format values returned by ScreenShot2.
#define QIMAGE_FORMAT_RGB32                4
#define QIMAGE_FORMAT_ARGB32               5
#define QIMAGE_FORMAT_ARGB32_PREMULTIPLIED 6

// Initial reader buffer; doubled as needed. The image size is only known from
// the D-Bus reply, which may arrive after the pixels, so the reader grows a
// buffer until EOF rather than allocating up front.
#define KWIN_CAPTURE_INITIAL_CAP (8u << 20) // 8 MiB

// Upper bound on how long a single read may stall. Bounds the worst case if
// KWin never finishes writing, so a misbehaving compositor cannot hang us.
#define KWIN_CAPTURE_READ_TIMEOUT_MS 10000

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

// State handed to the reader thread. Only the reader touches these fields while
// it runs; the main thread reads them exclusively after pthread_join(), which
// establishes the necessary happens-before ordering -- so no lock is needed.
struct pipe_reader {
    int      fd;   // read end of the pipe (owned by the caller, not the reader)
    uint8_t *data; // grown buffer of the bytes read so far
    size_t   len;  // valid bytes in `data`
    size_t   cap;  // allocated capacity of `data`
    bool     failed;
};

// Reads the pipe until EOF (KWin closes the write end when the image is fully
// written) into a growing buffer. Never writes to the pipe, so it cannot raise
// SIGPIPE; a poll() timeout bounds the worst case.
static void *pipe_reader_thread(void *arg) {
    struct pipe_reader *r = arg;
    for (;;) {
        if (r->len == r->cap) {
            size_t   ncap = r->cap ? r->cap * 2 : KWIN_CAPTURE_INITIAL_CAP;
            uint8_t *nd   = realloc(r->data, ncap);
            if (nd == NULL) {
                LOG_ERR("Failed to grow screenshot buffer.");
                r->failed = true;
                break;
            }
            r->data = nd;
            r->cap  = ncap;
        }

        struct pollfd pfd = {.fd = r->fd, .events = POLLIN};
        int           pr  = poll(&pfd, 1, KWIN_CAPTURE_READ_TIMEOUT_MS);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_ERR("poll() on screenshot pipe failed: %s", strerror(errno));
            r->failed = true;
            break;
        }
        if (pr == 0) {
            LOG_ERR("Timed out reading the screenshot pipe.");
            r->failed = true;
            break;
        }

        ssize_t n = read(r->fd, r->data + r->len, r->cap - r->len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_ERR("Failed reading screenshot pipe: %s", strerror(errno));
            r->failed = true;
            break;
        }
        if (n == 0) {
            break; // EOF: all write ends are closed, image complete.
        }
        r->len += (size_t)n;
    }
    return NULL;
}

// Reads width/height/stride/format from the a{sv} reply. Returns false if any
// of the four is missing or could not be parsed; the caller then treats the
// reply as unusable. Bailing out on a failed read matters: sd_bus leaves the
// destination untouched on failure, so carrying on would hand an
// uninitialized dimension to the buffer arithmetic below.
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
        if (sd_bus_message_read(reply, "s", &key) < 0) {
            return false;
        }

        // All four fields we need are u (uint32); skip anything else.
        uint32_t *dest = NULL;
        if (strcmp(key, "width") == 0) {
            dest   = width;
            have_w = true;
        } else if (strcmp(key, "height") == 0) {
            dest   = height;
            have_h = true;
        } else if (strcmp(key, "stride") == 0) {
            dest   = stride;
            have_s = true;
        } else if (strcmp(key, "format") == 0) {
            dest   = qformat;
            have_f = true;
        }

        if (dest != NULL) {
            if (sd_bus_message_read(reply, "v", "u", dest) < 0) {
                return false;
            }
        } else if (sd_bus_message_skip(reply, "v") < 0) {
            return false;
        }

        if (sd_bus_message_exit_container(reply) < 0) {
            return false;
        }
    }
    sd_bus_message_exit_container(reply);
    return have_w && have_h && have_s && have_f;
}

struct scrcpy_buffer *
query_screenshot_kwin(struct state *state, struct rect region) {
    int32_t gx = state->current_output->x + region.x;
    int32_t gy = state->current_output->y + region.y;

    int pipefd[2] = {-1, -1};
    if (pipe(pipefd) < 0) {
        LOG_ERR("Failed to create screenshot pipe: %s", strerror(errno));
        return NULL;
    }

    sd_bus               *bus   = NULL;
    sd_bus_error          error = SD_BUS_ERROR_NULL;
    sd_bus_message       *m = NULL, *reply = NULL;
    struct scrcpy_buffer *result = NULL;
    struct pipe_reader    rd     = {0};
    pthread_t             tid;
    bool                  reader_running = false;
    int                   call_rc        = 0;

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

    // sd_bus_message_append("h", ...) duplicates the fd into the message, so we
    // drop our own write end now. Leaving it open would keep the pipe from ever
    // reaching EOF once KWin is done.
    close(pipefd[1]);
    pipefd[1] = -1;

    // Drain the pipe on a second thread WHILE the blocking call runs. KWin
    // streams the image through the pipe's small (64 KiB) kernel buffer; if we
    // only read after the call returned, KWin would block writing a large image
    // (buffer full, nobody draining) and we would block waiting for a reply it
    // hasn't sent yet -- a deadlock. The reader touches only `rd` and its fd;
    // the main thread inspects `rd` only after pthread_join(), so there is no
    // shared mutable state and no lock is required.
    rd.fd = pipefd[0];
    if (pthread_create(&tid, NULL, pipe_reader_thread, &rd) != 0) {
        LOG_ERR("Failed to start screenshot reader thread.");
        goto out;
    }
    reader_running = true;

    call_rc = sd_bus_call(bus, m, 0, &error, &reply);

    // Release the message so its duplicated write-end fd is closed; together
    // with KWin closing its end this lets the reader reach EOF.
    sd_bus_message_unref(m);
    m = NULL;

    pthread_join(tid, NULL);
    reader_running = false;

    if (call_rc < 0) {
        LOG_ERR(
            "CaptureArea failed: %s",
            error.message ? error.message : strerror(-call_rc)
        );
        goto out;
    }
    if (rd.failed) {
        LOG_ERR("Failed to read the screenshot image.");
        goto out;
    }

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

    // Every format we accept is 4 bytes per pixel, so a stride that cannot hold
    // one row (or a zero-sized image) means the geometry is not self-consistent
    // and the size arithmetic below would not describe the pixel data.
    if (width == 0 || height == 0 || (uint64_t)stride < (uint64_t)width * 4) {
        LOG_ERR(
            "Implausible screenshot geometry: %ux%u, stride %u.", width, height,
            stride
        );
        goto out;
    }

    size_t size = (size_t)stride * height;
    if (rd.len < size) {
        LOG_ERR(
            "Short screenshot read: got %zu bytes, expected %zu.", rd.len, size
        );
        goto out;
    }

    result = malloc(sizeof(*result));
    if (result == NULL) {
        LOG_ERR("Failed to allocate screenshot buffer.");
        goto out;
    }
    result->wl_buffer = NULL; // malloc'd, not SHM
    result->data      = rd.data;
    result->format    = wl_format;
    result->width     = (int32_t)width;
    result->height    = (int32_t)height;
    result->stride    = (int32_t)stride;
    rd.data           = NULL; // ownership transferred to result

out:
    // Never return while the reader might still be touching `rd`. In the normal
    // flow it was already joined above; this only fires on an early error after
    // the thread was created. The reader ends on its own via EOF or its poll
    // timeout, so we do not close its fd from under it.
    if (reader_running) {
        pthread_join(tid, NULL);
    }
    free(rd.data);
    if (pipefd[0] >= 0) {
        close(pipefd[0]);
    }
    if (pipefd[1] >= 0) {
        close(pipefd[1]);
    }
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
    return result;
}

#endif
