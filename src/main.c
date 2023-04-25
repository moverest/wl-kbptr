#include "log.h"
#include "mode.h"
#include "state.h"
#include "surface-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <cairo/cairo.h>
#include <math.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

static void send_frame(struct state *state) {
    struct surface_buffer *surface_buffer = get_next_buffer(
        state->wl_shm, &state->surface_buffer_pool, state->output_width,
        state->output_height
    );
    if (surface_buffer == NULL) {
        return;
    }
    surface_buffer->state = SURFACE_BUFFER_BUSY;

    cairo_t *cairo = surface_buffer->cairo;
    cairo_identity_matrix(cairo);
    state->mode->render(state, cairo);

    wl_surface_attach(state->wl_surface, surface_buffer->wl_buffer, 0, 0);
    wl_surface_damage(
        state->wl_surface, 0, 0, state->output_width, state->output_height
    );
    wl_surface_commit(state->wl_surface);
}

static void noop() {}

static void handle_keyboard_keymap(
    void *data, struct wl_keyboard *keyboard, uint32_t format, int fd,
    uint32_t size
) {
    struct state *state = data;
    if (state->xkb_state != NULL) {
        xkb_state_unref(state->xkb_state);
        state->xkb_state = NULL;
    }
    if (state->xkb_keymap != NULL) {
        xkb_keymap_unref(state->xkb_keymap);
        state->xkb_keymap = NULL;
    }

    switch (format) {
    case WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP:
        state->xkb_keymap = xkb_keymap_new_from_names(
            state->xkb_context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS
        );
        break;

    case WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1:;
        void *buffer = mmap(NULL, size - 1, PROT_READ, MAP_PRIVATE, fd, 0);
        if (buffer == MAP_FAILED) {
            LOG_ERR("Could not mmap keymap data.");
            return;
        }

        state->xkb_keymap = xkb_keymap_new_from_buffer(
            state->xkb_context, buffer, size - 1, XKB_KEYMAP_FORMAT_TEXT_V1,
            XKB_KEYMAP_COMPILE_NO_FLAGS
        );

        munmap(buffer, size - 1);
        close(fd);
        break;
    }

    state->xkb_state = xkb_state_new(state->xkb_keymap);
}

static void handle_keyboard_key(
    void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time,
    uint32_t key, uint32_t key_state
) {
    struct state       *state = data;
    char                text[64];
    const xkb_keycode_t key_code = key + 8;
    const xkb_keysym_t  key_sym =
        xkb_state_key_get_one_sym(state->xkb_state, key_code);
    xkb_keysym_to_utf8(key_sym, text, sizeof(text));

    if (key_state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        bool redraw = state->mode->key(state, key_sym, text);
        if (redraw) {
            send_frame(state);
        }
    }
}

static void handle_keyboard_modifiers(
    void *data, struct wl_keyboard *keyboard, uint32_t serial,
    uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked,
    uint32_t group
) {
    struct state *state = data;
    xkb_state_update_mask(
        state->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group
    );
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
    .keymap      = handle_keyboard_keymap,
    .enter       = noop,
    .leave       = noop,
    .key         = handle_keyboard_key,
    .modifiers   = handle_keyboard_modifiers,
    .repeat_info = noop,
};

static void handle_seat_capabilities(
    void *data, struct wl_seat *wl_seat, uint32_t capabilities
) {
    struct state *state = data;
    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
        state->wl_keyboard = wl_seat_get_keyboard(wl_seat);
        wl_keyboard_add_listener(
            state->wl_keyboard, &wl_keyboard_listener, data
        );
    }
}

const struct wl_seat_listener wl_seat_listener = {
    .name         = noop,
    .capabilities = handle_seat_capabilities,
};

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

static void handle_layer_surface_configure(
    void *data, struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
    uint32_t width, uint32_t height
) {
    struct state *state  = data;
    state->output_width  = width;
    state->output_height = height;
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
    send_frame(state);
}

static void handle_layer_surface_closed(
    void *data, struct zwlr_layer_surface_v1 *layer_surface
) {
    struct state *state = data;
    state->running      = false;
}

const struct zwlr_layer_surface_v1_listener wl_layer_surface_listener = {
    .configure = handle_layer_surface_configure,
    .closed    = handle_layer_surface_closed,
};

int main() {
    struct state state = {
        .wl_display       = NULL,
        .wl_registry      = NULL,
        .wl_compositor    = NULL,
        .wl_shm           = NULL,
        .wl_seat          = NULL,
        .wl_keyboard      = NULL,
        .wl_layer_shell   = NULL,
        .wl_surface       = NULL,
        .wl_layer_surface = NULL,
        .xkb_context      = NULL,
        .xkb_keymap       = NULL,
        .xkb_state        = NULL,
        .running          = true,
        .mode             = &tile_mode_interface,
    };

    tile_mode_enter(&state);

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

    state.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (state.xkb_context == NULL) {
        LOG_ERR("Could not create XKB context.");
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

    state.wl_surface       = wl_compositor_create_surface(state.wl_compositor);
    state.wl_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        state.wl_layer_shell, state.wl_surface, NULL,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "wl-kbptr"
    );
    zwlr_layer_surface_v1_add_listener(
        state.wl_layer_surface, &wl_layer_surface_listener, &state
    );
    zwlr_layer_surface_v1_set_anchor(
        state.wl_layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                                    ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                                    ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                    ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
    );
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        state.wl_layer_surface, true
    );
    wl_surface_commit(state.wl_surface);
    while (state.running && wl_display_dispatch(state.wl_display)) {}

    wl_surface_destroy(state.wl_surface);
    zwlr_layer_surface_v1_destroy(state.wl_layer_surface);

    surface_buffer_pool_destroy(&state.surface_buffer_pool);

    if (state.wl_keyboard != NULL) {
        wl_keyboard_destroy(state.wl_keyboard);
    }

    if (state.xkb_state != NULL) {
        xkb_state_unref(state.xkb_state);
    }
    if (state.xkb_keymap != NULL) {
        xkb_keymap_unref(state.xkb_keymap);
    }
    wl_seat_destroy(state.wl_seat);

    zwlr_layer_shell_v1_destroy(state.wl_layer_shell);
    wl_shm_destroy(state.wl_shm);
    wl_compositor_destroy(state.wl_compositor);
    wl_registry_destroy(state.wl_registry);
    wl_display_disconnect(state.wl_display);
    xkb_context_unref(state.xkb_context);

    return 0;
}
