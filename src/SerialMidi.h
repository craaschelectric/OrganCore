// SerialMidi.h
// Serial MIDI input processing.
// Messages on VIRTUAL_CHAIN_MIDI_CH are injected into the virtual scan chain.
// All other noteOn, noteOff, CC are forwarded to USB MIDI.

#ifndef SERIAL_MIDI_H
#define SERIAL_MIDI_H

#include "OrganCore.h"
#include "ScanChain.h"

void serialMidiAttach(HardwareSerial& port);   // port must be begun by the sketch
void serialMidiProcess();  // Call from main loop; no-op until attached

#endif // SERIAL_MIDI_H
