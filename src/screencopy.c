#if OPENCV_ENABLED

#include "screencopy.h"

#include "log.h"
#include "state.h"
#include "surface_buffer.h"
#include "wlr-screencopy-unstable-v1-client-protocol.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

enum screen_capture_state {
    CAPTURE_NOT_REQUESTED,
    CAPTURE_REQUESTED,
    CAPTURE_FAILED,
    CAPTURE_SUCCESS,
};

struct scrcpy_state {
    struct wl_shm                   *wl_shm;
    struct zwlr_screencopy_frame_v1 *wl_screencopy_frame;
    struct scrcpy_buffer            *scrcpy_buffer;
    enum screen_capture_state        screen_capture_state;
};

static struct scrcpy_buffer *create_scrcpy_buffer(
    struct wl_shm *shm, enum wl_shm_format format, uint32_t width,
    uint32_t height, uint32_t stride
) {
    size_t size = stride * height;

    int fd = allocate_shm_file(size);
    if (fd == -1) {
        LOG_ERR("Could not allocate SHM file.");
        return NULL;
    }

    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        LOG_ERR("Could not mmap shared buffer for surface buffer.");

        close(fd);
        return NULL;
    }

    struct wl_shm_pool *wl_shm_pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer   *wl_buffer   = wl_shm_pool_create_buffer(
        wl_shm_pool, 0, width, height, stride, format
    );
    wl_shm_pool_destroy(wl_shm_pool);

    close(fd);

    struct scrcpy_buffer *buffer = malloc(sizeof(struct scrcpy_buffer));

    buffer->wl_buffer = wl_buffer;
    buffer->format    = format;
    buffer->data      = data;
    buffer->width     = width;
    buffer->height    = height;
    buffer->stride    = stride;

    return buffer;
}

void destroy_scrcpy_buffer(struct scrcpy_buffer *buf) {
    if (buf != NULL) {
        munmap(buf->data, buf->stride * buf->height);
        wl_buffer_destroy(buf->wl_buffer);
        free(buf);
    }
}

static void screencopy_frame_handle_buffer(
    void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t format,
    uint32_t width, uint32_t height, uint32_t stride
) {
    struct scrcpy_state *state = data;
    state->scrcpy_buffer =
        create_scrcpy_buffer(state->wl_shm, format, width, height, stride);

    zwlr_screencopy_frame_v1_copy(frame, state->scrcpy_buffer->wl_buffer);
}

static void screencopy_frame_handle_ready(
    void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t tv_sec_hi,
    uint32_t tv_sec_lo, uint32_t tv_nsec
) {
    struct scrcpy_state *state  = data;
    state->screen_capture_state = CAPTURE_SUCCESS;
}

static void screencopy_frame_handle_failed(
    void *data, struct zwlr_screencopy_frame_v1 *frame
) {
    struct scrcpy_state *state  = data;
    state->screen_capture_state = CAPTURE_FAILED;
    LOG_ERR("Could not capture screen.");
}

static void noop() {}

const struct zwlr_screencopy_frame_v1_listener screencopy_frame_listener = {
    .buffer       = screencopy_frame_handle_buffer,
    .flags        = noop,
    .ready        = screencopy_frame_handle_ready,
    .failed       = screencopy_frame_handle_failed,
    .buffer_done  = noop,
    .damage       = noop,
    .linux_dmabuf = noop,
};

struct scrcpy_buffer *
query_screenshot(struct state *state, struct rect region) {
    struct scrcpy_state scrcpy_state;
    scrcpy_state.wl_shm = state->wl_shm;

    if (state->wl_screencopy_manager == NULL) {
        LOG_ERR("Could not load `zwlr_screencopy_manager_v1`.");
        exit(1);
    }

    scrcpy_state.wl_screencopy_frame =
        zwlr_screencopy_manager_v1_capture_output_region(
            state->wl_screencopy_manager, false,
            state->current_output->wl_output, region.x, region.y, region.w,
            region.h
        );
    zwlr_screencopy_frame_v1_add_listener(
        scrcpy_state.wl_screencopy_frame, &screencopy_frame_listener,
        &scrcpy_state
    );

    scrcpy_state.screen_capture_state = CAPTURE_REQUESTED;
    while (scrcpy_state.screen_capture_state == CAPTURE_REQUESTED) {
        wl_display_roundtrip(state->wl_display);
    }

    zwlr_screencopy_frame_v1_destroy(scrcpy_state.wl_screencopy_frame);

    return scrcpy_state.scrcpy_buffer;
}

#endif
