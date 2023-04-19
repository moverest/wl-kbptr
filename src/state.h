#include <wayland-client.h>

struct state {
	struct wl_display *wl_display;
	struct wl_registry *wl_registry;
	struct wl_compositor *wl_compositor;
	struct wl_shm *wl_shm;
	struct wl_seat *wl_seat;
	struct wl_keyboard *wl_keyboard;
	struct zwlr_layer_shell_v1 *wl_layer_shell;
};
