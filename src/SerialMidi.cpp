// SerialMidi.cpp
// Serial MIDI input processing.
//
// Messages on VIRTUAL_CHAIN_MIDI_CH are intercepted and mapped into
// inputBuffer[VIRTUAL_CHAIN_INDEX] as virtual scan chain bits.
// All other messages are forwarded to USB MIDI.

#include "SerialMidi.h"
#include "MidiOut.h"   // forwarded serial-MIDI also mirrors to the pipe controller

static uint8_t midiState = 0;
static uint8_t midiData[2];
static uint8_t midiDataIdx = 0;
static uint8_t midiDataNeeded = 0;
static HardwareSerial* smPort = nullptr;
void serialMidiAttach(HardwareSerial& port) { smPort = &port; }   // sketch owns begin()

void serialMidiProcess() {
    if (!smPort) return;
    while (smPort->available()) {
        uint8_t byte = smPort->read();

        if (byte & 0x80) {
            if (byte >= 0xF8) continue;          // Real-time: ignore
            if (byte >= 0xF0) {                   // System common: reset
                midiState = 0;
                midiDataIdx = 0;
                continue;
            }
            midiState = byte;
            midiDataIdx = 0;
            uint8_t cmd = byte & 0xF0;
            midiDataNeeded = (cmd == 0xC0 || cmd == 0xD0) ? 1 : 2;
        } else {
            if (midiState == 0) continue;
            midiData[midiDataIdx++] = byte;

            if (midiDataIdx >= midiDataNeeded) {
                uint8_t cmd = midiState & 0xF0;
                uint8_t ch  = midiState & 0x0F;  // 0-indexed

                // Check if this is a virtual chain message
                if (ch == VIRTUAL_CHAIN_MIDI_CH && (cmd == 0x90 || cmd == 0x80)) {
                    uint8_t note = midiData[0];

                    if (note >= VIRTUAL_CHAIN_BASE_NOTE && note <= VIRTUAL_CHAIN_MAX_NOTE) {
                        uint16_t bitIdx = note - VIRTUAL_CHAIN_BASE_NOTE;
                        uint8_t word = bitIdx / 16;
                        uint8_t bit  = bitIdx % 16;

                        bool noteOn = (cmd == 0x90 && midiData[1] > 0);

                        if (noteOn) {
                            inputBuffer[VIRTUAL_CHAIN_INDEX][word] |= (1 << bit);
                        } else {
                            inputBuffer[VIRTUAL_CHAIN_INDEX][word] &= ~(1 << bit);
                        }

                        Serial.print("DBG: VChain ");
                        Serial.print(noteOn ? "ON " : "OFF ");
                        Serial.print("note=");
                        Serial.print(note);
                        Serial.print(" -> bit ");
                        Serial.println(bitIdx);
                    }
                    // Do NOT forward virtual chain messages to USB
                    midiDataIdx = 0;
                    continue;
                }

                // Not a virtual chain message — forward to USB MIDI
                uint8_t usbCh = ch + 1;  // usbMIDI uses 1-indexed channels

                switch (cmd) {
                    case 0x90:  // Note On
                        if (midiData[1] == 0) {
                            midiOutNoteOff(midiData[0], 0, usbCh);
                            Serial.print("DBG: SerMIDI NoteOff ch=");
                            Serial.print(usbCh); Serial.print(" note=");
                            Serial.println(midiData[0]);
                        } else {
                            midiOutNoteOn(midiData[0], midiData[1], usbCh);
                            Serial.print("DBG: SerMIDI NoteOn ch=");
                            Serial.print(usbCh); Serial.print(" note=");
                            Serial.print(midiData[0]); Serial.print(" vel=");
                            Serial.println(midiData[1]);
                        }
                        break;
                    case 0x80:  // Note Off
                        midiOutNoteOff(midiData[0], midiData[1], usbCh);
                        Serial.print("DBG: SerMIDI NoteOff ch=");
                        Serial.print(usbCh); Serial.print(" note=");
                        Serial.println(midiData[0]);
                        break;
                    case 0xB0:  // CC
                        midiOutControlChange(midiData[0], midiData[1], usbCh);
                        Serial.print("DBG: SerMIDI CC ch=");
                        Serial.print(usbCh); Serial.print(" cc=");
                        Serial.print(midiData[0]); Serial.print(" val=");
                        Serial.println(midiData[1]);
                        break;
                }
                midiDataIdx = 0;  // Reset for running status
            }
        }
    }
}
