#ifndef __SURFACE_STATE_H_INCLUDED__
#define __SURFACE_STATE_H_INCLUDED__

#include "surface-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

#include <stdbool.h>
#include <wayland-client.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon.h>

#define NO_AREA_SELECTION   -1
#define HOME_ROW_LEN        8
#define HOME_ROW_BUFFER_LEN 128
#define BISECT_MAX_HISTORY  10

struct mode_interface;

struct tile_mode_state {
    int sub_area_rows;
    int sub_area_width;
    int sub_area_width_off;

    int sub_area_columns;
    int sub_area_height;
    int sub_area_height_off;

    char area_selection[3];
};

struct rect {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
};

struct bisect_mode_state {
    struct rect areas[BISECT_MAX_HISTORY];
    int         current;
};

struct output {
    struct wl_list    link; // type: struct output
    struct wl_output *wl_output;
    int32_t           scale;
};

struct state {
    struct wl_display                      *wl_display;
    struct wl_registry                     *wl_registry;
    struct wl_compositor                   *wl_compositor;
    struct wl_shm                          *wl_shm;
    struct wl_seat                         *wl_seat;
    struct wl_keyboard                     *wl_keyboard;
    struct zwlr_layer_shell_v1             *wl_layer_shell;
    struct zwlr_virtual_pointer_manager_v1 *wl_virtual_pointer_mgr;
    struct surface_buffer_pool              surface_buffer_pool;
    struct wl_surface                      *wl_surface;
    struct zwlr_layer_surface_v1           *wl_layer_surface;
    struct xkb_context                     *xkb_context;
    struct xkb_keymap                      *xkb_keymap;
    struct xkb_state                       *xkb_state;
    struct wl_list                          outputs;
    struct output                          *current_output;
    bool                                    running;
    uint32_t                                surface_height;
    uint32_t                                surface_width;
    char                   home_row_buffer[HOME_ROW_BUFFER_LEN];
    char                 **home_row;
    struct rect            result;
    struct mode_interface *mode;
    union {
        struct tile_mode_state   tile;
        struct bisect_mode_state bisect;
    } mode_state;
};

#endif
