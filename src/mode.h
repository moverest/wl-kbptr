#ifndef __MODE_H_INCLUDED__
#define __MODE_H_INCLUDED__

#include "state.h"

#include <cairo.h>
#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

struct mode_interface {
    bool (*key)(struct state *, xkb_keysym_t, char *text);
    void (*render)(struct state *, cairo_t *);
};

extern struct mode_interface tile_mode_interface;
extern struct mode_interface bisect_mode_interface;

void tile_mode_enter(struct state *state);
void tile_mode_reenter(struct state *state);
void tile_mode_state_free(struct tile_mode_state *tms);
void bisect_mode_enter(struct state *state, struct rect area);

#endif
