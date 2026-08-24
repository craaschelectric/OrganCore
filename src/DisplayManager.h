// DisplayManager.h
// Touchscreen UI for the Op62-MVUMC console: a run screen (memory control band
// with -32/-1/level/+1/+32, last-general name, and a 4x2 grid of the first 8
// screen-stop tabs) and a config screen (small menu that currently offers
// expression calibration and, on tuning builds, tuning/temperature). Built on
// TeensyUserInterface over the ILI9341 + XPT2046.
//
// The same six-function interface as the original stub, so the .ino is
// unchanged:
//   displayInit()             once in setup(), after configLoad()
//   displayUpdate()           every loop: reactive repaint of the run screen
//   displayProcessTouch()     every loop: handle run-screen touches
//   displayPowerShouldBeOn()  whether the console power supply should be enabled
//   displayScanChainsActive() false while a blocking config screen owns the loop
//
// Screen-stop toggling is virtualized: a tab touch writes the stop's virtual
// chain-3 input bit, and the tested processStopInputs() path sees the edge and
// does the toggle + MIDI. The tab lamp paints from stopCommandedState[], so it
// follows the actual stop state (including after a combination recall).

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "OrganCore.h"

extern uint8_t currentScreen;

// True once displayInit() has run (ui.begin() done). Lets a blocking operation
// that may run before the display is up (e.g. a first-boot flash format) decide
// whether it can safely draw a progress screen.
extern bool displayReady;

void displayInit();
void displayUpdate();
void displayProcessTouch();
bool displayPowerShouldBeOn();
bool displayScanChainsActive();

// Force a full run-screen repaint on the next displayUpdate(). Used after a
// blocking screen (e.g. the startup wait) has overpainted the run screen.
void displayForceRepaint();

#endif // DISPLAY_MANAGER_H
