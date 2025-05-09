#include "config.h"
#include "mode.h"
#include "state.h"
#include "utils.h"
#include "utils_cairo.h"

#include <stdlib.h>

#define ARROW_SIZE 5

// Split direction -- defines the "shrink" direction of the rectangle
enum split_dir {
    SPLIT_DIR_DOWN  = 0,
    SPLIT_DIR_UP    = 1,
    SPLIT_DIR_LEFT  = 2,
    SPLIT_DIR_RIGHT = 3,
};

void *split_mode_enter(struct state *state, struct rect area) {
    struct split_mode_state *ms = malloc(sizeof(struct split_mode_state));
    ms->areas[0]                = area;
    ms->current                 = 0;
    return ms;
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

static void render_split_render_arrow(
    cairo_t *cairo, int x, int y, enum split_dir dir, uint32_t color
) {
#define SIMPLE_ARROWS
#if !defined(SIMPLE_ARROWS)
    char *label;
    switch (dir) {
    case SPLIT_DIR_LEFT:
        label = "←";
        break;

    case SPLIT_DIR_RIGHT:
        label = "→";
        break;

    case SPLIT_DIR_UP:
        label = "↑";
        break;

    case SPLIT_DIR_DOWN:
        label = "↓";
        break;
    }

    cairo_arc(cairo, x + .5, y + .5, 10, 0, 2 * M_PI);
    cairo_set_source_u32(cairo, 0x44444455);
    cairo_fill(cairo);

    cairo_set_source_u32(cairo, color);
    cairo_arc(cairo, x, y, 10, 0, 2 * M_PI);
    cairo_set_line_width(cairo, 1);
    cairo_stroke(cairo);

    cairo_set_font_size(cairo, 15);
    cairo_text_extents_t te;
    cairo_text_extents(cairo, label, &te);
    cairo_move_to(cairo, x - te.x_advance / 2., y + te.height / 2.);
    cairo_set_source_u32(cairo, color);
    cairo_show_text(cairo, label);

#else
    static const struct {
        char dx1 : 2;
        char dy1 : 2;
        char dx2 : 2;
        char dy2 : 2;
    } arrow_points[] = {
        [SPLIT_DIR_LEFT] =
            {
                .dx1 = 1,
                .dy1 = 1,
                .dx2 = 1,
                .dy2 = -1,
            },
        [SPLIT_DIR_RIGHT] =
            {
                .dx1 = -1,
                .dy1 = 1,
                .dx2 = -1,
                .dy2 = -1,
            },
        [SPLIT_DIR_UP] =
            {
                .dx1 = -1,
                .dy1 = 1,
                .dx2 = 1,
                .dy2 = 1,
            },

        [SPLIT_DIR_DOWN] =
            {
                .dx1 = -1,
                .dy1 = -1,
                .dx2 = 1,
                .dy2 = -1,
            },
    };

    if (dir < 0 || dir >= 4) {
        return;
    }

    cairo_set_source_u32(cairo, color);
    cairo_set_line_join(cairo, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width(cairo, 2);
    cairo_move_to(
        cairo, x + ARROW_SIZE * arrow_points[dir].dx1 + .5,
        y + ARROW_SIZE * arrow_points[dir].dy1 + .5
    );
    cairo_line_to(cairo, x + .5, y + .5);
    cairo_line_to(
        cairo, x + ARROW_SIZE * arrow_points[dir].dx2 + .5,
        y + ARROW_SIZE * arrow_points[dir].dy2 + .5
    );
    cairo_stroke(cairo);
#endif
}

static void split_mode_render_markers(
    cairo_t *cairo, struct rect *area, uint32_t vcolor, uint32_t hcolor
) {
    uint32_t middle_x = area->x + area->w / 2;
    uint32_t middle_y = area->y + area->h / 2;

    cairo_set_source_u32(cairo, hcolor);
    cairo_set_line_width(cairo, 1);
    cairo_move_to(cairo, middle_x + .5, area->y + .5);
    cairo_line_to(cairo, middle_x + .5, area->y + area->h + .5);
    cairo_stroke(cairo);

    cairo_set_source_u32(cairo, vcolor);
    cairo_set_line_width(cairo, 1);
    cairo_move_to(cairo, area->x + .5, middle_y + .5);
    cairo_line_to(cairo, area->x + area->w + .5, middle_y + .5);
    cairo_stroke(cairo);

    /*

   x  x+w/4    x+w
   +--+-----+--+ y
   |  .     .  |
   +.....0.....+ y+h/4 (2)
   |  .     .  |
   |..2..*..3..|
   |  .     .  |
   +.....1.....+ y+3h/4 (3)
   |  .     .  |
   +--+-----+--+ y+h
 (0)    x+3w/4

     */

    if (area->w > 1) {
        render_split_render_arrow(
            cairo, area->x + area->w / 4, area->y + area->h / 2, SPLIT_DIR_LEFT,
            hcolor
        );
        render_split_render_arrow(
            cairo, area->x + area->w * 3 / 4, area->y + area->h / 2,
            SPLIT_DIR_RIGHT, hcolor
        );
    }
    if (area->h > 1) {
        render_split_render_arrow(
            cairo, area->x + area->w / 2, area->y + area->h / 4, SPLIT_DIR_UP,
            vcolor
        );
        render_split_render_arrow(
            cairo, area->x + area->w / 2, area->y + area->h * 3 / 4,
            SPLIT_DIR_DOWN, vcolor
        );
    }
}

static void
split_mode_render(struct state *state, void *mode_state, cairo_t *cairo) {
    struct mode_split_config *config = &state->config.mode_split;
    struct split_mode_state  *ms     = mode_state;

    cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_u32(cairo, config->bg_color);
    cairo_paint(cairo);

    cairo_set_source_u32(cairo, config->history_border_color);
    cairo_set_line_width(cairo, 1);

    for (int i = 0; i <= ms->current; i++) {
        struct rect *area = &ms->areas[i];
        cairo_rectangle(
            cairo, area->x + .5, area->y + .5, area->w - 1, area->h - 1
        );
        cairo_stroke(cairo);
    }

    struct rect *area = &ms->areas[ms->current];
    cairo_set_source_u32(cairo, config->area_bg_color);
    cairo_rectangle(
        cairo, area->x + .5, area->y + .5, area->w - 1, area->h - 1
    );
    cairo_fill(cairo);
    split_mode_render_markers(
        cairo, area, config->vertical_color, config->horizontal_color
    );
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

    int matched_i = find_str(state->home_row, HOME_ROW_LEN_WITH_BTN, text);
    switch (matched_i) {
    case HOME_ROW_LEFT_CLICK:
        state->click = CLICK_LEFT_BTN;
        enter_next_mode(state, ms->areas[ms->current]);
        return false;

    case HOME_ROW_RIGHT_CLICK:
        state->click = CLICK_RIGHT_BTN;
        enter_next_mode(state, ms->areas[ms->current]);
        return false;

    case HOME_ROW_MIDDLE_CLICK:
        state->click = CLICK_MIDDLE_BTN;
        enter_next_mode(state, ms->areas[ms->current]);
        return false;

    default:
        break;
    }

    // Handle `wasd` and `hjkl` but only after home_row_keys
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
