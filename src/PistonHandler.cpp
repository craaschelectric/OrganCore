// PistonHandler.cpp
// Piston input processing with sequencer.

#include "PistonHandler.h"
#include "MidiOut.h"
#include "Combination.h"
#include "DisplayManager.h"   // currentScreen (SET is repurposed on the crescendo screen)
#include "Debug.h"

#if ORGAN_COMBINATION_MODE == COMBINATION_MODE_HW

// ------------------------------------------------------------
// HW-in-command build. Pistons are Hauptwerk-side (this file). The combination
// state below exists only so a display module can link in either back-end;
// combinationInit() is a no-op because there is no local SD combination here.
// ------------------------------------------------------------
bool        combinationAvailable  = false;
uint16_t    combinationMemoryLevel = 0;
const char* combinationErrorText   = nullptr;
void combinationInit() {}

// ============================================================
// State
// ============================================================

bool setHeld = false;
int8_t sequencerPosition = -1;  // -1 = no general active
char lastGeneralName[8] = "";
bool generalDisplayDirty = false;

// ============================================================
// Next/Previous Debounce
// ============================================================

static uint32_t sequencerDebounceUntil = 0;  // millis() value; ignore NEXT/PREV until then

// ============================================================
// SHIFT State
// ============================================================

static bool shiftHeld = false;
static bool shiftConsumed = false;  // True if another piston was pressed while SHIFT held

// ============================================================
// Helpers
// ============================================================

// Track the actual MIDI note sent on press, so NoteOff matches even if SHIFT released in between.
static uint8_t pistonSentNote[MAX_PISTONS];

// Send a piston MIDI NoteOn with optional SHIFT offset.
// Stores the sent note for later NoteOff.
static void sendPistonOn(uint8_t pistonIndex) {
    uint8_t note = pistonMidiNote[pistonIndex];
    if (note == 0xFF) return;

    if (shiftHeld) {
        note += SHIFT_NOTE_OFFSET;
        shiftConsumed = true;
    }

    pistonSentNote[pistonIndex] = note;
    midiOutNoteOn(note, 127, PISTON_MIDI_CHANNEL + 1);
}

// Send a piston MIDI NoteOff using the note that was sent on press.
static void sendPistonOff(uint8_t pistonIndex) {
    uint8_t note = pistonSentNote[pistonIndex];
    if (note == 0xFF) return;

    midiOutNoteOff(note, 0, PISTON_MIDI_CHANNEL + 1);
    pistonSentNote[pistonIndex] = 0xFF;
}

// Fire a sequencer entry: look up piston index from sequencerPistonList,
// send MIDI, update sequencer position and display.
static void fireSequencerEntry(uint8_t seqIndex) {
    if (seqIndex >= NUM_SEQUENCER_PISTONS) return;

    uint8_t pistonIndex = sequencerPistonList[seqIndex];

    // Sequencer-fired: send NoteOn then NoteOff as a pulse
    // (no physical piston press to release, since NEXT/PREV triggered this)
    sendPistonOn(pistonIndex);
    sendPistonOff(pistonIndex);

    // Update sequencer position
    sequencerPosition = seqIndex;

    // Update display with the general name for this sequencer entry
    memcpy(lastGeneralName, generalName[seqIndex], 7);
    lastGeneralName[7] = '\0';
    generalDisplayDirty = true;

    Serial.print("DBG: Sequencer[");
    Serial.print(seqIndex);
    Serial.print("] piston ");
    Serial.print(pistonIndex);
    Serial.print(" (");
    Serial.print(generalName[seqIndex]);
    Serial.print(") seq=");
    Serial.println(sequencerPosition);
}

// ============================================================
// Sequencer: Next / Previous
// ============================================================

static void handleNext() {
    if (sequencerPosition < 0) {
        // No general active — start at first sequencer entry
        fireSequencerEntry(0);
        return;
    }

    int8_t nextPos = sequencerPosition + 1;

    if (nextPos >= NUM_SEQUENCER_PISTONS) {
        // Wrap around: send MEM+, delay, then fire first entry
        Serial.println("DBG: Sequencer wrap NEXT -> MEM+ then first");
        midiOutNoteOn(MEM_UP_MIDI_NOTE, 127, MEM_UP_MIDI_CHANNEL + 1);
        midiOutNoteOff(MEM_UP_MIDI_NOTE, 0, MEM_UP_MIDI_CHANNEL + 1);
        delay(SEQUENCER_WRAP_DELAY_MS);
        fireSequencerEntry(0);
    } else {
        fireSequencerEntry(nextPos);
    }
}

static void handlePrevious() {
    if (sequencerPosition < 0) {
        // No general active — start at last sequencer entry
        fireSequencerEntry(NUM_SEQUENCER_PISTONS - 1);
        return;
    }

    int8_t prevPos = sequencerPosition - 1;

    if (prevPos < 0) {
        // Wrap around: send MEM-, delay, then fire last entry
        Serial.println("DBG: Sequencer wrap PREV -> MEM- then last");
        midiOutNoteOn(MEM_DOWN_MIDI_NOTE, 127, MEM_DOWN_MIDI_CHANNEL + 1);
        midiOutNoteOff(MEM_DOWN_MIDI_NOTE, 0, MEM_DOWN_MIDI_CHANNEL + 1);
        delay(SEQUENCER_WRAP_DELAY_MS);
        fireSequencerEntry(NUM_SEQUENCER_PISTONS - 1);
    } else {
        fireSequencerEntry(prevPos);
    }
}

// ============================================================
// Initialization
// ============================================================

void pistonInit() {
    setHeld = false;
    shiftHeld = false;
    shiftConsumed = false;
    sequencerPosition = -1;
    lastGeneralName[0] = '\0';
    generalDisplayDirty = false;
    memset(pistonSentNote, 0xFF, sizeof(pistonSentNote));
}

// ============================================================
// Process Pistons (call from main loop)
// ============================================================

void processPistons() {
    // --- Pass 1: Update SHIFT state first ---
    for (uint8_t i = 0; i < NUM_PISTONS; i++) {
        if (pistonType[i] != PISTON_TYPE_SHIFT) continue;
        if (!inputChanged(pistonAddr[i])) continue;

        bool pressed = readInput(pistonAddr[i]);

        if (pressed) {
            shiftHeld = true;
            shiftConsumed = false;
            Serial.println("DBG: SHIFT pressed");
        } else {
            // SHIFT released
            if (!shiftConsumed) {
                // Pressed alone — send own NoteOn/NoteOff
                uint8_t note = pistonMidiNote[i];
                if (note != 0xFF) {
                    midiOutNoteOn(note, 127, PISTON_MIDI_CHANNEL + 1);
                    midiOutNoteOff(note, 0, PISTON_MIDI_CHANNEL + 1);
                    Serial.println("DBG: SHIFT released alone, sent own MIDI");
                }
            }
            shiftHeld = false;
            shiftConsumed = false;
            Serial.println("DBG: SHIFT released");
        }
    }

    // --- Pass 2: Update SET state ---
    // On the crescendo programming screen the SET button is repurposed to store
    // the displayed level (crescendoProgrammingPoll handles the edge), so we do
    // NOT arm combination capture here -- keep setHeld clear so generals/
    // divisionals recall normally as a starting registration.
    if (currentScreen == SCREEN_CRESCENDO) {
        setHeld = false;
    } else {
        for (uint8_t i = 0; i < NUM_PISTONS; i++) {
            if (pistonType[i] != PISTON_TYPE_SET) continue;
            if (!inputChanged(pistonAddr[i])) continue;

            bool pressed = readInput(pistonAddr[i]);

            if (pressed) {
                setHeld = true;
                sendPistonOn(i);
                Serial.println("DBG: SET pressed");
            } else {
                setHeld = false;
                sendPistonOff(i);
                Serial.println("DBG: SET released");
            }
        }
    }

    // --- Pass 3: Process all other pistons ---
    for (uint8_t i = 0; i < NUM_PISTONS; i++) {
        // Skip SHIFT and SET — already handled above
        if (pistonType[i] == PISTON_TYPE_SHIFT) continue;
        if (pistonType[i] == PISTON_TYPE_SET) continue;

        if (!inputChanged(pistonAddr[i])) continue;

        bool pressed = readInput(pistonAddr[i]);

        if (pressed) {
            // --- Press ---
            if (shiftHeld) shiftConsumed = true;

            switch (pistonType[i]) {

                case PISTON_TYPE_GENERAL: {
                    // Send NoteOn and update sequencer position
                    sendPistonOn(i);
                    int8_t seqPos = -1;
                    for (uint8_t s = 0; s < NUM_SEQUENCER_PISTONS; s++) {
                        if (sequencerPistonList[s] == i) {
                            seqPos = s;
                            break;
                        }
                    }
                    if (seqPos >= 0) {
                        sequencerPosition = seqPos;
                        memcpy(lastGeneralName, generalName[seqPos], 7);
                        lastGeneralName[7] = '\0';
                        generalDisplayDirty = true;
                    }
                    Serial.print("DBG: General piston ");
                    Serial.print(i);
                    Serial.print(" seq=");
                    Serial.println(seqPos);
                    break;
                }

                case PISTON_TYPE_DIVISIONAL:
                    sendPistonOn(i);
                    Serial.print("DBG: Divisional ");
                    Serial.println(i);
                    break;

                case PISTON_TYPE_NEXT:
                    if (millis() >= sequencerDebounceUntil) {
                        handleNext();
                        sequencerDebounceUntil = millis() + SEQUENCER_DEBOUNCE_MS;
                        Serial.println("DBG: NEXT pressed");
                    }
                    break;

                case PISTON_TYPE_PREV:
                    if (millis() >= sequencerDebounceUntil) {
                        handlePrevious();
                        sequencerDebounceUntil = millis() + SEQUENCER_DEBOUNCE_MS;
                        Serial.println("DBG: PREV pressed");
                    }
                    break;

                case PISTON_TYPE_GC:
                    sendPistonOn(i);
                    lastGeneralName[0] = '\0';
                    sequencerPosition = -1;
                    generalDisplayDirty = true;
                    Serial.println("DBG: GC pressed");
                    break;
            }
        } else {
            // --- Release: send NoteOff for pistons that sent NoteOn ---
            switch (pistonType[i]) {
                case PISTON_TYPE_GENERAL:
                case PISTON_TYPE_DIVISIONAL:
                case PISTON_TYPE_GC:
                    sendPistonOff(i);
                    break;

                // PREV/NEXT have no MIDI, nothing to release
                default:
                    break;
            }
        }
    }
}

#endif // ORGAN_COMBINATION_MODE == COMBINATION_MODE_HW
