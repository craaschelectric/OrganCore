// CoreConfig.h  -  instrument-INVARIANT core constants
// Limits, type enums, and address macros shared by every organ.
// Per-instrument values (pins, counts, MIDI channels, timing) are
// declared extern in OrganConfig.h and defined by the sketch.
#ifndef ORGANCORE_CORECONFIG_H
#define ORGANCORE_CORECONFIG_H

#include <Arduino.h>

// ---- Fixed capacities (used for all static array sizing) ----
constexpr uint8_t  MAX_CHAINS         = 8;
constexpr uint16_t BITS_PER_CHAIN     = 256;
constexpr uint8_t  WORDS_PER_CHAIN    = 16;
constexpr uint16_t MAX_STOPS          = 256;
constexpr uint8_t  MAX_KEYBOARDS      = 8;
constexpr uint8_t  MAX_EXPRESSIONS    = 4;
constexpr uint8_t  MAX_ACTIVE_COILS   = 128;
constexpr uint8_t  MAX_PISTONS        = 128;  // raised from 48 to fit 64 generals + divisionals on a virtual console; == COMBO_PISTON_CAP ceiling. Backward-compatible: only grows RAM arrays, never moves an SD record.
constexpr uint8_t  MAX_REMAPS         = 16;
constexpr uint8_t  MAX_SEQUENCER_PISTONS = 64;  // raised from 24 so the sequencer can walk up to 64 generals per memory level
constexpr uint8_t  MAX_DISPLAY_LINES  = 4;

constexpr uint16_t ADDR_DISABLED      = 0x800;

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

// ---- CWB address helpers  0xCWB (C=chain 0-7, W=word 0-F, B=bit 0-F) ----
#define ADDR_CHAIN(a)    (((a) >> 8) & 0x07)
#define ADDR_WORD(a)     (((a) >> 4) & 0x0F)
#define ADDR_BIT(a)      ((a) & 0x0F)
#define MAKE_ADDR(c,w,b) (((c)<<8)|((w)<<4)|(b))
#define ADDR_VALID(a)    ((a) != ADDR_DISABLED)

#endif // ORGANCORE_CORECONFIG_H
