#include "mode.h"
#include "state.h"
#include "utils.h"

#include <string.h>

#define LABEL_FONT_SIZE 20
#define LABEL_PADDING   10
#define DIVIDE_8_RATIO  1.8
#define POINTER_SIZE    20

void bisect_mode_enter(struct state *state, struct rect area) {
    state->mode                       = &bisect_mode_interface;
    state->mode_state.bisect.areas[0] = area;
    state->mode_state.bisect.current  = 0;
}

static void bisect_mode_render(struct state *state, cairo_t *cairo) {
    cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cairo, .2, .2, .2, .3);
    cairo_paint(cairo);

    struct rect *area =
        &state->mode_state.bisect.areas[state->mode_state.bisect.current];

    bool divide_8 = area->w > area->h * DIVIDE_8_RATIO;

    const int sub_area_columns = divide_8 ? 4 : 2;
    const int sub_area_rows    = 2;

    const int sub_area_width      = area->w / sub_area_columns;
    const int sub_area_width_off  = area->w % sub_area_columns;
    const int sub_area_height     = area->h / sub_area_rows;
    const int sub_area_height_off = area->h % sub_area_rows;

    for (int i = 0; i < sub_area_columns; i++) {
        for (int j = 0; j < sub_area_rows; j++) {
            const int x =
                area->x + i * sub_area_width + min(i, sub_area_width_off);
            const int y =
                area->y + j * sub_area_height + min(j, sub_area_height_off);
            const int w = sub_area_width + (i < sub_area_width_off ? 1 : 0);
            const int h = sub_area_height + (j < sub_area_height_off ? 1 : 0);

            if ((i + j) % 2 == 0) {
                cairo_set_source_rgba(cairo, 0, .2, 0, .3);
            } else {
                cairo_set_source_rgba(cairo, 0, 0, .2, .3);
            }
            cairo_rectangle(cairo, x, y, w, h);
            cairo_fill(cairo);

            cairo_set_line_width(cairo, 1);
            cairo_rectangle(cairo, x + .5, y + .5, w - 1, h - 1);
            if ((i + j) % 2 == 0) {
                cairo_set_source_rgba(cairo, 0, .3, 0, .7);
            } else {
                cairo_set_source_rgba(cairo, 0, 0, .3, .7);
            }
            cairo_stroke(cairo);
        }
    }

    cairo_set_source_rgba(cairo, .7, .2, .2, .7);
    const int pointer_x = area->x + area->w / 2;
    const int pointer_y = area->y + area->h / 2;
    cairo_move_to(
        cairo, pointer_x + .5, pointer_y - (int)(POINTER_SIZE / 2) + .5
    );
    cairo_line_to(
        cairo, pointer_x + .5, pointer_y + (int)(POINTER_SIZE / 2) + .5
    );
    cairo_stroke(cairo);
    cairo_move_to(
        cairo, pointer_x - (int)(POINTER_SIZE / 2) + .5, pointer_y + .5
    );
    cairo_line_to(
        cairo, pointer_x + (int)(POINTER_SIZE / 2) + .5, pointer_y + .5
    );
    cairo_stroke(cairo);

    cairo_select_font_face(
        cairo, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL
    );
    cairo_set_font_size(cairo, LABEL_FONT_SIZE);
    cairo_set_source_rgba(cairo, 1, 1, 1, .9);
    char label[64];

    // Top label
    if (divide_8) {
        snprintf(
            label, sizeof(label), "%s %s %s %s", state->home_row[0],
            state->home_row[1], state->home_row[2], state->home_row[3]
        );
    } else {
        snprintf(
            label, sizeof(label), "%s %s", state->home_row[0],
            state->home_row[1]
        );
    }

    cairo_text_extents_t te;
    cairo_text_extents(cairo, label, &te);
    cairo_move_to(
        cairo, area->x + (int)(area->w / 2) - (int)(te.width / 2),
        area->y - LABEL_PADDING
    );
    cairo_show_text(cairo, label);

    // Bottom label
    if (divide_8) {
        snprintf(
            label, sizeof(label), "%s %s %s %s", state->home_row[4],
            state->home_row[5], state->home_row[6], state->home_row[7]
        );
    } else {
        snprintf(
            label, sizeof(label), "%s %s", state->home_row[2],
            state->home_row[3]
        );
    }

    cairo_text_extents(cairo, label, &te);
    cairo_move_to(
        cairo, area->x + (int)(area->w / 2) - (int)(te.width / 2),
        area->y + area->h + te.height + LABEL_PADDING
    );
    cairo_show_text(cairo, label);
}

static void idx_to_rect(struct rect *area, int idx, struct rect *rect) {
    bool divide_8 = area->w > area->h * DIVIDE_8_RATIO;

    const int sub_area_columns = divide_8 ? 4 : 2;
    const int sub_area_rows    = 2;

    const int sub_area_width      = area->w / sub_area_columns;
    const int sub_area_width_off  = area->w % sub_area_columns;
    const int sub_area_height     = area->h / sub_area_rows;
    const int sub_area_height_off = area->h % sub_area_rows;

    const int i = idx % sub_area_columns;
    const int j = idx / sub_area_columns;

    rect->x = area->x + i * sub_area_width + min(i, sub_area_width_off);
    rect->y = area->y + j * sub_area_height + min(j, sub_area_height_off);
    rect->w = sub_area_width + (i < sub_area_width_off ? 1 : 0);
    rect->h = sub_area_height + (j < sub_area_height_off ? 1 : 0);
}

static bool
bisect_mode_key(struct state *state, xkb_keysym_t keysym, char *text) {
    struct bisect_mode_state *mode_state = &state->mode_state.bisect;

    switch (keysym) {
    case XKB_KEY_Escape:
        state->running = false;
        break;

    case XKB_KEY_Return:
    case XKB_KEY_space:
        memcpy(
            &state->result, &mode_state->areas[mode_state->current],
            sizeof(struct rect)
        );
        state->running = false;
        return false;

    case XKB_KEY_BackSpace:
        if (mode_state->current > 0) {
            mode_state->current--;
        }
        return true;

    default:;
        int matched_i = find_str(state->home_row, HOME_ROW_LEN, text);
        if (matched_i < 0) {
            return false;
        }

        struct rect *new_area;
        const bool   last_area = mode_state->current + 1 >= 10;
        new_area               = last_area ? &state->result
                                           : &mode_state->areas[mode_state->current + 1];
        idx_to_rect(
            &mode_state->areas[mode_state->current], matched_i, new_area
        );

        if (last_area) {
            state->running = false;
            return false;
        } else {
            mode_state->current++;
            return true;
        }
    }

    return false;
}

struct mode_interface bisect_mode_interface = {
    .key    = bisect_mode_key,
    .render = bisect_mode_render,
};
