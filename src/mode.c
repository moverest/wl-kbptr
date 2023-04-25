#include "mode.h"

#include "state.h"

#include <cairo.h>
#include <stdbool.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>

// TODO: Generate this from the keyboard key map we get when it's loaded.
// This is the home row for the Dvorak layout.
const char *home_row[] = {"a", "o", "e", "u", "h", "t", "n", "s"};

void tile_mode_enter(struct state *state) {
    memset(
        state->mode_state.tile.area_selection, NO_AREA_SELECTION,
        sizeof(state->mode_state.tile.area_selection) /
            sizeof(state->mode_state.tile.area_selection[0])
    );
}

static int min(int a, int b) {
    return a < b ? a : b;
}

static int max(int a, int b) {
    return a > b ? a : b;
}

static void idx_to_label(
    int idx, int num, char *selection, char *label_selected,
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

bool tile_mode_key(struct state *state, xkb_keysym_t keysym, char *text) {
    switch (keysym) {
    case XKB_KEY_BackSpace:;
        int i;
        for (i = 0; i < 3 && state->mode_state.tile.area_selection[i] !=
                                 NO_AREA_SELECTION;
             i++) {
            ;
        }
        if (i > 0) {
            state->mode_state.tile.area_selection[i - 1] = NO_AREA_SELECTION;
            return true;
        }
        break;

    case XKB_KEY_Escape:
        state->running = false;
        break;
    default:;
        int matched_i = -1;
        for (int i = 0; i < (sizeof(home_row) / sizeof(home_row[0])); i++) {
            if (strcmp(text, home_row[i]) == 0) {
                matched_i = i;
                break;
            }
        }

        if (matched_i != -1) {
            for (int i = 0; i < 3; i++) {
                if (state->mode_state.tile.area_selection[i] ==
                    NO_AREA_SELECTION) {
                    state->mode_state.tile.area_selection[i] = matched_i;
                    return true;
                }
            }
        }
    }

    return false;
}

void tile_mode_render(struct state *state, cairo_t *cairo) {
    const int max_num_sub_areas = 8 * 8 * 8;
    const int area_size         = state->output_width * state->output_height;
    const int sub_area_size     = area_size / max_num_sub_areas;

    int       sub_area_height     = sqrt(sub_area_size / 2.);
    const int sub_area_rows       = state->output_height / sub_area_height;
    const int sub_area_height_off = state->output_height % sub_area_rows;
    sub_area_height               = state->output_height / sub_area_rows;

    int       sub_area_width     = sqrt(sub_area_size * 2);
    const int sub_area_columns   = state->output_width / sub_area_width;
    const int sub_area_width_off = state->output_width % sub_area_columns;
    sub_area_width               = state->output_width / sub_area_columns;

    char label_selected[32];
    char label_unselected[32];
    int  count = 0;

    cairo_select_font_face(
        cairo, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL
    );
    cairo_set_font_size(cairo, (int)(sub_area_height / 2));

    for (int i = 0; i < sub_area_columns; i++) {
        for (int j = 0; j < sub_area_rows; j++) {
            const int x = i * sub_area_width + min(i, sub_area_width_off);
            const int w = sub_area_width + (i < sub_area_width_off ? 1 : 0);
            const int y = j * sub_area_height + min(j, sub_area_height_off);
            const int h = sub_area_height + (j < sub_area_height_off ? 1 : 0);

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
                    label_selected, label_unselected
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
