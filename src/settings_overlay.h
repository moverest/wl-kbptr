// SPDX-License-Identifier: GPL-3.0-only

#ifndef __SETTINGS_OVERLAY_H_INCLUDED__
#define __SETTINGS_OVERLAY_H_INCLUDED__

#include "config.h"

#include <cairo.h>
#include <stdbool.h>
#include <stdio.h>
#include <xkbcommon/xkbcommon.h>

struct state;

/**
 * `settings_overlay_init` builds the editable field list from the live
 * configuration and snapshots its values so changes can be tracked. Returns
 * non-zero on failure.
 */
int settings_overlay_init(struct state *state);

/**
 * `settings_overlay_handle_key` handles a key press while the overlay is open.
 * Returns whether the surface needs to be redrawn.
 */
bool settings_overlay_handle_key(
    struct state *state, xkb_keysym_t keysym, char *text
);

/**
 * `settings_overlay_render` draws the settings panel on top of the current
 * mode's rendering.
 */
void settings_overlay_render(struct state *state, cairo_t *cairo);

/**
 * `settings_overlay_save` writes the changed fields back to the configuration
 * file, preserving the comments and ordering of the existing one. Returns
 * non-zero on failure.
 */
int settings_overlay_save(struct state *state);

void settings_overlay_free(struct state *state);

/**
 * `settings_overlay_patch_config` copies `in` to `out`, replacing the values of
 * the fields listed in `changes` and appending the ones it did not find. `in`
 * may be NULL when there is no existing configuration file. Exposed for
 * testing.
 */
void settings_overlay_patch_config(
    FILE *in, FILE *out, const struct config_field_ref *changes,
    const char **values, int num_changes
);

#endif
