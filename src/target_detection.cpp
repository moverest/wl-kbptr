#include "target_detection.h"

#include "log.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdint.h>
#include <stdlib.h>
#include <wayland-client.h>

static cv::Mat get_gray_scale_from_buffer(
    void *data, uint32_t height, uint32_t width, enum wl_shm_format format
) {
    cv::Mat buf;
    int     in_out[3];

    switch (format) {
    case WL_SHM_FORMAT_ARGB8888:
    case WL_SHM_FORMAT_XRGB8888:
        buf       = cv::Mat(height, width, CV_8UC4, data);
        in_out[0] = 1;
        in_out[1] = 1;
        in_out[2] = 2;
        break;

    case WL_SHM_FORMAT_XBGR8888:
    case WL_SHM_FORMAT_ABGR8888:
        buf       = cv::Mat(height, width, CV_8UC4, data);
        in_out[0] = 2;
        in_out[1] = 1;
        in_out[2] = 0;
        break;

    default:
        LOG_ERR("Unsupported format (%o).", format);
        exit(1);
    }

    cv::Mat in_channels[4];
    cv::split(buf, in_channels);

    cv::Mat out_channels[3] = {
        in_channels[in_out[0]],
        in_channels[in_out[1]],
        in_channels[in_out[2]],
    };

    cv::Mat out;
    cv::merge(out_channels, 3, out);

    cv::Mat grayed;
    cv::cvtColor(out, grayed, cv::COLOR_BGR2GRAY);

    return grayed;
}

static void apply_transform(
    cv::Mat &m, enum wl_output_transform transform, uint32_t &width,
    uint32_t &height
) {
    cv::Mat tmp;
    bool    switch_width_height = false;

    switch (transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
        break;

    case WL_OUTPUT_TRANSFORM_90:
        cv::rotate(m, tmp, cv::ROTATE_90_CLOCKWISE);
        m                   = tmp;
        switch_width_height = true;
        break;

    case WL_OUTPUT_TRANSFORM_270:
        cv::rotate(m, tmp, cv::ROTATE_90_COUNTERCLOCKWISE);
        m                   = tmp;
        switch_width_height = true;
        break;

    case WL_OUTPUT_TRANSFORM_180:
        cv::rotate(m, tmp, cv::ROTATE_180);
        m = tmp;
        break;

    case WL_OUTPUT_TRANSFORM_FLIPPED:
        cv::flip(m, tmp, 1);
        m = tmp;
        break;

    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
        cv::rotate(m, tmp, cv::ROTATE_90_CLOCKWISE);
        cv::flip(tmp, m, 1);
        switch_width_height = true;
        break;

    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
        cv::rotate(m, tmp, cv::ROTATE_180);
        cv::flip(tmp, m, 1);
        break;

    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
        cv::rotate(m, tmp, cv::ROTATE_90_COUNTERCLOCKWISE);
        cv::flip(tmp, m, 1);
        switch_width_height = true;
        break;
    }

    if (switch_width_height) {
        uint32_t tmp = height;
        height       = width;
        width        = tmp;
    }
}

int compute_target_from_img_buffer(
    void *data, uint32_t height, uint32_t width, uint32_t stride,
    enum wl_shm_format format, enum wl_output_transform transform,
    struct rect initial_area, struct rect **areas
) {
    cv::Mat m1 = get_gray_scale_from_buffer(data, height, width, format);
    apply_transform(m1, transform, width, height);

    double scale = ((double)height) / ((double)initial_area.h);

    cv::Mat m2;
    cv::Mat kernel =
        cv::Mat::ones(round(2.5 * scale), round(3.5 * scale), CV_8U);

    cv::Canny(m1, m2, 70, 220);
    cv::dilate(m2, m1, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(m1, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    int areas_cap   = 256;
    *areas          = (struct rect *)malloc(sizeof(struct rect) * areas_cap);
    int areas_count = 0;

    for (const std::vector<cv::Point> &contour : contours) {
        cv::Rect rect = cv::boundingRect(contour);

        int x = round(rect.x / scale) + initial_area.x;
        int y = round(rect.y / scale) + initial_area.y;
        int h = round(rect.height / scale);
        int w = round(rect.width / scale);

        if (h >= 50 || w >= 500 || h <= 3 || w <= 7) {
            continue;
        }

        if (areas_count >= areas_cap) {
            areas_cap *= 2;
            *areas =
                (struct rect *)realloc(*areas, sizeof(struct rect) * areas_cap);
        }

        struct rect *area = &(*areas)[areas_count];
        area->x           = x;
        area->y           = y;
        area->w           = w;
        area->h           = h;

        areas_count++;
    }

    return areas_count;
}
