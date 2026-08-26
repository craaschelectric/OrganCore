// InputRemap.h
// Input remap: merges duplicate physical inputs into canonical addresses.
// Applied after scanning (including virtual chain injection), before any handler.
// Each remap OR's the "from" bit into the "to" bit and clears "from",
// in both inputBuffer and inputBufferPrev so edge detection works correctly.

#ifndef INPUT_REMAP_H
#define INPUT_REMAP_H

#include "OrganCore.h"
#include "ScanChain.h"
#include "CombinationConfig.h"
#include "RemapStore.h"   // defines ORGANCORE_HAS_REMAP_STORE when the live store is compiled

// Source selection: in local-capture (SD) mode, once a valid REMAP.DAT has been
// loaded, the builder-assigned live table is the remap source. Otherwise (HW
// mode, or SD mode with no file yet) the const remapFrom[]/remapTo[] from
// OrganConfig.h are used. The merge logic below is identical for both — only the
// array pointers and count differ.
inline void applyRemaps() {
#ifdef ORGANCORE_HAS_REMAP_STORE
    const bool      remapLive = remapSourceIsLive();
    const uint16_t* fromArr   = remapLive ? liveRemapFrom : remapFrom;
    const uint16_t* toArr     = remapLive ? liveRemapTo   : remapTo;
    const uint16_t  remapN    = remapLive ? liveNumRemaps : NUM_REMAPS;
#else
    const uint16_t* fromArr   = remapFrom;
    const uint16_t* toArr     = remapTo;
    const uint16_t  remapN    = NUM_REMAPS;
#endif

    for (uint16_t i = 0; i < remapN; i++) {
        uint16_t from = fromArr[i];
        uint16_t to   = toArr[i];
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
