// StartupScreen.h
// Blocking "Starting Up" splash shown at the end of setup(). It waits solely
// for the startup-handshake NoteOn (STARTUP_WAIT_MIDI_CHANNEL/NOTE) to arrive
// over USB-MIDI, displaying a live seconds counter until then. No timeout, no
// touch-to-skip. The sketch's usbMIDI NoteOn handler sets startupNoteSeen when
// the matching note arrives; this screen polls it.

#ifndef STARTUP_SCREEN_H
#define STARTUP_SCREEN_H

#include <Arduino.h>

// Set true by the sketch's NoteOn handler on the handshake note; the screen
// spins until it becomes true. Cleared at the top of startupWaitScreenRun().
extern bool startupNoteSeen;

void startupWaitScreenRun();

#endif // STARTUP_SCREEN_H
