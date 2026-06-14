// SPDX-License-Identifier: GPL-3.0-only

#ifndef __POINTER_H_INCLUDED__
#define __POINTER_H_INCLUDED__

#include "state.h"

#include <stdbool.h>
#include <stdint.h>

// Public seam used by main.c. Selects a backend at call time and performs the
// absolute pointer move (plus click, taken from state->click).
void move_pointer(
    struct state *state, uint32_t x, uint32_t y, enum click click
);

// wlroots backend (zwlr_virtual_pointer). Always built.
void pointer_wlr_move(
    struct state *state, uint32_t x, uint32_t y, enum click click
);

#if KDE_ENABLED
// KDE backend (EIS + libei). Built only with -Dkde=enabled.
bool pointer_kde_available(struct state *state);
void pointer_kde_move(
    struct state *state, uint32_t x, uint32_t y, enum click click
);
#endif

#endif
