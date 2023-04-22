#include "surface-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <stdbool.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

struct state {
    struct wl_display            *wl_display;
    struct wl_registry           *wl_registry;
    struct wl_compositor         *wl_compositor;
    struct wl_shm                *wl_shm;
    struct wl_seat               *wl_seat;
    struct wl_keyboard           *wl_keyboard;
    struct zwlr_layer_shell_v1   *wl_layer_shell;
    struct surface_buffer_pool    surface_buffer_pool;
    struct wl_surface            *wl_surface;
    struct zwlr_layer_surface_v1 *wl_layer_surface;
    struct xkb_context           *xkb_context;
    struct xkb_keymap            *xkb_keymap;
    struct xkb_state             *xkb_state;
    bool                          running;
    uint32_t                      output_height;
    uint32_t                      output_width;
};
