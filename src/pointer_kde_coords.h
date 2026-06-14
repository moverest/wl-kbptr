// SPDX-License-Identifier: GPL-3.0-only

#ifndef __POINTER_KDE_COORDS_H_INCLUDED__
#define __POINTER_KDE_COORDS_H_INCLUDED__

#include <stdbool.h>
#include <stdint.h>

// An EIS device region in global logical coordinates.
struct eis_region {
    int32_t  x;
    int32_t  y;
    uint32_t width;
    uint32_t height;
};

// Result of mapping a global point into a region.
struct region_point {
    uint32_t region_index; // index into the regions array
    double   x;            // region-local x
    double   y;            // region-local y
};

// Maps a global point (gx, gy) to the containing region and its region-local
// coordinates. Returns true if a region contains the point; false otherwise
// (out is left untouched). Uses half-open intervals [x, x+width) and
// [y, y+height).
bool map_global_to_region(
    const struct eis_region *regions, uint32_t num_regions, int32_t gx,
    int32_t gy, struct region_point *out
);

#endif
