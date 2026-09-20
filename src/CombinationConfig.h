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
// As of 1.10.0 each combination is its own small file, not a record inside one
// big file. A combination is CB_<level>_<pistonAddr>.DAT, a crescendo level is
// CR_<level>.DAT, each a bare COMBO_RECORD_SIZE-byte bitmap with no header. This
// replaced the single 8 MB COMB.DAT whose per-capture seek-and-write stalled for
// ~45 s on QSPI LittleFS (block-chain walk on flush).
//
// Two consequences of the filename keys:
//   - Combinations key on the piston INPUT ADDRESS, not its table index, so
//     inserting or reordering pistons no longer rebinds stored data to the wrong
//     button. (The old offset model was safe only for append-only table growth;
//     the address key is safe for any edit.)
//   - A piston/level never set simply has no file; loading it yields an all-zero
//     record, which is the correct "nothing stored" blank.
// ============================================================

constexpr uint16_t COMBO_STOP_CAP    = 512;   // bits per record (fixed)
constexpr uint16_t COMBO_PISTON_CAP  = 128;   // vestigial since 1.10.0 (per-file scheme); kept for tooling
constexpr uint16_t COMBO_MEM_LEVELS  = 1024;  // memory levels (fixed, 2^10)

constexpr uint16_t COMBO_RECORD_SIZE = COMBO_STOP_CAP / 8;   // 64 bytes
constexpr uint8_t  COMBO_HEADER_SIZE = 16;   // vestigial since 1.10.0; no per-file has a header
// COMBO_MAGIC_* and COMBO_FORMAT_VERSION below are vestigial since 1.10.0 -- no
// per-file record has a header. Kept so external tooling still compiles; harmless.

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
