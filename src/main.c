#include "log.h"
#include "state.h"
#include "surface-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <string.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

static void noop() {}

static void handle_registry_global(
    void *data, struct wl_registry *registry, uint32_t name,
    const char *interface, uint32_t version
) {
    struct state *state = data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->wl_compositor =
            wl_registry_bind(registry, name, &wl_compositor_interface, 4);

    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->wl_shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);

    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        state->wl_layer_shell =
            wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 2);

    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        state->wl_seat =
            wl_registry_bind(registry, name, &wl_seat_interface, 7);
        wl_seat_add_listener(state->wl_seat, &wl_seat_listener, state);
    }
}

const struct wl_registry_listener wl_registry_listener = {
    .global        = handle_registry_global,
    .global_remove = noop,
};

int main() {
    struct state state = {
        .wl_display     = NULL,
        .wl_registry    = NULL,
        .wl_compositor  = NULL,
        .wl_shm         = NULL,
        .wl_seat        = NULL,
        .wl_keyboard    = NULL,
        .wl_layer_shell = NULL,
    };

    state.wl_display = wl_display_connect(NULL);
    if (state.wl_display == NULL) {
        LOG_ERR("Failed to connect to Wayland compositor.");
        return 1;
    }

    state.wl_registry = wl_display_get_registry(state.wl_display);
    if (state.wl_registry == NULL) {
        LOG_ERR("Failed to get Wayland registry.");
        return 1;
    }

    wl_registry_add_listener(state.wl_registry, &wl_registry_listener, &state);
    wl_display_roundtrip(state.wl_display);

    if (state.wl_compositor == NULL) {
        LOG_ERR("Failed to get Wayland compositor object.");
        return 1;
    }

    if (state.wl_shm == NULL) {
        LOG_ERR("Failed to get Wayland share memory object.");
        return 1;
    }

    if (state.wl_layer_shell == NULL) {
        LOG_ERR("Failed to get zwlr_layer_shell_v1 object.");
        return 1;
    }

    if (state.wl_seat == NULL) {
        LOG_ERR("Failed to get Wayland seat object.");
        return 1;
    }

    surface_buffer_pool_init(&state.surface_buffer_pool);

    surface_buffer_pool_destroy(&state.surface_buffer_pool);

    wl_seat_destroy(state.wl_seat);
    zwlr_layer_shell_v1_destroy(state.wl_layer_shell);
    wl_shm_destroy(state.wl_shm);
    wl_compositor_destroy(state.wl_compositor);
    wl_registry_destroy(state.wl_registry);
    wl_display_disconnect(state.wl_display);

    return 0;
}
