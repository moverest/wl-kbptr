#ifndef __SURFACE_BUFFER_H_INCLUDED__
#define __SURFACE_BUFFER_H_INCLUDED__

#include <cairo/cairo.h>
#include <wayland-client.h>

enum surface_buffer_state {
    // This must be set to 0 as we set the whole structure to 0 when it's not
    // initialized.
    SURFACE_BUFFER_UNITIALIZED = 0,

    SURFACE_BUFFER_READY = 1,
    SURFACE_BUFFER_BUSY  = 2,
};

struct surface_buffer {
    enum surface_buffer_state state;
    struct wl_buffer         *wl_buffer;
    cairo_surface_t          *cairo_surface;
    cairo_t                  *cairo;
    void                     *data;
    size_t                    data_size;
    uint32_t                  width;
    uint32_t                  height;
};

struct surface_buffer_pool {
    struct surface_buffer buffers[2];
};

void surface_buffer_pool_init(struct surface_buffer_pool *pool);
void surface_buffer_pool_destroy(struct surface_buffer_pool *pool);

struct surface_buffer *get_next_buffer(
    struct wl_shm *wl_shm, struct surface_buffer_pool *pool, uint32_t width,
    uint32_t height
);

int allocate_shm_file(size_t size);

#endif
