// MidiOut.h
// The single USB-MIDI transmit choke point, and the only place in the library
// that knows usbMIDI counts channels from 1. Every outgoing message goes through
// these functions, which send it to usbMIDI (-> the sample engine, GrandOrgue or
// Hauptwerk) AND mirror the raw bytes to the pipe-controller serial link so the
// downstream driver sees an exact duplicate of the USB stream.
//
// On this console the mirror is Serial8, which the BTLE bridge carries to the
// external pipe controller. Serial8 is shared with the temperature input
// (TempSensor reads its Rx), so the sketch begins Serial8 once and passes the
// same reference here and to tempSensorAttach(); midiOutAttach() only stores the
// pointer, it does not begin the port.
//
// The library never names Serial8. If no mirror port is attached (mirror ==
// nullptr) these behave exactly like the old direct usbMIDI calls, so a console
// with no pipe link works unchanged.
//
// CHANNEL NUMBERING: 0-based, 0..15, everywhere in this library and in every
// ConfigData. That is what MIDI itself uses -- the channel is the low nibble of
// the status byte -- and it is what the serial mirror writes. Teensy's usbMIDI
// API is the one thing that wants 1..16, so the +1 happens inside these three
// functions, immediately before the usbMIDI call, and nowhere else.
//
// The three channel-carrying functions were renamed at 1.8.0 (midiOut* ->
// midiSend*) precisely so that a call site still passing a 1-based channel
// fails to COMPILE. A silent off-by-one channel is a miserable thing to chase.
//
// Argument order still matches usbMIDI: (note, velocity, channel) and
// (cc, value, channel).

#ifndef MIDIOUT_H
#define MIDIOUT_H

#include <Arduino.h>

// Attach the pipe-mirror serial port (already begun by the sketch). Pass the
// same HardwareSerial the sketch begins for the shared Serial8 link. Passing a
// port here enables the mirror; not calling it leaves the mirror off.
void midiOutAttach(HardwareSerial& mirrorPort);

void midiSendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);        // channel 0..15
void midiSendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);      // channel 0..15
void midiSendControlChange(uint8_t cc, uint8_t value, uint8_t channel);     // channel 0..15

// data must be a complete, framed message beginning with 0xF0 and ending 0xF7.
void midiOutSysEx(uint16_t length, const uint8_t* data);

#endif // MIDIOUT_H
