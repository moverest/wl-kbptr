// SPDX-License-Identifier: GPL-3.0-only

#ifndef __SCREENCOPY_H_INCLUDED__
#define __SCREENCOPY_H_INCLUDED__

#if OPENCV_ENABLED

#include <wayland-client.h>

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
struct scrcpy_buffer *query_screenshot(
    struct state *state, struct wl_output *wl_output, struct rect region
);

void destroy_scrcpy_buffer(struct scrcpy_buffer *buf);

#endif

#endif
