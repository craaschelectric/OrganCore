// OrganConfig.h  -  the per-instrument config CONTRACT
// The library declares every instrument symbol here as extern const;
// the sketch's ConfigData.cpp defines them (external linkage). Static
// array sizing uses the MAX_* caps from CoreConfig.h, so nothing here
// needs to be a compile-time constant.
#ifndef ORGANCORE_ORGANCONFIG_H
#define ORGANCORE_ORGANCONFIG_H

#include <Arduino.h>
#include "CoreConfig.h"

// ---- Counts ----
extern const uint8_t  NUM_CHAINS;
extern const uint16_t NUM_STOPS;
extern const uint8_t  NUM_KEYBOARDS;
extern const uint8_t  NUM_EXPRESSIONS;
extern const uint8_t  NUM_PISTONS;
extern const uint8_t  NUM_REMAPS;
extern const uint8_t  NUM_SEQUENCER_PISTONS;
extern const uint8_t  NUM_GENERALS;
extern const uint8_t  NUM_DISPLAY_LINES;

// ---- Timing (per-instrument; used as runtime delays) ----
extern const uint32_t BIT_TIME_US;
extern const uint32_t SYNC_PULSE_US;
extern const uint32_t SYNC_SETTLE_US;
extern const uint32_t SAM_PULSE_MS;
extern const uint32_t MILLIS_ROLLOVER_GUARD_MS;

// ---- Chains ----
extern const uint8_t  chainDataInPin[];
extern const uint8_t  chainDataOutPin[];
extern const uint8_t  chainClockPin[];
extern const uint8_t  chainSyncPin[];
extern const uint16_t chainBitsUsed[];
extern const uint8_t  chainType[];
extern const uint8_t  chainDir[];
extern const uint16_t inputInvertMask[][WORDS_PER_CHAIN];

// Per-chain strobe polarity for SHIFTREG chains — the active level of the
// parallel-load (input) or output-latch (output) pulse. true = active-HIGH
// (CD4021 parallel-load, CD4094 strobe, 74HC595 latch); false = active-LOW
// (74HC597 PL). Unused for MULTIDROP / VIRTUAL / SERIAL_SAM chains (set to
// false there — the value is not consulted).
extern const bool     chainStrobeActiveHigh[];

// Per-chain shift order for SHIFTREG chains. true = MSB-first: the first bit
// clocked (input) or shifted (output) is the highest, so the buffer bit index
// equals the physical bit number in the data-chain list (CD4021 / CD4094 on the
// Rodgers 760). false = LSB-first / clock-order (the original behavior). Unused
// for MULTIDROP / VIRTUAL / SERIAL_SAM chains (set false).
extern const bool     chainMsbFirst[];

// Virtual chain (serial-MIDI / touch fed)
extern const uint8_t  VIRTUAL_CHAIN_INDEX;
extern const uint8_t  VIRTUAL_CHAIN_MIDI_CH;
extern const uint8_t  VIRTUAL_CHAIN_BASE_NOTE;
extern const uint8_t  VIRTUAL_CHAIN_MAX_NOTE;

// ---- Stops ----
extern const uint16_t stopSenseAddr[];
extern const uint16_t stopOnCoilAddr[];
extern const uint16_t stopOffCoilAddr[];
extern const uint16_t stopLightAddr[];
extern const uint16_t stopPulseMs[];
extern const uint16_t stopDebounceMs[];
extern const uint8_t  stopFlags[];        // STOP_* bitmask
extern const uint8_t  stopDivision[];     // 0..7 or STOP_DIVISION_NONE
extern const uint8_t  stopMidiChannel[];  // per-stop MIDI channel (0-indexed)
extern const uint8_t  stopMidiNote[];     // per-stop MIDI note
extern const uint8_t  SAM_RETRY_MAX;
extern const uint16_t SAM_RETRY_PULSE_INCREMENT_MS;

// Display names for the screen stops, one per screen stop in tab order (the
// order they appear on the touchscreen's 4x3 grid). Index 0 is the first
// screen stop, not stop index 0 — see screenStopIndex[] for the mapping back
// to the global stop index. NUM_SCREEN_STOPS entries.
extern const uint8_t     NUM_SCREEN_STOPS;
extern const uint16_t    screenStopIndex[];   // global stop index for each tab
extern const char* const screenStopName[];    // label painted on each tab

// ---- Keyboards ----
extern const uint8_t  kbdChain[];
extern const uint16_t kbdStartBit[];
extern const uint16_t kbdEndBit[];
extern const uint8_t  kbdMidiChannel[];
extern const uint8_t  kbdLowNote[];
extern const uint8_t  kbdVelocity[];

// ---- Pistons ----
extern const uint8_t  pistonType[];
extern const uint16_t pistonAddr[];
extern const uint8_t  pistonMidiNote[];
extern const uint8_t  pistonDivision[];   // for local-SD divisional recall
extern const uint8_t  sequencerPistonList[];
extern const char     generalName[][7];
extern const uint32_t SEQUENCER_WRAP_DELAY_MS;
extern const uint32_t SEQUENCER_DEBOUNCE_MS;

// ---- Expression ----
extern const uint8_t  exprType[];
extern const uint8_t  exprMidiCC[];
extern const uint8_t  exprMidiChannel[];
extern const uint8_t  exprDeadband[];
extern const uint8_t  exprAnalogPin[];
extern const uint16_t exprAnalogMin[];
extern const uint16_t exprAnalogMax[];
extern const uint16_t exprDiscreteStart[];
extern const uint16_t exprDiscreteEnd[];

// ---- Input remap ----
extern const uint16_t remapFrom[];
extern const uint16_t remapTo[];

// ---- MIDI channel assignments (0-indexed) ----
extern const uint8_t  MIDI_CH_STOPS_1;
extern const uint8_t  MIDI_CH_STOPS_2;
extern const uint8_t  MIDI_CH_EXPRESSION;
extern const uint8_t  MIDI_CH_KEYBOARD_BASE;
extern const uint8_t  PISTON_MIDI_CHANNEL;
extern const uint8_t  SHIFT_NOTE_OFFSET;

// ---- Startup handshake (hold until the sample engine is ready) ----
// When enabled, setup() shows a blocking "Starting Up" screen with a seconds
// counter and waits SOLELY for one NoteOn matching the channel+note below,
// arriving over USB-MIDI. That note means nothing else to the console.
// No timeout, no touch-to-skip.
extern const bool     STARTUP_WAIT_ENABLED;
extern const uint8_t  STARTUP_WAIT_MIDI_CHANNEL;   // 0-based, like the other MIDI_CH_* symbols
extern const uint8_t  STARTUP_WAIT_MIDI_NOTE;

// ---- Touch buttons (Mem/Save) ----
extern const uint8_t  MEM_UP_MIDI_CHANNEL;
extern const uint8_t  MEM_UP_MIDI_NOTE;
extern const uint8_t  MEM_DOWN_MIDI_CHANNEL;
extern const uint8_t  MEM_DOWN_MIDI_NOTE;
extern const uint8_t  SAVE_BUTTON_MIDI_CHANNEL;
extern const uint8_t  SAVE_BUTTON_MIDI_NOTE;

// ---- SysEx / display ----
extern const uint8_t  HW_SYSEX_MFG_ID;
extern const uint8_t  HW_SYSEX_MSG_TYPE;
extern const uint8_t  displayLineLCD[];
extern const uint8_t  displayLineOffset[];
extern const uint8_t  displayLineLen[];

// ---- TFT + touch controller pins (ILI9341 + XPT2046, TUI-owned SPI bus) ----
extern const uint8_t  TFT_CS_PIN;
extern const uint8_t  TFT_DC_PIN;
extern const uint8_t  TOUCH_CS_PIN;
extern const char* const displayLineLabel[];
extern const uint8_t  SYSEX_SAVE_LINE_INDEX;
extern const char     SYSEX_SAVE_TRIGGER[];

// ---- Power / display misc ----
extern const uint8_t  POWER_SUPPLY_PIN;
extern const uint8_t  BACKLIGHT_PIN;
extern const uint8_t  SCREEN1_BACKLIGHT_SECONDS;
extern const bool     HIDE_CONFIG_SCREEN;

#endif // ORGANCORE_ORGANCONFIG_H
