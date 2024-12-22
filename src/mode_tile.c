#include "config.h"
#include "log.h"
#include "mode.h"
#include "state.h"
#include "utils.h"

#include <cairo.h>
#include <stdbool.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>

static char determine_label_length(struct tile_mode_state *ms) {
    int areas = ms->sub_area_columns * ms->sub_area_rows;
    if (areas <= 8) {
        return 1;
    } else if (areas <= 8 * 8) {
        return 2;
    } else {
        return 3;
    }
}

void tile_mode_enter(struct state *state) {
    state->mode = &tile_mode_interface;

    memset(
        state->mode_state.tile.area_selection, NO_AREA_SELECTION,
        sizeof(state->mode_state.tile.area_selection) /
            sizeof(state->mode_state.tile.area_selection[0])
    );

    if (state->initial_area.w == -1) {
        state->initial_area.x = 0;
        state->initial_area.y = 0;
        state->initial_area.w = state->surface_width;
        state->initial_area.h = state->surface_height;
    } else {
        if (state->initial_area.x < 0) {
            state->initial_area.w += state->initial_area.x;
            state->initial_area.x  = 0;
        }

        if (state->initial_area.y < 0) {
            state->initial_area.h += state->initial_area.y;
            state->initial_area.y  = 0;
        }

        if (state->initial_area.w + state->initial_area.x >
            state->current_output->width) {
            state->initial_area.w =
                state->current_output->width - state->initial_area.x;
        }

        if (state->initial_area.h + state->initial_area.y >
            state->current_output->height) {
            state->initial_area.h =
                state->current_output->height - state->initial_area.y;
        }
    }

    if (state->initial_area.w <= 0 || state->initial_area.h <= 0) {
        state->running = false;
        LOG_ERR(
            "Initial area (%dx%d) is too small.", state->initial_area.w,
            state->initial_area.h
        );
        return;
    }

    const struct mode_tile_config *config = &state->config.mode_tile;
    const int area_size = state->initial_area.w * state->initial_area.h;
    const int sub_area_size =
        max(area_size / config->max_num_sub_areas, config->min_sub_area_size);

    struct tile_mode_state *ms = &state->mode_state.tile;

    ms->sub_area_height = sqrt(sub_area_size / 2.);
    ms->sub_area_rows   = state->initial_area.h / ms->sub_area_height;
    if (ms->sub_area_rows == 0) {
        ms->sub_area_rows = 1;
    }
    ms->sub_area_height_off = state->initial_area.h % ms->sub_area_rows;
    ms->sub_area_height     = state->initial_area.h / ms->sub_area_rows;

    ms->sub_area_width   = sqrt(sub_area_size * 2);
    ms->sub_area_columns = state->initial_area.w / ms->sub_area_width;
    if (ms->sub_area_columns == 0) {
        ms->sub_area_columns = 1;
    }
    ms->sub_area_width_off = state->initial_area.w % ms->sub_area_columns;
    ms->sub_area_width     = state->initial_area.w / ms->sub_area_columns;

    ms->label_length = determine_label_length(ms);
}

// `tile_mode_back` goes back in history. Returns true if there was something to
// go back to.
static bool tile_mode_back(struct tile_mode_state *mode_state) {
    int i;
    for (i = 0; i < mode_state->label_length &&
                mode_state->area_selection[i] != NO_AREA_SELECTION;
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
    int idx, int num, int8_t *selection, char **home_row, char *label_selected,
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

        curr_label  = stpcpy(curr_label, home_row[idx % 8]);
        idx        /= 8;
    }
}

static bool selectable_area(int idx, int num, int8_t selection[]) {
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

#define INCOMPLETE_AREA_SELECTION -1
#define OUT_OF_RANGE_AREA_IDX     -2

static int get_selected_area_idx(struct tile_mode_state *mode_state) {
    int idx  = 0;
    int base = 1;
    for (int i = 0; i < mode_state->label_length; i++) {
        if (mode_state->area_selection[i] == NO_AREA_SELECTION) {
            return INCOMPLETE_AREA_SELECTION;
        }
        idx  += base * mode_state->area_selection[i];
        base *= 8;
    }

    if (idx >= mode_state->sub_area_rows * mode_state->sub_area_columns) {
        return OUT_OF_RANGE_AREA_IDX;
    }

    return idx;
}

static struct rect
idx_to_rect(struct tile_mode_state *mode_state, int idx, int x_off, int y_off) {
    int column = idx / mode_state->sub_area_rows;
    int row    = idx % mode_state->sub_area_rows;

    return (struct rect){
        .x = column * mode_state->sub_area_width +
             min(column, mode_state->sub_area_width_off) + x_off,
        .w = mode_state->sub_area_width +
             (column < mode_state->sub_area_width_off ? 1 : 0),
        .y = row * mode_state->sub_area_height +
             min(row, mode_state->sub_area_height_off) + y_off,
        .h = mode_state->sub_area_height +
             (row < mode_state->sub_area_height_off ? 1 : 0),
    };
}

static bool tile_mode_key(struct state *state, xkb_keysym_t keysym, char *text) {
    struct tile_mode_state *ms = &state->mode_state.tile;
    struct mode_tile_config *config = &state->config.mode_tile;

    switch (keysym) {
    case XKB_KEY_BackSpace:
        return tile_mode_back(ms);

    case XKB_KEY_Escape:
        state->running = false;
        return false;
        
    default:;
        int matched_i = find_str(state->home_row, HOME_ROW_LEN, text);

        if (matched_i != -1) {
            for (int i = 0; i < 3; i++) {
                if (ms->area_selection[i] == NO_AREA_SELECTION) {
                    ms->area_selection[i] = matched_i;

                    int area_idx = get_selected_area_idx(ms);
                    if (area_idx == OUT_OF_RANGE_AREA_IDX) {
                        ms->area_selection[i] = NO_AREA_SELECTION;
                        return false;
                    }

                    if (area_idx != INCOMPLETE_AREA_SELECTION) {
                        struct rect target = idx_to_rect(
                            ms, area_idx, state->initial_area.x,
                            state->initial_area.y
                        );
                        
                        if (config->enable_bisect) {
                            bisect_mode_enter(state, target);
                            return true;
                        } else {
                            // Copy the complete target rect
                            memcpy(&state->result, &target, sizeof(struct rect));
                            state->running = false;
                            return false;  // Important: return false when exiting
                        }
                    }
                    return true;
                }
            }
        }
    }

    return false;
}

void tile_mode_render(struct state *state, cairo_t *cairo) {
    struct mode_tile_config *config = &state->config.mode_tile;
    char                     label_selected[32];
    char                     label_unselected[32];
    int                      count = 0;
    struct tile_mode_state  *ms    = &state->mode_state.tile;

    cairo_select_font_face(
        cairo, config->label_font_family, CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_NORMAL
    );
    cairo_set_font_size(cairo, (int)(ms->sub_area_height / 2));

    cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_u32(cairo, config->unselectable_bg_color);
    cairo_paint(cairo);

    cairo_translate(cairo, state->initial_area.x, state->initial_area.y);

    cairo_set_source_u32(cairo, config->unselectable_bg_color);
    cairo_rectangle(
        cairo, .5, .5, state->initial_area.w - 1, state->initial_area.h - 1
    );
    cairo_set_line_width(cairo, 1);
    cairo_stroke(cairo);

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
                count, ms->label_length, state->mode_state.tile.area_selection
            );

            cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
            if (selectable) {
                cairo_set_source_u32(cairo, config->selectable_bg_color);
                cairo_rectangle(cairo, x, y, w, h);
                cairo_fill(cairo);

                cairo_set_source_u32(cairo, config->selectable_border_color);
                cairo_rectangle(cairo, x + .5, y + .5, w - 1, h - 1);
                cairo_set_line_width(cairo, 1);
                cairo_stroke(cairo);

                idx_to_label(
                    count, ms->label_length,
                    state->mode_state.tile.area_selection, state->home_row,
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
                    y + (int)((h + max(te_selected.height, te_unselected.height)
                              ) /
                              2)
                );
                cairo_set_source_u32(cairo, config->label_select_color);
                cairo_show_text(cairo, label_selected);
                cairo_set_source_u32(cairo, config->label_color);
                cairo_show_text(cairo, label_unselected);
            }

            count++;
        }
    }

    cairo_translate(cairo, -state->initial_area.x, -state->initial_area.y);
}

struct mode_interface tile_mode_interface = {
    .key    = tile_mode_key,
    .render = tile_mode_render,
};
