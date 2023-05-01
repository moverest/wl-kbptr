#include "mode.h"
#include "state.h"
#include "utils.h"

#include <cairo.h>
#include <stdbool.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>

void tile_mode_enter(struct state *state) {
    state->mode = &tile_mode_interface;

    memset(
        state->mode_state.tile.area_selection, NO_AREA_SELECTION,
        sizeof(state->mode_state.tile.area_selection) /
            sizeof(state->mode_state.tile.area_selection[0])
    );

    const int max_num_sub_areas = 8 * 8 * 8;
    const int area_size         = state->surface_width * state->surface_height;
    const int sub_area_size     = area_size / max_num_sub_areas;

    struct tile_mode_state *ms = &state->mode_state.tile;

    ms->sub_area_height     = sqrt(sub_area_size / 2.);
    ms->sub_area_rows       = state->surface_height / ms->sub_area_height;
    ms->sub_area_height_off = state->surface_height % ms->sub_area_rows;
    ms->sub_area_height     = state->surface_height / ms->sub_area_rows;

    ms->sub_area_width     = sqrt(sub_area_size * 2);
    ms->sub_area_columns   = state->surface_width / ms->sub_area_width;
    ms->sub_area_width_off = state->surface_width % ms->sub_area_columns;
    ms->sub_area_width     = state->surface_width / ms->sub_area_columns;
}

// `tile_mode_back` goes back in history. Returns true if there was something to
// go back to.
static bool tile_mode_back(struct tile_mode_state *mode_state) {
    int i;
    for (i = 0; i < 3 && mode_state->area_selection[i] != NO_AREA_SELECTION;
         i++) {
        ;
    }
    if (i > 0) {
        mode_state->area_selection[i - 1] = NO_AREA_SELECTION;
        return true;
    }

    return false;
}

// `tile_mode_reenter` reenters the tile mode. We assume that the saved state is
// valid and goes back in history once.
void tile_mode_reenter(struct state *state) {
    state->mode = &tile_mode_interface;
    tile_mode_back(&state->mode_state.tile);
}

static void idx_to_label(
    int idx, int num, char *selection, char **home_row, char *label_selected,
    char *label_unselected
) {
    label_selected[0]   = 0;
    label_unselected[0] = 0;

    bool  unselected_part = false;
    char *curr_label      = label_selected;

    for (int i = 0; i < num; i++) {
        if (!unselected_part && selection[i] == NO_AREA_SELECTION) {
            curr_label      = label_unselected;
            unselected_part = true;
        }

        curr_label = stpcpy(curr_label, home_row[idx % 8]);
        idx        /= 8;
    }
}

static bool selectable_area(int idx, int num, char selection[]) {
    for (int i = 0; i < num; i++) {
        if (selection[i] == NO_AREA_SELECTION) {
            return true;
        }

        if (selection[i] != idx % 8) {
            return false;
        }
        idx /= 8;
    }

    return true;
}

static int get_selected_area_idx(struct tile_mode_state *mode_state) {
    int idx  = 0;
    int base = 1;
    for (int i = 0; i < 3; i++) {
        if (mode_state->area_selection[i] == NO_AREA_SELECTION) {
            return -1;
        }
        idx  += base * mode_state->area_selection[i];
        base *= 8;
    }

    return idx;
}

static struct rect idx_to_rect(struct tile_mode_state *mode_state, int idx) {
    int column = idx / mode_state->sub_area_rows;
    int row    = idx % mode_state->sub_area_rows;

    return (struct rect){
        .x = column * mode_state->sub_area_width +
             min(column, mode_state->sub_area_width_off),
        .w = mode_state->sub_area_width +
             (column < mode_state->sub_area_width_off ? 1 : 0),
        .y = row * mode_state->sub_area_height +
             min(row, mode_state->sub_area_height_off),
        .h = mode_state->sub_area_height +
             (row < mode_state->sub_area_height_off ? 1 : 0),
    };
}

static bool
tile_mode_key(struct state *state, xkb_keysym_t keysym, char *text) {
    struct tile_mode_state *ms = &state->mode_state.tile;

    switch (keysym) {
    case XKB_KEY_BackSpace:
        return tile_mode_back(ms);
        break;

    case XKB_KEY_Escape:
        state->running = false;
        break;
    default:;
        int matched_i = find_str(state->home_row, HOME_ROW_LEN, text);

        if (matched_i != -1) {
            for (int i = 0; i < 3; i++) {
                if (ms->area_selection[i] == NO_AREA_SELECTION) {
                    ms->area_selection[i] = matched_i;

                    if (i == 2) {
                        bisect_mode_enter(
                            state, idx_to_rect(ms, get_selected_area_idx(ms))
                        );
                    }
                    return true;
                }
            }
        }
    }

    return false;
}

void tile_mode_render(struct state *state, cairo_t *cairo) {

    char                    label_selected[32];
    char                    label_unselected[32];
    int                     count = 0;
    struct tile_mode_state *ms    = &state->mode_state.tile;

    cairo_select_font_face(
        cairo, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL
    );
    cairo_set_font_size(cairo, (int)(ms->sub_area_height / 2));

    for (int i = 0; i < ms->sub_area_columns; i++) {
        for (int j = 0; j < ms->sub_area_rows; j++) {
            const int x =
                i * ms->sub_area_width + min(i, ms->sub_area_width_off);
            const int w =
                ms->sub_area_width + (i < ms->sub_area_width_off ? 1 : 0);
            const int y =
                j * ms->sub_area_height + min(j, ms->sub_area_height_off);
            const int h =
                ms->sub_area_height + (j < ms->sub_area_height_off ? 1 : 0);

            const bool selectable = selectable_area(
                count, 3, state->mode_state.tile.area_selection
            );

            cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
            if (selectable) {
                cairo_set_source_rgba(cairo, 0, .2, 0, .3);
            } else {
                cairo_set_source_rgba(cairo, .2, .2, .2, .3);
            }
            cairo_rectangle(cairo, x, y, w, h);
            cairo_fill(cairo);
            if (selectable) {
                cairo_set_source_rgba(cairo, 0, .3, 0, .7);
                cairo_rectangle(cairo, x + .5, y + .5, w - 1, h - 1);
                cairo_set_line_width(cairo, 1);
                cairo_stroke(cairo);

                idx_to_label(
                    count, 3, state->mode_state.tile.area_selection,
                    state->home_row, label_selected, label_unselected
                );

                cairo_text_extents_t te_selected, te_unselected;
                cairo_text_extents(cairo, label_selected, &te_selected);
                cairo_text_extents(cairo, label_unselected, &te_unselected);

                // Centers the label.
                cairo_move_to(
                    cairo,
                    x + (w - te_selected.x_advance - te_unselected.x_advance) /
                            2,
                    y + (int
                        )((h + max(te_selected.height, te_unselected.height)) /
                          2)
                );
                cairo_set_source_rgba(cairo, 1, .8, 0, .9);
                cairo_show_text(cairo, label_selected);
                cairo_set_source_rgba(cairo, 1, 1, 1, .9);
                cairo_show_text(cairo, label_unselected);
            }

            count++;
        }
    }
}

struct mode_interface tile_mode_interface = {
    .key    = tile_mode_key,
    .render = tile_mode_render,
};
