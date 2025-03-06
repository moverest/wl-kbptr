#ifndef __SCREENCOPY_H_INCLUDED__
#define __SCREENCOPY_H_INCLUDED__

#if OPENCV_ENABLED

#include <wayland-client.h>

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
struct scrcpy_buffer *query_screenshot(struct state *state, struct rect region);

void destroy_scrcpy_buffer(struct scrcpy_buffer *buf);

#endif

#endif
