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

static void compute_rects(
    const std::vector<std::vector<cv::Point>> &contours,
    std::vector<cv::Rect2d> &rects, double scale, double x_off, double y_off
) {
    rects.clear();
    rects.reserve(contours.size());

    for (const std::vector<cv::Point> &contour : contours) {
        cv::Rect rect = cv::boundingRect(contour);

        rects.push_back(cv::Rect2d(
            rect.x / scale + x_off, rect.y / scale + y_off, rect.width / scale,
            rect.height / scale
        ));
    }
}

static size_t filter_rects(
    const std::vector<cv::Rect2d> &rects,
    const std::vector<cv::Vec4i> &hierachy, std::vector<bool> &filtered
) {
    filtered.assign(rects.size(), false);

    for (size_t i = 0; i < rects.size(); i++) {
        const auto &rect = rects[i];

        if (rect.height >= 50 || rect.width >= 500 || rect.height <= 3 ||
            rect.width <= 7) {
            filtered[i] = true;
            continue;
        }
    }

    std::vector<int> to_explore;
    for (size_t i = 0; i < rects.size(); i++) {
        if (hierachy[i][3] >= 0) {
            to_explore.push_back(i);
        }
    }

    while (!to_explore.empty()) {
        int i = to_explore.back();
        to_explore.pop_back();

        if (!filtered[i]) {
            int parent_i = hierachy[i][3];
            if (filtered[parent_i]) {
                goto filtered;
            }

            const auto &rect = rects[i];
            if (rect.height <= 7) {
                filtered[i] = true;
                goto filtered;
            }

            const auto &parent_rect = rects[parent_i];

            const double center_x = rect.x + rect.width / 2.;
            const double center_y = rect.y + rect.height / 2.;
            const double parent_center_x =
                parent_rect.x + parent_rect.width / 2.;
            const double parent_center_y =
                parent_rect.y + parent_rect.height / 2.;

            if (abs(center_x - parent_center_x) < 7 &&
                abs(center_y - parent_center_y) < 7) {
                filtered[i] = true;
                goto filtered;
            }
        }

    filtered:
        int child = hierachy[i][0];
        while (child >= 0) {
            to_explore.push_back(child);
            child = hierachy[child][2];
        }
    }

    size_t not_filtered_count = 0;
    for (const auto &curr : filtered) {
        if (!curr) {
            not_filtered_count += 1;
        }
    }

    return not_filtered_count;
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
    std::vector<cv::Vec4i>              hierachy;
    cv::findContours(
        m1, contours, hierachy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE
    );

    std::vector<cv::Rect2d> rects;
    std::vector<bool>       filtered;

    compute_rects(contours, rects, scale, initial_area.x, initial_area.y);
    int final_rect_count = filter_rects(rects, hierachy, filtered);

    size_t area_i = 0;
    *areas = (struct rect *)malloc(sizeof(struct rect) * final_rect_count);
    for (size_t i = 0; i < rects.size(); i++) {
        if (filtered[i]) {
            continue;
        }

        const auto   rect = rects[i];
        struct rect *area = &(*areas)[area_i];
        area->x           = round(rect.x);
        area->y           = round(rect.y);
        area->w           = round(rect.width);
        area->h           = round(rect.height);

        area_i++;
    }

    return final_rect_count;
}
