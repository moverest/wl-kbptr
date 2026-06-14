// SPDX-License-Identifier: GPL-3.0-only

#if KDE_ENABLED

#include "pointer.h"

#include "state.h"

bool pointer_kde_available(struct state *state) {
    (void)state;
    return false;
}

void pointer_kde_move(
    struct state *state, uint32_t x, uint32_t y, enum click click
) {
    (void)state;
    (void)x;
    (void)y;
    (void)click;
}

#endif
