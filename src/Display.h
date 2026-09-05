// Display.h
// The single shared TeensyUserInterface instance. Defined in DisplayManager.cpp
// and used by both the DisplayManager (run/config screens) and the separate
// screen modules (ExpressionCalScreen), so there is exactly one 'ui' owning the
// ILI9341 + XPT2046 SPI hardware.
//
// ALWAYS poll touch through uiGetTouchEvents(), never ui.getTouchEvents()
// directly: the wrapper applies this console's TOUCH_INVERT_X / TOUCH_INVERT_Y
// after TUI has converted the raw reading to screen pixels. Calling TUI's
// version straight bypasses the inversion, and on a mirrored panel every tap
// then lands on the wrong side with nothing to show why.

#ifndef ORGANCORE_DISPLAY_H
#define ORGANCORE_DISPLAY_H

#include <TeensyUserInterface.h>
#include <font_Arial.h>
#include <font_ArialBold.h>

extern TeensyUserInterface ui;

// Poll for a touch event, then mirror the coordinates per the instrument's
// TOUCH_INVERT_X / TOUCH_INVERT_Y. Leaves ui.touchEventType alone and rewrites
// ui.touchEventX / ui.touchEventY in place, so every checkForButtonClicked()
// and checkForTouchEventInRect() downstream sees corrected coordinates.
void uiGetTouchEvents();

#endif // ORGANCORE_DISPLAY_H
