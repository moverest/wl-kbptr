// SPDX-License-Identifier: GPL-3.0-only

#include "mode.h"
#include "state.h"
#include "utils.h"

#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

static void *drag_mode_enter(struct state *state, struct rect area) {
    if (state->drag_phase == 0) {
        state->drag_start_x          = area.x;
        state->drag_start_y          = area.y + area.h / 2;
        state->drag_phase            = 1;
        state->pending_drag_restart  = true;
    } else {
        state->drag_phase = 0;
        state->click      = CLICK_DRAG;
        struct rect end_point = {
            .x = area.x + area.w - 1,
            .y = area.y + area.h / 2,
            .w = 1,
            .h = 1,
        };
        enter_next_mode(state, end_point);
    }
    return NULL;
}

static void drag_mode_reenter(struct state *state, void *mode_state) {
    // Going back from drag phase 1 resets to phase 0.
    if (state->drag_phase == 1) {
        state->drag_phase = 0;
    }
    reenter_prev_mode(state);
}

static bool drag_mode_key(
    struct state *state, void *mode_state, xkb_keysym_t keysym, char *text
) {
    return false;
}

static void drag_mode_render(
    struct state *state, void *mode_state, cairo_t *cairo
) {}

static void drag_mode_free(void *mode_state) {}

static void drag_mode_restart(struct state *state, void *mode_state) {}

struct mode_interface drag_mode_interface = {
    .name    = "drag",
    .enter   = drag_mode_enter,
    .reenter = drag_mode_reenter,
    .key     = drag_mode_key,
    .render  = drag_mode_render,
    .free    = drag_mode_free,
    .restart = drag_mode_restart,
};
