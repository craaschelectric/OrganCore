// CombinationConfig.h  -  combination back-end selection + SD file format.
//
// ORGAN_COMBINATION_MODE is the LAST compile-time switch in this library, and it
// is here because it has to be, not to save space. PistonHandler.cpp (Hauptwerk
// owns the combination action) and CombinationSD.cpp (we do) DEFINE THE SAME
// SYMBOLS -- processPistons(), pistonInit(), combinationAvailable, setHeld,
// sequencerPosition. Compiling both would be a duplicate-symbol link error, so
// exactly one back-end builds and there is no runtime dispatch. It is exclusive
// by construction.
//
// In practice nobody edits it: local-SD is the default and every current console
// uses it. Everything else that used to live here -- the storage medium and the
// builder piston-assignment enable -- is now ordinary instrument config in
// OrganConfig.h (COMBINATION_USE_SPIFLASH, PISTON_ASSIGN_ENABLED), decided at run
// time. A library header should never need editing to move between consoles.
#ifndef ORGANCORE_COMBINATIONCONFIG_H
#define ORGANCORE_COMBINATIONCONFIG_H

#include "CoreConfig.h"

// ---- Active back-end (edit this line, or pass -DORGAN_COMBINATION_MODE) ----
#ifndef ORGAN_COMBINATION_MODE
#define ORGAN_COMBINATION_MODE COMBINATION_MODE_SD
#endif

// ---- Derived: is the builder piston-assignment CODE compiled in? ----
// The store, the assign screen and the boot-time REMAP.DAT load exist only in
// local-capture mode -- in HW mode there is no SD card and Hauptwerk owns the
// combination action, so they have nothing to attach to. That is the only
// condition now: whether a given console OFFERS the feature is
// PISTON_ASSIGN_ENABLED in its config data, checked at run time.
#if ORGAN_COMBINATION_MODE == COMBINATION_MODE_SD
#define ORGANCORE_HAS_REMAP_STORE 1
#endif

// ============================================================
// SD combination file format
//
// One flat, fixed-record binary file. Uniqueness of a (memory level, piston)
// capture comes from a computed byte offset into a 2-D array laid out with
// level as the major axis and piston as the minor axis:
//
//   offset(level, piston) = COMBO_HEADER_SIZE
//                         + (level * COMBO_PISTON_CAP + piston) * COMBO_RECORD_SIZE
//
// The layout uses FIXED caps (not the instrument's live NUM_STOPS/NUM_PISTONS),
// so adding stops or pistons later never moves an existing record and stored
// registrations survive untouched. That guarantee holds for APPEND-ONLY growth
// of the stop/piston tables: inserting or reordering existing indices remaps
// every bit and silently corrupts stored data (append-only discipline).
// ============================================================

constexpr uint16_t COMBO_STOP_CAP    = 512;   // bits per record (fixed)
constexpr uint16_t COMBO_PISTON_CAP  = 128;   // records per level (fixed)
constexpr uint16_t COMBO_MEM_LEVELS  = 1024;  // memory levels (fixed, 2^10)

constexpr uint16_t COMBO_RECORD_SIZE = COMBO_STOP_CAP / 8;   // 64 bytes
constexpr uint8_t  COMBO_HEADER_SIZE = 16;

// Bit i of a record = stop index i is ON. Byte i/8, bit i%8, LSB-first.
// (Stated explicitly so a future PC-side combination editor matches the firmware.)

// Header: magic[4] "OCMB", version, reserved, STOP_CAP(u16), PISTON_CAP(u16),
// MEM_LEVELS(u16), reserved[4]. A magic/version/cap mismatch blanks the card.
constexpr char    COMBO_MAGIC_0 = 'O';
constexpr char    COMBO_MAGIC_1 = 'C';
constexpr char    COMBO_MAGIC_2 = 'M';
constexpr char    COMBO_MAGIC_3 = 'B';
constexpr uint8_t COMBO_FORMAT_VERSION = 1;

// The two arrays the record indexes into are sized by the MAX_* caps, so those
// caps must fit the file's fixed caps. These are the real compile-time guards
// (NUM_STOPS/NUM_PISTONS are extern const, not constant expressions, so they
// cannot be static_assert'd; they are always <= their MAX_*).
static_assert(MAX_STOPS   <= COMBO_STOP_CAP,   "MAX_STOPS exceeds SD record capacity");
static_assert(MAX_PISTONS <= COMBO_PISTON_CAP, "MAX_PISTONS exceeds SD file capacity");

#endif // ORGANCORE_COMBINATIONCONFIG_H
