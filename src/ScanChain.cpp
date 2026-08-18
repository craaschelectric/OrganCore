// ScanChain.cpp
// Low-level scan chain I/O driver.
// Virtual chains (CHAIN_TYPE_VIRTUAL) are skipped during hardware scanning —
// their inputBuffer is populated by SerialMidi.cpp.

#include "ScanChain.h"

// ============================================================
// Buffers
// ============================================================

uint16_t inputBuffer[MAX_CHAINS][WORDS_PER_CHAIN];
uint16_t inputBufferPrev[MAX_CHAINS][WORDS_PER_CHAIN];
uint16_t outputBuffer[MAX_CHAINS][WORDS_PER_CHAIN];
uint16_t outputBufferPrev[MAX_CHAINS][WORDS_PER_CHAIN];

// Serial-SAM transport (attached by the sketch; null = no serial-SAM chain)
static HardwareSerial* samPort = nullptr;
void scanSerialSamAttach(HardwareSerial& port) { samPort = &port; }   // sketch owns begin()

// Transmit one CHAIN_TYPE_SERIAL_SAM chain as 0xCn + 4-byte frames (MSB word first).
static void transmitSerialSam(uint8_t c) {
    if (!samPort) return;
    uint16_t words  = (chainBitsUsed[c] + 15) / 16;
    uint16_t frames = (words + 1) / 2;
    for (uint16_t f = 0; f < frames; f++) {
        uint16_t hi = (2*f + 1 < WORDS_PER_CHAIN) ? outputBuffer[c][2*f + 1] : 0;
        uint16_t lo = outputBuffer[c][2*f];
        uint8_t frame[5] = { (uint8_t)(0xC0 | f),
                             (uint8_t)((hi >> 8) & 0xFF), (uint8_t)(hi & 0xFF),
                             (uint8_t)((lo >> 8) & 0xFF), (uint8_t)(lo & 0xFF) };
        for (uint8_t b = 0; b < 5; b++) { while (!samPort->availableForWrite()) {} samPort->write(frame[b]); }
    }
}

// ============================================================
// Initialization
// ============================================================

void scanInit() {
    memset(inputBuffer, 0, sizeof(inputBuffer));
    memset(inputBufferPrev, 0, sizeof(inputBufferPrev));
    memset(outputBuffer, 0, sizeof(outputBuffer));
    memset(outputBufferPrev, 0xFF, sizeof(outputBufferPrev));

    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        // Skip virtual chains — no hardware to configure
        if (chainType[c] == CHAIN_TYPE_VIRTUAL) continue;

        pinMode(chainClockPin[c], OUTPUT);
        pinMode(chainSyncPin[c], OUTPUT);

        if (chainDir[c] == CHAIN_DIR_INPUT) {
            pinMode(chainDataInPin[c], INPUT);
        } else {
            pinMode(chainDataOutPin[c], OUTPUT);
        }

        if (chainType[c] == CHAIN_TYPE_MULTIDROP) {
            digitalWrite(chainSyncPin[c], LOW);
        } else {
            // SHIFTREG (input parallel-load or output latch): idle is the
            // de-asserted level of the strobe. Active-high strobe (74HC595
            // latch, CD4094 strobe, CD4021 parallel-load) idles LOW; active-low
            // strobe (74HC597 PL) idles HIGH.
            digitalWrite(chainSyncPin[c], chainStrobeActiveHigh[c] ? LOW : HIGH);
        }

        digitalWrite(chainClockPin[c], LOW);
    }
}

// ============================================================
// Check if any output chain has changed
// ============================================================

static bool outputChanged() {
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        if (chainDir[c] != CHAIN_DIR_OUTPUT) continue;
        if (chainType[c] == CHAIN_TYPE_VIRTUAL) continue;
        for (uint8_t w = 0; w < WORDS_PER_CHAIN; w++) {
            if (outputBuffer[c][w] != outputBufferPrev[c][w]) return true;
        }
    }
    return false;
}

// ============================================================
// Debug: log non-zero output buffer contents
// ============================================================

static void debugLogOutputBuffer() {
#if DEBUG_ENABLED
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        if (chainDir[c] != CHAIN_DIR_OUTPUT) continue;
        if (chainType[c] == CHAIN_TYPE_VIRTUAL) continue;
        for (uint8_t w = 0; w < WORDS_PER_CHAIN; w++) {
            if (outputBuffer[c][w] != 0) {
                Serial.print("DBG: OutBuf[");
                Serial.print(c);
                Serial.print("][");
                Serial.print(w);
                Serial.print("]=0x");
                Serial.println(outputBuffer[c][w], HEX);
            }
        }
    }
#endif
}

// ============================================================
// Scan Input Chains (597 + multidrop) — skips virtual
// ============================================================

static void scanInputChains() {
    uint16_t maxBits = 0;
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        if (chainDir[c] != CHAIN_DIR_INPUT) continue;
        if (chainType[c] == CHAIN_TYPE_VIRTUAL) continue;
        if (chainBitsUsed[c] > maxBits) maxBits = chainBitsUsed[c];
    }
    if (maxBits == 0) return;

    // Sync pulse: assert
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        if (chainDir[c] != CHAIN_DIR_INPUT) continue;
        if (chainType[c] == CHAIN_TYPE_VIRTUAL) continue;
        if (chainType[c] == CHAIN_TYPE_MULTIDROP)
            digitalWrite(chainSyncPin[c], HIGH);
        else
            // SHIFTREG parallel-load: assert at the strobe's active level
            // (CD4021 load = active-high; 74HC597 PL = active-low).
            digitalWrite(chainSyncPin[c], chainStrobeActiveHigh[c] ? HIGH : LOW);
    }
    delayMicroseconds(SYNC_PULSE_US);

    // Sync pulse: de-assert
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        if (chainDir[c] != CHAIN_DIR_INPUT) continue;
        if (chainType[c] == CHAIN_TYPE_VIRTUAL) continue;
        if (chainType[c] == CHAIN_TYPE_MULTIDROP)
            digitalWrite(chainSyncPin[c], LOW);
        else
            digitalWrite(chainSyncPin[c], chainStrobeActiveHigh[c] ? LOW : HIGH);
    }
    delayMicroseconds(SYNC_SETTLE_US);

    // Clock through all bits
    for (uint16_t bitIdx = 0; bitIdx < maxBits; bitIdx++) {
        uint8_t word = bitIdx / 16;
        uint8_t bit  = bitIdx % 16;

        // 74HC597 / CD4021: Read Q before clocking
        for (uint8_t c = 0; c < NUM_CHAINS; c++) {
            if (bitIdx >= chainBitsUsed[c]) continue;
            if (chainDir[c] != CHAIN_DIR_INPUT) continue;
            if (chainType[c] != CHAIN_TYPE_SHIFTREG) continue;
            // MSB-first (CD4021 default): the first bit clocked out is the
            // highest, so store it at the top index and count down — buffer
            // index then equals the physical bit number in the data-chain list.
            uint16_t destBit = chainMsbFirst[c] ? (chainBitsUsed[c] - 1 - bitIdx) : bitIdx;
            uint8_t  dw = destBit / 16;
            uint8_t  db = destBit % 16;
            bool inVal = digitalReadFast(chainDataInPin[c]);
            if (inVal)
                inputBuffer[c][dw] |= (1 << db);
            else
                inputBuffer[c][dw] &= ~(1 << db);
        }

        delayMicroseconds(BIT_TIME_US);

        // Clock HIGH
        for (uint8_t c = 0; c < NUM_CHAINS; c++) {
            if (bitIdx >= chainBitsUsed[c]) continue;
            if (chainDir[c] != CHAIN_DIR_INPUT) continue;
            if (chainType[c] == CHAIN_TYPE_VIRTUAL) continue;
            digitalWriteFast(chainClockPin[c], HIGH);
        }

        delayMicroseconds(BIT_TIME_US);

        // Clock LOW
        for (uint8_t c = 0; c < NUM_CHAINS; c++) {
            if (bitIdx >= chainBitsUsed[c]) continue;
            if (chainDir[c] != CHAIN_DIR_INPUT) continue;
            if (chainType[c] == CHAIN_TYPE_VIRTUAL) continue;
            digitalWriteFast(chainClockPin[c], LOW);
        }

        delayMicroseconds(BIT_TIME_US);

        // Multidrop: Read input data
        for (uint8_t c = 0; c < NUM_CHAINS; c++) {
            if (bitIdx >= chainBitsUsed[c]) continue;
            if (chainDir[c] != CHAIN_DIR_INPUT) continue;
            if (chainType[c] != CHAIN_TYPE_MULTIDROP) continue;
            bool inVal = digitalReadFast(chainDataInPin[c]);
            if (inVal)
                inputBuffer[c][word] |= (1 << bit);
            else
                inputBuffer[c][word] &= ~(1 << bit);
        }
    }
}

// ============================================================
// Shift Out Output Chains (595 only, when changed)
// ============================================================

static void scanOutputChains() {
    if (!outputChanged()) return;
    debugLogOutputBuffer();

    // Serial-SAM output chains: push the coil snapshot as 0xCn frames.
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        if (chainDir[c] != CHAIN_DIR_OUTPUT) continue;
        if (chainType[c] != CHAIN_TYPE_SERIAL_SAM) continue;
        transmitSerialSam(c);
    }

    // Shift-register (595) output chains: bit-bang + latch.
    uint16_t maxBits = 0;
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        if (chainDir[c] != CHAIN_DIR_OUTPUT) continue;
        if (chainType[c] != CHAIN_TYPE_SHIFTREG) continue;
        if (chainBitsUsed[c] > maxBits) maxBits = chainBitsUsed[c];
    }
    if (maxBits > 0) {
        for (uint16_t bitIdx = 0; bitIdx < maxBits; bitIdx++) {
            for (uint8_t c = 0; c < NUM_CHAINS; c++) {
                if (bitIdx >= chainBitsUsed[c]) continue;
                if (chainDir[c] != CHAIN_DIR_OUTPUT || chainType[c] != CHAIN_TYPE_SHIFTREG) continue;
                // MSB-first (CD4094 default): shift the highest bit out first,
                // so buffer bit number matches the physical lamp position.
                uint16_t srcBit = chainMsbFirst[c] ? (chainBitsUsed[c] - 1 - bitIdx) : bitIdx;
                digitalWrite(chainDataOutPin[c], (outputBuffer[c][srcBit / 16] >> (srcBit % 16)) & 1);
            }
            delayMicroseconds(BIT_TIME_US);
            for (uint8_t c = 0; c < NUM_CHAINS; c++) {
                if (bitIdx >= chainBitsUsed[c]) continue;
                if (chainDir[c] != CHAIN_DIR_OUTPUT || chainType[c] != CHAIN_TYPE_SHIFTREG) continue;
                digitalWriteFast(chainClockPin[c], HIGH);
            }
            delayMicroseconds(BIT_TIME_US);
            for (uint8_t c = 0; c < NUM_CHAINS; c++) {
                if (bitIdx >= chainBitsUsed[c]) continue;
                if (chainDir[c] != CHAIN_DIR_OUTPUT || chainType[c] != CHAIN_TYPE_SHIFTREG) continue;
                digitalWriteFast(chainClockPin[c], LOW);
            }
            delayMicroseconds(BIT_TIME_US);
        }
        // Latch: strobe the stored-output register at its active level
        // (74HC595 / CD4094 strobe = active-high).
        for (uint8_t c = 0; c < NUM_CHAINS; c++) {
            if (chainDir[c] != CHAIN_DIR_OUTPUT || chainType[c] != CHAIN_TYPE_SHIFTREG) continue;
            digitalWrite(chainSyncPin[c], chainStrobeActiveHigh[c] ? HIGH : LOW);
        }
        delayMicroseconds(SYNC_PULSE_US);
        for (uint8_t c = 0; c < NUM_CHAINS; c++) {
            if (chainDir[c] != CHAIN_DIR_OUTPUT || chainType[c] != CHAIN_TYPE_SHIFTREG) continue;
            digitalWrite(chainSyncPin[c], chainStrobeActiveHigh[c] ? LOW : HIGH);
        }
        delayMicroseconds(SYNC_SETTLE_US);
    }

    // Save prev for every output chain (serial-SAM + shiftreg) so we only resend on change.
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        if (chainDir[c] != CHAIN_DIR_OUTPUT) continue;
        if (chainType[c] == CHAIN_TYPE_VIRTUAL) continue;
        memcpy(outputBufferPrev[c], outputBuffer[c], sizeof(outputBuffer[c]));
    }
}

// ============================================================
// Core Scan - All Chains
// ============================================================

void scanAllChains() {
    scanInputChains();

    // Apply inversion mask to physical input chains (not virtual)
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        if (chainDir[c] != CHAIN_DIR_INPUT) continue;
        if (chainType[c] == CHAIN_TYPE_VIRTUAL) continue;
        uint8_t words = (chainBitsUsed[c] + 15) / 16;
        for (uint8_t w = 0; w < words; w++) {
            inputBuffer[c][w] ^= inputInvertMask[c][w];
        }
    }

    scanOutputChains();
}

// ============================================================
// Buffer Management
// ============================================================

void saveInputState() {
    memcpy(inputBufferPrev, inputBuffer, sizeof(inputBuffer));
}
