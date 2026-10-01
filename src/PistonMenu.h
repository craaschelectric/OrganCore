// PistonMenu.h  -  the config menu and its screens, driven by console pistons.
//
// Used when the instrument sets TOUCH_ENABLED = false (1.12.0). Grown out of
// the Opus 57 DisplayLocal module. Nothing here ever reads touch.
//
// Opening: hold SET and press GENERAL CANCEL. That press does not cancel. The
// combination back-end sets displayMenuRequested and displayUpdate() calls
// pistonMenuRun(), so the sketch's loop() needs nothing new.
//
// Controls, the same on every screen:
//   SET             act: choose the highlighted item, capture a value, store
//                   a crescendo level, reset the tuning trim
//   GENERAL CANCEL  back: leave the screen (without saving, where that matters)
//   MEM+ / MEM-     move the highlight in a list (fall back to NEXT / PREV)
//   NEXT / PREV     step a value: crescendo level, tuning trim (fall back to
//                   MEM+ / MEM-)
// Every console has SET and GENERAL CANCEL. A console with only one of a pair
// still works, because every list wraps.
//
// Menu items, each shown only when it applies:
//   Calibrate Shoes  every analog shoe, including the crescendo shoe
//   Crescendo        when a crescendo shoe and its storage are present
//   Tuning           when ORGAN_TUNING_PRESENT
//
// All screens are blocking, like the touch config screens. While one is open
// the keyboards do not play. Each screen keeps scanning, applies the duplicate-
// button remaps, and pumps usbMIDI, the power switch, the temperature sensor
// and the pitch loop. The crescendo screen also runs the stop handlers, so a
// registration can be built from drawknobs or tabs on the host screen while
// programming, and SAM stops that move on a level recall finish their pulses.

#ifndef ORGANCORE_PISTONMENU_H
#define ORGANCORE_PISTONMENU_H

#include "OrganCore.h"

// Once, from displayInit(), when TOUCH_ENABLED is false. Looks up the control
// pistons by type and reports any missing ones on the serial line.
void pistonMenuInit();

// Blocking. Runs the menu until GENERAL CANCEL is pressed on it. On return the
// input state is current (no stale edges) and setHeld matches the SET piston.
void pistonMenuRun();

#endif // ORGANCORE_PISTONMENU_H
