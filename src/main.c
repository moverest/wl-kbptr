#include "log.h"
#include "mode.h"
#include "state.h"
#include "surface-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

#include <cairo/cairo.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

static void send_frame(struct state *state) {
    const int32_t scale =
        state->current_output == NULL ? 1 : state->current_output->scale;
    struct surface_buffer *surface_buffer = get_next_buffer(
        state->wl_shm, &state->surface_buffer_pool,
        state->surface_width * scale, state->surface_height * scale
    );
    if (surface_buffer == NULL) {
        return;
    }
    surface_buffer->state = SURFACE_BUFFER_BUSY;

    cairo_t *cairo = surface_buffer->cairo;
    cairo_identity_matrix(cairo);
    cairo_scale(cairo, scale, scale);
    state->mode->render(state, cairo);

    wl_surface_set_buffer_scale(state->wl_surface, scale);
    wl_surface_attach(state->wl_surface, surface_buffer->wl_buffer, 0, 0);
    wl_surface_damage(
        state->wl_surface, 0, 0, state->surface_width, state->surface_height
    );
    wl_surface_commit(state->wl_surface);
}

static void noop() {}

static void load_home_row(
    struct xkb_keymap *keymap, char **home_row, char *home_row_buffer
) {
    static const xkb_keycode_t key_codes[] = {
        0x26, // a
        0x27, // s
        0x28, // d
        0x29, // u
        0x2c, // j
        0x2d, // k
        0x2e, // l
        0x2f, // m
    };

    struct xkb_state *xkb_state   = xkb_state_new(keymap);
    char             *buffer      = home_row_buffer;
    size_t            buffer_size = 128;

    for (int i = 0; i < sizeof(key_codes) / sizeof(key_codes[0]); i++) {
        xkb_keysym_t keysym =
            xkb_state_key_get_one_sym(xkb_state, key_codes[i]);
        int char_len = xkb_keysym_to_utf8(keysym, buffer, buffer_size);
        if (char_len < 0) {
            LOG_ERR("Could not load home row keys. Buffer is too small.");
            exit(1);
        }

        if (char_len == 0) {
            LOG_ERR(
                "0x%x symkey does not have a UTF-8 representation in given "
                "keymap.",
                key_codes[i]
            );
            exit(1);
        }

        home_row[i] = buffer;
        buffer      += char_len;
    }

    xkb_state_unref(xkb_state);
}

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

    load_home_row(state->xkb_keymap, state->home_row, state->home_row_buffer);
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

static void free_outputs(struct wl_list *outputs) {
    struct output *output;
    struct output *tmp;
    wl_list_for_each_safe (output, tmp, outputs, link) {
        wl_output_destroy(output->wl_output);
        wl_list_remove(&output->link);
        free(output);
    }
}

static struct output *find_output_from_wl_output(
    struct wl_list *outputs, struct wl_output *wl_output
) {
    struct output *output;
    wl_list_for_each (output, outputs, link) {
        if (wl_output == output->wl_output) {
            return output;
        }
    }

    return NULL;
}

static void
handle_output_scale(void *data, struct wl_output *wl_output, int32_t scale) {
    struct output *output = data;
    output->scale         = scale;
}

const static struct wl_output_listener output_listener = {
    .name        = noop,
    .geometry    = noop,
    .mode        = noop,
    .scale       = handle_output_scale,
    .description = noop,
    .done        = noop,
};

static void handle_surface_enter(
    void *data, struct wl_surface *surface, struct wl_output *wl_output
) {
    struct state  *state = data;
    struct output *output =
        find_output_from_wl_output(&state->outputs, wl_output);
    state->current_output = output;
}

static const struct wl_surface_listener surface_listener = {
    .enter                      = handle_surface_enter,
    .leave                      = noop,
    .preferred_buffer_transform = noop,
    .preferred_buffer_scale     = noop,
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
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        struct wl_output *wl_output =
            wl_registry_bind(registry, name, &wl_output_interface, 3);
        struct output *output = calloc(1, sizeof(struct output));
        output->wl_output     = wl_output;
        output->scale         = 1;
        wl_output_add_listener(output->wl_output, &output_listener, output);
        wl_list_insert(&state->outputs, &output->link);
    } else if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
        state->wl_virtual_pointer_mgr = wl_registry_bind(
            registry, name, &zwlr_virtual_pointer_manager_v1_interface, 2
        );
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
    struct state *state   = data;
    state->surface_width  = width;
    state->surface_height = height;
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

    if (state->mode == NULL) {
        tile_mode_enter(state);
    }

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

static void click(struct state *state) {
    wl_display_roundtrip(state->wl_display);

    struct zwlr_virtual_pointer_v1 *virt_pointer =
        zwlr_virtual_pointer_manager_v1_create_virtual_pointer_with_output(
            state->wl_virtual_pointer_mgr, state->wl_seat,
            state->current_output->wl_output
        );

    int x = state->result.x + state->result.w / 2;
    int y = state->result.y + state->result.h / 2;

    zwlr_virtual_pointer_v1_motion_absolute(
        virt_pointer, 0, x, y, state->surface_width, state->surface_height
    );
    zwlr_virtual_pointer_v1_frame(virt_pointer);
    wl_display_roundtrip(state->wl_display);

    zwlr_virtual_pointer_v1_button(
        virt_pointer, 0, 272, WL_POINTER_BUTTON_STATE_PRESSED
    );
    zwlr_virtual_pointer_v1_frame(virt_pointer);
    wl_display_roundtrip(state->wl_display);

    zwlr_virtual_pointer_v1_button(
        virt_pointer, 0, 272, WL_POINTER_BUTTON_STATE_RELEASED
    );
    zwlr_virtual_pointer_v1_frame(virt_pointer);
    wl_display_roundtrip(state->wl_display);

    zwlr_virtual_pointer_v1_destroy(virt_pointer);
}

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
        .mode             = NULL,
        .result           = (struct rect){-1, -1, -1, -1},
        .home_row         = (char *[]){"", "", "", "", "", "", "", ""},
    };

    wl_list_init(&state.outputs);

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

    if (state.wl_virtual_pointer_mgr == NULL) {
        LOG_ERR("Could not load wlr_virtual_pointer_manager_v1 object.");
        return 1;
    }

    surface_buffer_pool_init(&state.surface_buffer_pool);

    state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
    wl_surface_add_listener(state.wl_surface, &surface_listener, &state);
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

    if (state.result.x != -1) {
        printf(
            "%dx%d+%d+%d\n", state.result.w, state.result.h, state.result.x,
            state.result.y
        );
        click(&state);
    }

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

    free_outputs(&state.outputs);

    zwlr_layer_shell_v1_destroy(state.wl_layer_shell);
    wl_shm_destroy(state.wl_shm);
    wl_compositor_destroy(state.wl_compositor);
    wl_registry_destroy(state.wl_registry);
    wl_display_disconnect(state.wl_display);
    xkb_context_unref(state.xkb_context);

    return 0;
}
