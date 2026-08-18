// Debug.cpp
// USB Serial debug output

#include "Debug.h"

#if DEBUG_ENABLED

// Saved copy of outputBuffer for debug change detection
static uint16_t debugOutputPrev[MAX_CHAINS][WORDS_PER_CHAIN];

// ============================================================
// Helpers
// ============================================================

// Print a 16-bit word as 4 hex digits
static void printHex16(uint16_t val) {
    if (val < 0x1000) Serial.print('0');
    if (val < 0x0100) Serial.print('0');
    if (val < 0x0010) Serial.print('0');
    Serial.print(val, HEX);
}

// Print a CWB address as 0xCWB
static void printCWB(uint8_t chain, uint8_t word, uint8_t bit) {
    Serial.print("0x");
    Serial.print(chain, HEX);
    Serial.print(word, HEX);
    Serial.print(bit, HEX);

}

// Number of 16-bit words needed for a chain
static uint8_t wordsForChain(uint8_t c) {
    return (chainBitsUsed[c] + 15) / 16;
}

// ============================================================
// Initialization
// ============================================================

void debugInit() {
    Serial.println("=== Organ Controller Debug ===");
    Serial.print("Chains: ");
    Serial.println(NUM_CHAINS);
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        Serial.print("  Chain ");
        Serial.print(c);
        Serial.print(": ");
        Serial.print(chainBitsUsed[c]);
        Serial.print(" bits, type=");
        Serial.print(chainType[c] == CHAIN_TYPE_MULTIDROP ? "MULTIDROP" : "SHIFTREG");
        Serial.print(", dir=");
        Serial.print(chainDir[c] == CHAIN_DIR_INPUT ? "IN" :
                       chainDir[c] == CHAIN_DIR_OUTPUT ? "OUT" : "BIDIR");
        Serial.print(chainDataInPin[c],DEC);Serial.print(" ");Serial.print(chainDataOutPin[c],DEC);Serial.print(" ");Serial.print(chainClockPin[c],DEC);Serial.print(" ");Serial.print(chainSyncPin[c],DEC);Serial.println(" ");
    }
    Serial.print("Stops: ");   Serial.println(NUM_STOPS);
    Serial.print("Keyboards: "); Serial.println(NUM_KEYBOARDS);
    Serial.println("==============================");
    
    memset(debugOutputPrev, 0, sizeof(debugOutputPrev));
}

// ============================================================
// Print Full Input Buffer
// ============================================================

void debugPrintInputs() {
    Serial.println("--- inputBuffer ---");
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        uint8_t words = wordsForChain(c);
        Serial.print("  C");
        Serial.print(c);
        Serial.print(": ");
        for (uint8_t w = 0; w < words; w++) {
            printHex16(inputBuffer[c][w]);
            Serial.print(' ');
        }
        Serial.println();
    }
}

// ============================================================
// Print Full Output Buffer
// ============================================================

void debugPrintOutputs() {
    Serial.println("--- outputBuffer ---");
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        if (chainDir[c] == CHAIN_DIR_INPUT) continue;  // Skip input-only chains
        uint8_t words = wordsForChain(c);
        Serial.print("  C");
        Serial.print(c);
        Serial.print(": ");
        for (uint8_t w = 0; w < words; w++) {
            printHex16(outputBuffer[c][w]);
            Serial.print(' ');
        }
        Serial.println();
    }
}

// ============================================================
// Report Changed Input Bits
// ============================================================

void debugReportChanges() {
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        if (chainDir[c] == CHAIN_DIR_OUTPUT) continue;  // Skip output-only chains
        uint8_t words = wordsForChain(c);
        for (uint8_t w = 0; w < words; w++) {
            uint16_t diff = inputBuffer[c][w] ^ inputBufferPrev[c][w];
            if (diff == 0) continue;
            
            for (uint8_t b = 0; b < 16; b++) {
                if ((diff >> b) & 1) {
                    bool newVal = (inputBuffer[c][w] >> b) & 1;
                    Serial.print("DBG: Input changed ");
                    printCWB(c, w, b);
                    Serial.print(" -> ");
                    Serial.println(newVal ? "HIGH" : "LOW");
                    debugPrintInputs();
                }
            }
        }
    }
}

// ============================================================
// Detect and Print Output Buffer Changes
// ============================================================

void debugCheckOutputChanges() {
    bool changed = false;
    
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        if (chainDir[c] == CHAIN_DIR_INPUT) continue;
        uint8_t words = wordsForChain(c);
        for (uint8_t w = 0; w < words; w++) {
            if (outputBuffer[c][w] != debugOutputPrev[c][w]) {
                if (!changed) {
                    Serial.println("DBG: outputBuffer changed:");
                    changed = true;
                }
                // Report individual bit changes
                uint16_t diff = outputBuffer[c][w] ^ debugOutputPrev[c][w];
                for (uint8_t b = 0; b < 16; b++) {
                    if ((diff >> b) & 1) {
                        bool newVal = (outputBuffer[c][w] >> b) & 1;
                        Serial.print("  ");
                        printCWB(c, w, b);
                        Serial.print(" -> ");
                        Serial.println(newVal ? "ON" : "OFF");
                    }
                }
            }
        }
    }
    
    if (changed) {
        debugPrintOutputs();
    }
    
    // Save current output state for next comparison
    memcpy(debugOutputPrev, outputBuffer, sizeof(outputBuffer));
}

#endif // DEBUG_ENABLED
