// InputRemap.h
// Input remap: merges duplicate physical inputs into canonical addresses.
// Applied after scanning (including virtual chain injection), before any handler.
// Each remap OR's the "from" bit into the "to" bit and clears "from",
// in both inputBuffer and inputBufferPrev so edge detection works correctly.

#ifndef INPUT_REMAP_H
#define INPUT_REMAP_H

#include "OrganCore.h"
#include "ScanChain.h"

inline void applyRemaps() {
    for (uint8_t i = 0; i < NUM_REMAPS; i++) {
        uint16_t from = remapFrom[i];
        uint16_t to   = remapTo[i];
        if (!ADDR_VALID(from) || !ADDR_VALID(to)) continue;

        uint8_t fChain = ADDR_CHAIN(from);
        uint8_t fWord  = ADDR_WORD(from);
        uint8_t fBit   = ADDR_BIT(from);
        uint8_t tChain = ADDR_CHAIN(to);
        uint8_t tWord  = ADDR_WORD(to);
        uint8_t tBit   = ADDR_BIT(to);

        // If the source bit is set, set the target and clear the source.
        // Both current and previous buffers must be handled so edge detection works.
        if ((inputBuffer[fChain][fWord] >> fBit) & 1) {
            inputBuffer[tChain][tWord] |= (1 << tBit);
            inputBuffer[fChain][fWord] &= ~(1 << fBit);
        }
        if ((inputBufferPrev[fChain][fWord] >> fBit) & 1) {
            inputBufferPrev[tChain][tWord] |= (1 << tBit);
            inputBufferPrev[fChain][fWord] &= ~(1 << fBit);
        }
    }
}

#endif // INPUT_REMAP_H
