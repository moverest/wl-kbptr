#include "utils_cairo.h"

#include <cairo.h>

void cairo_set_source_u32(void *cairo, uint32_t color) {
    cairo_set_source_rgba(
        (cairo_t *)cairo, (color >> 24 & 0xff) / 255.0,
        (color >> 16 & 0xff) / 255.0, (color >> 8 & 0xff) / 255.0,
        (color & 0xff) / 255.0
    );
}
