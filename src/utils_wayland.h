// SPDX-License-Identifier: GPL-3.0-only

#ifndef __UTILS_WAYLAND_H_INCLUDED__
#define __UTILS_WAYLAND_H_INCLUDED__

#include "state.h"

void move_pointer(
    struct state *state, uint32_t x, uint32_t y, enum click click
);

void drag_pointer(
    struct state *state, uint32_t start_x, uint32_t start_y, uint32_t end_x,
    uint32_t end_y
);

#endif
