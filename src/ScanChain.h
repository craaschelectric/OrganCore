// ScanChain.h
// Low-level scan chain I/O driver
//
// Provides:
//   scanInit()          - Configure pins, clear buffers
//   scanAllChains()     - Execute one full scan: loads inputBuffer, writes outputBuffer
//   inputBuffer[][]     - What we read from hardware
//   inputBufferPrev[][] - Previous scan (for change detection)
//   outputBuffer[][]    - What we write to hardware

#ifndef SCAN_CHAIN_H
#define SCAN_CHAIN_H

#include "OrganCore.h"

// ============================================================
// Buffers (globally accessible)
// ============================================================

extern uint16_t inputBuffer[MAX_CHAINS][WORDS_PER_CHAIN];
extern uint16_t inputBufferPrev[MAX_CHAINS][WORDS_PER_CHAIN];
extern uint16_t outputBuffer[MAX_CHAINS][WORDS_PER_CHAIN];
extern uint16_t outputBufferPrev[MAX_CHAINS][WORDS_PER_CHAIN];

// ============================================================
// Functions
// ============================================================

void scanInit();
void scanAllChains();
void saveInputState();

// Attach the UART used for CHAIN_TYPE_SERIAL_SAM output (0xCn frames).
void scanSerialSamAttach(HardwareSerial& port);   // port must be begun by the sketch

// ============================================================
// Bit Access Helpers
// ============================================================

inline bool readInput(uint16_t addr) {
    if (!ADDR_VALID(addr)) return false;
    return (inputBuffer[ADDR_CHAIN(addr)][ADDR_WORD(addr)] >> ADDR_BIT(addr)) & 1;
}

inline bool readInputPrev(uint16_t addr) {
    if (!ADDR_VALID(addr)) return false;
    return (inputBufferPrev[ADDR_CHAIN(addr)][ADDR_WORD(addr)] >> ADDR_BIT(addr)) & 1;
}

inline bool inputChanged(uint16_t addr) {
    if (!ADDR_VALID(addr)) return false;
    return readInput(addr) != readInputPrev(addr);
}

inline void setOutput(uint16_t addr, bool value) {
    if (!ADDR_VALID(addr)) return;
    uint8_t c = ADDR_CHAIN(addr);
    uint8_t w = ADDR_WORD(addr);
    uint8_t b = ADDR_BIT(addr);
    if (value)
        outputBuffer[c][w] |= (1 << b);
    else
        outputBuffer[c][w] &= ~(1 << b);
}

inline bool readOutput(uint16_t addr) {
    if (!ADDR_VALID(addr)) return false;
    return (outputBuffer[ADDR_CHAIN(addr)][ADDR_WORD(addr)] >> ADDR_BIT(addr)) & 1;
}

#endif // SCAN_CHAIN_H
