#ifndef __LABEL_H_INCLUDED__
#define __LABEL_H_INCLUDED__

#include <stdbool.h>

typedef struct {
    /*               keys             keys[num_symbols]
     *               |                |
     *  | 4 ||xxxx|| 0 | 2 | 4 | 6 ||`a`| 0 |`b`| 0 |`c`| 0 |`d`| 0 |
     *    ^   ^      ^-----------^    ^---------------------------^
     *    |   |         offsets               strings
     *    |   pointer to symbol data
     *  number of symbols
     *
     * The symbol field's data is identical in structure to the keys field.
     */
    unsigned char num_symbols;
    char         *symbols;
    char          keys[];
} label_symbols_t;

typedef struct {
    label_symbols_t *label_symbols;
    int              num_labels;
    unsigned char    len;
    unsigned char    next;
    unsigned char    input[];
} label_selection_t;

// Create a `label_symbols_t` from a string of characters.
// Returns `NULL` upon error.
label_symbols_t *label_symbols_from_str(char *s);
// Create a `label_symbols_t` from a string of label characters and a
// possibly-empty string of key characters.
// Returns `NULL` upon error.
label_symbols_t *label_symbols_from_strs(char *symbols, char *keys);


// Free memory of a `label_symbols_t`.
void label_symbols_free(label_symbols_t *ls);

// Get pointer to string of the key at given index.
// Returns value <0 upon error.
char *label_symbols_idx_to_key_ptr(label_symbols_t *label_symbols, int idx);

// Find key index from given string.
// Returns value <0 upon error.
int label_symbols_find_key_idx(label_symbols_t *label_symbols, char *s);

// Create a `label_selection_t`.
label_selection_t *
label_selection_new(label_symbols_t *label_symbols, int num_labels);

// Clear selection.
void label_selection_clear(label_selection_t *label_selection);

enum label_selection_append_ret {
    LABEL_SELECTION_APPEND_SUCCESS      = 0,
    LABEL_SELECTION_APPEND_IDX_OVERFLOW = 1,
    LABEL_SELECTION_APPEND_FULL         = 2
};
// Append to selection.
enum label_selection_append_ret
label_selection_append(label_selection_t *label_selection, int idx);

// Erase last symbol.
// Returns true if it did else false.
bool label_selection_back(label_selection_t *label_selection);

// Returns true if `start` is the same as `reference` beginning else false.
bool label_selection_is_included(
    label_selection_t *reference, label_selection_t *start
);

// Returns associated label index.
int label_selection_to_idx(label_selection_t *label_selection);

// Set selection from associated index.
int label_selection_set_from_idx(label_selection_t *label_selection, int idx);

// Set to selection with incremented associated index.
int label_selection_incr(label_selection_t *label_selection);

// Get size of buffer needed to store label's string.
int label_selection_str_max_len(label_selection_t *label_selection);

// Get label's string.
void label_selection_str(label_selection_t *label_selection, char *out);

// Get label string split at `cut`.
void label_selection_str_split(
    label_selection_t *label_selection, char *prefix, char *suffix, int cut
);

// Free memory of `label_selection_t`.
void label_selection_free(label_selection_t *label_selection);

#endif
