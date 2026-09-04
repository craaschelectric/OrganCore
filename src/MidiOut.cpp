// MidiOut.cpp
// The single USB-MIDI transmit path. Owns the send AND (compile-time) the
// MIDI-TX logging that used to live in Debug.h's send wrappers.
//
// Channels arrive 0-based, as MIDI numbers them. The serial mirror writes that
// straight into the status byte's low nibble; usbMIDI gets channel + 1 because
// its API counts from 1. Those three +1s are the library's whole exposure to
// that convention.

#include "MidiOut.h"
#include "Debug.h"   // DEBUG_ENABLED

static HardwareSerial* mirror = nullptr;

void midiOutAttach(HardwareSerial& mirrorPort) {
    mirror = &mirrorPort;   // sketch already called begin() on the shared port
}

void midiSendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
#if DEBUG_ENABLED
    Serial.print("MIDI TX: NoteOn  ch="); Serial.print(channel);
    Serial.print("(usb "); Serial.print(channel + 1); Serial.print(")");
    Serial.print(" note="); Serial.print(note);
    Serial.print(" vel=");  Serial.println(velocity);
#endif
    usbMIDI.sendNoteOn(note, velocity, channel + 1);
    if (mirror) {
        mirror->write((uint8_t)(0x90 | (channel & 0x0F)));
        mirror->write(note);
        mirror->write(velocity);
    }
}

void midiSendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
#if DEBUG_ENABLED
    Serial.print("MIDI TX: NoteOff ch="); Serial.print(channel);
    Serial.print("(usb "); Serial.print(channel + 1); Serial.print(")");
    Serial.print(" note="); Serial.println(note);
#endif
    usbMIDI.sendNoteOff(note, velocity, channel + 1);
    if (mirror) {
        mirror->write((uint8_t)(0x80 | (channel & 0x0F)));
        mirror->write(note);
        mirror->write(velocity);
    }
}

void midiSendControlChange(uint8_t cc, uint8_t value, uint8_t channel) {
#if DEBUG_ENABLED
    Serial.print("MIDI TX: CC      ch="); Serial.print(channel);
    Serial.print("(usb "); Serial.print(channel + 1); Serial.print(")");
    Serial.print(" cc="); Serial.print(cc);
    Serial.print(" val="); Serial.println(value);
#endif
    usbMIDI.sendControlChange(cc, value, channel + 1);
    if (mirror) {
        mirror->write((uint8_t)(0xB0 | (channel & 0x0F)));
        mirror->write(cc);
        mirror->write(value);
    }
}

void midiOutSysEx(uint16_t length, const uint8_t* data) {
#if DEBUG_ENABLED
    Serial.print("MIDI TX: SysEx  len="); Serial.println(length);
#endif
    usbMIDI.sendSysEx(length, data, true);   // data already framed F0..F7
    if (mirror) {
        for (uint16_t i = 0; i < length; i++) mirror->write(data[i]);
    }
}
