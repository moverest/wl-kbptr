#include "surface_buffer.h"

#include "log.h"

#include <cairo/cairo.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define CAIRO_SURFACE_FORMAT CAIRO_FORMAT_ARGB32

static int create_shm_file(void) {
    char name[] = "/tmp/wl-shm-XXXXXX";
    int  fd     = mkostemp(name, O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }

    unlink(name);
    return fd;
}

int allocate_shm_file(size_t size) {
    int fd = create_shm_file();
    if (fd < 0) {
        return -1;
    }

    int err;
    while ((err = ftruncate(fd, size)) && errno == EINTR) {}
    if (err) {
        close(fd);
        return -1;
    }

    return fd;
}

static void handle_buffer_release(void *data, struct wl_buffer *wl_buffer) {
    ((struct surface_buffer *)data)->state = SURFACE_BUFFER_READY;
}

static const struct wl_buffer_listener wl_buffer_listener = {
    .release = handle_buffer_release,
};

static struct surface_buffer *surface_buffer_init(
    struct wl_shm *wl_shm, struct surface_buffer *buffer, int32_t width,
    int32_t height
) {
    const uint32_t stride =
        cairo_format_stride_for_width(CAIRO_SURFACE_FORMAT, width);
    const uint32_t data_size = height * stride;
    void          *data;

    int fd = allocate_shm_file(data_size);
    if (fd < 0) {
        LOG_ERR("Could not allocate shared buffer for surface buffer.");
        return NULL;
    }

    data = mmap(NULL, data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        LOG_ERR("Could not mmap shared buffer for surface buffer.");

        close(fd);
        return NULL;
    }

    struct wl_shm_pool *wl_shm_pool = wl_shm_create_pool(wl_shm, fd, data_size);
    buffer->wl_buffer               = wl_shm_pool_create_buffer(
        wl_shm_pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888
    );
    wl_buffer_add_listener(buffer->wl_buffer, &wl_buffer_listener, buffer);
    wl_shm_pool_destroy(wl_shm_pool);

    close(fd);

    buffer->data      = data;
    buffer->data_size = data_size;
    buffer->width     = width;
    buffer->height    = height;
    buffer->state     = SURFACE_BUFFER_READY;

    buffer->cairo_surface = cairo_image_surface_create_for_data(
        buffer->data, CAIRO_SURFACE_FORMAT, width, height, stride
    );
    buffer->cairo = cairo_create(buffer->cairo_surface);

    return buffer;
}

static void surface_buffer_destroy(struct surface_buffer *buffer) {
    if (buffer->state == SURFACE_BUFFER_UNITIALIZED) {
        return;
    }

    if (buffer->cairo) {
        cairo_destroy(buffer->cairo);
    }

    if (buffer->cairo_surface) {
        cairo_surface_destroy(buffer->cairo_surface);
    }

    if (buffer->wl_buffer) {
        wl_buffer_destroy(buffer->wl_buffer);
    }

    if (buffer->data) {
        munmap(buffer->data, buffer->data_size);
    }

    memset(buffer, 0, sizeof(struct surface_buffer));
}

void surface_buffer_pool_init(struct surface_buffer_pool *pool) {
    memset(pool, 0, sizeof(struct surface_buffer_pool));
}

void surface_buffer_pool_destroy(struct surface_buffer_pool *pool) {
    surface_buffer_destroy(&pool->buffers[0]);
    surface_buffer_destroy(&pool->buffers[1]);
}

struct surface_buffer *get_next_buffer(
    struct wl_shm *wl_shm, struct surface_buffer_pool *pool, uint32_t width,
    uint32_t height
) {
    struct surface_buffer *buffer = NULL;
    for (size_t i = 0; i < 2; i++) {
        if (pool->buffers[i].state != SURFACE_BUFFER_BUSY) {
            buffer = &pool->buffers[i];
            break;
        }
    }

    if (buffer == NULL) {
        LOG_WARN("All surface buffers are busy.");
        return NULL;
    }

    if (buffer->width != width || buffer->height != height) {
        surface_buffer_destroy(buffer);
    }

    if (buffer->state == SURFACE_BUFFER_UNITIALIZED) {
        if (surface_buffer_init(wl_shm, buffer, width, height) == NULL) {
            LOG_ERR("Could not initialize next buffer.");
            return NULL;
        }
    }

    return buffer;
}
