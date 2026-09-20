// Crescendo.cpp  -  blind crescendo overlay + level programming.
// See Crescendo.h for the model. CRESC.DAT lives on whichever medium
// OrganStorage mounted (SD card or QSPI flash), alongside COMB.DAT. Levels reuse the
// combination record layout (64-byte / 512-stop bitmap, bit i = stop i,
// LSB-first) with its own header magic "OCRC" and 31 records.

#include "Crescendo.h"
#include "CombinationConfig.h"   // COMBO_RECORD_SIZE (the 64-byte record shape)
#include "StopHandler.h"         // stopCommandedState, stopSetState, stopSendToEngine, stopEngineSuppressed
#include "ScanChain.h"           // readInput / inputChanged (console SET piston)
#include "ExpressionCalibration.h"
#include "DisplayManager.h"      // currentScreen
#include "OrganStorage.h"        // organFS / organStorageMount() — same medium as COMB.DAT
#include <stdio.h>                // snprintf for the per-level filename

// Per-level crescendo files, mirroring the per-piston combination scheme (1.10.0).
// Each level 1..31 is its own 64-byte file CR_<level>.DAT, created on demand when
// that level is programmed. Replaces the single CRESC.DAT (a header plus 31
// records at computed offsets), which had the same LittleFS/QSPI defect as the
// old combo monolith: a seek-and-write into one file walks the block chain on
// flush. Small here, so it never stalled as badly as combinations, but the fix
// is the same and removes the boot-time format. A level never programmed has no
// file; loading it yields an all-zero (unset) record, the old blank behaviour.
static const char* LEGACY_CRESC_FILENAME = "CRESC.DAT";   // removed at init if present

static void crescFileName(char* out, uint8_t level) {
    snprintf(out, 20, "CR_%03u.DAT", (unsigned)level);
}

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

// Read a level's record into buf. Returns true if any bit is set (a stored,
// non-blank level); false on an all-zero record (unset) or a read error.
static bool crescLoadRecord(uint8_t level, uint8_t* buf) {
    memset(buf, 0, COMBO_RECORD_SIZE);
    if (!crescendoAvailable || level < 1 || level > CRESC_MAX_LEVEL) return false;
    char name[20];
    crescFileName(name, level);
    if (!organFS->exists(name)) return false;         // never programmed -> unset
    File f = organFS->open(name, FILE_READ);
    if (!f) return false;
    int n = f.read(buf, COMBO_RECORD_SIZE);
    f.close();
    if (n != (int)COMBO_RECORD_SIZE) { memset(buf, 0, COMBO_RECORD_SIZE); return false; }
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

    // Mount is shared and idempotent: whichever of combinationInit() /
    // crescendoInit() runs first brings the medium up.
    if (!organStorageMount()) {
        Serial.println("DBG: Crescendo storage mount failed -> crescendo disabled");
        return;
    }
    // No file to open or format: levels are per-file, created on demand at store.
    // Clear away the old single CRESC.DAT if a previous firmware left one; its
    // levels do not carry into the per-file scheme.
    if (organFS->exists(LEGACY_CRESC_FILENAME)) {
        organFS->remove(LEGACY_CRESC_FILENAME);
        Serial.println("DBG: removed legacy CRESC.DAT (per-file scheme now)");
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
    char name[20];
    crescFileName(name, crescendoProgLevel);
    File f = organFS->open(name, FILE_WRITE_BEGIN);
    if (!f) {
        Serial.print("DBG: Crescendo store open failed "); Serial.println(name);
        return;
    }
    f.seek(0);
    f.write(rec, COMBO_RECORD_SIZE);
    f.close();
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
