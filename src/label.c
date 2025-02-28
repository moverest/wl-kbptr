#include "label.h"

#include "log.h"
#include "utils.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

label_symbols_t *label_symbols_from_str(char *s) {
    char *c = s;

    uint32_t r;
    int      c_len;

    int len         = sizeof(label_symbols_t);
    int num_symbols = 0;

    while ((c_len = str_to_rune(c, &r)) > 0) {
        // One byte for the indices and one of the end of string (`\0`).
        c           += c_len;
        len         += c_len + 2;
        num_symbols += 1;
    }

    if (c_len < 0) {
        LOG_ERR("Invalid UTF-8 input.");
        return NULL;
    }

    if (num_symbols < 2) {
        LOG_ERR(
            "Not enough characters (%d). Must have at least 2.", num_symbols
        );
        return NULL;
    }

    if (num_symbols >= 255) {
        LOG_ERR("Too many characters (%d).", num_symbols);
        return NULL;
    }

    label_symbols_t *label_symbols = malloc(len);

    label_symbols->num_symbols = num_symbols;
    unsigned char *indices     = (unsigned char *)label_symbols->data;
    char          *str         = &label_symbols->data[num_symbols];

    c              = s;
    int str_offset = 0;
    for (int i = 0; i < num_symbols; i++) {
        c_len      = str_to_rune(c, &r);
        indices[i] = str_offset;
        memcpy(str + str_offset, c, c_len);
        str[str_offset + c_len] = '\0';

        str_offset += c_len + 1;
        c          += c_len;
    }

    return label_symbols;
}

void label_symbols_free(label_symbols_t *ls) {
    free(ls);
}

char *label_symbols_idx_to_ptr(label_symbols_t *label_symbols, int idx) {
    if (idx < 0 || idx >= label_symbols->num_symbols) {
        LOG_ERR("Label symbols index (%d) out of bound.", idx);
        return NULL;
    }

    return ((char *)label_symbols->data) + label_symbols->num_symbols +
           ((unsigned char *)label_symbols->data)[idx];
}

int label_symbols_find_idx(label_symbols_t *label_symbols, char *s) {
    for (int i = 0; i < label_symbols->num_symbols; i++) {
        if (strcmp(label_symbols_idx_to_ptr(label_symbols, i), s) == 0) {
            return i;
        }
    }

    return -1;
}

label_selection_t *
label_selection_new(label_symbols_t *label_symbols, int num_labels) {
    label_selection_t *l =
        malloc(sizeof(label_symbols_t) + label_symbols->num_symbols);

    l->num_labels = num_labels;

    l->len = 0;
    while (num_labels > 0) {
        l->len++;
        num_labels /= label_symbols->num_symbols;
    }

    l->next          = 0;
    l->label_symbols = label_symbols;
    return l;
}

void label_selection_clear(label_selection_t *label_selection) {
    label_selection->next = 0;
}

static int label_selection_to_partial_idx(label_selection_t *label_selection) {
    int idx         = 0;
    int factor      = 1;
    int num_symbols = label_selection->label_symbols->num_symbols;

    for (int i = 0; i < label_selection->next; i++) {
        idx    += label_selection->input[i] * factor;
        factor *= num_symbols;
    }

    return idx;
}

enum label_selection_append_ret
label_selection_append(label_selection_t *label_selection, int idx) {
    if (label_selection->next >= label_selection->label_symbols->num_symbols) {
        return LABEL_SELECTION_APPEND_FULL;
    }

    label_selection->input[label_selection->next++] = idx;

    if (label_selection_to_partial_idx(label_selection) >=
        label_selection->num_labels) {
        label_selection->next--;
        return LABEL_SELECTION_APPEND_IDX_OVERFLOW;
    }

    return LABEL_SELECTION_APPEND_SUCCESS;
}

bool label_selection_back(label_selection_t *label_selection) {
    if (label_selection->next == 0) {
        return false;
    }

    label_selection->next--;
    return true;
}

bool label_selection_is_included(
    label_selection_t *reference, label_selection_t *start
) {
    if (reference->label_symbols != start->label_symbols ||
        reference->len != start->len || reference->next < start->next) {
        return false;
    }

    for (int i = 0; i < start->next; i++) {
        if (reference->input[i] != start->input[i]) {
            return false;
        }
    }

    return true;
}

int label_selection_to_idx(label_selection_t *label_selection) {
    if (label_selection->next != label_selection->len) {
        return -1;
    }

    return label_selection_to_partial_idx(label_selection);
}

int label_selection_set_from_idx(label_selection_t *label_selection, int idx) {
    int num_symbols = label_selection->label_symbols->num_symbols;

    for (label_selection->next = 0;
         label_selection->next < label_selection->len;
         label_selection->next++) {
        label_selection->input[label_selection->next]  = idx % num_symbols;
        idx                                           /= num_symbols;
    }

    return idx == 0;
}

int label_selection_incr(label_selection_t *label_selection) {
    int num_symbols = label_selection->label_symbols->num_symbols;

    for (int i = 0; i < label_selection->len; i++) {
        label_selection->input[i] += 1;

        if (label_selection->input[i] < num_symbols) {
            return 1;
        }

        label_selection->input[i] %= num_symbols;
    }

    return 0;
}

static int label_symbols_max_str_len(label_symbols_t *label_symbols) {
    unsigned char *indices = (unsigned char *)label_symbols->data;
    int            i;

    int max_len  = 0;
    int curr_len = 0;

    for (i = 1; i < label_symbols->num_symbols; i++) {
        curr_len = indices[i] - indices[i - 1] - 1;
        if (curr_len > max_len) {
            max_len = curr_len;
        }
    }

    curr_len = strlen(
        label_symbols_idx_to_ptr(label_symbols, label_symbols->num_symbols - 1)
    );
    if (curr_len > max_len) {
        max_len = curr_len;
    }

    return curr_len;
}

int label_selection_str_max_len(label_selection_t *label_selection) {
    return label_symbols_max_str_len(label_selection->label_symbols) *
           label_selection->len;
}

void label_selection_str(label_selection_t *label_selection, char *out) {
    label_symbols_t *label_symbols = label_selection->label_symbols;
    for (int i = 0; i < label_selection->next; i++) {
        out = stpcpy(
            out,
            label_symbols_idx_to_ptr(label_symbols, label_selection->input[i])
        );
    }

    *out = '\0';
}

void label_selection_str_split(
    label_selection_t *label_selection, char *prefix, char *suffix, int cut
) {
    if (cut < 0) {
        cut = 0;
    } else if (cut >= label_selection->next) {
        cut = label_selection->next;
    }

    label_symbols_t *label_symbols = label_selection->label_symbols;
    for (int i = 0; i < cut; i++) {
        prefix = stpcpy(
            prefix,
            label_symbols_idx_to_ptr(label_symbols, label_selection->input[i])
        );
    }
    *prefix = '\0';

    for (int i = cut; i < label_selection->next; i++) {
        suffix = stpcpy(
            suffix,
            label_symbols_idx_to_ptr(label_symbols, label_selection->input[i])
        );
    }
    *suffix = '\0';
}

void label_selection_free(label_selection_t *label_selection) {
    free(label_selection);
}
