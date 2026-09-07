// OrganStorage.cpp  -  mount the console's filesystem, once, for everyone.
// See OrganStorage.h for why this is a separate module.

#include "OrganStorage.h"
#include "OrganConfig.h"   // COMBINATION_USE_SPIFLASH
#include <SD.h>
#include <LittleFS.h>

// Chip select for the SD card. Teensy 4.1's built-in socket by default;
// override with -DCOMBINATION_SD_CS=<pin> for an external SPI card. (Kept under
// the original name so an existing build flag still applies.)
#ifndef COMBINATION_SD_CS
#define COMBINATION_SD_CS BUILTIN_SDCARD
#endif

// Both media are always built; COMBINATION_USE_SPIFLASH picks one at boot. SD
// (SDClass) and LittleFS_QSPI both derive from FS on the Teensy core, so they
// share one File type and the exists()/open()/remove() API -- the only
// media-specific call is begin(), right here.
//
// LittleFS_QSPI wraps LittleFS_QSPIFlash and LittleFS_QPINAND and points an
// internal FS* at whichever one answers at begin(), so a console can carry NOR
// or NAND on the back-side pad without a library edit. Note it derives from FS
// rather than from LittleFS, unlike the two classes it holds. That is fine
// here: organFS is an FS* and this library only ever calls exists(), open() and
// remove() through it. Anything added later that needs a LittleFS-specific
// method (quickFormat(), lowLevelFormat(), mediaPresent()) would have to reach
// through LittleFS_QSPI::fs() instead, which returns the active LittleFS* or
// null.
//
// EXTERNAL DEPENDENCY -- NOT SATISFIED BY THIS REPO:
// Either backing class mounts only if the chip's JEDEC ID appears in LittleFS's
// known_chips[] table (LittleFS.cpp, bundled with Teensyduino). That table
// covers Winbond, GigaDevice, Adesto, Spansion and Microchip parts.
// chip_lookup() wants an exact three-byte match, there is no SFDP fallback, and
// begin() cannot distinguish "chip I don't recognise" from "no chip fitted".
//
// Opus 57 carries a Boya BY25Q128ES (JEDEC ID 68 40 18), a functional W25Q128JV
// clone that is absent from the stock table, so it needs a patched LittleFS
// holding this row:
//
//   {{0x68, 0x40, 0x18}, 24, 256, 65536, 0xD8, 16777216, 3000, 2000000, "BY25Q128ES"},
//
// Keep that patch in the sketchbook's libraries/LittleFS, not in the copy under
// Arduino15, which a Teensyduino update silently reverts. A fresh clone of this
// repo has no way to discover any of it, which is why it is written down here.
static LittleFS_QSPI organFlash;   // on-board QSPI, Teensy 4.1 back-side pads

FS*         organFS          = nullptr;
const char* organStorageError = nullptr;

bool organStorageMount() {
    if (organFS) return true;

    if (COMBINATION_USE_SPIFLASH) {
        if (!organFlash.begin()) {
            organStorageError = "SPI FLASH MISSING";
            // The display string can only say "missing", but an unrecognised
            // chip fails here exactly like an absent one, so name both causes
            // on the serial line. This is the failure that costs an afternoon.
            Serial.println("DBG: LittleFS QSPI begin failed -> no storage");
            Serial.println("DBG:   chip absent, mis-soldered, or its JEDEC ID is");
            Serial.println("DBG:   not in LittleFS known_chips[] -- see the note");
            Serial.println("DBG:   above organFlash in OrganStorage.cpp");
            return false;
        }
        organFS = &organFlash;
        Serial.print("DBG: storage mounted on QSPI ");
        Serial.println(organFlash.getMediaName());
    } else {
        if (!SD.begin(COMBINATION_SD_CS)) {
            organStorageError = "SD CARD MISSING";
            Serial.println("DBG: SD.begin failed -> no storage");
            return false;
        }
        organFS = &SD;
        Serial.println("DBG: storage mounted on SD card");
    }

    organStorageError = nullptr;
    return true;
}
