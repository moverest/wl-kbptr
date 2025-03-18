#ifndef __TARGET_DETECTION_INCLUDED__
#define __TARGET_DETECTION_INCLUDED__

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#if OPENCV_ENABLED

#include "utils.h"

#include <stdint.h>
#include <wayland-client.h>

EXTERNC int compute_target_from_img_buffer(
    void *data, uint32_t height, uint32_t width, uint32_t stride,
    enum wl_shm_format format, enum wl_output_transform transform,
    struct rect initial_area, struct rect **areas
);

#endif

#undef EXTERNC

#endif
