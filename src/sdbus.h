// SPDX-License-Identifier: GPL-3.0-only

#ifndef __SDBUS_H_INCLUDED__
#define __SDBUS_H_INCLUDED__

// basu ships its sd-bus header under <basu/sd-bus.h>, not <systemd/sd-bus.h>.
// Meson defines USE_BASU_SDBUS when it falls back to basu (see meson.build).
#ifdef USE_BASU_SDBUS
#include <basu/sd-bus.h>
#else
#include <systemd/sd-bus.h>
#endif

#endif
