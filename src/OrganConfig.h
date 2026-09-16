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
extern const uint16_t NUM_REMAPS;   // widened from uint8_t: MAX_REMAPS is now 256
extern const uint8_t  NUM_SEQUENCER_PISTONS;
extern const uint8_t  NUM_GENERALS;
extern const uint8_t  NUM_DIVISIONS;  // divisions this console has (<= REMAP_DIVISIONS); bounds the piston-assign divisional walk. Frozen order: 0 Pedal,1 Great,2 Swell,3 Choir,4 Solo,...
extern const uint8_t  NUM_DISPLAY_LINES;

// ---- Timing (per-instrument; used as runtime delays) ----
extern const uint32_t BIT_TIME_US;
extern const uint32_t SYNC_PULSE_US;
extern const uint32_t SYNC_SETTLE_US;
extern const uint32_t SAM_PULSE_MS;
extern const uint32_t MILLIS_ROLLOVER_GUARD_MS;

// Stop-contact settling window. A lamp driver switching can couple into that
// stop's own sense line, so a lamp that just changed may fake a contact
// closure on the next scan and toggle the stop straight back off.
// processStopInputs() ignores stop contact edges for this long after any
// commanded stop change. Set 0 on a console with no observed coupling.
extern const uint32_t STOP_INPUT_SETTLE_MS;

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

// ---- Combination action ----
// Where the console's files live: the SD card (false) or the Teensy 4.1
// on-board QSPI flash via LittleFS (true). This covers ALL of them -- COMB.DAT,
// CRESC.DAT and REMAP.DAT -- so a flash console keeps its crescendo and its
// builder piston assignment (they used to be stranded on the card). Layouts are
// byte-for-byte identical on both media; only the medium changes. Both are
// always compiled and the choice is made at mount (see OrganStorage).
extern const bool     COMBINATION_USE_SPIFLASH;

// Does this console offer field builder piston assignment -- the "Assign
// Pistons" config screen and the REMAP.DAT store that overrides the const
// remapFrom[]/remapTo[]? False on a console whose input map is fully defined in
// config data, which is the usual case: no menu entry, no REMAP.DAT loaded, and
// applyRemaps() uses only the const arrays.
extern const bool     PISTON_ASSIGN_ENABLED;

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
extern const char* const screenStopName[];    // label painted on each tab — first line
// Second and third label lines, painted under screenStopName[] and centred
// independently of it. A null or empty string omits that line entirely, and the
// lines that remain are centred in the tab as a block, so a console with
// one-line names looks exactly as it did before these were added.
extern const char* const screenStopNameLine2[];
extern const char* const screenStopNameLine3[];

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

// ---- MIDI channel assignments ----
// Every MIDI channel in this contract is 0-based, 0..15 -- the number that goes
// in the status byte's low nibble. Nothing here is ever 1-based; the only place
// the library knows about usbMIDI's 1..16 API is inside MidiOut's three send
// functions.
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

// Console name across the top of the run screen -- the one place an instrument
// names itself on its own display. Keep it short: the title bar is 320 px wide
// and shares the row with nothing else, so roughly 20 characters at the title
// font.
extern const char* const CONSOLE_NAME;

// ---- TFT + touch controller pins (ILI9341 + XPT2046, TUI-owned SPI bus) ----
extern const uint8_t  TFT_CS_PIN;
extern const uint8_t  TFT_DC_PIN;
extern const uint8_t  TOUCH_CS_PIN;

// Display orientation: an ORIENT_* value from CoreConfig.h. The run screen's
// layout is fixed 320x240 landscape, so use ORIENT_LANDSCAPE_4PIN_LEFT or
// ORIENT_LANDSCAPE_4PIN_RIGHT -- the two differ by 180 degrees, which is the
// flip an inverted mount needs and the only display change supported.
extern const uint8_t  TFT_ORIENTATION;

// Touch inversion, per axis, applied on top of the display orientation. Panels
// vary in how the touch layer is wired relative to the glass: some are mirrored
// on one axis, some are end-for-end on both. One flag per axis covers every
// case -- inverting BOTH is exactly a 180-degree touch rotation, so a panel
// whose touch layer is simply upside down sets both true. A normal panel sets
// both false.
//
// This is deliberately NOT a second orientation value: an orientation can only
// rotate, and a single-axis mirror (a real panel fault, see Opus 67) is not a
// rotation of anything.
extern const bool     TOUCH_INVERT_X;
extern const bool     TOUCH_INVERT_Y;
extern const char* const displayLineLabel[];
extern const uint8_t  SYSEX_SAVE_LINE_INDEX;
extern const char     SYSEX_SAVE_TRIGGER[];

// ---- Console power control (see OrganPower.h, and POWER-CONTROL.md) ----
// Every part is optional. A console that owns its own power sets
// POWER_KEEPALIVE_PIN and POWER_HOST_SHUTDOWN_PIN to 255, POWER_SWITCH_ADDR to
// ADDR_DISABLED and POWER_SHUTDOWN_SYSEX_LEN to 0, and OrganPower does nothing.
//
// Replaces POWER_SUPPLY_PIN, which was declared here from 1.0 and read by no
// library code at any point.

// Held HIGH for as long as the console should stay powered. On a retrofit whose
// supply latches through a relay the control computer must hold in, this is
// that relay. 255 = the console owns its own power.
extern const uint8_t  POWER_KEEPALIVE_PIN;

// Which level on that pin means "stay powered".
//
//   true  - a relay or transistor driven directly, as on a retrofit whose
//           supply latches through one. Asserted by driving HIGH, released by
//           driving LOW. Push-pull both ways.
//   false - an ATX supply's PS_ON#, or anything else with its own pull-up.
//           Asserted by driving LOW, released by going HI-Z and letting that
//           pull-up take the line.
//
// The drive style is NOT a separate setting, because it is not a free choice.
// ATX pulls PS_ON# up to +5VSB, and a Teensy 4.x pin is not 5V tolerant: a
// push-pull HIGH there would put a 3.3V driver against a 5V rail. Active-low is
// therefore always released open-drain. Active-high has no such constraint and
// is driven both ways, which is more positive.
//
// Either way an unpowered or resetting board leaves the pin hi-Z, which reads
// as released in both polarities -- so the supply drops on reset and during the
// bootloader. That is correct in both cases, and it is why the console
// power-cycles on every firmware upload.
//
// On an ATX console the momentary power switch is wired straight across PS_ON#
// to ground, in parallel with this pin: holding it starts the supply with no
// firmware involved, powerInit() then holds PS_ON# down itself, and the
// organist lets go. Two pull-downs on one node, neither ever driving it high.
//
// The controller on such a console must NOT be powered from +5VSB. It has to
// die with the supply, or it survives its own shutdown, never re-asserts
// PS_ON#, and the next press holds the supply up only while the button is
// held. Everything dies together so the next press is a cold boot. See
// POWER-CONTROL.md.
extern const bool     POWER_KEEPALIVE_ACTIVE_HIGH;

// The console power switch, as an input bit address. Momentary. On the consoles
// this was built for it is also the ON switch -- it bypasses the supply relay,
// so it is held down throughout setup(); OrganPower ignores it until it has
// been seen released once. ADDR_DISABLED = no power switch.
extern const uint16_t POWER_SWITCH_ADDR;

// How long that contact must read pressed CONTINUOUSLY before the shutdown
// runs. One noisy scan must not be able to take the organ down mid-service.
extern const uint32_t POWER_SWITCH_HOLD_MS;

// Driven LOW to ask a host computer to halt; otherwise left hi-Z so the host's
// own pull-up holds it high. Never driven HIGH, so no state of this board can
// halt the host by accident. 255 = no host.
extern const uint8_t  POWER_HOST_SHUTDOWN_PIN;

// ACTIVE LOW, read with INPUT_PULLUP. The host pulls it DOWN to say "a shutdown
// request will be honoured now", and releases it as it halts.
//
// Active low is not a style choice. A bare input floats, and a floating pin
// reads whatever is in the air -- an unwired, unpowered or crashed host would
// report ready at random, and a random "ready" cuts mains from a machine that
// was never listening. With our pull-up, every one of those cases reads HIGH,
// which is "not ready", which is the safe answer. It also matches the rest of
// this design, where every signal asserts by pulling to ground. Gates the power switch, and its falling edge ends
// the shutdown wait early. 255 = no wire, in which case "ready" means
// powerBootComplete() has been called and the wait always runs the full
// POWER_HOST_HALT_MS. Running the wire is strongly preferred: without it the
// power switch does nothing until setup() has finished, which on a console with
// STARTUP_WAIT_ENABLED means waiting for the sample engine to load.
extern const uint8_t  POWER_HOST_READY_PIN;

// Backstop for the halt wait, in ms. Used in full when there is no readiness
// wire, and as an upper bound when there is. Measure the host's real halt time
// and leave generous margin -- expiring early cuts mains mid-write.
extern const uint32_t POWER_HOST_HALT_MS;

// A complete framed SysEx message (0xF0 ... 0xF7) sent on the way down, after
// the general cancel and before the host is asked to halt. For whatever else on
// the instrument needs telling -- Opus 62 uses it to drop its blower relay. It
// goes to usbMIDI and to the pipe mirror both. Set _LEN to 0 for none.
extern const uint8_t  POWER_SHUTDOWN_SYSEX[];
extern const uint16_t POWER_SHUTDOWN_SYSEX_LEN;

// ---- Display misc ----
extern const uint8_t  BACKLIGHT_PIN;
extern const uint8_t  SCREEN1_BACKLIGHT_SECONDS;
extern const bool     HIDE_CONFIG_SCREEN;

#endif // ORGANCORE_ORGANCONFIG_H
