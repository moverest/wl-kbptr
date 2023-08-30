#include <cairo.h>
#include <stdint.h>
#include <string.h>

int min(int a, int b) {
    return a < b ? a : b;
}

int max(int a, int b) {
    return a > b ? a : b;
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

void cairo_set_source_u32(void *cairo, uint32_t color) {
    cairo_set_source_rgba(
        (cairo_t *)cairo, (color >> 24 & 0xff) / 255.0,
        (color >> 16 & 0xff) / 255.0, (color >> 8 & 0xff) / 255.0,
        (color & 0xff) / 255.0
    );
}
