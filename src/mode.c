// SPDX-License-Identifier: GPL-3.0-only

#include "mode.h"

#include "log.h"
#include "utils_cairo.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

extern struct mode_interface tile_mode_interface;
extern struct mode_interface floating_mode_interface;
extern struct mode_interface bisect_mode_interface;
extern struct mode_interface split_mode_interface;
extern struct mode_interface click_mode_interface;
extern struct mode_interface drag_mode_interface;

struct mode_interface *mode_interfaces[] = {
    &tile_mode_interface,  &floating_mode_interface, &bisect_mode_interface,
    &split_mode_interface, &click_mode_interface,    &drag_mode_interface,
    NULL,
};

static struct mode_interface *find_mode_interface_by_name(char *name) {
    struct mode_interface **curr_mode_interface = mode_interfaces;
    while (*curr_mode_interface != NULL) {
        if (strcmp(name, (*curr_mode_interface)->name) == 0) {
            return *curr_mode_interface;
        }

        curr_mode_interface++;
    }

    return NULL;
}

int load_modes(struct state *state, char *modes) {
    char buf[strlen(modes) + 1];
    strcpy(buf, modes);

    int mode_i = 0;

    static const char *delim = ",";
    char              *tok   = strtok(buf, delim);
    while (tok != NULL) {
        if (mode_i >= MAX_NUM_MODES) {
            LOG_ERR("Only %d modes can be specified.", MAX_NUM_MODES);
            return 1;
        }

        struct mode_interface *mode_interface =
            find_mode_interface_by_name(tok);
        if (mode_interface == NULL) {
            LOG_ERR("Unknown mode '%s'.", tok);
            return 2;
        }

        state->mode_interfaces[mode_i++] = mode_interface;
        tok                              = strtok(NULL, delim);
    }

    if (mode_i < MAX_NUM_MODES) {
        state->mode_interfaces[mode_i] = NULL;
    }

    state->current_mode = NO_MODE_ENTERED;
    return 0;
}

void enter_next_mode(struct state *state, struct rect area) {
    if (has_last_mode_returned(state)) {
        return;
    }

    state->current_mode += 1;

    if (has_last_mode_returned(state)) {
        memcpy(&state->result, &area, sizeof(struct rect));
        return;
    }

    state->mode_states[state->current_mode] =
        state->mode_interfaces[state->current_mode]->enter(state, area);

    if (state->pending_drag_restart) {
        state->pending_drag_restart = false;
        int drag_idx                = state->current_mode;
        for (int i = 0; i < drag_idx; i++) {
            state->mode_interfaces[i]->restart(state, state->mode_states[i]);
        }
        state->current_mode = 0;
    }
}

bool has_last_mode_returned(struct state *state) {
    return state->current_mode >= MAX_NUM_MODES ||
           state->mode_interfaces[state->current_mode] == NULL;
}

bool reenter_prev_mode(struct state *state) {
    if (state->current_mode <= 0) {
        return false;
    }

    void *current_mode_state = state->mode_states[state->current_mode];
    if (current_mode_state != NULL) {
        state->mode_interfaces[state->current_mode]->free(current_mode_state);
    }

    state->current_mode--;
    state->mode_interfaces[state->current_mode]->reenter(
        state, state->mode_states[state->current_mode]
    );

    return true;
}

void free_mode_states(struct state *state) {
    if (state->current_mode == NO_MODE_ENTERED) {
        return;
    }

    for (int i = 0; i <= state->current_mode; i++) {
        if (state->mode_interfaces[i] == NULL ||
            state->mode_states[i] == NULL) {
            return;
        }

        state->mode_interfaces[i]->free(state->mode_states[i]);
    }
}

bool mode_handle_key(struct state *state, xkb_keysym_t sym, char *text) {
    if (has_last_mode_returned(state)) {
        return false;
    }

    return state->mode_interfaces[state->current_mode]->key(
        state, state->mode_states[state->current_mode], sym, text
    );
}
void mode_render(struct state *state, cairo_t *cairo) {
    if (has_last_mode_returned(state)) {
        return;
    }

    state->mode_interfaces[state->current_mode]->render(
        state, state->mode_states[state->current_mode], cairo
    );

    if (state->drag_phase == 1) {
        double                  s      = state->config.mode_drag.start_marker_size;
        enum drag_marker_shape  shape  = state->config.mode_drag.start_marker_shape;
        double                  x      = state->drag_start_x;
        double                  y      = state->drag_start_y;

        cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
        cairo_set_source_u32(cairo, state->config.mode_drag.start_marker_color);

        if (shape == DRAG_MARKER_CIRCLE) {
            cairo_arc(cairo, x, y, s, 0, 2 * M_PI);
            cairo_fill(cairo);
        } else {
            // Caret: a vertical bar with serifs, like a text insertion cursor.
            // Bar: width = s/4 (min 1.5), height = s*2, centred on (x, y).
            double bar_w = s / 4.0 < 1.5 ? 1.5 : s / 4.0;
            double bar_h = s * 2.0;
            double serif_w = s * 0.75;
            cairo_set_line_width(cairo, bar_w);
            cairo_set_line_cap(cairo, CAIRO_LINE_CAP_SQUARE);
            // Vertical stroke
            cairo_move_to(cairo, x, y - bar_h / 2);
            cairo_line_to(cairo, x, y + bar_h / 2);
            cairo_stroke(cairo);
            // Top serif
            cairo_move_to(cairo, x - serif_w / 2, y - bar_h / 2);
            cairo_line_to(cairo, x + serif_w / 2, y - bar_h / 2);
            cairo_stroke(cairo);
            // Bottom serif
            cairo_move_to(cairo, x - serif_w / 2, y + bar_h / 2);
            cairo_line_to(cairo, x + serif_w / 2, y + bar_h / 2);
            cairo_stroke(cairo);
        }
    }
}
