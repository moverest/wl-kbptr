#include "target_detection.h"

#include "log.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <pixman.h>
#include <stdint.h>
#include <stdlib.h>
#include <wayland-client.h>

static pixman_format_code_t get_pixman_format(enum wl_shm_format format) {
    switch (format) {
    case WL_SHM_FORMAT_RGB332:
        return PIXMAN_r3g3b2;
    case WL_SHM_FORMAT_BGR233:
        return PIXMAN_b2g3r3;
    case WL_SHM_FORMAT_ARGB4444:
        return PIXMAN_a4r4g4b4;
    case WL_SHM_FORMAT_XRGB4444:
        return PIXMAN_x4r4g4b4;
    case WL_SHM_FORMAT_ABGR4444:
        return PIXMAN_a4b4g4r4;
    case WL_SHM_FORMAT_XBGR4444:
        return PIXMAN_x4b4g4r4;
    case WL_SHM_FORMAT_ARGB1555:
        return PIXMAN_a1r5g5b5;
    case WL_SHM_FORMAT_XRGB1555:
        return PIXMAN_x1r5g5b5;
    case WL_SHM_FORMAT_ABGR1555:
        return PIXMAN_a1b5g5r5;
    case WL_SHM_FORMAT_XBGR1555:
        return PIXMAN_x1b5g5r5;
    case WL_SHM_FORMAT_RGB565:
        return PIXMAN_r5g6b5;
    case WL_SHM_FORMAT_BGR565:
        return PIXMAN_b5g6r5;
    case WL_SHM_FORMAT_RGB888:
        return PIXMAN_r8g8b8;
    case WL_SHM_FORMAT_BGR888:
        return PIXMAN_b8g8r8;
    case WL_SHM_FORMAT_ARGB8888:
        return PIXMAN_a8r8g8b8;
    case WL_SHM_FORMAT_XRGB8888:
        return PIXMAN_x8r8g8b8;
    case WL_SHM_FORMAT_ABGR8888:
        return PIXMAN_a8b8g8r8;
    case WL_SHM_FORMAT_XBGR8888:
        return PIXMAN_x8b8g8r8;
    case WL_SHM_FORMAT_BGRA8888:
        return PIXMAN_b8g8r8a8;
    case WL_SHM_FORMAT_BGRX8888:
        return PIXMAN_b8g8r8x8;
    case WL_SHM_FORMAT_RGBA8888:
        return PIXMAN_r8g8b8a8;
    case WL_SHM_FORMAT_RGBX8888:
        return PIXMAN_r8g8b8x8;
    case WL_SHM_FORMAT_ARGB2101010:
        return PIXMAN_a2r10g10b10;
    case WL_SHM_FORMAT_ABGR2101010:
        return PIXMAN_a2b10g10r10;
    case WL_SHM_FORMAT_XRGB2101010:
        return PIXMAN_x2r10g10b10;
    case WL_SHM_FORMAT_XBGR2101010:
        return PIXMAN_x2b10g10r10;
    default:
        return (pixman_format_code_t)0;
    }
}

static pixman_image_t *make_pixman_image_a8r8g8b8(
    void *data, uint32_t width, uint32_t height, uint32_t stride,
    enum wl_shm_format format
) {
    pixman_format_code_t pixman_format = get_pixman_format(format);
    if (pixman_format == 0) {
        LOG_ERR("Unsupported format 0x%08x.", format);
        return NULL;
    }

    pixman_image_t *in_image = pixman_image_create_bits(
        pixman_format, width, height, (uint32_t *)data, stride
    );
    if (in_image == NULL) {
        LOG_ERR("Failed to create pixman image.");
        return NULL;
    }

    pixman_image_t *out_image = pixman_image_create_bits(
        PIXMAN_a8r8g8b8, width, height, NULL, width * height * 4
    );
    if (out_image == NULL) {
        LOG_ERR("Failed to create (out) pixman image.");
        return NULL;
    }

    pixman_image_composite32(
        PIXMAN_OP_SRC, in_image, NULL, out_image, 0, 0, 0, 0, 0, 0, width,
        height
    );

    pixman_image_unref(in_image);

    return out_image;
}

static cv::Mat get_gray_scale_from_buffer(
    void *data, uint32_t height, uint32_t width, uint32_t stride,
    enum wl_shm_format format
) {
    pixman_image_t *image = NULL;
    cv::Mat         buf;
    int             in_out[3];

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
        image = make_pixman_image_a8r8g8b8(data, width, height, stride, format);
        if (image == NULL) {
            exit(1);
        }
        buf = cv::Mat(height, width, CV_8UC4, pixman_image_get_data(image));
        in_out[0] = 1;
        in_out[1] = 1;
        in_out[2] = 2;
        break;
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

    if (image != NULL) {
        pixman_image_unref(image);
    }

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

            // Inner targets that are flat are most likely lines forming an
            // icon, e.g. a hamburger menu.
            if (rect.height <= 6) {
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

            // There's not much reasons to keep inner targets that have the same
            // center as this is where the user is going to click most likely.
            if (abs(center_x - parent_center_x) < 8 &&
                abs(center_y - parent_center_y) < 8) {
                filtered[i] = true;
                goto filtered;
            }

            // If the parent target is a square, it's most likely a button with
            // a single option or an icon.
            if (abs(parent_rect.height - parent_rect.width) < 5 &&
                parent_rect.height < 40 && parent_rect.width < 40) {
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
    cv::Mat m1 =
        get_gray_scale_from_buffer(data, height, width, stride, format);
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
