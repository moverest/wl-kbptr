// SPDX-License-Identifier: GPL-3.0-only

#ifndef __EIS_DBUS_H_INCLUDED__
#define __EIS_DBUS_H_INCLUDED__

#if KDE_ENABLED

// Calls org.kde.KWin.EIS.RemoteDesktop.connectToEIS on the session bus and
// returns a duplicated, owned socket fd on success, or -1 on failure.
int eis_dbus_connect(void);

#endif

#endif
