// PistonHandler.h
// Piston input processing: generals (with sequencer), divisionals,
// Previous/Next, GC, SET, and SHIFT.
//
// All pistons are defined in ConfigData.h with a type, CWB address,
// and MIDI note. Edge detection uses inputBuffer vs inputBufferPrev.
//
// Sequencer: tracks current general position. Next/Previous step through.
// At wrap-around, sends MEM+/MEM- MIDI, blocking delay, then the
// wrapped-to general. Last general fired is displayed on screen.
//
// SHIFT: modifier piston. While held, adds SHIFT_NOTE_OFFSET to the
// next piston's MIDI note. If pressed and released alone, sends its
// own NoteOn/NoteOff.
//
// SET: NoteOn on press, NoteOff on release. Sets setHeld flag for display.

#ifndef PISTON_HANDLER_H
#define PISTON_HANDLER_H

#include "OrganCore.h"
#include "ScanChain.h"

// True while the SET piston is physically held down.
// Read by DisplayManager to show/hide "SET" indicator.
extern bool setHeld;

// Current sequencer position (index into sequencerPistonList), or -1 if no general active.
extern int8_t sequencerPosition;

// Display name of last general fired (empty string = cleared by GC).
extern char lastGeneralName[8];

// True when lastGeneralName has changed and display needs updating.
extern bool generalDisplayDirty;

void pistonInit();
void processPistons();

#endif // PISTON_HANDLER_H
