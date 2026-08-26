// RemapStore.cpp  -  SD-backed builder-assignable input remap table.
//
// Compiled only in local-capture mode. Shares the combination card: the SD
// library is already brought up by combinationInit() (SD.begin), so this store
// must be initialized AFTER combinationInit() and never calls SD.begin itself.
//
// File format is documented in RemapStore.h.

#include "CombinationConfig.h"

// SD-card media only. The SPI-flash (LittleFS) combination media path does not
// yet have a remap store; on that media applyRemaps() falls back to the const
// remap arrays. See RemapStore.h for the guard rationale.
#if (ORGAN_COMBINATION_MODE == COMBINATION_MODE_SD) && \
    (ORGAN_COMBINATION_MEDIA != COMBINATION_MEDIA_SPIFLASH)

#include "RemapStore.h"
#include "Debug.h"
#include <SD.h>

// Same card / CS as the combination file (documented default in CombinationSD.cpp).
#ifndef COMBINATION_SD_CS
#define COMBINATION_SD_CS BUILTIN_SDCARD
#endif

static const char* REMAP_FILENAME = "REMAP.DAT";

// ============================================================
// Live table (defines the externs from RemapStore.h)
// ============================================================

uint16_t liveRemapFrom[MAX_REMAPS];
uint16_t liveRemapTo[MAX_REMAPS];
uint16_t liveNumRemaps  = 0;
bool     remapLiveLoaded = false;

// ============================================================
// Header pack / validate
// ============================================================

// Fill a 16-byte header for the current in-RAM count.
static void packHeader(uint8_t h[REMAP_HEADER_SIZE]) {
    memset(h, 0, REMAP_HEADER_SIZE);
    h[0] = REMAP_MAGIC_0; h[1] = REMAP_MAGIC_1;
    h[2] = REMAP_MAGIC_2; h[3] = REMAP_MAGIC_3;
    h[4] = REMAP_FORMAT_VERSION;
    // [5] reserved
    h[6] = REMAP_SLOT_CHAIN & 0xFF; h[7] = (REMAP_SLOT_CHAIN >> 8) & 0xFF;
    h[8] = MAX_REMAPS       & 0xFF; h[9] = (MAX_REMAPS       >> 8) & 0xFF;
    h[10] = liveNumRemaps   & 0xFF; h[11] = (liveNumRemaps   >> 8) & 0xFF;
    // [12..15] reserved
}

// Validate a read header. On success, *countOut receives the record count.
static bool validateHeader(const uint8_t h[REMAP_HEADER_SIZE], uint16_t* countOut) {
    if (h[0] != REMAP_MAGIC_0 || h[1] != REMAP_MAGIC_1 ||
        h[2] != REMAP_MAGIC_2 || h[3] != REMAP_MAGIC_3) return false;
    if (h[4] != REMAP_FORMAT_VERSION) return false;

    uint16_t chain = (uint16_t)h[6] | ((uint16_t)h[7] << 8);
    uint16_t cap   = (uint16_t)h[8] | ((uint16_t)h[9] << 8);
    if (chain != REMAP_SLOT_CHAIN) return false;   // slot layout must match this build
    if (cap   != MAX_REMAPS)       return false;   // frozen cap must match

    uint16_t count = (uint16_t)h[10] | ((uint16_t)h[11] << 8);
    if (count > MAX_REMAPS) return false;           // corrupt count
    *countOut = count;
    return true;
}

// ============================================================
// Write the whole file from the live table (header + count records)
// ============================================================

static bool writeFile() {
    SD.remove(REMAP_FILENAME);
    File f = SD.open(REMAP_FILENAME, FILE_WRITE);   // O_RDWR | O_CREAT
    if (!f) return false;

    uint8_t h[REMAP_HEADER_SIZE];
    packHeader(h);
    f.seek(0);
    if (f.write(h, REMAP_HEADER_SIZE) != (size_t)REMAP_HEADER_SIZE) { f.close(); return false; }

    for (uint16_t i = 0; i < liveNumRemaps; i++) {
        uint8_t rec[REMAP_RECORD_SIZE];
        rec[0] = liveRemapFrom[i] & 0xFF; rec[1] = (liveRemapFrom[i] >> 8) & 0xFF;
        rec[2] = liveRemapTo[i]   & 0xFF; rec[3] = (liveRemapTo[i]   >> 8) & 0xFF;
        if (f.write(rec, REMAP_RECORD_SIZE) != (size_t)REMAP_RECORD_SIZE) { f.close(); return false; }
    }
    f.flush();
    f.close();
    return true;
}

// ============================================================
// Public API
// ============================================================

void remapStoreInit() {
    liveNumRemaps   = 0;
    remapLiveLoaded = false;

    // Missing file -> never calibrated: leave live empty, const defaults win.
    if (!SD.exists(REMAP_FILENAME)) {
        Serial.println("DBG: REMAP.DAT absent -> using const remap defaults");
        return;
    }

    File f = SD.open(REMAP_FILENAME, FILE_READ);
    if (!f) {
        Serial.println("DBG: REMAP.DAT open failed -> using const remap defaults");
        return;
    }

    uint8_t h[REMAP_HEADER_SIZE];
    uint16_t count = 0;
    bool headerOk = (f.read(h, REMAP_HEADER_SIZE) == (int)REMAP_HEADER_SIZE)
                 && validateHeader(h, &count);

    if (!headerOk) {
        // Present but foreign/old/corrupt -> blank to a valid zero-count file,
        // which counts as "deliberately cleared" (live source, empty table).
        f.close();
        liveNumRemaps   = 0;
        remapLiveLoaded = true;
        writeFile();
        Serial.println("DBG: REMAP.DAT invalid -> blanked to zero-count");
        return;
    }

    // Read the records.
    bool readOk = true;
    for (uint16_t i = 0; i < count; i++) {
        uint8_t rec[REMAP_RECORD_SIZE];
        if (f.read(rec, REMAP_RECORD_SIZE) != (int)REMAP_RECORD_SIZE) { readOk = false; break; }
        liveRemapFrom[i] = (uint16_t)rec[0] | ((uint16_t)rec[1] << 8);
        liveRemapTo[i]   = (uint16_t)rec[2] | ((uint16_t)rec[3] << 8);
    }
    f.close();

    if (!readOk) {
        // Truncated file -> blank to zero-count (cleared).
        liveNumRemaps   = 0;
        remapLiveLoaded = true;
        writeFile();
        Serial.println("DBG: REMAP.DAT truncated -> blanked to zero-count");
        return;
    }

    liveNumRemaps   = count;
    remapLiveLoaded = true;
    Serial.print("DBG: REMAP.DAT loaded, remaps=");
    Serial.println(liveNumRemaps);
}

bool remapStoreSave() {
    remapLiveLoaded = true;   // saving makes the live table authoritative
    return writeFile();
}

bool remapAdd(uint16_t fromAddr, uint16_t toAddr) {
    if (!ADDR_VALID(fromAddr) || !ADDR_VALID(toAddr)) return false;
    if (liveNumRemaps >= MAX_REMAPS) return false;

    // Silently succeed on an exact duplicate (same builder button pressed twice).
    for (uint16_t i = 0; i < liveNumRemaps; i++) {
        if (liveRemapFrom[i] == fromAddr && liveRemapTo[i] == toAddr) return true;
    }
    liveRemapFrom[liveNumRemaps] = fromAddr;
    liveRemapTo[liveNumRemaps]   = toAddr;
    liveNumRemaps++;
    return true;
}

void remapClearSlot(uint16_t slotAddr) {
    // Compact out every entry whose "to" is this slot.
    uint16_t w = 0;
    for (uint16_t r = 0; r < liveNumRemaps; r++) {
        if (liveRemapTo[r] == slotAddr) continue;
        liveRemapFrom[w] = liveRemapFrom[r];
        liveRemapTo[w]   = liveRemapTo[r];
        w++;
    }
    liveNumRemaps = w;
}

void remapClearAll() {
    liveNumRemaps = 0;
}

#endif // ORGAN_COMBINATION_MODE == COMBINATION_MODE_SD
