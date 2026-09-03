// RemapStore.h
// SD-backed builder-assignable input remap table.
//
// Compiled ONLY in local-capture mode (ORGAN_COMBINATION_MODE == COMBINATION_MODE_SD).
// In HW mode there is no SD card and Hauptwerk owns the combination action, so
// builder piston assignment does not exist; applyRemaps() falls back to the
// const remapFrom[]/remapTo[] from OrganConfig.h and this file compiles out.
//
// In SD mode this store owns the LIVE remap table in RAM (liveRemapFrom/To,
// liveNumRemaps). applyRemaps() reads those instead of the const arrays. The
// table is persisted to REMAP.DAT on the same card as the combination file.
//
// File format (little-endian), REMAP.DAT:
//   header (16 bytes):
//     [0..3]  magic "OCRM"
//     [4]     format version (REMAP_FORMAT_VERSION)
//     [5]     reserved
//     [6..7]  slot chain index (u16)  -- frozen; validated against REMAP_SLOT_CHAIN
//     [8..9]  MAX_REMAPS (u16)         -- frozen cap; validated
//     [10..11] count (u16)             -- number of live remap records (0..MAX_REMAPS)
//     [12..15] reserved
//   records (4 bytes each, count of them):
//     [0..1]  from address (u16)
//     [2..3]  to address   (u16)
//
// A count of 0 is a VALID file meaning "deliberately cleared" (Start Over).
// A MISSING file means "never calibrated" -> applyRemaps uses const defaults.
// Any magic/version/cap/chain mismatch blanks the file (treated as cleared).

#ifndef REMAP_STORE_H
#define REMAP_STORE_H

#include "CombinationConfig.h"

// ORGANCORE_HAS_REMAP_STORE is computed in CombinationConfig.h and means only
// that the combination action is local (SD mode) -- in HW mode there is no card
// to attach to and this header declares nothing. Whether a console OFFERS field
// piston assignment is PISTON_ASSIGN_ENABLED in its config data: when false,
// remapStoreInit() is never called, remapLiveLoaded stays false, and
// applyRemaps() uses the const arrays.
#ifdef ORGANCORE_HAS_REMAP_STORE

#include "OrganCore.h"

// ---- File format constants (FROZEN; bump version to migrate) ----
constexpr char    REMAP_MAGIC_0 = 'O';
constexpr char    REMAP_MAGIC_1 = 'C';
constexpr char    REMAP_MAGIC_2 = 'R';
constexpr char    REMAP_MAGIC_3 = 'M';
constexpr uint8_t REMAP_FORMAT_VERSION = 1;
constexpr uint8_t REMAP_HEADER_SIZE    = 16;
constexpr uint8_t REMAP_RECORD_SIZE    = 4;

// ---- Live remap table (read by applyRemaps() in SD mode) ----
// Sized to the frozen cap. liveNumRemaps is the count currently in use.
extern uint16_t liveRemapFrom[MAX_REMAPS];
extern uint16_t liveRemapTo[MAX_REMAPS];
extern uint16_t liveNumRemaps;

// True after remapStoreInit() found and loaded a valid REMAP.DAT (count >= 0).
// False means no valid file was present at boot (never calibrated): the caller
// leaves applyRemaps() on the const defaults. See remapSourceIsLive().
extern bool     remapLiveLoaded;

// Load REMAP.DAT into the live arrays. If the file is missing, leaves the live
// table empty and remapLiveLoaded = false (const defaults stay in effect). If
// the file is present but foreign/old, blanks it to a valid zero-count file and
// sets remapLiveLoaded = true (deliberately-cleared semantics). Call once at
// boot, after the SD card is up (the combination back-end brings it up).
void remapStoreInit();

// True when applyRemaps() should read the live table rather than the const
// arrays: i.e. a valid REMAP.DAT was loaded. Used by the compile-time source
// switch in InputRemap.h together with the SD-mode guard.
inline bool remapSourceIsLive() { return remapLiveLoaded; }

// Persist the current live table to REMAP.DAT (header + liveNumRemaps records).
// Returns false on any SD error. Called by the assign screen on Save.
bool remapStoreSave();

// Append one {from -> to} remap to the live table. Rejects the add (returns
// false) if the table is full or either address is invalid. Does NOT write to
// SD -- the screen batches edits and calls remapStoreSave() on Save. Duplicates
// (same from and to already present) are silently ignored as success.
bool remapAdd(uint16_t fromAddr, uint16_t toAddr);

// Remove every live remap whose "to" equals slotAddr ("Clear this function").
// Compacts the arrays. In-RAM only; screen saves later.
void remapClearSlot(uint16_t slotAddr);

// Empty the live table ("Start Over"). In-RAM only; screen saves later. After a
// subsequent remapStoreSave() the file is a valid zero-count file (cleared),
// which is distinct from a missing file (never calibrated).
void remapClearAll();

#endif // ORGANCORE_HAS_REMAP_STORE
#endif // REMAP_STORE_H
