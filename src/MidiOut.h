// MidiOut.h
// The single USB-MIDI transmit choke point. Every outgoing message goes through
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
// Argument order matches usbMIDI:
// (note, velocity, channel) / (cc, value, channel), channel 1-based.

#ifndef MIDIOUT_H
#define MIDIOUT_H

#include <Arduino.h>

// Attach the pipe-mirror serial port (already begun by the sketch). Pass the
// same HardwareSerial the sketch begins for the shared Serial8 link. Passing a
// port here enables the mirror; not calling it leaves the mirror off.
void midiOutAttach(HardwareSerial& mirrorPort);

void midiOutNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);
void midiOutNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);
void midiOutControlChange(uint8_t cc, uint8_t value, uint8_t channel);

// data must be a complete, framed message beginning with 0xF0 and ending 0xF7.
void midiOutSysEx(uint16_t length, const uint8_t* data);

#endif // MIDIOUT_H
