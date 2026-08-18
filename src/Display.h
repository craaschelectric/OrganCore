// Display.h
// The single shared TeensyUserInterface instance. Defined in DisplayManager.cpp
// and used by both the DisplayManager (run/config screens) and the separate
// screen modules (ExpressionCalScreen), so there is exactly one 'ui' owning the
// ILI9341 + XPT2046 SPI hardware.

#ifndef ORGANCORE_DISPLAY_H
#define ORGANCORE_DISPLAY_H

#include <TeensyUserInterface.h>
#include <font_Arial.h>
#include <font_ArialBold.h>

extern TeensyUserInterface ui;

#endif // ORGANCORE_DISPLAY_H
