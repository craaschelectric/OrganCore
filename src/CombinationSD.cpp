// CombinationSD.cpp  -  local combination back-end (SD card or on-board QSPI flash).
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
#include "RemapStore.h"       // builder piston-assign store; init after SD is mounted (SD-card media only)
#endif
#include "Display.h"          // shared 'ui' for the format progress screen
#include "DisplayManager.h"   // displayReady, displayForceRepaint
#include <stdio.h>
#include <SD.h>

#if ORGAN_COMBINATION_MEDIA == COMBINATION_MEDIA_SPIFLASH
  #include <LittleFS.h>
  // On-board QSPI flash on the Teensy 4.1 back-side pads.
  static LittleFS_QSPIFlash comboFlash;
  static FS& comboFS = comboFlash;
#else
  static FS& comboFS = SD;
#endif
// SD (SDClass) and LittleFS_QSPIFlash both derive from FS on the Teensy core, so
// they share one File type and the exists()/open()/remove() API — the only
// media-specific call is begin() in combinationInit().

// Chip select for the combination card. Teensy 4.1's built-in socket by default;
// override with -DCOMBINATION_SD_CS=<pin> for an external SPI card.
#ifndef COMBINATION_SD_CS
#define COMBINATION_SD_CS BUILTIN_SDCARD
#endif

static const char* COMBO_FILENAME = "COMB.DAT";

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

static File     comboFile;
static uint32_t sequencerDebounceUntil = 0;
static uint8_t  recordBuf[COMBO_RECORD_SIZE];

// ============================================================
// SD file helpers
// ============================================================

static uint32_t comboFileSize() {
    return (uint32_t)COMBO_HEADER_SIZE
         + (uint32_t)COMBO_MEM_LEVELS * COMBO_PISTON_CAP * COMBO_RECORD_SIZE;
}

static uint32_t recordOffset(uint16_t level, uint8_t pistonIndex) {
    return (uint32_t)COMBO_HEADER_SIZE
         + ((uint32_t)level * COMBO_PISTON_CAP + pistonIndex) * COMBO_RECORD_SIZE;
}

// Read the 16-byte header and confirm magic, version, and the three caps match
// this build's format. Any mismatch means blank the card.
static bool validateHeader() {
    uint8_t h[COMBO_HEADER_SIZE];
    comboFile.seek(0);
    if (comboFile.read(h, COMBO_HEADER_SIZE) != (int)COMBO_HEADER_SIZE) return false;

    if (h[0] != COMBO_MAGIC_0 || h[1] != COMBO_MAGIC_1 ||
        h[2] != COMBO_MAGIC_2 || h[3] != COMBO_MAGIC_3) return false;
    if (h[4] != COMBO_FORMAT_VERSION) return false;

    uint16_t stopCap   = (uint16_t)h[6]  | ((uint16_t)h[7]  << 8);
    uint16_t pistonCap = (uint16_t)h[8]  | ((uint16_t)h[9]  << 8);
    uint16_t levels    = (uint16_t)h[10] | ((uint16_t)h[11] << 8);
    if (stopCap != COMBO_STOP_CAP)     return false;
    if (pistonCap != COMBO_PISTON_CAP) return false;
    if (levels != COMBO_MEM_LEVELS)    return false;

    return true;
}

// Draw a "formatting" screen with a progress bar while the file is blanked.
// The blank is a long, blocking operation on QSPI NOR (~1-3 min for the full
// 8 MB, since each 4 KB sector must be erased), so this gives feedback instead
// of a dead screen. Safe to call before the display is up: it no-ops until
// displayInit() has run (displayReady). Draw the frame once (first=true), then
// grow the fill on each whole-percent change.
static void formatProgressDraw(uint8_t percent, bool first) {
    if (!displayReady) return;   // display not initialised yet -> format silently

    const int16_t barW = 240, barH = 22;
    const int16_t barX = ui.displaySpaceCenterX - barW / 2;
    const int16_t barY = ui.displaySpaceCenterY - barH / 2 + 6;

    if (first) {
        ui.drawTitleBar("Preparing Memory");
        ui.clearDisplaySpace();
        ui.lcdSetFont(Arial_10_Bold);
        ui.lcdSetFontColor(LCD_WHITE);
        ui.lcdSetCursorXY(ui.displaySpaceCenterX, barY - 26);
        ui.lcdPrintCentered("Formatting combination memory");
        ui.lcdDrawRectangle(barX, barY, barW, barH, LCD_WHITE);   // bar outline
    }

    int16_t fillW = (int16_t)(((int32_t)(barW - 4) * percent) / 100);
    ui.lcdDrawFilledRectangle(barX + 2, barY + 2, fillW, barH - 4, LCD_WHITE);

    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)percent);
    ui.lcdDrawFilledRectangle(barX, barY + barH + 4, barW, 16, LCD_BLACK);   // clear old %
    ui.lcdSetFontColor(LCD_WHITE);
    ui.lcdSetCursorXY(ui.displaySpaceCenterX, barY + barH + 6);
    ui.lcdPrintCentered(buf);
}

// Create (or overwrite) the file: write the header, then zero-fill every record.
// An all-zero record is a blank piston (all stops off), so no per-record valid
// flag is needed.
static bool formatComboFile() {
    comboFile.close();
    comboFS.remove(COMBO_FILENAME);
    comboFile = comboFS.open(COMBO_FILENAME, FILE_WRITE);
    if (!comboFile) return false;

    uint8_t h[COMBO_HEADER_SIZE];
    memset(h, 0, sizeof(h));
    h[0] = COMBO_MAGIC_0; h[1] = COMBO_MAGIC_1;
    h[2] = COMBO_MAGIC_2; h[3] = COMBO_MAGIC_3;
    h[4] = COMBO_FORMAT_VERSION;
    h[6]  = COMBO_STOP_CAP   & 0xFF; h[7]  = (COMBO_STOP_CAP   >> 8) & 0xFF;
    h[8]  = COMBO_PISTON_CAP & 0xFF; h[9]  = (COMBO_PISTON_CAP >> 8) & 0xFF;
    h[10] = COMBO_MEM_LEVELS & 0xFF; h[11] = (COMBO_MEM_LEVELS >> 8) & 0xFF;

    comboFile.seek(0);
    if (comboFile.write(h, COMBO_HEADER_SIZE) != (size_t)COMBO_HEADER_SIZE) return false;

    uint8_t zeros[512];
    memset(zeros, 0, sizeof(zeros));
    const uint32_t total = (uint32_t)COMBO_MEM_LEVELS * COMBO_PISTON_CAP * COMBO_RECORD_SIZE;
    uint32_t remaining = total;
    uint8_t  lastPct = 255;

    formatProgressDraw(0, true);
    while (remaining > 0) {
        uint16_t chunk = remaining >= 512 ? 512 : (uint16_t)remaining;
        if (comboFile.write(zeros, chunk) != (size_t)chunk) return false;
        remaining -= chunk;

        uint8_t pct = (uint8_t)(((uint64_t)(total - remaining) * 100) / total);
        if (pct != lastPct) { lastPct = pct; formatProgressDraw(pct, false); }
    }
    comboFile.flush();

    // The format screen overpainted the run screen; ask for a full repaint so
    // the next displayUpdate() restores it (harmless if the display isn't up).
    if (displayReady) displayForceRepaint();

    Serial.println("DBG: Combination file formatted (blanked)");
    return true;
}

// Open the file for read+write, creating or blanking it as needed.
static bool openOrCreateComboFile() {
    bool needFormat = !comboFS.exists(COMBO_FILENAME);

    comboFile = comboFS.open(COMBO_FILENAME, FILE_WRITE);   // FILE_WRITE = O_RDWR|O_CREAT
    if (!comboFile) return false;

    if (!needFormat) {
        if (comboFile.size() != comboFileSize() || !validateHeader()) {
            needFormat = true;   // wrong size or foreign/old format -> blank
        }
    }
    if (needFormat) return formatComboFile();
    return true;
}

// ============================================================
// Record bit access  (bit i = stop index i; byte i/8, bit i%8, LSB-first)
// ============================================================

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

    comboFile.seek(recordOffset(combinationMemoryLevel, pistonIndex));
    comboFile.write(recordBuf, COMBO_RECORD_SIZE);
    comboFile.flush();

    Serial.print("DBG: Capture piston "); Serial.print(pistonIndex);
    Serial.print(" @level "); Serial.println(combinationMemoryLevel);
}

void combinationRecall(uint8_t pistonIndex) {
    if (!combinationAvailable) return;

    comboFile.seek(recordOffset(combinationMemoryLevel, pistonIndex));
    if (comboFile.read(recordBuf, COMBO_RECORD_SIZE) != (int)COMBO_RECORD_SIZE) {
        Serial.println("DBG: Recall short read, ignored");
        return;
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

#if ORGAN_COMBINATION_MEDIA == COMBINATION_MEDIA_SPIFLASH
    if (!comboFlash.begin()) {
        combinationErrorText = "SPI FLASH MISSING";
        Serial.println("DBG: LittleFS QSPI begin failed -> combination disabled");
        return;
    }
#else
    if (!SD.begin(COMBINATION_SD_CS)) {
        combinationErrorText = "SD CARD MISSING";
        Serial.println("DBG: SD.begin failed -> combination disabled");
        return;
    }
#endif
    if (!openOrCreateComboFile()) {
        combinationErrorText = "SD FILE ERROR";
        Serial.println("DBG: combination file open/create failed -> combination disabled");
        return;
    }

    combinationAvailable = true;
    Serial.print("DBG: Combination ready @level "); Serial.println(combinationMemoryLevel);

#ifdef ORGANCORE_HAS_REMAP_STORE
    // The SD card is now mounted and owned here; load the builder piston-assign
    // table from the same card. Guarded to SD-card media (the store does not yet
    // support SPI-flash). Missing REMAP.DAT => const remap defaults stay in effect.
    remapStoreInit();
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
