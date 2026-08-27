// PistonAssignSlots.h
// Pure slot-cursor logic for the builder piston-assignment screen. No TUI, no
// hardware -- host-testable. Walks the frozen virtual-slot regions and maps the
// cursor to a canonical slot address on REMAP_SLOT_CHAIN.
//
// Regions, in walk order:
//   REGION_KNOWN_CTRL : REMAP_KNOWN_CONTROLS slots  (bit REMAP_SLOT_CTRL_BASE+i)
//   REGION_GENERAL    : NUM_GENERALS slots          (bit REMAP_SLOT_GEN_BASE+i)
//   REGION_DIVISIONAL : NUM_DIVISIONS*REMAP_PISTONS_PER_DIV slots
//                                                    (bit REMAP_SLOT_DIV_BASE+i)
//   REGION_SPARE_CTRL : REMAP_SPARE_CONTROLS slots  (bit REMAP_SLOT_SPARE_BASE+i)
//
// Next-function advances one slot within the walkable set, crossing region and
// division boundaries linearly. Next-block jumps to the first slot of the next
// block: within divisionals, the next DIVISION; otherwise the next region. Both
// clamp at the final slot (the last spare control) -- there is nowhere past it.

#ifndef PISTON_ASSIGN_SLOTS_H
#define PISTON_ASSIGN_SLOTS_H

#include "OrganCore.h"

enum AssignRegion : uint8_t {
    REGION_KNOWN_CTRL = 0,
    REGION_GENERAL    = 1,
    REGION_DIVISIONAL = 2,
    REGION_SPARE_CTRL = 3
};

struct AssignCursor {
    uint8_t  region;         // AssignRegion
    uint16_t indexInRegion;  // 0-based slot within the region
};

// ---- Region sizes for THIS instrument (clamped to the frozen caps) ----
inline uint16_t regionCount(uint8_t region) {
    switch (region) {
        case REGION_KNOWN_CTRL: return REMAP_KNOWN_CONTROLS;
        case REGION_GENERAL:    return (NUM_GENERALS <= REMAP_MAX_GENERALS)
                                       ? NUM_GENERALS : REMAP_MAX_GENERALS;
        case REGION_DIVISIONAL: {
            uint16_t divs = (NUM_DIVISIONS <= REMAP_DIVISIONS)
                            ? NUM_DIVISIONS : REMAP_DIVISIONS;
            return divs * REMAP_PISTONS_PER_DIV;
        }
        case REGION_SPARE_CTRL: return REMAP_SPARE_CONTROLS;
        default: return 0;
    }
}

// Slot bit offset (within REMAP_SLOT_CHAIN) for a region's index-0 slot.
inline uint16_t regionBase(uint8_t region) {
    switch (region) {
        case REGION_KNOWN_CTRL: return REMAP_SLOT_CTRL_BASE;
        case REGION_GENERAL:    return REMAP_SLOT_GEN_BASE;
        case REGION_DIVISIONAL: return REMAP_SLOT_DIV_BASE;
        case REGION_SPARE_CTRL: return REMAP_SLOT_SPARE_BASE;
        default: return 0;
    }
}

// The canonical virtual address the cursor currently points at.
inline uint16_t assignCursorSlotAddr(const AssignCursor* c) {
    uint16_t bit = regionBase(c->region) + c->indexInRegion;
    return (uint16_t)MAKE_ADDR(REMAP_SLOT_CHAIN, bit >> 4, bit & 0x0F);
}

inline void assignCursorInit(AssignCursor* c) {
    c->region = REGION_KNOWN_CTRL;
    c->indexInRegion = 0;
    // A region could be empty on a degenerate instrument (e.g. 0 generals);
    // skip forward to the first non-empty region.
    while (c->region <= REGION_SPARE_CTRL && regionCount(c->region) == 0) {
        c->region++;
        c->indexInRegion = 0;
    }
}

// True if the cursor is on the very last walkable slot (nothing beyond).
inline bool assignCursorAtEnd(const AssignCursor* c) {
    // last region with any slots:
    uint8_t lastRegion = REGION_SPARE_CTRL;
    while (lastRegion > 0 && regionCount(lastRegion) == 0) lastRegion--;
    return c->region == lastRegion && c->indexInRegion + 1 >= regionCount(lastRegion);
}

// Advance one slot. Crosses region boundaries; stops at the final slot.
inline void assignCursorNextFunction(AssignCursor* c) {
    if (c->indexInRegion + 1 < regionCount(c->region)) {
        c->indexInRegion++;
        return;
    }
    // move to the next non-empty region
    uint8_t r = c->region;
    while (r < REGION_SPARE_CTRL) {
        r++;
        if (regionCount(r) > 0) { c->region = r; c->indexInRegion = 0; return; }
    }
    // already in the last region: clamp to its last slot
    uint16_t n = regionCount(c->region);
    if (n > 0) c->indexInRegion = n - 1;
}

// Jump to the head of the next block. Within divisionals, the next DIVISION;
// otherwise the head of the next non-empty region. Clamps at the end.
inline void assignCursorNextBlock(AssignCursor* c) {
    if (c->region == REGION_DIVISIONAL) {
        uint8_t curDiv = c->indexInRegion / REMAP_PISTONS_PER_DIV;
        uint16_t divCount = regionCount(REGION_DIVISIONAL) / REMAP_PISTONS_PER_DIV;
        if (curDiv + 1 < divCount) {
            c->indexInRegion = (uint16_t)(curDiv + 1) * REMAP_PISTONS_PER_DIV;
            return;
        }
        // past the last division -> fall through to next region (spare controls)
    }
    uint8_t r = c->region;
    while (r < REGION_SPARE_CTRL) {
        r++;
        if (regionCount(r) > 0) { c->region = r; c->indexInRegion = 0; return; }
    }
    // no further block: clamp to last slot of current region
    uint16_t n = regionCount(c->region);
    if (n > 0) c->indexInRegion = n - 1;
}

#endif // PISTON_ASSIGN_SLOTS_H
