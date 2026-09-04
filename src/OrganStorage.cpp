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
// (SDClass) and LittleFS_QSPIFlash both derive from FS on the Teensy core, so
// they share one File type and the exists()/open()/remove() API -- the only
// media-specific call is begin(), right here.
static LittleFS_QSPIFlash organFlash;   // on-board QSPI flash, Teensy 4.1 back-side pads

FS*         organFS          = nullptr;
const char* organStorageError = nullptr;

bool organStorageMount() {
    if (organFS) return true;

    if (COMBINATION_USE_SPIFLASH) {
        if (!organFlash.begin()) {
            organStorageError = "SPI FLASH MISSING";
            Serial.println("DBG: LittleFS QSPI begin failed -> no storage");
            return false;
        }
        organFS = &organFlash;
        Serial.println("DBG: storage mounted on QSPI flash");
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
