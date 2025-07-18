#ifndef __UTILS_WAYLAND_H_INCLUDED__
#define __UTILS_WAYLAND_H_INCLUDED__

#include "state.h"

void move_pointer(
    struct state *state, uint32_t x, uint32_t y, enum click click
);

#endif
