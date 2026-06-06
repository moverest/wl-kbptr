// SPDX-License-Identifier: GPL-3.0-only

#include "config.h"
#include "label.h"
#include "mode.h"
#include "state.h"
#include "utils.h"
#include "utils_cairo.h"

#include <cairo.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>

#define MIN_SUB_AREA_SIZE (25 * 50)
// Upper bound on pending sub-rectangles when computing one output's exclusive
// area.  In any real monitor layout this will never be reached.
#define MAX_PENDING_RECTS 64

// Returns the intersection of a and b.  w or h will be 0 if they are disjoint.
static struct rect rect_intersect(struct rect a, struct rect b) {
    int32_t x1 = max(a.x, b.x);
    int32_t y1 = max(a.y, b.y);
    int32_t x2 = min(a.x + a.w, b.x + b.w);
    int32_t y2 = min(a.y + a.h, b.y + b.h);
    return (struct rect){
        .x = x1, .y = y1,
        .w = x2 > x1 ? x2 - x1 : 0,
        .h = y2 > y1 ? y2 - y1 : 0,
    };
}

// Subtract rectangle b from rectangle a using a cross decomposition:
// full-height left/right strips, then top/bottom middle strips.
// Stores up to 4 non-empty results in out[] and returns the count.
// If a and b do not intersect, returns 1 with out[0] = a.
static int rect_subtract(struct rect a, struct rect b, struct rect out[4]) {
    struct rect i = rect_intersect(a, b);
    if (i.w <= 0 || i.h <= 0) {
        out[0] = a;
        return 1;
    }
    int n = 0;
    if (i.x > a.x)
        out[n++] = (struct rect){.x = a.x, .y = a.y, .w = i.x - a.x, .h = a.h};
    if (i.x + i.w < a.x + a.w)
        out[n++] = (struct rect){.x = i.x + i.w, .y = a.y, .w = (a.x + a.w) - (i.x + i.w), .h = a.h};
    if (i.y > a.y)
        out[n++] = (struct rect){.x = i.x, .y = a.y, .w = i.w, .h = i.y - a.y};
    if (i.y + i.h < a.y + a.h)
        out[n++] = (struct rect){.x = i.x, .y = i.y + i.h, .w = i.w, .h = (a.y + a.h) - (i.y + i.h)};
    return n;
}

void *tile_mode_enter(struct state *state, struct rect area) {
    struct tile_mode_state *ms = calloc(1, sizeof(*ms));
    ms->area                   = area;

    const int max_num_sub_areas = 26 * 26;

    ms->label_symbols =
        label_symbols_from_str(state->config.mode_tile.label_symbols);
    if (ms->label_symbols == NULL) {
        ms->label_selection = NULL;
        state->running      = false;
        return ms;
    }

    if (state->config.general.all_outputs &&
        !wl_list_empty(&state->overlay_surfaces)) {
        // Exclusive-region approach: each output is assigned only the pixels
        // that belong exclusively to it — its full bounds minus any area
        // already claimed by a previously processed output.  This correctly
        // handles any overlap topology: side-by-side, corner overlap,
        // landscape+portrait, and full mirror (which yields no exclusive area
        // for the second output and therefore no labels there).

        // Count monitors and compute average monitor area for a consistent
        // cell size across all regions.
        int64_t total_area = 0;
        int     n          = 0;
        struct overlay_surface *ov;
        wl_list_for_each (ov, &state->overlay_surfaces, link) {
            if (ov->output != NULL) {
                total_area += (int64_t)ov->output->width * ov->output->height;
                n++;
            }
        }
        if (n == 0) {
            ms->label_selection = NULL;
            state->running      = false;
            return ms;
        }

        int32_t avg_area      = (int32_t)(total_area / n);
        int     sub_area_size = max(avg_area / max_num_sub_areas, MIN_SUB_AREA_SIZE);
        int     cell_h        = max((int)sqrt(sub_area_size / 2.), 1);
        int     cell_w        = max((int)sqrt(sub_area_size * 2.), 1);

        // Each output can produce at most MAX_PENDING_RECTS exclusive
        // sub-rectangles, so allocate that many slots per output.
        ms->regions     = malloc((size_t)n * MAX_PENDING_RECTS * sizeof(struct tile_region));
        ms->num_regions = 0;
        int label_offset = 0;

        // Full bounds of already-processed outputs, for subtraction.
        struct rect *processed   = malloc((size_t)n * sizeof(struct rect));
        int          n_processed = 0;

        wl_list_for_each (ov, &state->overlay_surfaces, link) {
            if (ov->output == NULL) continue;
            struct output *o = ov->output;

            // Ping-pong buffers: subtract each prior output's bounds from the
            // current pending set to obtain this output's exclusive rectangles.
            struct rect ping[MAX_PENDING_RECTS], pong[MAX_PENDING_RECTS];
            struct rect *cur = ping, *nxt = pong;
            int          n_cur = 1;
            cur[0] = (struct rect){.x = o->x, .y = o->y, .w = o->width, .h = o->height};

            for (int pi = 0; pi < n_processed && n_cur > 0; pi++) {
                int n_nxt = 0;
                for (int ri = 0; ri < n_cur; ri++) {
                    struct rect out4[4];
                    int         cnt = rect_subtract(cur[ri], processed[pi], out4);
                    for (int k = 0; k < cnt; k++) {
                        if (n_nxt < MAX_PENDING_RECTS)
                            nxt[n_nxt++] = out4[k];
                    }
                }
                struct rect *tmp = cur; cur = nxt; nxt = tmp;
                n_cur = n_nxt;
            }

            // Create one tile region per exclusive sub-rectangle.
            for (int ri = 0; ri < n_cur; ri++) {
                struct rect r_area = cur[ri];
                if (r_area.w <= 0 || r_area.h <= 0 ||
                    ms->num_regions >= n * MAX_PENDING_RECTS) {
                    continue;
                }
                struct tile_region *r = &ms->regions[ms->num_regions++];
                r->area               = r_area;
                r->rows               = max(r_area.h / cell_h, 1);
                r->cols               = max(r_area.w / cell_w, 1);
                r->cell_h             = r_area.h / r->rows;
                r->cell_h_off         = r_area.h % r->rows;
                r->cell_w             = r_area.w / r->cols;
                r->cell_w_off         = r_area.w % r->cols;
                r->label_offset       = label_offset;
                r->num_labels         = r->rows * r->cols;
                label_offset         += r->num_labels;
            }

            processed[n_processed++] =
                (struct rect){.x = o->x, .y = o->y, .w = o->width, .h = o->height};
        }

        free(processed);
        ms->label_selection = label_selection_new(ms->label_symbols, label_offset);
    } else {
        // Single-output path: flat grid over the whole area.
        int32_t density_area = ms->area.w * ms->area.h;
        int sub_area_size =
            max(density_area / max_num_sub_areas, MIN_SUB_AREA_SIZE);

        ms->sub_area_height = sqrt(sub_area_size / 2.);
        ms->sub_area_rows   = ms->area.h / ms->sub_area_height;
        if (ms->sub_area_rows == 0) {
            ms->sub_area_rows = 1;
        }
        ms->sub_area_height_off = ms->area.h % ms->sub_area_rows;
        ms->sub_area_height     = ms->area.h / ms->sub_area_rows;

        ms->sub_area_width   = sqrt(sub_area_size * 2);
        ms->sub_area_columns = ms->area.w / ms->sub_area_width;
        if (ms->sub_area_columns == 0) {
            ms->sub_area_columns = 1;
        }
        ms->sub_area_width_off = ms->area.w % ms->sub_area_columns;
        ms->sub_area_width     = ms->area.w / ms->sub_area_columns;

        int total_cells     = ms->sub_area_rows * ms->sub_area_columns;
        ms->label_selection = label_selection_new(ms->label_symbols, total_cells);
    }

    ms->label_font_face = cairo_toy_font_face_create(
        state->config.mode_tile.label_font_family, CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_NORMAL
    );

    return ms;
}

// `tile_mode_back` goes back in history. Returns true if there was something to
// go back to.
static bool tile_mode_back(struct tile_mode_state *mode_state) {
    return label_selection_back(mode_state->label_selection) != 0;
}

// `tile_mode_reenter` reenters the tile mode. We assume that the saved state is
// valid and goes back in history once.
void tile_mode_reenter(struct state *state, void *mode_state) {
    struct tile_mode_state *ms = mode_state;
    tile_mode_back(ms);
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

static bool tile_mode_key(
    struct state *state, void *mode_state, xkb_keysym_t keysym, char *text
) {
    struct tile_mode_state *ms = mode_state;

    switch (keysym) {
    case XKB_KEY_BackSpace:
        return tile_mode_back(ms);
        break;

    case XKB_KEY_Escape:
        state->running = false;
        break;
    default:;
        int symbol_idx = label_symbols_find_idx(ms->label_symbols, text);
        if (symbol_idx < 0) {
            return false;
        }

        label_selection_append(ms->label_selection, symbol_idx);

        int label_idx = label_selection_to_idx(ms->label_selection);
        if (label_idx >= 0) {
            if (ms->regions != NULL) {
                // Find which region this label belongs to, then compute the
                // cell rect within that region.
                for (int ri = 0; ri < ms->num_regions; ri++) {
                    struct tile_region *r = &ms->regions[ri];
                    if (label_idx < r->label_offset ||
                        label_idx >= r->label_offset + r->num_labels) {
                        continue;
                    }
                    int local = label_idx - r->label_offset;
                    int col   = local / r->rows;
                    int row   = local % r->rows;
                    int x = col * r->cell_w + min(col, r->cell_w_off);
                    int w = r->cell_w + (col < r->cell_w_off ? 1 : 0);
                    int y = row * r->cell_h + min(row, r->cell_h_off);
                    int h = r->cell_h + (row < r->cell_h_off ? 1 : 0);
                    enter_next_mode(
                        state,
                        (struct rect){
                            .x = r->area.x + x,
                            .y = r->area.y + y,
                            .w = w,
                            .h = h,
                        }
                    );
                    break;
                }
            } else {
                enter_next_mode(
                    state, idx_to_rect(ms, label_idx, ms->area.x, ms->area.y)
                );
            }
        }
        return true;
    }

    return false;
}

// Render one selectable cell at position (x, y) with size (w, h).
// curr_label is the label for this cell; selection is the current user input.
static void render_cell(
    struct mode_tile_config *config, cairo_t *cairo,
    label_selection_t *curr_label, label_selection_t *selection,
    int x, int y, int w, int h,
    char *label_selected_str, char *label_unselected_str
) {
    const bool selectable = label_selection_is_included(curr_label, selection);
    cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
    if (!selectable) {
        return;
    }

    cairo_set_source_u32(cairo, config->selectable_bg_color);
    cairo_rectangle(cairo, x, y, w, h);
    cairo_fill(cairo);

    cairo_set_source_u32(cairo, config->selectable_border_color);
    cairo_rectangle(cairo, x + .5, y + .5, w - 1, h - 1);
    cairo_set_line_width(cairo, 1);
    cairo_stroke(cairo);

    cairo_text_extents_t te_all;
    label_selection_str(curr_label, label_selected_str);
    cairo_text_extents(cairo, label_selected_str, &te_all);

    label_selection_str_split(
        curr_label, label_selected_str, label_unselected_str, selection->next
    );

    cairo_text_extents_t te_selected, te_unselected;
    cairo_text_extents(cairo, label_selected_str, &te_selected);
    cairo_text_extents(cairo, label_unselected_str, &te_unselected);

    cairo_move_to(
        cairo,
        x + (w - te_selected.x_advance - te_unselected.x_advance) / 2,
        y + (int)((h + te_all.height) / 2)
    );
    cairo_set_source_u32(cairo, config->label_select_color);
    cairo_show_text(cairo, label_selected_str);
    cairo_set_source_u32(cairo, config->label_color);
    cairo_show_text(cairo, label_unselected_str);
}

void tile_mode_render(struct state *state, void *mode_state, cairo_t *cairo) {
    struct mode_tile_config *config = &state->config.mode_tile;
    struct tile_mode_state  *ms     = mode_state;

    // Font size: for regions use the first region's cell height, otherwise
    // the single-output cell height.
    int ref_cell_h = (ms->regions != NULL && ms->num_regions > 0)
                         ? ms->regions[0].cell_h
                         : ms->sub_area_height;
    cairo_set_font_face(cairo, ms->label_font_face);
    cairo_set_font_size(
        cairo,
        compute_relative_font_size(&config->label_font_size, ref_cell_h)
    );

    // Paint background over the whole surface.
    cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_u32(cairo, config->unselectable_bg_color);
    cairo_paint(cairo);

    int num_labels = ms->label_selection->num_labels;
    label_selection_t *curr_label =
        label_selection_new(ms->label_symbols, num_labels);

    int  label_str_max_len = label_selection_str_max_len(curr_label) + 1;
    char label_selected_str[label_str_max_len];
    char label_unselected_str[label_str_max_len];

    if (ms->regions != NULL) {
        // Render cells in each exclusive sub-region.
        for (int ri = 0; ri < ms->num_regions; ri++) {
            struct tile_region *r = &ms->regions[ri];

            // Draw region outline.
            cairo_set_source_u32(cairo, config->unselectable_bg_color);
            cairo_rectangle(
                cairo, r->area.x + .5, r->area.y + .5,
                r->area.w - 1, r->area.h - 1
            );
            cairo_set_line_width(cairo, 1);
            cairo_stroke(cairo);

            label_selection_set_from_idx(curr_label, r->label_offset);

            for (int li = 0; li < r->num_labels; li++) {
                int col = li / r->rows;
                int row = li % r->rows;
                int x = r->area.x + col * r->cell_w + min(col, r->cell_w_off);
                int w = r->cell_w + (col < r->cell_w_off ? 1 : 0);
                int y = r->area.y + row * r->cell_h + min(row, r->cell_h_off);
                int h = r->cell_h + (row < r->cell_h_off ? 1 : 0);

                render_cell(
                    config, cairo, curr_label, ms->label_selection,
                    x, y, w, h, label_selected_str, label_unselected_str
                );
                label_selection_incr(curr_label);
            }
        }
    } else {
        // Single-output flat grid.
        cairo_translate(cairo, ms->area.x, ms->area.y);

        cairo_set_source_u32(cairo, config->unselectable_bg_color);
        cairo_rectangle(cairo, .5, .5, ms->area.w - 1, ms->area.h - 1);
        cairo_set_line_width(cairo, 1);
        cairo_stroke(cairo);

        label_selection_set_from_idx(curr_label, 0);

        for (int li = 0; li < num_labels; li++) {
            int column = li / ms->sub_area_rows;
            int row    = li % ms->sub_area_rows;

            int x = column * ms->sub_area_width +
                    min(column, ms->sub_area_width_off);
            int w = ms->sub_area_width +
                    (column < ms->sub_area_width_off ? 1 : 0);
            int y =
                row * ms->sub_area_height + min(row, ms->sub_area_height_off);
            int h = ms->sub_area_height +
                    (row < ms->sub_area_height_off ? 1 : 0);

            render_cell(
                config, cairo, curr_label, ms->label_selection,
                x, y, w, h, label_selected_str, label_unselected_str
            );
            label_selection_incr(curr_label);
        }

        cairo_translate(cairo, -ms->area.x, -ms->area.y);
    }

    label_selection_free(curr_label);
}

void tile_mode_state_free(void *mode_state) {
    struct tile_mode_state *ms = mode_state;
    cairo_font_face_destroy(ms->label_font_face);
    label_selection_free(ms->label_selection);
    label_symbols_free(ms->label_symbols);
    free(ms->regions);
    free(ms);
}

struct mode_interface tile_mode_interface = {
    .name    = "tile",
    .enter   = tile_mode_enter,
    .reenter = tile_mode_reenter,
    .key     = tile_mode_key,
    .render  = tile_mode_render,
    .free    = tile_mode_state_free,
};
