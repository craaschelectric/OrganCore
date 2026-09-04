// KeyboardHandler.cpp
// Keyboard input processing.
// Detects key changes in inputBuffer vs inputBufferPrev, sends MIDI notes.
// Pistons, SHIFT, and SET are handled by PistonHandler — not here.

#include "KeyboardHandler.h"
#include "MidiOut.h"
#include "Debug.h"

void processKeyboards() {
    for (uint8_t k = 0; k < NUM_KEYBOARDS; k++) {
        uint8_t chain = kbdChain[k];

        for (uint16_t bitIdx = kbdStartBit[k]; bitIdx <= kbdEndBit[k]; bitIdx++) {
            uint8_t word = bitIdx / 16;
            uint8_t bit  = bitIdx % 16;

            bool current  = (inputBuffer[chain][word] >> bit) & 1;
            bool previous = (inputBufferPrev[chain][word] >> bit) & 1;

            if (current == previous) continue;

            uint8_t midiNote = kbdLowNote[k] + (bitIdx - kbdStartBit[k]);
            uint8_t ch = kbdMidiChannel[k];

            if (current && !previous) {
                midiSendNoteOn(midiNote, kbdVelocity[k], ch);
            } else {
                midiSendNoteOff(midiNote, 0, ch);
            }
        }
    }
}
