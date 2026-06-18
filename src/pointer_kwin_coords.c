// SPDX-License-Identifier: GPL-3.0-only

#include "pointer_kwin_coords.h"

bool map_global_to_region(
    const struct eis_region *regions, uint32_t num_regions, int32_t gx,
    int32_t gy, struct region_point *out
) {
    for (uint32_t i = 0; i < num_regions; i++) {
        const struct eis_region *r = &regions[i];
        // Widen the upper-bound additions to int64_t to avoid signed overflow
        // (UB) when a region's origin plus extent exceeds INT32_MAX.
        if (gx >= r->x && (int64_t)gx < (int64_t)r->x + r->width &&
            gy >= r->y && (int64_t)gy < (int64_t)r->y + r->height) {
            out->region_index = i;
            out->x            = (double)(gx - r->x);
            out->y            = (double)(gy - r->y);
            return true;
        }
    }
    return false;
}
