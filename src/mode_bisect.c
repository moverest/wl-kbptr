#include "config.h"
#include "mode.h"
#include "state.h"
#include "utils.h"
#include "utils_cairo.h"

#include <math.h>
#include <string.h>

#define DIVIDE_8_RATIO 1.8

enum bisect_division {
    /*
     * The tiling mode should give us areas that are twice as wide as they are
     * tall. In this case, we want to divide the area in 8 instead of 4 as to
     * then get equal square areas.
     *
     *  +--+--+--+--+
     *  | 0| 1| 2| 3|
     *  +--+--+--+--+
     *  | 4| 5| 6| 7|
     *  +--+--+--+--+
     */
    DIVISION_8 = 0,
    /*
     * After the first pass, we should only get mostly square areas.
     *
     *  +--+--+
     *  | 0| 1|
     *  +--+--+
     *  | 2| 3|
     *  +--+--+
     */
    DIVISION_4,
    /*
     * Once we have divide the areas enough times, we will end up with a line,
     * i.e. a area with a width or height of a single pixel. In this case we
     * divide the area in two.
     *
     *    0  1
     *  +--+--+
     */
    DIVISION_HORIZONTAL,
    /*
     *    +
     *    | 0
     *    +
     *    | 1
     *    +
     */
    DIVISION_VERTICAL,
    /*
     * Given enough divisions, we should end up with a single pixel.
     *
     *    +
     */
    UNDIVIDABLE,
};

void bisect_mode_enter(struct state *state, struct rect area) {
    state->mode                       = &bisect_mode_interface;
    state->mode_state.bisect.areas[0] = area;
    state->mode_state.bisect.current  = 0;
}

static enum bisect_division determine_division(struct rect *area) {
    if (area->w <= 1 && area->h <= 1) {
        return UNDIVIDABLE;
    }

    if (area->w <= 1) {
        return DIVISION_VERTICAL;
    }

    if (area->h <= 1) {
        return DIVISION_HORIZONTAL;
    }

    return area->w > area->h * DIVIDE_8_RATIO ? DIVISION_8 : DIVISION_4;
}

struct division_interface {
    void (*render)(enum bisect_division, struct state *, cairo_t *);

    // `idx_to_sub_area` returns the sub-area indicated by the given index in
    // `rect` while also returning true. If the index does map to a sub-area,
    // `rect` is left unchanged and it returns false.
    bool (*idx_to_sub_area)(
        enum bisect_division, int idx, struct rect *current_area,
        struct rect *new_area
    );
};

static void division_4_or_8_render(
    enum bisect_division division, struct state *state, cairo_t *cairo
) {
    struct mode_bisect_config *config = &state->config.mode_bisect;
    struct rect               *area =
        &state->mode_state.bisect.areas[state->mode_state.bisect.current];

    bool divide_8 = division == DIVISION_8;

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

            cairo_set_source_u32(
                cairo, (i + j) % 2 == 0 ? config->even_area_bg_color
                                        : config->odd_area_bg_color
            );
            cairo_rectangle(cairo, x, y, w, h);
            cairo_fill(cairo);

            cairo_set_line_width(cairo, 1);
            cairo_rectangle(cairo, x + .5, y + .5, w - 1, h - 1);
            cairo_set_source_u32(
                cairo, (i + j) % 2 == 0 ? config->even_area_border_color
                                        : config->odd_area_border_color
            );
            cairo_stroke(cairo);
        }
    }

    cairo_select_font_face(
        cairo, config->label_font_family, CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_NORMAL
    );
    cairo_set_font_size(cairo, config->label_font_size);
    cairo_set_source_u32(cairo, config->label_color);
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
        area->y - config->label_padding
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
        area->y + area->h + te.height + config->label_padding
    );
    cairo_show_text(cairo, label);
}

static bool division_4_or_8_idx_to_rect(
    enum bisect_division division, int idx, struct rect *area, struct rect *rect
) {
    bool divide_8 = division == DIVISION_8;
    if (!divide_8 && idx >= 4) {
        return false;
    }

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

    return true;
}

static void division_horizontal_render(
    enum bisect_division division, struct state *state, cairo_t *cairo
) {
    struct mode_bisect_config *config = &state->config.mode_bisect;
    struct rect               *area =
        &state->mode_state.bisect.areas[state->mode_state.bisect.current];

    cairo_set_source_u32(cairo, config->even_area_border_color);
    cairo_set_line_width(cairo, 1);
    cairo_move_to(cairo, area->x + .5, area->y + .5);
    cairo_line_to(cairo, area->x + (int)(area->w / 2) - .5, area->y + .5);
    cairo_stroke(cairo);

    cairo_set_source_u32(cairo, config->odd_area_border_color);
    cairo_move_to(cairo, area->x + (int)(area->w / 2) + .5, area->y + .5);
    cairo_line_to(cairo, area->x + area->w + .5, area->y + .5);
    cairo_stroke(cairo);

    cairo_set_source_u32(cairo, config->label_color);

    cairo_text_extents_t te;
    cairo_text_extents(cairo, state->home_row[0], &te);
    cairo_move_to(
        cairo, area->x - config->label_padding - te.width,
        area->y + te.height / 2
    );
    cairo_show_text(cairo, state->home_row[0]);

    cairo_text_extents(cairo, state->home_row[1], &te);
    cairo_move_to(
        cairo, area->x + area->w + config->label_padding,
        area->y + te.height / 2
    );
    cairo_show_text(cairo, state->home_row[1]);
}

static bool division_horizontal_idx_to_rect(
    enum bisect_division division, int idx, struct rect *area, struct rect *rect
) {
    if (idx > 1) {
        return false;
    }

    rect->x = area->x + idx * area->w / 2;
    rect->y = area->y;
    rect->w = area->w / 2;
    rect->h = area->h;

    return true;
}

static void division_vertical_render(
    enum bisect_division division, struct state *state, cairo_t *cairo
) {

    struct mode_bisect_config *config = &state->config.mode_bisect;
    struct rect               *area =
        &state->mode_state.bisect.areas[state->mode_state.bisect.current];

    cairo_set_source_u32(cairo, config->even_area_border_color);
    cairo_set_line_width(cairo, 1);
    cairo_move_to(cairo, area->x + .5, area->y + .5);
    cairo_line_to(cairo, area->x + .5, area->y - .5 + (int)(area->h / 2));
    cairo_stroke(cairo);

    cairo_set_source_u32(cairo, config->odd_area_border_color);
    cairo_move_to(cairo, area->x + .5, area->y + .5 + (int)(area->h / 2));
    cairo_line_to(cairo, area->x + .5, area->y + .5 + area->h);
    cairo_stroke(cairo);

    cairo_set_source_u32(cairo, config->label_color);

    cairo_text_extents_t te;
    cairo_text_extents(cairo, state->home_row[0], &te);
    cairo_move_to(
        cairo, area->x - te.width / 2, area->y - config->label_padding
    );
    cairo_show_text(cairo, state->home_row[0]);

    cairo_text_extents(cairo, state->home_row[1], &te);
    cairo_move_to(
        cairo, area->x - te.width / 2,
        area->y + area->h + config->label_padding + te.height
    );
    cairo_show_text(cairo, state->home_row[1]);
}

static bool division_vertical_idx_to_rect(
    enum bisect_division division, int idx, struct rect *area, struct rect *rect
) {
    if (idx > 1) {
        return false;
    }

    rect->x = area->x;
    rect->y = area->y + idx * area->h / 2;
    rect->w = area->w;
    rect->h = area->h / 2;

    return true;
}

static void undividable_render(
    enum bisect_division division, struct state *state, cairo_t *cairo
) {
    struct mode_bisect_config *config = &state->config.mode_bisect;
    struct rect               *area =
        &state->mode_state.bisect.areas[state->mode_state.bisect.current];

    cairo_set_source_u32(cairo, config->pointer_color);
    cairo_arc(
        cairo, area->x + .5, area->y + .5, config->pointer_size / 4., 0,
        2 * M_PI
    );
    cairo_set_line_width(cairo, 1);
    cairo_stroke(cairo);
}

static bool undividable_select_idx() {
    return false;
}

static const struct division_interface division_interfaces[] = {
    [DIVISION_8] =
        {.render          = division_4_or_8_render,
         .idx_to_sub_area = division_4_or_8_idx_to_rect},
    [DIVISION_4] =
        {.render          = division_4_or_8_render,
         .idx_to_sub_area = division_4_or_8_idx_to_rect},
    [DIVISION_VERTICAL] =
        {.render          = division_vertical_render,
         .idx_to_sub_area = division_vertical_idx_to_rect},
    [DIVISION_HORIZONTAL] =
        {.render          = division_horizontal_render,
         .idx_to_sub_area = division_horizontal_idx_to_rect},
    [UNDIVIDABLE] =
        {.render = undividable_render, .idx_to_sub_area = undividable_select_idx
        },
};

static void bisect_mode_render(struct state *state, cairo_t *cairo) {
    struct mode_bisect_config *config     = &state->config.mode_bisect;
    struct bisect_mode_state  *mode_state = &state->mode_state.bisect;
    struct rect *area = &mode_state->areas[state->mode_state.bisect.current];

    cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_u32(cairo, config->unselectable_bg_color);
    cairo_paint(cairo);

    cairo_set_source_u32(cairo, config->history_border_color);
    cairo_set_line_width(cairo, 1);

    cairo_rectangle(
        cairo, state->initial_area.x + .5, state->initial_area.y + .5,
        state->initial_area.w - 1, state->initial_area.h - 1
    );
    cairo_stroke(cairo);

    for (int i = 0; i < mode_state->current; i++) {
        struct rect *area = &mode_state->areas[i];
        cairo_rectangle(
            cairo, area->x + .5, area->y + .5, area->w - 1, area->h - 1
        );
        cairo_stroke(cairo);
    }

    if (mode_state->current < BISECT_MAX_HISTORY) {
        enum bisect_division division = determine_division(area);
        division_interfaces[division].render(division, state, cairo);
    }

    cairo_set_line_width(cairo, 1);
    cairo_set_source_u32(cairo, config->pointer_color);
    const int pointer_x = area->x + area->w / 2;
    const int pointer_y = area->y + area->h / 2;
    cairo_move_to(
        cairo, pointer_x + .5, pointer_y - (int)(config->pointer_size / 2) + .5
    );
    cairo_line_to(
        cairo, pointer_x + .5, pointer_y + (int)(config->pointer_size / 2) + .5
    );
    cairo_stroke(cairo);
    cairo_move_to(
        cairo, pointer_x - (int)(config->pointer_size / 2) + .5, pointer_y + .5
    );
    cairo_line_to(
        cairo, pointer_x + (int)(config->pointer_size / 2) + .5, pointer_y + .5
    );
    cairo_stroke(cairo);
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
        } else {
            tile_mode_reenter(state);
        }
        return true;

    default:
        if (mode_state->current + 1 >= BISECT_MAX_HISTORY) {
            return false;
        }

        struct rect         *area     = &mode_state->areas[mode_state->current];
        enum bisect_division division = determine_division(area);

        int matched_i = find_str(state->home_row, HOME_ROW_LEN_WITH_BTN, text);
        if (matched_i < 0) {
            return false;
        }

        if (matched_i >= HOME_ROW_LEN) {
            switch (matched_i) {
            case HOME_ROW_LEFT_CLICK:
                state->click = CLICK_LEFT_BTN;
                break;
            case HOME_ROW_RIGHT_CLICK:
                state->click = CLICK_RIGHT_BTN;
                break;
            case HOME_ROW_MIDDLE_CLICK:
                state->click = CLICK_MIDDLE_BTN;
                break;
            }

            memcpy(
                &state->result, &mode_state->areas[mode_state->current],
                sizeof(struct rect)
            );
            state->running = false;
            return false;
        }

        if (division == UNDIVIDABLE) {
            return false;
        }

        struct rect *new_area = &mode_state->areas[mode_state->current + 1];
        if (division_interfaces[division].idx_to_sub_area(
                division, matched_i, area, new_area
            )) {
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
