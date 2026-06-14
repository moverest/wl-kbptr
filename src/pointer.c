// SPDX-License-Identifier: GPL-3.0-only

#include "pointer.h"

#include "state.h"

void move_pointer(
    struct state *state, uint32_t x, uint32_t y, enum click click
) {
    pointer_wlr_move(state, x, y, click);
}
