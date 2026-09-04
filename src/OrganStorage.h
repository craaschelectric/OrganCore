// OrganStorage.h  -  the console's one filesystem.
//
// Every file the console keeps -- COMB.DAT, CRESC.DAT, REMAP.DAT -- lives on
// the same medium, chosen by COMBINATION_USE_SPIFLASH in the instrument config:
// the SD card (false) or the Teensy 4.1 on-board QSPI flash via LittleFS (true).
//
// This exists because three modules need that filesystem and only one of them
// can own the mount. CombinationSD.cpp used to own it privately, so Crescendo
// and RemapStore were written against the global SD object and stayed on the
// card no matter what COMBINATION_USE_SPIFLASH said -- which made a flash
// console silently lose its crescendo and its builder piston assignment. The
// mount now lives here, both media are always compiled, and organFS points at
// whichever one came up.
//
// organStorageMount() is idempotent: whichever module runs first mounts, the
// rest get the same handle back. So there is no required ordering between
// combinationInit() and crescendoInit(). (remapStoreInit() is still called from
// the end of combinationInit(), where it always was.)
#ifndef ORGANCORE_ORGANSTORAGE_H
#define ORGANCORE_ORGANSTORAGE_H

#include <Arduino.h>
#include <FS.h>

// The mounted filesystem, or null if nothing is mounted yet or the mount
// failed. Every use must be guarded.
extern FS* organFS;

// Names the fault after a failed mount ("SD CARD MISSING" / "SPI FLASH
// MISSING"), null otherwise. combinationInit() copies it into
// combinationErrorText for the display.
extern const char* organStorageError;

// Mount the configured medium if it isn't mounted already. Returns true when
// organFS is usable. Safe to call from every module that needs storage.
bool organStorageMount();

#endif // ORGANCORE_ORGANSTORAGE_H
