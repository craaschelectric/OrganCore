// KeyboardHandler.h
// Detects key changes in inputBuffer vs inputBufferPrev, sends MIDI notes.
// Only processes keyboard ranges — pistons are handled by PistonHandler.

#ifndef KEYBOARD_HANDLER_H
#define KEYBOARD_HANDLER_H

#include "OrganCore.h"
#include "ScanChain.h"

void processKeyboards();

#endif // KEYBOARD_HANDLER_H
