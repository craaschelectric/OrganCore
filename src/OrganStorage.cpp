// OrganStorage.cpp  -  mount the console's filesystem, once, for everyone.
// See OrganStorage.h for why this is a separate module.

#include "OrganStorage.h"
#include "OrganConfig.h"   // COMBINATION_USE_SPIFLASH
#include <SD.h>
#include "OrganQSPIFlash.h"

// Chip select for the SD card. Teensy 4.1's built-in socket by default;
// override with -DCOMBINATION_SD_CS=<pin> for an external SPI card. (Kept under
// the original name so an existing build flag still applies.)
#ifndef COMBINATION_SD_CS
#define COMBINATION_SD_CS BUILTIN_SDCARD
#endif

// Both media are always built; COMBINATION_USE_SPIFLASH picks one at boot. SD
// (SDClass) and OrganQSPIFlash (a LittleFS) both derive from FS on the Teensy
// core, so they share one File type and the exists()/open()/remove() API -- the
// only media-specific call is begin(), right here.
//
// QSPI uses OrganCore's own OrganQSPIFlash (1.12.0), not LittleFS_QSPI. The
// stock class only mounts chips listed in LittleFS.cpp's private table, which
// lacks the Boya BY25Q128ES several consoles carry, so every build used to need
// a hand-patched LittleFS that a Teensyduino update silently reverted.
// OrganQSPIFlash carries the full stock NOR table plus the Boya, so a stock
// Teensyduino works on any machine. If you still have a patched copy in your
// sketchbook's libraries/LittleFS, delete it -- it is no longer needed and only
// shadows the stock library. NOR only: QSPI NAND is not supported.
static OrganQSPIFlash organFlash;   // on-board QSPI, Teensy 4.1 back-side pads

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
            Serial.println("DBG: QSPI flash begin failed -> no storage");
            Serial.printf("DBG:   JEDEC ID read %02X %02X %02X\n",
                          organFlash.lastJedecId[0], organFlash.lastJedecId[1],
                          organFlash.lastJedecId[2]);
            Serial.println("DBG:   00 00 00 or FF FF FF = no chip answered (absent or");
            Serial.println("DBG:   mis-soldered); anything else = a part not in");
            Serial.println("DBG:   OrganQSPIFlash.cpp's table -- add a row for it");
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
