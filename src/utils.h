#ifndef __UTILS_H_INCLUDED__
#define __UTILS_H_INCLUDED__

#include <stddef.h>
#include <stdint.h>

struct rect {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
};

int max(int a, int b);
int min(int a, int b);
int find_str(char **strs, size_t len, char *to_find);

// Extract first rune (32 bit UTF-8 code) in string.
// Return its encoded length in bytes or < 0 if invalid.
int str_to_rune(char *s, uint32_t *rune);

// Return index (in number of rune, not byte position) of given character.
// If not found, return < 0.
int str_index(char *s, uint32_t rune);

#endif
