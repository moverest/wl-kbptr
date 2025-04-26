#include "config.h"
#include "mode.h"
#include "state.h"
#include "utils.h"
#include "utils_cairo.h"

#include <stdlib.h>

// Split direction -- defines the "shrink" direction of the rectangle
enum split_dir {
    SPLIT_DIR_DOWN,
    SPLIT_DIR_UP,
    SPLIT_DIR_LEFT,
    SPLIT_DIR_RIGHT,
};

void *split_mode_enter(struct state *state, struct rect area) {
    struct split_mode_state *ms = malloc(sizeof(struct split_mode_state));
    ms->areas[0]                = area;
    ms->current                 = 0;
    return ms;
}

static void
split_mode_render_grid(struct state *state, void *mode_state, cairo_t *cairo) {
    struct mode_split_config *config              = &state->config.mode_split;
    struct split_mode_state  *ms                  = mode_state;
    struct rect              *area                = &ms->areas[ms->current];
    const int                 ngrid               = 2;
    const int                 sub_area_width      = area->w / ngrid;
    const int                 sub_area_width_off  = area->w % ngrid;
    const int                 sub_area_height     = area->h / ngrid;
    const int                 sub_area_height_off = area->h % ngrid;

    for (int i = 0; i < ngrid; i++) {
        for (int j = 0; j < ngrid; j++) {
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
}

static void split_mode_render_cursor(
    struct state *state, void *mode_state, cairo_t *cairo
) {
    struct mode_split_config *config = &state->config.mode_split;
    struct split_mode_state  *ms     = mode_state;
    struct rect              *area   = &ms->areas[ms->current];

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

    // Draw "undividable" marker, draw a red circle around the cursor
    if (area->w <= 1 && area->h <= 1) {
        cairo_set_source_u32(cairo, config->pointer_color);
        cairo_arc(
            cairo, area->x + .5, area->y + .5, config->pointer_size / 4., 0,
            2 * M_PI
        );
        cairo_set_line_width(cairo, 1);
        cairo_stroke(cairo);
    }
}

static void
split_mode_render(struct state *state, void *mode_state, cairo_t *cairo) {
    struct mode_split_config *config = &state->config.mode_split;
    struct split_mode_state  *ms     = mode_state;

    cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_u32(cairo, config->unselectable_bg_color);
    cairo_paint(cairo);

    cairo_set_source_u32(cairo, config->history_border_color);
    cairo_set_line_width(cairo, 1);

    for (int i = 0; i < ms->current; i++) {
        struct rect *area = &ms->areas[i];
        cairo_rectangle(
            cairo, area->x + .5, area->y + .5, area->w - 1, area->h - 1
        );
        cairo_stroke(cairo);
    }

    split_mode_render_grid(state, mode_state, cairo);
    split_mode_render_cursor(state, mode_state, cairo);
}

static bool
split_mode_split(struct state *state, void *mode_state, enum split_dir dir) {
    struct split_mode_state *ms = mode_state;

    if (ms->current + 1 >= SPLIT_MAX_HISTORY) {
        return false;
    }

    struct rect *area     = &ms->areas[ms->current];
    struct rect *new_area = &ms->areas[ms->current + 1];

    *new_area = *area;

    switch (dir) {
    case SPLIT_DIR_RIGHT:
        if (area->w <= 1) {
            return false; /* Cannot split further */
        }
        new_area->w  = area->w / 2;
        new_area->x += new_area->w;
        break;

    case SPLIT_DIR_LEFT:
        if (area->w <= 1) {
            return false; /* Cannot split further */
        }
        new_area->w = area->w / 2;
        break;

    case SPLIT_DIR_UP:
        if (area->h <= 1) {
            return false; /* Cannot split further */
        }
        new_area->h = area->h / 2;
        break;

    case SPLIT_DIR_DOWN:
        if (area->h <= 1) {
            return false; /* Cannot split further */
        }
        new_area->h  = area->h / 2;
        new_area->y += new_area->h;
        break;
    }

    ms->current++;
    return true;
}

static bool split_mode_key(
    struct state *state, void *mode_state, xkb_keysym_t keysym, char *text
) {
    struct split_mode_state *ms = mode_state;

    switch (keysym) {
    case XKB_KEY_Escape:
        state->running = false;
        break;

    case XKB_KEY_Return:
    case XKB_KEY_space:
        enter_next_mode(state, ms->areas[ms->current]);
        return true;

    case XKB_KEY_BackSpace:
        if (ms->current > 0) {
            ms->current--;
        } else {
            reenter_prev_mode(state);
        }
        return true;

    case XKB_KEY_Left:
        return split_mode_split(state, mode_state, SPLIT_DIR_LEFT);

    case XKB_KEY_Right:
        return split_mode_split(state, mode_state, SPLIT_DIR_RIGHT);

    case XKB_KEY_Up:
        return split_mode_split(state, mode_state, SPLIT_DIR_UP);

    case XKB_KEY_Down:
        return split_mode_split(state, mode_state, SPLIT_DIR_DOWN);
    }

    switch (text[0]) {
    case 'a':
    case 'h':
        return split_mode_split(state, mode_state, SPLIT_DIR_LEFT);

    case 'd':
    case 'l':
        return split_mode_split(state, mode_state, SPLIT_DIR_RIGHT);

    case 'w':
    case 'k':
        return split_mode_split(state, mode_state, SPLIT_DIR_UP);

    case 's':
    case 'j':
        return split_mode_split(state, mode_state, SPLIT_DIR_DOWN);
    }

    return false;
}

void split_mode_reenter(struct state *state, void *mode_state) {}
void split_mode_free(void *mode_state) {
    free(mode_state);
}

struct mode_interface split_mode_interface = {
    .name    = "split",
    .enter   = split_mode_enter,
    .reenter = split_mode_reenter,
    .key     = split_mode_key,
    .render  = split_mode_render,
    .free    = split_mode_free,
};
