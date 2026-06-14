// SPDX-License-Identifier: GPL-3.0-only

#if KDE_ENABLED

#include "eis_dbus.h"

#include "log.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#ifdef USE_BASU_SDBUS
#include <basu/sd-bus.h>
#else
#include <systemd/sd-bus.h>
#endif

// KWin EIS device-type bitmask (keyboard=1, pointer=2, touch=4). We request all
// and let libei negotiate the pointer/button capabilities it actually binds.
#define EIS_DEVICE_TYPES 7

bool eis_dbus_connect(struct eis_connection *conn) {
    conn->bus = NULL;
    conn->fd  = -1;

    sd_bus_error    error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    sd_bus         *bus   = NULL;
    int             fd    = -1;
    int             ret;

    ret = sd_bus_open_user(&bus);
    if (ret < 0) {
        LOG_ERR("Failed to connect to session bus: %s", strerror(-ret));
        goto fail;
    }

    ret = sd_bus_call_method(
        bus, "org.kde.KWin", "/org/kde/KWin/EIS/RemoteDesktop",
        "org.kde.KWin.EIS.RemoteDesktop", "connectToEIS", &error, &reply, "i",
        EIS_DEVICE_TYPES
    );
    if (ret < 0) {
        // error.message is NULL on transport failures (e.g. KWin not running).
        LOG_ERR(
            "connectToEIS call failed: %s",
            error.message ? error.message : strerror(-ret)
        );
        goto fail;
    }

    int reply_fd;
    int handle; // KWin disconnect cookie; unused, disconnect is implicit.
    ret = sd_bus_message_read(reply, "hi", &reply_fd, &handle);
    if (ret < 0) {
        LOG_ERR("Failed to read fd from reply: %s", strerror(-ret));
        goto fail;
    }

    // The fd is owned by the message; dup it so it outlives the message.
    fd = dup(reply_fd);
    if (fd < 0) {
        LOG_ERR("Failed to dup EIS fd: %s", strerror(errno));
        goto fail;
    }

    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);

    // Keep the bus open: KWin ties the EIS session to this D-Bus connection and
    // tears it down as soon as the connection drops.
    conn->bus = bus;
    conn->fd  = fd;
    return true;

fail:
    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
    return false;
}

void eis_dbus_disconnect(struct eis_connection *conn) {
    if (conn->bus != NULL) {
        sd_bus_unref(conn->bus);
        conn->bus = NULL;
    }
}

#endif
