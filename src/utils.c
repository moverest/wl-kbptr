#include <stdint.h>
#include <string.h>

int min(int a, int b) {
    return a < b ? a : b;
}

int max(int a, int b) {
    return a > b ? a : b;
}

int str_to_rune(char *str, uint32_t *rune) {
    unsigned char *c = ((unsigned char *)str);

    // `\0`, End of string byte.
    if (*c == 0) {
        *rune = 0;
        return 0;
    }

    uint32_t value;
    int      size;

    if ((*c & 0b10000000) == 0) {
        value = *c & 0b01111111;
        size  = 1;
    } else if ((*c & 0b11100000) == 0b11000000) {
        value = *c & 0b00011111;
        size  = 2;
    } else if ((*c & 0b11110000) == 0b11100000) {
        value = *c & 0b00001111;
        size  = 3;
    } else if ((*c & 0b11111000) == 0b11110000) {
        value = *c & 0b00000111;
        size  = 4;
    } else {
        return -1;
    }

    for (int i = 1; i < size; i++) {
        c++;
        if ((*c & 0b11000000) != 0b10000000) {
            return -1;
        }

        value <<= 6;
        value  += *c & 0b00111111;
    }

    *rune = value;
    return size;
}

int find_str(char **strs, size_t len, char *to_find) {
    int matched_i = -1;
    for (int i = 0; i < len; i++) {
        if (strcmp(to_find, strs[i]) == 0) {
            matched_i = i;
            break;
        }
    }

    return matched_i;
}
