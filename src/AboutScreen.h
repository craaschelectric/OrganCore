// AboutScreen.h  -  the About screen's picture (1.14.0).
//
// The last item in both config menus: the OrganCore version linked into this
// firmware, the sketch's firmware version, and the instrument's own description
// (venue, opus). This module only paints. Each menu supplies its own way out,
// because the two menus read different inputs: GENERAL CANCEL on the piston
// menu, a Back button on the touch menu. See PistonMenu.cpp and DisplayManager.cpp.
//
// The strings come from the instrument's ConfigData.cpp (FIRMWARE_VERSION,
// ABOUT_DESCRIPTION, declared in OrganConfig.h); the library version is
// ORGANCORE_VERSION in CoreConfig.h.

#ifndef ORGANCORE_ABOUTSCREEN_H
#define ORGANCORE_ABOUTSCREEN_H

#include "OrganCore.h"

// Draws the title bar and the whole display space. The lowest 40 pixels or so
// are left empty for the caller's hint line or Back button.
void aboutScreenPaint();

#endif // ORGANCORE_ABOUTSCREEN_H
