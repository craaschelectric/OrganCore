// Crescendo.cpp  -  blind crescendo overlay + level programming.
// See Crescendo.h for the model. Levels live in CRESC.DAT, reusing the
// combination record layout (64-byte / 512-stop bitmap, bit i = stop i,
// LSB-first) with its own header magic "OCRC" and 31 records.

#include "Crescendo.h"
#include "CombinationConfig.h"   // COMBO_RECORD_SIZE / COMBO_STOP_CAP / COMBO_HEADER_SIZE
#include "StopHandler.h"         // stopCommandedState, stopSetState, stopSendToEngine, stopEngineSuppressed
#include "ScanChain.h"           // readInput / inputChanged (console SET piston)
#include "ExpressionCalibration.h"
#include "DisplayManager.h"      // currentScreen
#include <SD.h>

// Chip select: Teensy 4.1 built-in socket by default (same card the combination
// file uses); override with -DCRESCENDO_SD_CS=<pin> for an external SPI card.
#ifndef CRESCENDO_SD_CS
#define CRESCENDO_SD_CS BUILTIN_SDCARD
#endif

static const char* CRESC_FILENAME = "CRESC.DAT";

static const char    CRESC_MAGIC_0 = 'O';
static const char    CRESC_MAGIC_1 = 'C';
static const char    CRESC_MAGIC_2 = 'R';
static const char    CRESC_MAGIC_3 = 'C';
static const uint8_t CRESC_FORMAT_VERSION = 1;

// A shoe resting on a bucket boundary must move at least this many ADC counts
// before the level is allowed to change again — kills stop-thrash at the edge.
static const uint16_t CRESC_HYSTERESIS_COUNTS = 6;

// ============================================================
// State
// ============================================================

bool    crescendoAvailable = false;
uint8_t crescendoLevel     = 0;    // live operational level (0 = off)
uint8_t crescendoProgLevel = 1;    // programming-screen displayed level (1..31)

static uint8_t crescSlot   = 0xFF; // expression slot typed EXPR_CRESCENDO (0xFF = none)
static uint16_t setPistonAddr = ADDR_DISABLED;  // console SET piston (for programming)

static File     crescFile;
static uint8_t  crescRecord[COMBO_RECORD_SIZE]; // cached record for the live operational level
static bool     lastSentEffective[MAX_STOPS];   // what the engine was last told (while engaged)
static uint16_t rawAtLastLevelChange = 0;       // hysteresis anchor

// ============================================================
// Record bit access (matches CombinationSD: bit i, byte i/8, bit i%8, LSB-first)
// ============================================================

static bool recordGetBit(const uint8_t* rec, uint16_t stopIndex) {
    return (rec[stopIndex >> 3] >> (stopIndex & 7)) & 1;
}
static void recordSetBit(uint8_t* rec, uint16_t stopIndex) {
    rec[stopIndex >> 3] |= (uint8_t)(1 << (stopIndex & 7));
}

static bool inCrescendoScope(uint16_t s) {
    return (stopFlags[s] & STOP_IN_GENERALS) != 0;
}

// ============================================================
// SD file
// ============================================================

static uint32_t crescFileSize() {
    return (uint32_t)COMBO_HEADER_SIZE + (uint32_t)CRESC_MAX_LEVEL * COMBO_RECORD_SIZE;
}
static uint32_t crescRecordOffset(uint8_t level) {   // level 1..31
    return (uint32_t)COMBO_HEADER_SIZE + (uint32_t)(level - 1) * COMBO_RECORD_SIZE;
}

static bool crescValidateHeader() {
    uint8_t h[COMBO_HEADER_SIZE];
    crescFile.seek(0);
    if (crescFile.read(h, COMBO_HEADER_SIZE) != (int)COMBO_HEADER_SIZE) return false;
    if (h[0] != CRESC_MAGIC_0 || h[1] != CRESC_MAGIC_1 ||
        h[2] != CRESC_MAGIC_2 || h[3] != CRESC_MAGIC_3) return false;
    if (h[4] != CRESC_FORMAT_VERSION) return false;
    uint16_t stopCap = (uint16_t)h[6] | ((uint16_t)h[7] << 8);
    uint16_t levels  = (uint16_t)h[8] | ((uint16_t)h[9] << 8);
    if (stopCap != COMBO_STOP_CAP)  return false;
    if (levels  != CRESC_MAX_LEVEL) return false;
    return true;
}

static bool crescFormatFile() {
    crescFile.close();
    SD.remove(CRESC_FILENAME);
    crescFile = SD.open(CRESC_FILENAME, FILE_WRITE);
    if (!crescFile) return false;

    uint8_t h[COMBO_HEADER_SIZE];
    memset(h, 0, sizeof(h));
    h[0] = CRESC_MAGIC_0; h[1] = CRESC_MAGIC_1; h[2] = CRESC_MAGIC_2; h[3] = CRESC_MAGIC_3;
    h[4] = CRESC_FORMAT_VERSION;
    h[6] = COMBO_STOP_CAP  & 0xFF; h[7] = (COMBO_STOP_CAP  >> 8) & 0xFF;
    h[8] = CRESC_MAX_LEVEL & 0xFF; h[9] = (CRESC_MAX_LEVEL >> 8) & 0xFF;

    crescFile.seek(0);
    if (crescFile.write(h, COMBO_HEADER_SIZE) != (size_t)COMBO_HEADER_SIZE) return false;

    uint8_t zeros[COMBO_RECORD_SIZE];
    memset(zeros, 0, sizeof(zeros));
    for (uint8_t l = 0; l < CRESC_MAX_LEVEL; l++) {
        if (crescFile.write(zeros, COMBO_RECORD_SIZE) != (size_t)COMBO_RECORD_SIZE) return false;
    }
    crescFile.flush();
    Serial.println("DBG: Crescendo file formatted (blanked)");
    return true;
}

static bool crescOpenOrCreate() {
    bool needFormat = !SD.exists(CRESC_FILENAME);
    crescFile = SD.open(CRESC_FILENAME, FILE_WRITE);   // O_RDWR|O_CREAT
    if (!crescFile) return false;
    if (!needFormat) {
        if (crescFile.size() != crescFileSize() || !crescValidateHeader()) needFormat = true;
    }
    if (needFormat) return crescFormatFile();
    return true;
}

// Read a level's record into buf. Returns true if any bit is set (a stored,
// non-blank level); false on an all-zero record (unset) or a read error.
static bool crescLoadRecord(uint8_t level, uint8_t* buf) {
    memset(buf, 0, COMBO_RECORD_SIZE);
    if (!crescendoAvailable || level < 1 || level > CRESC_MAX_LEVEL) return false;
    crescFile.seek(crescRecordOffset(level));
    if (crescFile.read(buf, COMBO_RECORD_SIZE) != (int)COMBO_RECORD_SIZE) return false;
    for (uint8_t i = 0; i < COMBO_RECORD_SIZE; i++) if (buf[i]) return true;
    return false;
}

// ============================================================
// Init
// ============================================================

void crescendoInit() {
    crescendoAvailable = false;
    crescSlot = 0xFF;
    setPistonAddr = ADDR_DISABLED;
    crescendoLevel = 0;
    crescendoProgLevel = 1;
    stopEngineSuppressed = false;

    for (uint8_t i = 0; i < NUM_EXPRESSIONS; i++) {
        if (exprType[i] == EXPR_CRESCENDO) { crescSlot = i; break; }
    }
    for (uint8_t i = 0; i < NUM_PISTONS; i++) {
        if (pistonType[i] == PISTON_TYPE_SET) { setPistonAddr = pistonAddr[i]; break; }
    }

    if (!SD.begin(CRESCENDO_SD_CS)) {
        Serial.println("DBG: Crescendo SD.begin failed -> crescendo disabled");
        return;
    }
    if (!crescOpenOrCreate()) {
        Serial.println("DBG: Crescendo file open/create failed -> crescendo disabled");
        return;
    }
    crescendoAvailable = true;
    Serial.print("DBG: Crescendo ready (shoe slot ");
    Serial.print(crescSlot);
    Serial.println(")");
}

// ============================================================
// Shoe -> level (0..31) with edge hysteresis
// ============================================================

static uint8_t crescLevelFromShoe() {
    uint16_t raw = analogRead(exprAnalogPin[crescSlot]);
    uint16_t lo  = calibratedExprMin[crescSlot];
    uint16_t hi  = calibratedExprMax[crescSlot];

    uint8_t candidate;
    if (hi <= lo) {
        candidate = 0;                         // degenerate calibration -> off
    } else {
        uint16_t clamped = raw < lo ? lo : (raw > hi ? hi : raw);
        candidate = (uint8_t)(((uint32_t)(clamped - lo) * CRESC_MAX_LEVEL) / (hi - lo)); // 0..31
    }

    if (candidate == crescendoLevel) return crescendoLevel;
    // A change is only accepted once the shoe has moved past the hysteresis band
    // from where the last change was accepted; a real sweep clears it easily.
    uint16_t moved = (raw > rawAtLastLevelChange) ? (raw - rawAtLastLevelChange)
                                                  : (rawAtLastLevelChange - raw);
    if (moved < CRESC_HYSTERESIS_COUNTS) return crescendoLevel;
    rawAtLastLevelChange = raw;
    return candidate;
}

// ============================================================
// Operation: blind overlay
// ============================================================

void crescendoPoll() {
    if (currentScreen != SCREEN_OPERATIONAL) return;   // no overlay while programming/config
    if (crescSlot == 0xFF || !crescendoAvailable) return;

    uint8_t newLevel = crescLevelFromShoe();

    bool wasEngaged = (crescendoLevel > 0);
    bool nowEngaged = (newLevel > 0);

    if (newLevel != crescendoLevel && nowEngaged) {
        crescLoadRecord(newLevel, crescRecord);        // cache the new level's stops
    }

    if (!wasEngaged && nowEngaged) {
        // Engage: the engine currently holds exactly the base (commanded), so
        // seed from it and take over sending.
        for (uint16_t s = 0; s < NUM_STOPS; s++) lastSentEffective[s] = stopCommandedState[s];
        stopEngineSuppressed = true;
    }

    if (wasEngaged || nowEngaged) {
        // Recompute effective = base OR level, send only what changed. Runs every
        // loop while engaged (catches manual base changes) and once on release
        // (nowEngaged false -> effective collapses to the base).
        for (uint16_t s = 0; s < NUM_STOPS; s++) {
            bool eff = stopCommandedState[s] || (nowEngaged && recordGetBit(crescRecord, s));
            if (eff != lastSentEffective[s]) {
                stopSendToEngine(s, eff);
                lastSentEffective[s] = eff;
            }
        }
    }

    if (wasEngaged && !nowEngaged) {
        stopEngineSuppressed = false;   // base is authoritative again
    }

    crescendoLevel = newLevel;
}

// ============================================================
// Programming screen
// ============================================================

// Drive the console + lamps to a stored level (NOT blind — this is editing).
// An unset (all-zero) level leaves the console unchanged.
static void crescRecallProgLevel() {
    uint8_t buf[COMBO_RECORD_SIZE];
    if (!crescLoadRecord(crescendoProgLevel, buf)) return;   // unset -> leave as-is
    for (uint16_t s = 0; s < NUM_STOPS; s++) {
        if (!inCrescendoScope(s)) continue;
        stopSetState(s, recordGetBit(buf, s));
    }
}

// If an operational overlay is still latched (e.g. we came from operation through
// the blocking config menu), collapse the engine back to the base and drop
// suppression so programming starts from a clean, visible state.
static void crescReleaseOverlay() {
    if (crescendoLevel > 0 || stopEngineSuppressed) {
        for (uint16_t s = 0; s < NUM_STOPS; s++) {
            if (lastSentEffective[s] != stopCommandedState[s]) {
                stopSendToEngine(s, stopCommandedState[s]);
                lastSentEffective[s] = stopCommandedState[s];
            }
        }
    }
    stopEngineSuppressed = false;
    crescendoLevel = 0;
}

void crescendoProgEnter() {
    crescReleaseOverlay();
    crescendoProgLevel = 1;
    crescRecallProgLevel();
}

void crescendoProgExit() {
    // Operational crescendoPoll() re-engages from the current shoe position.
    // crescendoLevel is 0 after release, so the next poll sees a clean edge.
}

void crescendoProgNav(int8_t delta) {
    int16_t v = (int16_t)crescendoProgLevel + delta;
    if (v < 1) v = 1;
    if (v > CRESC_MAX_LEVEL) v = CRESC_MAX_LEVEL;
    crescendoProgLevel = (uint8_t)v;
    crescRecallProgLevel();
}

void crescendoProgStore() {
    if (!crescendoAvailable) return;

    uint8_t rec[COMBO_RECORD_SIZE];
    memset(rec, 0, sizeof(rec));
    for (uint16_t s = 0; s < NUM_STOPS; s++) {
        if (inCrescendoScope(s) && stopCommandedState[s]) recordSetBit(rec, s);
    }
    crescFile.seek(crescRecordOffset(crescendoProgLevel));
    crescFile.write(rec, COMBO_RECORD_SIZE);
    crescFile.flush();
    Serial.print("DBG: Crescendo store level "); Serial.println(crescendoProgLevel);

    // Auto-increment (clamp at 31), then recall the new level if it is set.
    if (crescendoProgLevel < CRESC_MAX_LEVEL) crescendoProgLevel++;
    crescRecallProgLevel();
}

void crescendoProgrammingPoll() {
    if (!ADDR_VALID(setPistonAddr)) return;
    if (inputChanged(setPistonAddr) && readInput(setPistonAddr)) {  // press edge
        crescendoProgStore();
    }
}
