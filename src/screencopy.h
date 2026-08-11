// SPDX-License-Identifier: GPL-3.0-only

#ifndef __SCREENCOPY_H_INCLUDED__
#define __SCREENCOPY_H_INCLUDED__

#if OPENCV_ENABLED

#include <wayland-client.h>

// A captured screen region. `wl_buffer != NULL` means `data` is an mmap'd SHM
// buffer (wlroots screencopy path); `wl_buffer == NULL` means `data` was
// malloc'd (KWin ScreenShot2 path). destroy_scrcpy_buffer() uses this to free
// correctly.
struct scrcpy_buffer {
    struct wl_buffer  *wl_buffer;
    void              *data;
    enum wl_shm_format format;
    int32_t            width;
    int32_t            height;
    int32_t            stride;
};

struct state;
struct rect;

// Dispatcher: picks the wlroots or KWin backend at runtime.
struct scrcpy_buffer *query_screenshot(struct state *state, struct rect region);

// wlroots (zwlr_screencopy) backend.
struct scrcpy_buffer *
query_screenshot_wlr(struct state *state, struct rect region);

void destroy_scrcpy_buffer(struct scrcpy_buffer *buf);

#endif

#endif
