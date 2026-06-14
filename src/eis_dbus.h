// SPDX-License-Identifier: GPL-3.0-only

#ifndef __EIS_DBUS_H_INCLUDED__
#define __EIS_DBUS_H_INCLUDED__

#if KDE_ENABLED

#include <stdbool.h>

struct sd_bus;

// Holds the D-Bus connection that owns an EIS session plus its socket fd. The
// D-Bus connection MUST stay alive for the whole lifetime of the EIS/libei
// usage: KWin tears the EIS session down when this connection drops.
struct eis_connection {
    struct sd_bus *bus;
    int            fd;
};

// Calls org.kde.KWin.EIS.RemoteDesktop.connectToEIS on the session bus. On
// success fills *conn (conn->fd >= 0, conn->bus kept open) and returns true.
// On failure returns false and leaves nothing to clean up.
bool eis_dbus_connect(struct eis_connection *conn);

// Drops the D-Bus connection. Call only after you are done with the EIS fd
// (i.e. after libei has been torn down). Does not touch conn->fd.
void eis_dbus_disconnect(struct eis_connection *conn);

#endif

#endif
