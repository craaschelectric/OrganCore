// CoreConfig.h  -  instrument-INVARIANT core constants
// Limits, type enums, and address macros shared by every organ.
// Per-instrument values (pins, counts, MIDI channels, timing) are
// declared extern in OrganConfig.h and defined by the sketch.
#ifndef ORGANCORE_CORECONFIG_H
#define ORGANCORE_CORECONFIG_H

#include <Arduino.h>

// ---- Fixed capacities (used for all static array sizing) ----
constexpr uint8_t  MAX_CHAINS         = 12;  // raised from 8 for the builder piston-assign slot chain; chain index field widened to 4 bits (see ADDR_CHAIN) and ADDR_DISABLED moved off chain 8. Backward-compatible: only grows RAM scan buffers.
constexpr uint16_t BITS_PER_CHAIN     = 256;
constexpr uint8_t  WORDS_PER_CHAIN    = 16;
constexpr uint16_t MAX_STOPS          = 256;
constexpr uint8_t  MAX_KEYBOARDS      = 8;
constexpr uint8_t  MAX_EXPRESSIONS    = 4;
constexpr uint8_t  MAX_ACTIVE_COILS   = 128;
constexpr uint8_t  MAX_PISTONS        = 128;  // raised from 48 to fit 64 generals + divisionals on a virtual console; == COMBO_PISTON_CAP ceiling. Backward-compatible: only grows RAM arrays, never moves an SD record.
constexpr uint16_t MAX_REMAPS         = 256;  // raised from 16 (uint8_t) for SD-backed builder piston assignment; live arrays are 256*2*2 = 1KB RAM. Frozen cap in the REMAP.DAT format.
constexpr uint8_t  MAX_SEQUENCER_PISTONS = 64;  // raised from 24 so the sequencer can walk up to 64 generals per memory level
constexpr uint8_t  MAX_DISPLAY_LINES  = 4;

// Moved from 0x800 to 0xF00 when MAX_CHAINS rose past 8: 0x800 decodes as a REAL
// address (chain 8, word 0, bit 0) once chains 8..11 exist. 0xF00 is chain 15 —
// above MAX_CHAINS (12), so no physical or virtual chain can occupy it. Any
// ConfigData.cpp that wrote the raw literal 0x800 for a disabled address must
// switch to ADDR_DISABLED (or 0xF00).
constexpr uint16_t ADDR_DISABLED      = 0xF00;

// ---- Chain types / direction ----
constexpr uint8_t CHAIN_TYPE_MULTIDROP  = 0;  // SAM sense bus: sync HIGH->LOW
constexpr uint8_t CHAIN_TYPE_SHIFTREG   = 1;  // 595/597
constexpr uint8_t CHAIN_TYPE_VIRTUAL    = 2;  // fed from serial MIDI or touch
constexpr uint8_t CHAIN_TYPE_SERIAL_SAM = 3;  // coils driven as 0xCn frames over a UART
constexpr uint8_t CHAIN_DIR_INPUT  = 0;
constexpr uint8_t CHAIN_DIR_OUTPUT = 1;

// ---- Piston types ----
constexpr uint8_t PISTON_TYPE_GENERAL    = 0;
constexpr uint8_t PISTON_TYPE_DIVISIONAL = 1;
constexpr uint8_t PISTON_TYPE_PREV       = 2;
constexpr uint8_t PISTON_TYPE_NEXT       = 3;
constexpr uint8_t PISTON_TYPE_GC         = 4;
constexpr uint8_t PISTON_TYPE_SET        = 5;
constexpr uint8_t PISTON_TYPE_SHIFT      = 6;
constexpr uint8_t PISTON_TYPE_MEM_UP     = 7;  // local-SD combination
constexpr uint8_t PISTON_TYPE_MEM_DOWN   = 8;
constexpr uint8_t PISTON_TYPE_MEM_ZERO   = 9;

// ---- Builder piston-assignment virtual slots (FROZEN LAYOUT) ----
// Canonical destination addresses for the SD-backed builder-assignable remap
// system (see RemapStore / PistonAssignScreen). Every builder-assigned piston
// remaps ONTO one of these virtual bits; the instrument's piston list points its
// canonical addresses here. These constants are FROZEN the day a builder first
// saves REMAP.DAT: the stored file holds "to" addresses computed from this
// layout, so changing a cap or stride renumbers slots and silently corrupts
// stored assignments (same append-only rule as the SD combination format). To
// grow later: append a NEW block at a fresh address range, never widen an
// existing block in place, and bump REMAP_FORMAT_VERSION.
//
// The slots live on a dedicated virtual chain (REMAP_SLOT_CHAIN) that NO
// hardware scan writes — only applyRemaps() sets bits there. Single linear
// allocation on that chain, in walk order:
//
//   [ 7 known controls ][ 64 generals ][ 128 divisionals ][ 12 spare controls ]
//   bit 0..6             bit 7..70      bit 71..198         bit 199..210
//
// Divisionals are division-major, fixed stride 16: division d, piston p (0-based)
// is slot (REMAP_SLOT_DIV_BASE + d*16 + p). Division order is frozen:
// 0 Pedal, 1 Great, 2 Swell, 3 Choir, 4 Solo, then any further divisions.
// 211 slots used of 256 on the chain (45 spare bits, deliberately unallocated).
//
// The slot chain INDEX is frozen too: stored "to" addresses embed it, so it must
// be identical on every console and survive a console adding real chains later.
// Chain 11 is the TOP index (MAX_CHAINS-1); real chains allocate upward from 0,
// so reserving the top keeps them clear. A console needing all 12 chains for
// hardware cannot use SD piston assignment (enforce NUM_CHAINS <= REMAP_SLOT_CHAIN
// with a static_assert in the sketch's ConfigData).
constexpr uint8_t  REMAP_SLOT_CHAIN       = 11;

constexpr uint8_t  REMAP_KNOWN_CONTROLS   = 7;   // Set, GC, Next, Prev, Mem+, Mem-, Shift (this order)
constexpr uint8_t  REMAP_MAX_GENERALS     = 64;
constexpr uint8_t  REMAP_DIVISIONS        = 8;
constexpr uint8_t  REMAP_PISTONS_PER_DIV  = 16;
constexpr uint8_t  REMAP_SPARE_CONTROLS   = 12;  // reserved, renameable later; walked LAST

constexpr uint16_t REMAP_DIVISIONALS      = REMAP_DIVISIONS * REMAP_PISTONS_PER_DIV;  // 128
constexpr uint16_t REMAP_TOTAL_SLOTS      = REMAP_KNOWN_CONTROLS + REMAP_MAX_GENERALS
                                          + REMAP_DIVISIONALS + REMAP_SPARE_CONTROLS;  // 211

// Bit offsets of each region within REMAP_SLOT_CHAIN (linear, in walk order).
constexpr uint16_t REMAP_SLOT_CTRL_BASE   = 0;
constexpr uint16_t REMAP_SLOT_GEN_BASE    = REMAP_SLOT_CTRL_BASE + REMAP_KNOWN_CONTROLS;   // 7
constexpr uint16_t REMAP_SLOT_DIV_BASE    = REMAP_SLOT_GEN_BASE  + REMAP_MAX_GENERALS;     // 71
constexpr uint16_t REMAP_SLOT_SPARE_BASE  = REMAP_SLOT_DIV_BASE  + REMAP_DIVISIONALS;      // 199

static_assert(REMAP_TOTAL_SLOTS <= BITS_PER_CHAIN, "remap slots exceed one virtual chain");
static_assert(REMAP_SLOT_CHAIN  <  MAX_CHAINS,     "remap slot chain index out of range");

// Known-control slot indices within the control block (0..6), fixed order.
constexpr uint8_t  REMAP_CTRL_SET   = 0;
constexpr uint8_t  REMAP_CTRL_GC    = 1;
constexpr uint8_t  REMAP_CTRL_NEXT  = 2;
constexpr uint8_t  REMAP_CTRL_PREV  = 3;
constexpr uint8_t  REMAP_CTRL_MEMUP = 4;
constexpr uint8_t  REMAP_CTRL_MEMDN = 5;
constexpr uint8_t  REMAP_CTRL_SHIFT = 6;

// ---- Combination back-end mode (compile-time selection) ----
// Which combination action is compiled in. Exactly one back-end is built.
//   HW: pistons are Hauptwerk-side (piston -> MIDI note -> the PC engine builds
//       the combination and commands stops back). The original tested behavior.
//   SD: pistons are local. Capture/recall run on the controller, reading/writing
//       an SD file and driving stops directly through StopHandler.
// The active mode is selected in CombinationConfig.h (ORGAN_COMBINATION_MODE).
// These are #defines (not constexpr) because the back-end guards are #if
// directives, which the preprocessor evaluates before C++ constants exist.
#define COMBINATION_MODE_HW 0
#define COMBINATION_MODE_SD 1

// ---- Stop flags / division ----
constexpr uint8_t STOP_IN_GENERALS    = 0x01;
constexpr uint8_t STOP_IN_DIVISIONALS = 0x02;
constexpr uint8_t STOP_GC_IMMUNE      = 0x04;
constexpr uint8_t STOP_IN_SFZ         = 0x08;
constexpr uint8_t STOP_SCREEN         = 0x10;  // no coil/lamp; sense is a touch-fed virtual bit
constexpr uint8_t STOP_DIVISION_NONE  = 0xFF;

// ---- Expression types ----
constexpr uint8_t EXPR_ANALOG    = 0;
constexpr uint8_t EXPR_DISCRETE  = 1;
constexpr uint8_t EXPR_CRESCENDO = 2;  // analog shoe that drives the blind crescendo (0-31), sends no CC

// ---- Screen states ----
constexpr uint8_t SCREEN_OPERATIONAL   = 2;
constexpr uint8_t SCREEN_CONFIG        = 3;  // blocking config menu (scanning paused)
constexpr uint8_t SCREEN_CRESCENDO     = 4;  // non-blocking crescendo programming (scanning runs)

// ---- CWB address helpers  0xCWB (C=chain 0-F, W=word 0-F, B=bit 0-F) ----
// Chain field is 4 bits (was 3): supports chain indices 0..11 (MAX_CHAINS=12).
#define ADDR_CHAIN(a)    (((a) >> 8) & 0x0F)
#define ADDR_WORD(a)     (((a) >> 4) & 0x0F)
#define ADDR_BIT(a)      ((a) & 0x0F)
#define MAKE_ADDR(c,w,b) (((c)<<8)|((w)<<4)|(b))
#define ADDR_VALID(a)    ((a) != ADDR_DISABLED)

#endif // ORGANCORE_CORECONFIG_H
