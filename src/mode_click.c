#include "mode.h"

static void *click_mode_enter(struct state *state, struct rect area) {
    state->click = state->config.mode_click.button;
    enter_next_mode(state, area);
    return NULL;
}

static void click_mode_reenter(struct state *state, void *mode_state) {
    reenter_prev_mode(state);
}

static bool click_mode_key(
    struct state *state, void *mode_state, xkb_keysym_t keysym, char *text
) {
    return false;
}

static void
click_mode_render(struct state *state, void *mode_state, cairo_t *cairo) {}

static void click_mode_free(void *mode_state) {}

struct mode_interface click_mode_interface = {
    .name    = "click",
    .enter   = click_mode_enter,
    .reenter = click_mode_reenter,
    .key     = click_mode_key,
    .render  = click_mode_render,
    .free    = click_mode_free,
};
