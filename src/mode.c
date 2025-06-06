#include "mode.h"

#include "log.h"

#include <stdlib.h>
#include <string.h>

extern struct mode_interface tile_mode_interface;
extern struct mode_interface floating_mode_interface;
extern struct mode_interface bisect_mode_interface;
extern struct mode_interface split_mode_interface;
extern struct mode_interface click_mode_interface;

struct mode_interface *mode_interfaces[] = {
    &tile_mode_interface,  &floating_mode_interface, &bisect_mode_interface,
    &split_mode_interface, &click_mode_interface,    NULL,
};

static struct mode_interface *find_mode_interface_by_name(char *name) {
    struct mode_interface **curr_mode_interface = mode_interfaces;
    while (*curr_mode_interface != NULL) {
        if (strcmp(name, (*curr_mode_interface)->name) == 0) {
            return *curr_mode_interface;
        }

        curr_mode_interface++;
    }

    return NULL;
}

int load_modes(struct state *state, char *modes) {
    char buf[strlen(modes) + 1];
    strcpy(buf, modes);

    int mode_i = 0;

    static const char *delim = ",";
    char              *tok   = strtok(buf, delim);
    while (tok != NULL) {
        if (mode_i >= MAX_NUM_MODES) {
            LOG_ERR("Only %d modes can be specified.", MAX_NUM_MODES);
            return 1;
        }

        struct mode_interface *mode_interface =
            find_mode_interface_by_name(tok);
        if (mode_interface == NULL) {
            LOG_ERR("Unknown mode '%s'.", tok);
            return 2;
        }

        state->mode_interfaces[mode_i++] = mode_interface;
        tok                              = strtok(NULL, delim);
    }

    if (mode_i < MAX_NUM_MODES) {
        state->mode_interfaces[mode_i] = NULL;
    }

    state->current_mode = NO_MODE_ENTERED;
    return 0;
}

void enter_next_mode(struct state *state, struct rect area) {
    state->current_mode += 1;

    if (has_last_mode_returned(state)) {
        memcpy(&state->result, &area, sizeof(struct rect));
        return;
    }

    state->mode_states[state->current_mode] =
        state->mode_interfaces[state->current_mode]->enter(state, area);
}

bool has_last_mode_returned(struct state *state) {
    return state->current_mode >= MAX_NUM_MODES ||
           state->mode_interfaces[state->current_mode] == NULL;
}

bool reenter_prev_mode(struct state *state) {
    if (state->current_mode <= 0) {
        return false;
    }

    state->mode_interfaces[state->current_mode]->free(
        state->mode_states[state->current_mode]
    );

    state->current_mode--;
    state->mode_interfaces[state->current_mode]->reenter(
        state, state->mode_states[state->current_mode]
    );

    return true;
}

void free_mode_states(struct state *state) {
    if (state->current_mode == NO_MODE_ENTERED) {
        return;
    }

    for (int i = 0; i <= state->current_mode; i++) {
        if (state->mode_interfaces[i] == NULL) {
            return;
        }

        state->mode_interfaces[i]->free(state->mode_states[i]);
    }
}

bool mode_handle_key(struct state *state, xkb_keysym_t sym, char *text) {
    if (has_last_mode_returned(state)) {
        return false;
    }

    return state->mode_interfaces[state->current_mode]->key(
        state, state->mode_states[state->current_mode], sym, text
    );
}
void mode_render(struct state *state, cairo_t *cairo) {
    if (has_last_mode_returned(state)) {
        return;
    }

    return state->mode_interfaces[state->current_mode]->render(
        state, state->mode_states[state->current_mode], cairo
    );
}
