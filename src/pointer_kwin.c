// SPDX-License-Identifier: GPL-3.0-only

#if KWIN_ENABLED

#include "eis_dbus.h"
#include "log.h"
#include "pointer.h"
#include "pointer_kwin_coords.h"
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

// Gap injected before the press and again before the release of a click. The
// EIS round-trips order the frames but do not advance ei_now() far enough to
// separate them, so without this pause press and release carry the same
// millisecond timestamp, i.e. a zero-duration click. Qt accepts that, but
// Chromium-based Wayland clients discard it. A gap on the order of tens of
// milliseconds matches real hardware and is seen as a distinct press/release by
// every toolkit.
#define KWIN_CLICK_GAP_US 60000

struct kwin_ei_state {
    struct eis_connection conn;
    struct ei            *ei;
    struct ei_seat       *seat;
    struct ei_device     *device;
    bool                  device_resumed;
    bool                  pong_received;
    bool                  failed;
    // Set once we have tried (and failed) to connect, so we don't re-attempt --
    // and re-prompt the user -- on every keystroke.
    bool connect_failed;
    // Monotonically increasing sequence number for ei_device_start_emulating(),
    // as required by libei when the session is reused across multiple moves.
    uint32_t sequence;
};

static void drain_events(struct kwin_ei_state *s) {
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
            // KWin may offer more than one device; the one that later resumes
            // (EI_EVENT_DEVICE_RESUMED) is the one we ultimately emulate on.
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

static bool pump_until_resumed(struct kwin_ei_state *s) {
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
static void flush_until_pong(struct kwin_ei_state *s) {
    // Reset so this can be called more than once per connection: each call
    // waits for the pong matching the ping it just sent, not a stale one.
    s->pong_received = false;

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

static struct ei_device *kwin_connect_device(struct kwin_ei_state *s) {
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
    // s->conn must stay open until after ei_unref() (see pointer_kwin_move).
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

// Lazily open the EIS session and cache it on the state so the same libei
// device is reused for every pointer move instead of reconnecting (and making
// KWin build/tear down a virtual device) on every keystroke. Returns the ready
// session, or NULL if it could not be established -- in which case the failure
// is remembered so we don't retry, and re-prompt, on subsequent moves.
static struct kwin_ei_state *kwin_ensure_connected(struct state *state) {
    if (state->pointer_kwin != NULL) {
        struct kwin_ei_state *s = state->pointer_kwin;
        return (s->connect_failed || s->failed || s->device == NULL) ? NULL : s;
    }

    struct kwin_ei_state *s = calloc(1, sizeof(*s));
    if (s == NULL) {
        return NULL;
    }
    state->pointer_kwin = s;

    if (kwin_connect_device(s) == NULL) {
        if (s->ei != NULL) {
            ei_unref(s->ei);
            s->ei = NULL;
        }
        eis_dbus_disconnect(&s->conn);
        s->connect_failed = true;
        return NULL;
    }
    return s;
}

void pointer_kwin_move(
    struct state *state, uint32_t x, uint32_t y, enum click click
) {
    struct kwin_ei_state *s = kwin_ensure_connected(state);
    if (s == NULL) {
        return;
    }
    struct ei_device *device = s->device;

    int32_t gx = state->current_output->x + (int32_t)x;
    int32_t gy = state->current_output->y + (int32_t)y;

    struct eis_region regions[MAX_REGIONS];
    uint32_t          num_regions = 0;
    struct ei_region *r;
    for (size_t i = 0; (r = ei_device_get_region(device, i)) != NULL &&
                       num_regions < MAX_REGIONS;
         i++) {
        regions[num_regions].x      = (int32_t)ei_region_get_x(r);
        regions[num_regions].y      = (int32_t)ei_region_get_y(r);
        regions[num_regions].width  = ei_region_get_width(r);
        regions[num_regions].height = ei_region_get_height(r);
        num_regions++;
    }

    double              tx, ty;
    struct region_point p;
    if (map_global_to_region(regions, num_regions, gx, gy, &p)) {
        tx = (double)regions[p.region_index].x + p.x;
        ty = (double)regions[p.region_index].y + p.y;
    } else {
        // No region covers the target: send the raw global coordinates and let
        // the EIS implementation decide. This is also where a layout with more
        // than MAX_REGIONS regions would land, so trace it -- the pointer may
        // end up somewhere unexpected.
        LOG_DEBUG(
            "No EIS region contains (%d,%d) among %u region(s); sending global "
            "coordinates.",
            gx, gy, num_regions
        );
        tx = (double)gx;
        ty = (double)gy;
    }

    // A sender must bracket emulated events with start/stop emulating, and
    // each logical hardware event must be terminated by ei_device_frame(). The
    // sequence number must increase on each start since the session is reused.
    ei_device_start_emulating(device, ++s->sequence);

    // Inject motion, press and release as separate frames, round-tripping with
    // the EIS implementation between each step. This mirrors the working
    // wlr_virtual_pointer path (pointer_wlr.c), which does a wl_display
    // roundtrip after every event. Without the round-trips KWin receives the
    // whole motion+press+release burst at once and the press/release share a
    // near-identical ei_now() timestamp, producing a zero-duration click that
    // does not reliably register. Each flush_until_pong() forces KWin to
    // process the preceding frame -- and gives the press/release a real
    // wall-clock gap -- before the next is sent.
    uint64_t t_motion = ei_now(s->ei);
    ei_device_pointer_motion_absolute(device, tx, ty);
    ei_device_frame(device, t_motion);
    flush_until_pong(s);

    if (click != CLICK_NONE) {
        uint32_t btn = click == CLICK_RIGHT_BTN    ? EIS_BTN_RIGHT
                       : click == CLICK_MIDDLE_BTN ? EIS_BTN_MIDDLE
                                                   : EIS_BTN_LEFT;

        // Separate the press and release in wall-clock time so they are not
        // seen as a zero-duration click (see KWIN_CLICK_GAP_US).
        usleep(KWIN_CLICK_GAP_US);

        uint64_t t_press = ei_now(s->ei);
        ei_device_button_button(device, btn, true);
        ei_device_frame(device, t_press);
        flush_until_pong(s);

        usleep(KWIN_CLICK_GAP_US);

        uint64_t t_release = ei_now(s->ei);
        ei_device_button_button(device, btn, false);
        ei_device_frame(device, t_release);
        flush_until_pong(s);
    }

    ei_device_stop_emulating(device);
    ei_dispatch(s->ei);

    // Final round-trip to guarantee the queued frames (notably the final
    // button-release and stop-emulating) are written out. The session itself is
    // kept open for reuse and released by pointer_kwin_destroy() at exit.
    flush_until_pong(s);
}

void pointer_kwin_destroy(struct state *state) {
    struct kwin_ei_state *s = state->pointer_kwin;
    if (s == NULL) {
        return;
    }
    if (s->ei != NULL) {
        ei_unref(s->ei);
    }
    // Drop the D-Bus connection only now that libei is fully torn down.
    eis_dbus_disconnect(&s->conn);
    free(s);
    state->pointer_kwin = NULL;
}

#endif
