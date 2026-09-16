// ConfigData.cpp  -  Opus 62 (Rodgers 760) instrument definition
// Generated from Chains760Usable.xlsx. Every symbol in the OrganCore contract
// (OrganConfig.h + TuningConfig.h) with external linkage. Stops are lamped
// drawstops: commanded-is-truth, no coils.
//
// Written against OrganCore 1.8.4. Since 1.6.1 this file gained the display
// orientation and per-axis touch inversion (1.7.0), which stopped being
// hard-coded in the library's displayInit(); PITCH_PULSE_MIDI_CHANNEL became
// PITCH_PULSE_MIDI_CH, 0-based like every other channel here (1.8.0); and
// CONSOLE_NAME took over the run-screen title bar, which was the literal
// "Op62-MVUMC" inside DisplayManager.cpp until 1.8.2. Storage medium,
// piston-assign enable and presence of pipes have been contract values since
// 1.6.1. Nothing about this console needs a build flag or a local edit to the
// library.
//
// EVERY MIDI channel below is 0-based, 0..15 -- the number that goes in the
// status byte. The sample engine's UI counts from 1, so its "channel 9" is 8
// here. The only +1 in the whole system is inside MidiOut's three send
// functions, immediately before the usbMIDI call.
#include <OrganCore.h>
#include "Config.h"   // EEPROM addrs referenced below

// ---- Counts ----
extern const uint8_t  NUM_CHAINS = 7;          // 0,1 keys | 2 virtual/TFT | 3,4 pistons/toe | 5 stops | 6 lamps
extern const uint16_t NUM_STOPS = 87;          // 70 drawstops + 5 lamped toggles + 12 screen
extern const uint8_t  NUM_KEYBOARDS = 3;
extern const uint8_t  NUM_EXPRESSIONS = 4;
extern const uint8_t  NUM_PISTONS = 31;
extern const uint16_t NUM_REMAPS = 11;              // widened uint8_t -> uint16_t (MAX_REMAPS is now 256)
extern const uint8_t  NUM_SEQUENCER_PISTONS = 10;
extern const uint8_t  NUM_GENERALS = 10;
extern const uint8_t  NUM_DIVISIONS = 4;            // this console: 0 Swell,1 Great,2 Choir,3 Pedal. UNUSED while piston-assign is off; see caveat in notes if ever enabled.
extern const uint8_t  NUM_DISPLAY_LINES = 3;

// ---- Timing (CD4021/CD4094 are slow CMOS; bench-tune) ----
extern const uint32_t BIT_TIME_US = 5;         // ~10us bit clock (5us/phase)  *** VERIFY ***
extern const uint32_t SYNC_PULSE_US = 5;
extern const uint32_t SYNC_SETTLE_US = 5;
extern const uint32_t SAM_PULSE_MS = 200;      // unused (no coils)
extern const uint32_t MILLIS_ROLLOVER_GUARD_MS = 500;
extern const uint32_t SEQUENCER_WRAP_DELAY_MS = 100;
extern const uint32_t SEQUENCER_DEBOUNCE_MS = 300;

// ---- Chains ----  (shared input clock=2/load=3; output clk=9/strobe=10)  *** VERIFY PINS ***
extern const uint8_t  chainDataInPin[MAX_CHAINS]  = { 4, 5, 255, 7, 8, 1, 255, 255 };
extern const uint8_t  chainDataOutPin[MAX_CHAINS] = { 255,255,255,255,255,255, 16, 255 };
extern const uint8_t  chainClockPin[MAX_CHAINS]   = { 2, 2, 255, 2, 2, 2, 9, 255 };
extern const uint8_t  chainSyncPin[MAX_CHAINS]    = { 3, 3, 255, 3, 3, 3, 10, 255 };
extern const uint16_t chainBitsUsed[MAX_CHAINS]   = { 104,104, 16, 104,104,104, 136, 0 };
extern const uint8_t  chainType[MAX_CHAINS]       = { CHAIN_TYPE_SHIFTREG, CHAIN_TYPE_SHIFTREG, CHAIN_TYPE_VIRTUAL, CHAIN_TYPE_SHIFTREG, CHAIN_TYPE_SHIFTREG, CHAIN_TYPE_SHIFTREG, CHAIN_TYPE_SHIFTREG, CHAIN_TYPE_VIRTUAL };
extern const uint8_t  chainDir[MAX_CHAINS]        = { CHAIN_DIR_INPUT, CHAIN_DIR_INPUT, CHAIN_DIR_INPUT, CHAIN_DIR_INPUT, CHAIN_DIR_INPUT, CHAIN_DIR_INPUT, CHAIN_DIR_OUTPUT, CHAIN_DIR_INPUT };
extern const uint16_t inputInvertMask[MAX_CHAINS][WORDS_PER_CHAIN] = { {0},{0},{0},{0},{0},{0},{0},{0} };
// CD4021 parallel-load and CD4094 strobe are active-HIGH; virtual chain unused.
extern const bool     chainStrobeActiveHigh[MAX_CHAINS] = { true, true, false, true, true, true, true, false };
// CD4021/CD4094 shift MSB-first so CWB bit == data-chain-list bit.  *** flip per chain if bench shows LSB-first ***
extern const bool     chainMsbFirst[MAX_CHAINS] = { true, true, false, true, true, true, true, false };

extern const uint8_t  VIRTUAL_CHAIN_INDEX = 2;
extern const uint8_t  VIRTUAL_CHAIN_MIDI_CH = 8;    // ch9 screen stops (SerialMidi not attached on this console)
extern const uint8_t  VIRTUAL_CHAIN_BASE_NOTE = 102;
extern const uint8_t  VIRTUAL_CHAIN_MAX_NOTE = 113;

// ---- Combination action ----
// The Teensy 4.1's on-board QSPI flash, not the SD socket. Since 1.7.0 this one
// flag picks the medium for EVERY file the console keeps -- COMB.DAT, CRESC.DAT
// and REMAP.DAT all live on whatever OrganStorage mounts -- so there is no
// second setting to keep in step and no card to leave in a drawer.
//
// This supersedes the 2026-08-25 decision to run on SD. The changeover is not
// migratory: the flash comes up empty, combinationInit() finds no COMB.DAT and
// formats a fresh one behind the progress screen, and the combinations and
// crescendo program on the old card are not read again. They are still ON that
// card if they are ever wanted back.
//
// The mount can fail for a reason the display cannot express. LittleFS_QSPI
// only recognises a chip whose three-byte JEDEC ID is in LittleFS's
// known_chips[] table, there is no SFDP fallback, and begin() cannot tell an
// unlisted chip from an absent one -- both read out as "SPI FLASH MISSING".
// 1.8.4 prints the fuller story on the serial line and names the mounted part
// on success, so watch USB serial on first boot rather than the screen.
extern const bool     COMBINATION_USE_SPIFLASH = true;

// No field builder piston assignment on this console. Every piston on the 760 is
// known and fixed in pistonAddr[]/pistonType[] below, so there is nothing to
// reassign: no "Assign Pistons" entry in the config menu, no REMAP.DAT loaded,
// and applyRemaps() uses only the const remapFrom[]/remapTo[] arrays.
extern const bool     PISTON_ASSIGN_ENABLED = false;

// ---- Stops (0..69 drawstops, 70..74 lamped toggles, 75..86 screen). No coils. ----
// Stops 80, 81 and 82 are blanked (sense and light ADDR_DISABLED, flags 0) and
// are intentionally dead. They were the "Stop 6/7/8" placeholder screen stops;
// those three tab slots now mirror real drawstops 51, 12 and 28 instead, so
// nothing references 80..82 any more. Blanked rather than left populated so a
// combination recall does not set stops that have no tab and no lamp and send
// ch9 notes 107..109 to the engine for nothing.
extern const uint16_t stopSenseAddr[MAX_STOPS] = {
    0x503, 0x504, 0x505, 0x506, 0x507, 0x50A, 0x50B, 0x50C, 0x50D, 0x50E, 0x50F, 0x513,
    0x514, 0x515, 0x516, 0x517, 0x51A, 0x51B, 0x51C, 0x51D, 0x51E, 0x51F, 0x523, 0x524,
    0x525, 0x526, 0x527, 0x52A, 0x52B, 0x52C, 0x52D, 0x52E, 0x52F, 0x530, 0x531, 0x532,
    0x533, 0x535, 0x536, 0x538, 0x539, 0x53A, 0x53C, 0x541, 0x543, 0x544, 0x545, 0x546,
    0x547, 0x54A, 0x54B, 0x54C, 0x54D, 0x54E, 0x54F, 0x553, 0x554, 0x555, 0x556, 0x557,
    0x55B, 0x55C, 0x55D, 0x55E, 0x55F, 0x563, 0x564, 0x565, 0x566, 0x567, 0x44A, 0x347,
    0x455, 0x454, 0x457, 0x200, 0x201, 0x202, 0x203, 0x204, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, 0x208,
    0x209, 0x20A, 0x20B,
};
extern const uint16_t stopOnCoilAddr[MAX_STOPS] = {
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
};
extern const uint16_t stopOffCoilAddr[MAX_STOPS] = {
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
};
extern const uint16_t stopLightAddr[MAX_STOPS] = {
    0x67C, 0x67B, 0x67A, 0x679, 0x678, 0x675, 0x674, 0x673, 0x672, 0x671, 0x670, 0x66C,
    0x66B, 0x66A, 0x669, 0x668, 0x665, 0x664, 0x663, 0x662, 0x661, 0x660, 0x65C, 0x65B,
    0x65A, 0x659, 0x658, 0x655, 0x654, 0x653, 0x652, 0x651, 0x650, 0x64F, 0x64E, 0x64D,
    0x64C, 0x64A, 0x649, 0x647, 0x646, 0x645, 0x643, 0x63E, 0x63C, 0x63B, 0x63A, 0x639,
    0x638, 0x625, 0x624, 0x623, 0x622, 0x621, 0x620, 0x61C, 0x61B, 0x61A, 0x619, 0x618,
    0x614, 0x613, 0x612, 0x611, 0x610, 0x60C, 0x60B, 0x60A, 0x609, 0x608, 0x62F, 0x602,
    0x603, 0x604, 0x684, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
    ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED,
};
extern const uint8_t  stopFlags[MAX_STOPS] = {
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS,
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS,
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS,
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS,
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS,
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS,
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS,
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS,
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS, STOP_IN_GENERALS, STOP_IN_GENERALS,
    STOP_IN_GENERALS, STOP_IN_GENERALS, STOP_IN_GENERALS, STOP_IN_GENERALS,
    STOP_IN_GENERALS, STOP_IN_GENERALS, STOP_IN_GENERALS, STOP_IN_GENERALS,
    STOP_IN_GENERALS, STOP_IN_GENERALS, STOP_IN_GENERALS, STOP_IN_GENERALS,
    STOP_IN_GENERALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS,
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS,
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS,
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS,
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS,
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS, STOP_IN_GENERALS|STOP_IN_DIVISIONALS, 0, 0,
    0, 0, 0, STOP_IN_GENERALS|STOP_IN_DIVISIONALS|STOP_SCREEN,
    STOP_IN_GENERALS|STOP_IN_DIVISIONALS|STOP_SCREEN, STOP_IN_GENERALS|STOP_IN_DIVISIONALS|STOP_SCREEN, STOP_IN_GENERALS|STOP_IN_DIVISIONALS|STOP_SCREEN, STOP_IN_GENERALS|STOP_IN_DIVISIONALS|STOP_SCREEN,
    0, 0, 0, STOP_IN_GENERALS|STOP_SCREEN,
    STOP_IN_GENERALS|STOP_SCREEN, STOP_IN_GENERALS|STOP_SCREEN, STOP_IN_GENERALS|STOP_SCREEN,
};
extern const uint8_t  stopDivision[MAX_STOPS] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE,
    STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE,
    STOP_DIVISION_NONE, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE,
    STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, 0, 3, 3, 3, 1, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE,
    STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE,
};
extern const uint8_t  stopMidiChannel[MAX_STOPS] = {
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7,
    7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8,
};
extern const uint8_t  stopMidiNote[MAX_STOPS] = {
    1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 17,
    18, 19, 20, 21, 24, 25, 26, 27, 28, 29, 33, 34,
    35, 36, 37, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 51, 52, 54, 55, 56, 58, 63, 65, 66, 67, 68,
    69, 72, 73, 74, 75, 76, 77, 81, 82, 83, 84, 85,
    89, 90, 91, 92, 93, 97, 98, 99, 100, 101, 36, 17,
    47, 46, 49, 102, 103, 104, 105, 106, 107, 108, 109, 110,
    111, 112, 113,
};
extern const uint16_t stopPulseMs[MAX_STOPS] = { 0 };
extern const uint16_t stopDebounceMs[MAX_STOPS] = { 0 };
extern const uint8_t  SAM_RETRY_MAX = 3;
extern const uint16_t SAM_RETRY_PULSE_INCREMENT_MS = 50;

// ---- Keyboards: Swell(ch0 blk), Great(ch1 blk), Pedal(chain3) ----
extern const uint8_t  kbdChain[MAX_KEYBOARDS]       = { 0, 1, 3, 0,0,0,0,0 };
extern const uint16_t kbdStartBit[MAX_KEYBOARDS]    = { 40, 40, 72, 0,0,0,0,0 };
extern const uint16_t kbdEndBit[MAX_KEYBOARDS]      = { 100, 100, 103, 0,0,0,0,0 };
extern const uint8_t  kbdMidiChannel[MAX_KEYBOARDS] = { 2, 1, 0, 0,0,0,0,0 };  // Swell ch3/Great ch2/Pedal ch1 (0-based)
extern const uint8_t  kbdLowNote[MAX_KEYBOARDS]     = { 36, 36, 36, 0,0,0,0,0 };
extern const uint8_t  kbdVelocity[MAX_KEYBOARDS]    = { 127,127,127, 0,0,0,0,0 };

// ---- Pistons ----
extern const uint8_t  pistonType[MAX_PISTONS] = {
    PISTON_TYPE_GENERAL,     // GEN01
    PISTON_TYPE_GENERAL,     // GEN02
    PISTON_TYPE_GENERAL,     // GEN03
    PISTON_TYPE_GENERAL,     // GEN04
    PISTON_TYPE_GENERAL,     // GEN05
    PISTON_TYPE_GENERAL,     // GEN06
    PISTON_TYPE_GENERAL,     // GEN07
    PISTON_TYPE_GENERAL,     // GEN08
    PISTON_TYPE_GENERAL,     // GEN09
    PISTON_TYPE_GENERAL,     // GEN10
    PISTON_TYPE_DIVISIONAL,  // SW div
    PISTON_TYPE_DIVISIONAL,  // SW div
    PISTON_TYPE_DIVISIONAL,  // SW div
    PISTON_TYPE_DIVISIONAL,  // SW div
    PISTON_TYPE_DIVISIONAL,  // SW div
    PISTON_TYPE_DIVISIONAL,  // GT div
    PISTON_TYPE_DIVISIONAL,  // GT div
    PISTON_TYPE_DIVISIONAL,  // GT div
    PISTON_TYPE_DIVISIONAL,  // GT div
    PISTON_TYPE_DIVISIONAL,  // GT div
    PISTON_TYPE_DIVISIONAL,  // PED div
    PISTON_TYPE_DIVISIONAL,  // PED div
    PISTON_TYPE_DIVISIONAL,  // PED div
    PISTON_TYPE_DIVISIONAL,  // PED div
    PISTON_TYPE_GC,          // GC
    PISTON_TYPE_SET,         // SET
    PISTON_TYPE_PREV,        // Reed FF=PREV
    PISTON_TYPE_NEXT,        // SwAltMix=NEXT
    PISTON_TYPE_NEXT,        // GtAltMix=NEXT
    PISTON_TYPE_MEM_UP,      // M2=MEM+
    PISTON_TYPE_MEM_UP,      // MainChorusOff=MEM+
};

extern const uint32_t STOP_INPUT_SETTLE_MS = 100;   // lamp-to-sense coupling on the 760 harness

extern const uint16_t pistonAddr[MAX_PISTONS]     = { 0x439, 0x43A, 0x43B, 0x43C, 0x43D, 0x445, 0x446, 0x447, 0x448, 0x449, 0x43E, 0x43F, 0x440, 0x441, 0x442, 0x44E, 0x44F, 0x450, 0x451, 0x452, 0x341, 0x342, 0x343, 0x344, 0x458, 0x44B, 0x444, 0x443, 0x453, 0x44C, 0x456, };
extern const uint8_t  pistonMidiNote[MAX_PISTONS] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, };
extern const uint8_t  pistonDivision[MAX_PISTONS] = { STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 3, 3, 3, 3, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, STOP_DIVISION_NONE, };
extern const uint8_t  sequencerPistonList[MAX_SEQUENCER_PISTONS] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, };
extern const char     generalName[MAX_SEQUENCER_PISTONS][7] = { "GEN01", "GEN02", "GEN03", "GEN04", "GEN05", "GEN06", "GEN07", "GEN08", "GEN09", "GEN10", };

// ---- Expression (4 analog shoes; pins/CC/chan from Opus 62 Config) ----
extern const uint8_t  exprType[MAX_EXPRESSIONS]       = { EXPR_ANALOG, EXPR_ANALOG, EXPR_ANALOG, EXPR_ANALOG };
extern const uint8_t  exprMidiCC[MAX_EXPRESSIONS]     = { 11, 11, 11, 11 };
extern const uint8_t  exprMidiChannel[MAX_EXPRESSIONS]= { 2, 1, 0, 7 };   // 0-based (was 1-based 3/2/1/8)
extern const uint8_t  exprDeadband[MAX_EXPRESSIONS]   = { 1, 1, 1, 1 };
extern const uint8_t  exprAnalogPin[MAX_EXPRESSIONS]  = { 23, 22, 21, 20 };  // A9..A6  *** VERIFY ***
extern const uint16_t exprAnalogMin[MAX_EXPRESSIONS]  = { 0, 0, 0, 0 };   // seeded; calibrate on touch screen
extern const uint16_t exprAnalogMax[MAX_EXPRESSIONS]  = { 1023,1023,1023,1023 };
extern const uint16_t exprDiscreteStart[MAX_EXPRESSIONS] = { ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED };
extern const uint16_t exprDiscreteEnd[MAX_EXPRESSIONS]   = { ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED, ADDR_DISABLED };

// ---- Input remap: toe studs collapse onto their thumb-piston index ----
// applyRemaps() ORs the "from" bit into the "to" bit and CLEARS "from", in both
// inputBuffer and inputBufferPrev, immediately after scanAllChains(). So "to" is
// the canonical address that survives and "from" is the duplicate that is
// consumed. These eleven merge duplicate physical piston contacts on chain 3
// onto the canonical general pistons on chain 4.
//
// DO NOT REMAP INTO A VIRTUAL CHAIN. It was tried here for the screen-stop
// mirrors and it cannot work. applyRemaps() propagates a 1 and never propagates
// a 0 -- it does nothing at all when the source bit is clear. That is harmless
// for these eleven because their destinations are on real chain 4, which
// scanAllChains() overwrites from hardware every pass, so the destination is
// cleared for free. Every loop in ScanChain.cpp skips CHAIN_TYPE_VIRTUAL, so a
// virtual destination is never cleared by anything: the first time the source
// contact reads high the virtual bit latches at 1 permanently, even after the
// knob is pushed back in, and processStopInputs()'s rising-edge test
// (currentInput && !prevInput) can never fire again. The stop sticks and its tab
// goes dead. Mirroring the clear does not fix it either -- displayProcessTouch()
// forges the tab bit at the END of loop() and applyRemaps() runs at the top of
// the next one, so a clear-propagating remap would wipe the forged bit before
// processStopInputs() saw it and no tab would work at all.
extern const uint16_t remapFrom[MAX_REMAPS] = { 0x337, 0x338, 0x339, 0x33A, 0x33B, 0x33C, 0x33D, 0x33E, 0x33F, 0x340, 0x345, };
extern const uint16_t remapTo[MAX_REMAPS]   = { 0x439, 0x43A, 0x43B, 0x43C, 0x43D, 0x445, 0x446, 0x447, 0x448, 0x449, 0x44A, };

// ---- MIDI channels (0-based) ----
extern const uint8_t  MIDI_CH_STOPS_1 = 8;    // ch9 drawstops + screen stops
extern const uint8_t  MIDI_CH_STOPS_2 = 7;    // ch8 lamped toggle controls
extern const uint8_t  MIDI_CH_EXPRESSION = 8;
extern const uint8_t  MIDI_CH_KEYBOARD_BASE = 0;
extern const uint8_t  PISTON_MIDI_CHANNEL = 7;
extern const uint8_t  SHIFT_NOTE_OFFSET = 64;

// Startup handshake: wait for one NoteOn on the stops-1 channel, note 0 (not
// used by any stop -- see stopMidiNote[] above), before leaving the splash
// screen. GrandOrgue sends this once its engine is ready.
extern const bool     STARTUP_WAIT_ENABLED = true;
extern const uint8_t  STARTUP_WAIT_MIDI_CHANNEL = MIDI_CH_STOPS_1;
extern const uint8_t  STARTUP_WAIT_MIDI_NOTE = 0;

// Run-screen title bar. Was the literal "Op62-MVUMC" in the library's
// DisplayManager.cpp until 1.8.2; same text, now an instrument fact.
extern const char* const CONSOLE_NAME = "Op62-MVUMC";

// TFT + touch controller pins (ILI9341 + XPT2046). Previously hardcoded in
// the library's DisplayManager.cpp 
extern const uint8_t  TFT_CS_PIN = 14;
extern const uint8_t  TFT_DC_PIN = 17;
extern const uint8_t  TOUCH_CS_PIN = 15;

// Display orientation and touch inversion. Before OrganCore 1.7.0 displayInit()
// hard-coded LANDSCAPE_4PIN_RIGHT with no touch inversion; these three values
// reproduce that exactly, so the 760's panel behaves as it always has. If the
// screen reads upside down, the other landscape value (ORIENT_LANDSCAPE_4PIN_LEFT)
// is the 180-degree flip; if the glass is right but touches land opposite, set
// the axis flags instead -- both true is a 180-degree touch rotation, one true
// is the single-axis mirror some panels are wired with.
extern const uint8_t  TFT_ORIENTATION = ORIENT_LANDSCAPE_4PIN_RIGHT;
extern const bool     TOUCH_INVERT_X = false;
extern const bool     TOUCH_INVERT_Y = false;

extern const uint8_t  MEM_UP_MIDI_CHANNEL = 7;
extern const uint8_t  MEM_UP_MIDI_NOTE = 126;
extern const uint8_t  MEM_DOWN_MIDI_CHANNEL = 7;
extern const uint8_t  MEM_DOWN_MIDI_NOTE = 127;
extern const uint8_t  SAVE_BUTTON_MIDI_CHANNEL = 7;
extern const uint8_t  SAVE_BUTTON_MIDI_NOTE = 125;

// ---- SysEx / display ----
extern const uint8_t  HW_SYSEX_MFG_ID = 0x7D;
extern const uint8_t  HW_SYSEX_MSG_TYPE = 0x01;
extern const uint8_t  displayLineLCD[MAX_DISPLAY_LINES]    = { 0x00, 0x02, 0x01, 0xFF };
extern const uint8_t  displayLineOffset[MAX_DISPLAY_LINES] = { 5, 5, 5, 0 };
extern const uint8_t  displayLineLen[MAX_DISPLAY_LINES]    = { 16, 16, 16, 0 };
extern const char* const displayLineLabel[MAX_DISPLAY_LINES] = { "MEM", "SAVE", "APP", "" };
extern const uint8_t  SYSEX_SAVE_LINE_INDEX = 1;
extern const char     SYSEX_SAVE_TRIGGER[] = "ON";

// ---- Power / display misc ----
// ---- Console power control (OrganCore 1.9.0; see POWER-CONTROL.md) ----
// The Rodgers supply latches through a relay on the ROC 1767 board that this
// console has to hold in, and the Pi running GrandOrgue is on an outlet the
// same supply switches. So switching off is a sequence, not a contact, and
// OrganPower.cpp owns it.
//
// Teensy 33 -> 1K -> Q1 base on the ROC 1767 board. Q1 is a Darlington
// switching the +12 relay; HIGH here holds the whole console alive. Driven high
// by powerInit() as the first statement in setup(), because everything after it
// -- the QSPI mount, a first-boot combination format, the startup wait -- can
// block for a long time. A Teensy pin is hi-Z on reset and in the bootloader,
// so the relay drops and the amplifiers power-cycle on every firmware upload:
// expected, not a fault.
extern const uint8_t  POWER_KEEPALIVE_PIN      = 33;

// Relay console, not ATX: HIGH at Q1's base is "stay powered", and release is a
// positive drive LOW rather than hi-Z. (An ATX console sets this false, which
// also switches the release to open-drain -- PS_ON# is pulled up to +5VSB and a
// Teensy 4.x pin is not 5V tolerant.)
extern const bool     POWER_KEEPALIVE_ACTIVE_HIGH = true;

// Chain 4 bit 99 (word 6, bit 3), ACTIVE HIGH like every other contact on that
// chain. Momentary, and it is the ON switch too -- it bypasses the relay, so it
// is held down throughout boot and OrganPower ignores it until released once.
extern const uint16_t POWER_SWITCH_ADDR        = 0x463;
extern const uint32_t POWER_SWITCH_HOLD_MS     = 250;

// Teensy 36 -> Pi GPIO3 (40-pin header pin 5). A gpio-key overlay maps it to
// KEY_PROG1 and triggerhappy turns that into a poweroff.
extern const uint8_t  POWER_HOST_SHUTDOWN_PIN  = 36;

// Teensy 29 <- Pi GPIO23 (header pin 16), driven by the organ-ready systemd
// unit. HIGH once triggerhappy is up and a shutdown request will be honoured;
// LOW again as the Pi halts, which ends the wait below early. Set this to 255
// if the wire is not fitted -- the console then cannot be switched off until
// setup() has finished, which with STARTUP_WAIT_ENABLED means waiting for
// GrandOrgue to load.
extern const uint8_t  POWER_HOST_READY_PIN     = 29;

// Backstop only, now that the readiness line reports the real thing. Measured
// halt was ~4 s with a sample set loaded.
extern const uint32_t POWER_HOST_HALT_MS       = 15000;

// Blower off. The pipe driver watches for F0 7D 62 <state> F7 and switches the
// blower AC: 7D is the non-commercial manufacturer ID and 62 tags it as Opus
// 62, so nothing else on the line acts on it. The relay is LATCHING and the
// pipe driver asserts it on at its own boot, so there is no power-up command to
// send -- and a Teensy reset cannot silently restore wind.
extern const uint8_t  POWER_SHUTDOWN_SYSEX[]   = { 0xF0, 0x7D, 0x62, 0x00, 0xF7 };
extern const uint16_t POWER_SHUTDOWN_SYSEX_LEN = 5;

extern const uint8_t  BACKLIGHT_PIN = 32;
extern const uint8_t  SCREEN1_BACKLIGHT_SECONDS = 10;
extern const bool     HIDE_CONFIG_SCREEN = false;

// ---- Screen stops (touchscreen tabs) ----
// Slots 1-5 and 9-12 are screen-only stops (global indices 75..79, 83..86) on
// ch9 notes 102..106 and 110..113. Touch toggles them: their sense addresses are
// on the VIRTUAL chain, processTabTouch() forges the bit there, nothing rescans
// a virtual chain so the bit survives to processStopInputs(), and retireTabBits()
// clears it afterwards for one clean rising edge per tap.
//
// Slots 6-8 are DISPLAY-ONLY MIRRORS of three physical drawstops -- they point
// at global stops 51 (GT Flute 4), 12 (SW Nachthorn 4) and 28 (PED Subbass 16),
// the real entries, not copies. The Great Flute 4's console lamp is burned out;
// these tabs are its replacement indicator.
//
// They light correctly because repaintChangedTabs() reads
// stopCommandedState[screenStopIndex[t]], which is the real stop's state, set by
// the knob through processStopInputs() exactly as always.
//
// They are inert to touch, by construction rather than by a flag. Those stops'
// sense addresses are on chain 5, real hardware. processTabTouch() forges its
// bit there from displayProcessTouch(), which is the LAST call in loop(); the
// FIRST call of the next iteration is scanAllChains(), which overwrites that
// word from the physical contacts. The forged bit is destroyed before
// processStopInputs() can ever see it. Nothing reads inputBuffer in between, and
// saveInputState() has already run, so no spurious edge is produced either.
//
// Making these tabs live would take a library change, not a config change:
// processTabTouch() would have to set the commanded state directly instead of
// forging an input bit. The forging trick is only safe when the destination is a
// chain nothing rescans.
extern const uint8_t  NUM_SCREEN_STOPS = 12;
extern const uint16_t screenStopIndex[MAX_STOPS] = { 75, 76, 77, 78, 79, 51, 12, 28, 83, 84, 85, 86, };
extern const char* const screenStopName[MAX_STOPS] = {
    "SW: Vox",    "PED:Contra", "PED:",       "PED: Trom-",
    "GT: Quint-", "GT: Flute",  "SW: Nacht-", "PED: Sub-",
    "Stop 9",     "Stop 10",    "Stop 11",    "Stop 12",
};
extern const char* const screenStopNameLine2[MAX_STOPS] = {
    "Humana",     "Fagotto",    "Principal",  "pette",
    "aton",       "",           "horn",       "bass",
    "",           "",           "",           "",
};
extern const char* const screenStopNameLine3[MAX_STOPS] = {
    "8",          "32",         "16",         "8",
    "16",         "4",          "4",          "16",
    "",           "",           "",           "",
};

// ---- Tuning contract — Opus 62 set ----
// This is a pipe instrument, so the contract is written out in full here and
// <TuningDefaults.h> is NOT included; that header is for pipeless consoles and
// defines the same symbols at inert values.
extern const bool     ORGAN_TUNING_PRESENT     = true;
extern const bool     PITCH_PULSE_ENABLED      = true;
extern const bool     PITCH_SEND_TUNING_SYSEX  = false;  // GrandOrgue console: the nudge-note
                                                            // feedback loop below IS the transport.
                                                            // MTS master fine tuning is the Hauptwerk
                                                            // path; running both applied the same
                                                            // offset twice by two mechanisms.
extern const int8_t   MANUAL_OFFSET_MIN        = -25;
extern const int8_t   MANUAL_OFFSET_MAX        =  25;
extern const float    TEMP_REFERENCE_DEGC      = 25.0f;
extern const uint8_t  CENTS_PER_DEGREE         = 3;
extern const float    PITCH_A_REFERENCE_HZ     = 440.0f;
extern const uint8_t  PITCH_UP_MIDI_NOTE       = 0;
extern const uint8_t  PITCH_DOWN_MIDI_NOTE     = 1;
extern const uint8_t  PITCH_PULSE_MIDI_CH      = 14;   // was PITCH_PULSE_MIDI_CHANNEL = 15, the one 1-based symbol in the old contract; same wire channel
extern const uint8_t  PITCH_PULSE_ON_MS        = 20;
extern const uint16_t PITCH_PULSE_TIMEOUT_MS   = 5000;
extern const uint8_t  PITCH_SYSEX_LCD_NUM      = 0x0F;   // must differ from every displayLineLCD[] value