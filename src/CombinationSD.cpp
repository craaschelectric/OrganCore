// CombinationSD.cpp  -  local combination back-end (SD card or on-board QSPI flash).
//
// The medium is mounted by OrganStorage (organFS); this file just uses it.
//
// Compiled only when ORGAN_COMBINATION_MODE == COMBINATION_MODE_SD. This file
// provides the SD combination API AND the piston processing (pistonInit /
// processPistons), because in SD mode the Hauptwerk-side PistonHandler.cpp is
// compiled out. Recall/cancel drive stops through StopHandler::stopSetState,
// so the tested coil pulse/retry path and the sense->MIDI mirroring to the PC
// engine are reused unchanged.
//
// Capture gesture:  SET held + a general/divisional piston.
// Recall gesture:   the piston alone.
// Scope:            general  = every stop flagged STOP_IN_GENERALS (console-wide);
//                   divisional = STOP_IN_DIVISIONALS stops whose stopDivision
//                   matches the piston's pistonDivision; others untouched.
// Recall is absolute: in-scope stops are set to the stored on/off state.
//
// SD file layout is described in CombinationConfig.h. Capture writes one 64-byte
// record; recall reads one; a (level, piston) pair maps to a unique byte offset.

#include "CombinationConfig.h"

#if ORGAN_COMBINATION_MODE == COMBINATION_MODE_SD

#include "Combination.h"
#include "PistonHandler.h"
#include "StopHandler.h"
#include "ScanChain.h"
#include "PersistentConfig.h"
#include "Debug.h"
#ifdef ORGANCORE_HAS_REMAP_STORE
#include "RemapStore.h"       // builder piston-assign store; init after the card is mounted
#endif
#include "Display.h"          // shared 'ui' for the format progress screen
#include "DisplayManager.h"   // displayReady, displayForceRepaint
#include "OrganStorage.h"     // organFS / organStorageMount() — the shared mount
#include <stdio.h>

// Per-combination files. Each memory level / piston pair is its own tiny 64-byte
// file, named CB_<level4>_<pistonAddr4hex>.DAT, created on demand at capture.
// This replaces the original single 8 MB COMB.DAT that stored every level x
// piston record at a computed offset: on LittleFS/QSPI a seek-and-write into
// that huge file walked the whole block chain on flush, so one capture took
// ~45 s. A write to a 64-byte file has no chain to walk -- it is instant on
// flash and on SD. There is no format step and no boot pre-zeroing: a piston
// never set simply has no file, and recall of a missing file leaves the console
// blank, which is the correct "nothing stored" behaviour.
static const char* LEGACY_COMBO_FILENAME = "COMB.DAT";   // removed at mount if present

// ============================================================
// State (defines the externs from Combination.h and PistonHandler.h)
// ============================================================

bool        combinationAvailable   = false;
uint16_t    combinationMemoryLevel  = 0;
const char* combinationErrorText    = nullptr;

// Piston display state (declared in PistonHandler.h; the HW file that normally
// defines these is compiled out in SD mode).
bool    setHeld = false;
int8_t  sequencerPosition = -1;
char    lastGeneralName[8] = "";
bool    generalDisplayDirty = false;

static uint32_t sequencerDebounceUntil = 0;
static uint8_t  recordBuf[COMBO_RECORD_SIZE];

// ============================================================
// SD file helpers
// ============================================================

// Build the filename for one (level, piston) combination. Flat namespace, no
// directories -- the FS wrapper only promises exists()/open()/remove(), and a
// flat name works identically on QSPI LittleFS and on SD.
//
// Keyed on the piston's INPUT ADDRESS (pistonAddr[i]), not its array index. The
// address is a fixed hardware fact -- a given physical piston has the same
// address forever -- whereas the index is a position in the piston table that
// shifts whenever a piston is inserted or removed. Keying on the index would
// silently rebind every stored combination past an inserted piston to the wrong
// button. Keying on the address means a captured combination always recalls to
// the same physical piston, no matter how the table is later edited. Address is
// a 12-bit value (chain<<8 | word<<4 | bit), so %04X is exact.
//
// DUAL-INPUT PISTONS (thumb + toe for the same registration) are handled by the
// REMAP table, NOT by two piston-table entries. Remap the toe's input address to
// the thumb's, and give ONLY the thumb a piston-table entry. applyRemaps() moves
// the toe's bit onto the thumb address and clears the toe before any handler
// runs, so pressing either contact fires the thumb piston and this function sees
// the thumb (canonical) address either way -- one file, CB_<level>_<thumbAddr>.
// Do NOT give the toe its own piston entry: it would be a distinct address and a
// distinct file, silently splitting one logical piston into two half-registered
// combinations (thumb captures never recalled by toe, and vice versa).
static void comboFileName(char* out, uint16_t level, uint8_t pistonIndex) {
    snprintf(out, 20, "CB_%04u_%04X.DAT",
             (unsigned)level, (unsigned)pistonAddr[pistonIndex]);
}

static bool recordGetBit(const uint8_t* rec, uint16_t stopIndex) {
    return (rec[stopIndex >> 3] >> (stopIndex & 7)) & 1;
}

static void recordSetBit(uint8_t* rec, uint16_t stopIndex) {
    rec[stopIndex >> 3] |= (uint8_t)(1 << (stopIndex & 7));
}

// ============================================================
// Scope + current-state helpers
// ============================================================

// Is this stop captured/recalled by this piston? (Recomputed at both capture
// and recall from the piston type + division and the stop flags, so membership
// is never stored on the card.)
static bool stopInScope(uint8_t pistonIndex, uint16_t stopIndex) {
    uint8_t flags = stopFlags[stopIndex];
    if (pistonType[pistonIndex] == PISTON_TYPE_GENERAL) {
        return (flags & STOP_IN_GENERALS) != 0;
    }
    if (pistonType[pistonIndex] == PISTON_TYPE_DIVISIONAL) {
        return (flags & STOP_IN_DIVISIONALS) &&
               stopDivision[stopIndex] == pistonDivision[pistonIndex];
    }
    return false;
}

static bool stopIsSAM(uint16_t stopIndex) {
    return ADDR_VALID(stopOnCoilAddr[stopIndex]);
}

// Truth captured by SET: physical sense for a SAM stop; the commanded/virtual
// bit for a screen stop (which has no sense).
static bool stopCurrentTruth(uint16_t stopIndex) {
    return stopIsSAM(stopIndex) ? readInput(stopSenseAddr[stopIndex])
                                : stopCommandedState[stopIndex];
}

// ============================================================
// Capture / Recall / Cancel
// ============================================================

void combinationCapture(uint8_t pistonIndex) {
    if (!combinationAvailable) return;

    memset(recordBuf, 0, COMBO_RECORD_SIZE);
    for (uint16_t s = 0; s < NUM_STOPS; s++) {
        if (!stopInScope(pistonIndex, s)) continue;
        if (stopCurrentTruth(s)) recordSetBit(recordBuf, s);
    }

    char name[20];
    comboFileName(name, combinationMemoryLevel, pistonIndex);
    File f = organFS->open(name, FILE_WRITE_BEGIN);
    if (!f) {
        Serial.print("DBG: Capture open failed "); Serial.println(name);
        return;
    }
    f.seek(0);
    f.write(recordBuf, COMBO_RECORD_SIZE);
    f.close();

    Serial.print("DBG: Capture piston "); Serial.print(pistonIndex);
    Serial.print(" @level "); Serial.println(combinationMemoryLevel);
}

void combinationRecall(uint8_t pistonIndex) {
    if (!combinationAvailable) return;

    char name[20];
    comboFileName(name, combinationMemoryLevel, pistonIndex);

    // A piston that was never set at this level has no file. That is not an
    // error: it means "nothing stored", so recall all stops OFF -- exactly the
    // blank record the old preformatted file returned.
    memset(recordBuf, 0, COMBO_RECORD_SIZE);
    if (organFS->exists(name)) {
        File f = organFS->open(name, FILE_READ);
        if (f) {
            f.read(recordBuf, COMBO_RECORD_SIZE);
            f.close();
        }
    }

    for (uint16_t s = 0; s < NUM_STOPS; s++) {
        if (!stopInScope(pistonIndex, s)) continue;
        stopSetState(s, recordGetBit(recordBuf, s));
    }

    Serial.print("DBG: Recall piston "); Serial.print(pistonIndex);
    Serial.print(" @level "); Serial.println(combinationMemoryLevel);
}

// General Cancel. No SD access, so it works even when the card is unavailable.
void combinationCancel() {
    for (uint16_t s = 0; s < NUM_STOPS; s++) {
        if (stopFlags[s] & STOP_GC_IMMUNE) continue;
        stopSetState(s, false);
    }
    Serial.println("DBG: General Cancel");
}

// ============================================================
// Memory level
// ============================================================

void combinationMemStep(int16_t delta) {
    int32_t v = (int32_t)combinationMemoryLevel + delta;
    v %= COMBO_MEM_LEVELS;
    if (v < 0) v += COMBO_MEM_LEVELS;
    combinationMemoryLevel = (uint16_t)v;
    configSaveCombinationLevel(combinationMemoryLevel);
    generalDisplayDirty = true;
    Serial.print("DBG: Memory level -> "); Serial.println(combinationMemoryLevel);
}

void combinationMemZero() {
    combinationMemoryLevel = 0;
    configSaveCombinationLevel(0);
    generalDisplayDirty = true;
    Serial.println("DBG: Memory level -> 0");
}

// ============================================================
// Init
// ============================================================

void combinationInit() {
    combinationMemoryLevel = configCombinationLevel;   // remembered last level
    combinationAvailable = false;
    combinationErrorText = nullptr;

    if (!organStorageMount()) {
        combinationErrorText = organStorageError;
        Serial.println("DBG: storage mount failed -> combination disabled");
        return;
    }
    // No file to open or format: combinations are per-piston files created on
    // demand at capture. Clear away the old 8 MB monolith if a previous firmware
    // left one -- it is dead weight and its stored combinations do not carry over
    // to the per-file scheme.
    if (organFS->exists(LEGACY_COMBO_FILENAME)) {
        organFS->remove(LEGACY_COMBO_FILENAME);
        Serial.println("DBG: removed legacy COMB.DAT (per-file scheme now)");
    }

    combinationAvailable = true;
    Serial.print("DBG: Combination ready @level "); Serial.println(combinationMemoryLevel);

#ifdef ORGANCORE_HAS_REMAP_STORE
    // Storage is up; if this console offers builder piston assignment, load the
    // table from the same medium the combination file uses. Missing REMAP.DAT
    // (or the feature switched off) leaves the const remap defaults in effect.
    if (PISTON_ASSIGN_ENABLED) {
        remapStoreInit();
    }
#endif
}

// ============================================================
// Sequencer (Next / Previous step through the general list, recall each;
// at the ends, step one memory level and wrap — the local equivalent of the
// Hauptwerk-side "wrap -> MEM+ -> first general" baseline.)
// ============================================================

static void fireSequencerEntry(uint8_t seqIndex) {
    if (seqIndex >= NUM_SEQUENCER_PISTONS) return;
    combinationRecall(sequencerPistonList[seqIndex]);
    sequencerPosition = (int8_t)seqIndex;
    memcpy(lastGeneralName, generalName[seqIndex], 7);
    lastGeneralName[7] = '\0';
    generalDisplayDirty = true;
}

static void handleNext() {
    if (sequencerPosition < 0) { fireSequencerEntry(0); return; }
    int16_t nextPos = sequencerPosition + 1;
    if (nextPos >= (int16_t)NUM_SEQUENCER_PISTONS) {
        combinationMemStep(+1);
        fireSequencerEntry(0);
    } else {
        fireSequencerEntry((uint8_t)nextPos);
    }
}

static void handlePrevious() {
    if (sequencerPosition < 0) { fireSequencerEntry(NUM_SEQUENCER_PISTONS - 1); return; }
    int16_t prevPos = sequencerPosition - 1;
    if (prevPos < 0) {
        combinationMemStep(-1);
        fireSequencerEntry(NUM_SEQUENCER_PISTONS - 1);
    } else {
        fireSequencerEntry((uint8_t)prevPos);
    }
}

// ============================================================
// Piston processing
// ============================================================

void pistonInit() {
    setHeld = false;
    sequencerPosition = -1;
    lastGeneralName[0] = '\0';
    generalDisplayDirty = false;
    sequencerDebounceUntil = 0;
}

void processPistons() {
    // Pass 1: SET is a pure local modifier — track held state, send no MIDI.
    for (uint8_t i = 0; i < NUM_PISTONS; i++) {
        if (pistonType[i] != PISTON_TYPE_SET) continue;
        if (!inputChanged(pistonAddr[i])) continue;
        setHeld = readInput(pistonAddr[i]);
    }

    // Pass 2: act on the press edge of every other piston.
    for (uint8_t i = 0; i < NUM_PISTONS; i++) {
        uint8_t t = pistonType[i];
        if (t == PISTON_TYPE_SET) continue;
        if (!inputChanged(pistonAddr[i])) continue;
        if (!readInput(pistonAddr[i])) continue;      // press only

        switch (t) {
            case PISTON_TYPE_GENERAL:
            case PISTON_TYPE_DIVISIONAL:
                if (setHeld) {
                    combinationCapture(i);
                } else {
                    combinationRecall(i);
                    if (t == PISTON_TYPE_GENERAL) {
                        // Track sequencer position if this general is in the list.
                        for (uint8_t s = 0; s < NUM_SEQUENCER_PISTONS; s++) {
                            if (sequencerPistonList[s] == i) {
                                sequencerPosition = (int8_t)s;
                                memcpy(lastGeneralName, generalName[s], 7);
                                lastGeneralName[7] = '\0';
                                generalDisplayDirty = true;
                                break;
                            }
                        }
                    }
                }
                break;

            case PISTON_TYPE_NEXT:
                if (millis() >= sequencerDebounceUntil) {
                    handleNext();
                    sequencerDebounceUntil = millis() + SEQUENCER_DEBOUNCE_MS;
                }
                break;

            case PISTON_TYPE_PREV:
                if (millis() >= sequencerDebounceUntil) {
                    handlePrevious();
                    sequencerDebounceUntil = millis() + SEQUENCER_DEBOUNCE_MS;
                }
                break;

            case PISTON_TYPE_GC:
                combinationCancel();
                lastGeneralName[0] = '\0';
                sequencerPosition = -1;
                generalDisplayDirty = true;
                break;

            case PISTON_TYPE_MEM_UP:
                combinationMemStep(setHeld ? 20 : 1);
                break;

            case PISTON_TYPE_MEM_DOWN:
                combinationMemStep(setHeld ? -20 : -1);
                break;

            case PISTON_TYPE_MEM_ZERO:
                combinationMemZero();
                break;
        }
    }
}

#endif // ORGAN_COMBINATION_MODE == COMBINATION_MODE_SD
