// SPDX-License-Identifier: GPL-3.0-only

#ifndef __POINTER_H_INCLUDED__
#define __POINTER_H_INCLUDED__

#include "state.h"

#include <stdbool.h>
#include <stdint.h>

// Public seam used by main.c and the interactive modes. Selects a backend at
// call time and performs the absolute pointer move, followed by `click` unless
// that is CLICK_NONE (the bisect/split modes pass CLICK_NONE to track the
// cursor while narrowing down a target).
void move_pointer(
    struct state *state, uint32_t x, uint32_t y, enum click click
);

// wlroots backend (zwlr_virtual_pointer). Always built.
void pointer_wlr_move(
    struct state *state, uint32_t x, uint32_t y, enum click click
);

#if KWIN_ENABLED
// KWin backend (EIS + libei). Built only with -Dkwin=enabled.
//
// The EIS session is opened lazily on the first move and reused for every
// subsequent move/click; pointer_kwin_destroy() tears it down at exit. A failed
// connection is remembered so we don't retry (and re-prompt) on every keystroke.
void pointer_kwin_move(
    struct state *state, uint32_t x, uint32_t y, enum click click
);
void pointer_kwin_destroy(struct state *state);
#endif

#endif
