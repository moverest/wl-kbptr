// SPDX-License-Identifier: GPL-3.0-only

#include "log.h"
#include "pointer_kde_coords.h"

int main() {
    // Two side-by-side 1920x1080 outputs.
    struct eis_region regions[] = {
        {.x = 0, .y = 0, .width = 1920, .height = 1080},
        {.x = 1920, .y = 0, .width = 1920, .height = 1080},
    };
    struct region_point out;

    // Point in the first output.
    if (!map_global_to_region(regions, 2, 100, 200, &out)) {
        LOG_ERR("Expected point (100,200) to map.");
        return 1;
    }
    if (out.region_index != 0 || out.x != 100.0 || out.y != 200.0) {
        LOG_ERR(
            "Wrong mapping for (100,200): idx=%u x=%f y=%f", out.region_index,
            out.x, out.y
        );
        return 2;
    }

    // Point in the second output maps to region-local coords.
    if (!map_global_to_region(regions, 2, 2000, 300, &out)) {
        LOG_ERR("Expected point (2000,300) to map.");
        return 3;
    }
    if (out.region_index != 1 || out.x != 80.0 || out.y != 300.0) {
        LOG_ERR(
            "Wrong mapping for (2000,300): idx=%u x=%f y=%f", out.region_index,
            out.x, out.y
        );
        return 4;
    }

    // Point outside any region returns false.
    if (map_global_to_region(regions, 2, 5000, 5000, &out)) {
        LOG_ERR("Expected point (5000,5000) to NOT map.");
        return 5;
    }

    // Last pixel of the first output (half-open upper bound, must still map).
    if (!map_global_to_region(regions, 2, 1919, 1079, &out)) {
        LOG_ERR("Expected point (1919,1079) to map.");
        return 6;
    }
    if (out.region_index != 0 || out.x != 1919.0 || out.y != 1079.0) {
        LOG_ERR(
            "Wrong mapping for (1919,1079): idx=%u x=%f y=%f", out.region_index,
            out.x, out.y
        );
        return 7;
    }

    // Seam pixel: must belong to the second output, not alias to the first.
    if (!map_global_to_region(regions, 2, 1920, 0, &out)) {
        LOG_ERR("Expected point (1920,0) to map.");
        return 8;
    }
    if (out.region_index != 1 || out.x != 0.0 || out.y != 0.0) {
        LOG_ERR(
            "Wrong mapping for (1920,0): idx=%u x=%f y=%f", out.region_index,
            out.x, out.y
        );
        return 9;
    }

    return 0;
}
