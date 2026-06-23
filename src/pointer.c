// SPDX-License-Identifier: GPL-3.0-only

#include "pointer.h"

#include "state.h"

void move_pointer(
    struct state *state, uint32_t x, uint32_t y, enum click click
) {
    if (state->wl_virtual_pointer_mgr != NULL) {
        pointer_wlr_move(state, x, y, click);
        return;
    }

#if KWIN_ENABLED
    // No wlroots virtual pointer: try the KWin backend. It opens (and caches)
    // its own EIS session and is a no-op if that is unavailable.
    pointer_kwin_move(state, x, y, click);
    return;
#endif

    // No usable pointer backend: nothing to do (e.g. --only-print).
}
