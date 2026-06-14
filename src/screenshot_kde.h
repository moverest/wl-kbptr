// SPDX-License-Identifier: GPL-3.0-only

#ifndef __SCREENSHOT_KDE_H_INCLUDED__
#define __SCREENSHOT_KDE_H_INCLUDED__

#if OPENCV_ENABLED && KDE_ENABLED

#include "screencopy.h"

// Captures the given region (in output-local coordinates of the current output)
// via KWin's org.kde.KWin.ScreenShot2.CaptureArea and returns a malloc'd
// scrcpy_buffer (wl_buffer == NULL), or NULL on failure.
struct scrcpy_buffer *
query_screenshot_kde(struct state *state, struct rect region);

#endif

#endif
