// SPDX-License-Identifier: GPL-3.0-only

#if KDE_ENABLED

#include "pointer.h"

#include "eis_dbus.h"
#include "log.h"
#include "pointer_kde_coords.h"
#include "state.h"

#include <libei.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>

// Button codes match linux/input-event-codes.h, as required by
// ei_device_button_button().
#define EIS_BTN_LEFT   0x110
#define EIS_BTN_RIGHT  0x111
#define EIS_BTN_MIDDLE 0x112

#define MAX_REGIONS 16

struct kde_ei_state {
    struct eis_connection conn;
    struct ei            *ei;
    struct ei_seat       *seat;
    struct ei_device     *device;
    bool                  device_resumed;
    bool                  pong_received;
    bool                  failed;
};

static void drain_events(struct kde_ei_state *s) {
    struct ei_event *e;
    while ((e = ei_get_event(s->ei)) != NULL) {
        switch (ei_event_get_type(e)) {
        case EI_EVENT_SEAT_ADDED:
            s->seat = ei_event_get_seat(e);
            ei_seat_bind_capabilities(
                s->seat, EI_DEVICE_CAP_POINTER_ABSOLUTE, EI_DEVICE_CAP_BUTTON,
                NULL
            );
            break;
        case EI_EVENT_DEVICE_ADDED:
            s->device = ei_event_get_device(e);
            break;
        case EI_EVENT_DEVICE_RESUMED:
            s->device         = ei_event_get_device(e);
            s->device_resumed = true;
            break;
        case EI_EVENT_PONG:
            s->pong_received = true;
            break;
        case EI_EVENT_DISCONNECT:
            s->failed = true;
            break;
        default:
            break;
        }
        ei_event_unref(e);
    }
}

static bool pump_until_resumed(struct kde_ei_state *s) {
    while (!s->device_resumed && !s->failed) {
        // Dispatch first so the initial connection handshake -- and any request
        // queued during the previous drain (e.g. capability binding) -- is
        // driven before we block in poll(). Polling first would block waiting
        // for a response KWin has no reason to send yet, timing out.
        ei_dispatch(s->ei);
        drain_events(s);
        if (s->device_resumed || s->failed) {
            break;
        }
        struct pollfd pfd = {.fd = ei_get_fd(s->ei), .events = POLLIN};
        if (poll(&pfd, 1, 2000) <= 0) {
            break; // timeout or error
        }
    }
    return s->device_resumed && !s->failed;
}

// Deterministically flush queued outgoing events before teardown. libei 1.5.0
// exposes no explicit flush primitive, only the ei_ping() round-trip: requests
// are processed in-order by the EIS implementation, so once the matching
// EI_EVENT_PONG arrives, all previously queued motion/button frames have been
// written out and processed. Pump the existing poll/ei_dispatch/drain loop
// until the pong (or a disconnect, or the 2s timeout) is observed, then return
// so the caller can ei_unref(). On timeout we just proceed; we never hang.
static void flush_until_pong(struct kde_ei_state *s) {
    struct ei_ping *ping = ei_new_ping(s->ei);
    if (ping == NULL) {
        return;
    }
    ei_ping(ping);
    ei_ping_unref(ping);

    while (!s->pong_received && !s->failed) {
        ei_dispatch(s->ei);
        drain_events(s);
        if (s->pong_received || s->failed) {
            break;
        }
        struct pollfd pfd = {.fd = ei_get_fd(s->ei), .events = POLLIN};
        if (poll(&pfd, 1, 2000) <= 0) {
            break; // timeout or error: proceed to teardown
        }
    }
}

static struct ei_device *kde_connect_device(struct kde_ei_state *s) {
    if (!eis_dbus_connect(&s->conn)) {
        return NULL;
    }

    s->ei = ei_new_sender(NULL);
    if (s->ei == NULL) {
        close(s->conn.fd);
        eis_dbus_disconnect(&s->conn);
        return NULL;
    }

    // ei_setup_backend_fd() takes ownership of the fd (closes it on teardown)
    // and returns 0 on success or a negative errno on failure. Do not close the
    // fd after this call; ei_unref() releases it. The D-Bus connection in
    // s->conn must stay open until after ei_unref() (see pointer_kde_move).
    int err = ei_setup_backend_fd(s->ei, s->conn.fd);
    if (err < 0) {
        LOG_ERR("ei_setup_backend_fd failed.");
        return NULL;
    }

    if (!pump_until_resumed(s) || s->failed || s->device == NULL) {
        LOG_ERR("EIS device did not become ready.");
        return NULL;
    }
    return s->device;
}

bool pointer_kde_available(struct state *state) {
    (void)state;
    struct eis_connection conn;
    if (!eis_dbus_connect(&conn)) {
        return false;
    }
    close(conn.fd);
    eis_dbus_disconnect(&conn);
    return true;
}

void pointer_kde_move(
    struct state *state, uint32_t x, uint32_t y, enum click click
) {
    int32_t gx = state->current_output->x + (int32_t)x;
    int32_t gy = state->current_output->y + (int32_t)y;

    struct kde_ei_state s      = {0};
    struct ei_device   *device = kde_connect_device(&s);
    if (device == NULL) {
        if (s.ei != NULL) {
            ei_unref(s.ei);
        }
        eis_dbus_disconnect(&s.conn);
        return;
    }

    struct eis_region regions[MAX_REGIONS];
    uint32_t          num_regions = 0;
    struct ei_region *r;
    for (size_t i = 0;
         (r = ei_device_get_region(device, i)) != NULL
         && num_regions < MAX_REGIONS;
         i++) {
        regions[num_regions].x      = (int32_t)ei_region_get_x(r);
        regions[num_regions].y      = (int32_t)ei_region_get_y(r);
        regions[num_regions].width  = ei_region_get_width(r);
        regions[num_regions].height = ei_region_get_height(r);
        num_regions++;
    }

    double               tx, ty;
    struct region_point  p;
    if (map_global_to_region(regions, num_regions, gx, gy, &p)) {
        tx = (double)regions[p.region_index].x + p.x;
        ty = (double)regions[p.region_index].y + p.y;
    } else {
        tx = (double)gx;
        ty = (double)gy;
    }

    // A sender must bracket emulated events with start/stop emulating, and
    // each logical hardware event must be terminated by ei_device_frame().
    ei_device_start_emulating(device, 1);

    ei_device_pointer_motion_absolute(device, tx, ty);
    ei_device_frame(device, ei_now(s.ei));
    ei_dispatch(s.ei);

    if (click != CLICK_NONE) {
        uint32_t btn = click == CLICK_RIGHT_BTN    ? EIS_BTN_RIGHT
                       : click == CLICK_MIDDLE_BTN ? EIS_BTN_MIDDLE
                                                   : EIS_BTN_LEFT;
        ei_device_button_button(device, btn, true);
        ei_device_frame(device, ei_now(s.ei));
        ei_device_button_button(device, btn, false);
        ei_device_frame(device, ei_now(s.ei));
        ei_dispatch(s.ei);
    }

    ei_device_stop_emulating(device);
    ei_dispatch(s.ei);

    // Round-trip with the EIS implementation to guarantee the queued frames
    // (notably the final button-release) are written out before we destroy the
    // context; ei_unref() otherwise gives no such guarantee.
    flush_until_pong(&s);

    ei_unref(s.ei);
    // Drop the D-Bus connection only now that libei is fully torn down.
    eis_dbus_disconnect(&s.conn);
}

#endif
