// CombinationConfig.h  -  combination back-end selection + SD file format.
//
// The library is compiled separately from the sketch, so the sketch's Config.h
// cannot reach these translation units. The back-end is therefore selected
// here (or by a -DORGAN_COMBINATION_MODE=... build flag, which wins because of
// the #ifndef). Exactly one back-end compiles; there is no runtime dispatch.
#ifndef ORGANCORE_COMBINATIONCONFIG_H
#define ORGANCORE_COMBINATIONCONFIG_H

#include "CoreConfig.h"

// ---- Active back-end (edit this line, or pass -DORGAN_COMBINATION_MODE) ----
#ifndef ORGAN_COMBINATION_MODE
#define ORGAN_COMBINATION_MODE COMBINATION_MODE_SD
#endif

// ---- Storage medium (edit this line, or pass -DORGAN_COMBINATION_MEDIA) ----
// Where the combination file lives: the SD card, or the Teensy 4.1 on-board
// QSPI flash (soldered to the back-side pads) via LittleFS. The file layout is
// byte-for-byte identical on both; only the medium changes. Assumes a 16 MB
// (128 Mbit) flash part, which holds the full 8 MB file with room to spare.
#define COMBINATION_MEDIA_SD       0
#define COMBINATION_MEDIA_SPIFLASH 1
#ifndef ORGAN_COMBINATION_MEDIA
#define ORGAN_COMBINATION_MEDIA COMBINATION_MEDIA_SD
#endif

// ---- Builder piston assignment (edit this line, or pass -DORGAN_ENABLE_PISTON_ASSIGN) ----
// Compile-time enable for the field builder-piston-assignment feature (the SD
// remap store REMAP.DAT, the touchscreen "Assign Pistons" screen, and boot-time
// loading of saved assignments; applyRemaps() honors the loaded table).
//
//   1 (default) — the feature is compiled in on SD-card local-capture builds
//                 (it still requires ORGAN_COMBINATION_MODE == COMBINATION_MODE_SD
//                 and non-SPI-flash media; see ORGANCORE_HAS_REMAP_STORE).
//   0           — NONE of it is compiled: no store, no screen, no menu entry, no
//                 REMAP.DAT loading. applyRemaps() uses ONLY the const
//                 remapFrom[]/remapTo[] from OrganConfig.h. Choose this on
//                 consoles whose input map is fully defined in config data
//                 (e.g. Opus 62), where field reassignment is neither needed nor
//                 wanted — nothing about REMAP.DAT can then affect the build.
#ifndef ORGAN_ENABLE_PISTON_ASSIGN
#define ORGAN_ENABLE_PISTON_ASSIGN 1
#endif

// ---- Derived: is the builder piston-assignment feature compiled in? ----
// Computed here (not in RemapStore.h) so EVERY file that includes
// CombinationConfig.h can test it without pulling in RemapStore.h — which lets
// a disabled build omit the feature's source files entirely. Every piece of the
// feature keys on this one symbol: the store, the screen, the menu entry, the
// boot-time load, and the applyRemaps() source selection. It is defined only
// when local-capture mode, SD-card media, and the enable flag all hold.
#if (ORGAN_COMBINATION_MODE == COMBINATION_MODE_SD) && \
    (ORGAN_COMBINATION_MEDIA != COMBINATION_MEDIA_SPIFLASH) && \
    (ORGAN_ENABLE_PISTON_ASSIGN)
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
