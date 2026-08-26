// ============================================================
// PistonAssignScreen.cpp - TUI builder piston-assignment screen
// ============================================================
// NOTE: TeensyUserInterface is not compilable in the host harness; verify font
// names and method signatures against your TUI version on the Teensy (same
// caveat as ExpressionCalScreen.cpp). The slot-cursor logic lives in the pure,
// host-tested header PistonAssignSlots.h; only the ui.* and scan calls here are
// Teensy-only.
//
// Compiled only in local-capture mode (ORGAN_COMBINATION_MODE == COMBINATION_MODE_SD).
//
// The builder parks on a logical function (Set, General 12, "Great piston 3",
// ...) and presses the physical piston(s) that should trigger it. Each press is
// captured on its rising edge and remapped ONTO the current slot's canonical
// virtual address via the RemapStore. Presses are captured from ALL input
// chains, including CHAIN_TYPE_VIRTUAL (a MIDI pedalboard's embedded pistons
// report through a virtual chain and must be assignable) — only the reserved
// slot chain (REMAP_SLOT_CHAIN) is excluded, being a destination, never a source.
//
// Blocking by design — reached from the config menu while the main scan loop is
// paused, so this screen pumps scanAllChains() + serialMidiProcess() itself.

#include "PistonAssignScreen.h"
#ifdef ORGANCORE_HAS_REMAP_STORE

#include "OrganCore.h"
#include "ScanChain.h"
#include "SerialMidi.h"
#include "RemapStore.h"
#include "PistonAssignSlots.h"
#include "Display.h"            // shared ui instance

#include <stdio.h>
#include <string.h>

// Known-control display names, indexed by REMAP_CTRL_* (0..6).
static const char* const kKnownCtrlName[REMAP_KNOWN_CONTROLS] = {
    "Set", "General Cancel", "Next", "Previous", "Mem +", "Mem -", "Shift"
};

// Division display names by division number (frozen order: 0 Pedal, 1 Great,
// 2 Swell, 3 Choir, 4 Solo, then generic).
static const char* divisionName(uint8_t d) {
    switch (d) {
        case 0: return "Pedal";
        case 1: return "Great";
        case 2: return "Swell";
        case 3: return "Choir";
        case 4: return "Solo";
        default: return "Division";
    }
}

// Build the on-screen label for the current cursor into buf.
static void slotLabel(const AssignCursor& cur, char* buf, size_t n) {
    switch (cur.region) {
        case REGION_KNOWN_CTRL:
            snprintf(buf, n, "%s", kKnownCtrlName[cur.indexInRegion]);
            break;
        case REGION_GENERAL:
            snprintf(buf, n, "General %u", (unsigned)(cur.indexInRegion + 1));
            break;
        case REGION_DIVISIONAL: {
            uint8_t d = cur.indexInRegion / REMAP_PISTONS_PER_DIV;
            uint8_t p = cur.indexInRegion % REMAP_PISTONS_PER_DIV;
            snprintf(buf, n, "%s piston %u", divisionName(d), (unsigned)(p + 1));
            break;
        }
        case REGION_SPARE_CTRL:
            snprintf(buf, n, "Spare control %u", (unsigned)(cur.indexInRegion + 1));
            break;
        default:
            snprintf(buf, n, "?");
            break;
    }
}

// Last-seen input state for edge detection + per-address release-debounce. A set
// bit means "this input was down last look and has not yet released" — we only
// capture on a low->high transition. Kept separate from inputBufferPrev so this
// screen's own debounce never fights the scan pipeline's edge handling.
static uint16_t assignPrev[MAX_CHAINS][WORDS_PER_CHAIN];

static bool chainIsCapturable(uint8_t c) {
    if (c == REMAP_SLOT_CHAIN) return false;          // destination chain, never a source
    if (chainDir[c] != CHAIN_DIR_INPUT) return false; // outputs are not pistons
    return true;                                       // hardware AND virtual inputs both capture
}

// Scan for the first newly-pressed input bit. Returns its CWB address, or
// ADDR_DISABLED if none. Updates assignPrev so a held button captures once.
static uint16_t captureFirstPress() {
    uint16_t hit = ADDR_DISABLED;
    for (uint8_t c = 0; c < NUM_CHAINS; c++) {
        if (!chainIsCapturable(c)) continue;
        uint16_t words = (uint16_t)((chainBitsUsed[c] + 15) / 16);
        for (uint16_t w = 0; w < words; w++) {
            uint16_t now    = inputBuffer[c][w];
            uint16_t prev   = assignPrev[c][w];
            uint16_t rising = now & ~prev;   // bits that went 0->1 this look
            assignPrev[c][w] = now;          // release-debounce: track current state
            if (rising && hit == ADDR_DISABLED) {
                for (uint8_t b = 0; b < 16; b++) {
                    if (rising & (1u << b)) { hit = MAKE_ADDR(c, w, b); break; }
                }
            }
        }
    }
    return hit;
}

// Count live remaps whose "to" is the given slot address (on-screen tally).
static uint16_t countForSlot(uint16_t slotAddr) {
    uint16_t n = 0;
    for (uint16_t i = 0; i < liveNumRemaps; i++)
        if (liveRemapTo[i] == slotAddr) n++;
    return n;
}

void pistonAssignScreenRun() {
    // Edit the live table in place; snapshot it so Cancel can restore. The
    // store's in-RAM ops mutate liveRemap* directly.
    static uint16_t savedFrom[MAX_REMAPS];
    static uint16_t savedTo[MAX_REMAPS];
    uint16_t savedCount = liveNumRemaps;
    memcpy(savedFrom, liveRemapFrom, sizeof(uint16_t) * savedCount);
    memcpy(savedTo,   liveRemapTo,   sizeof(uint16_t) * savedCount);

    memset(assignPrev, 0, sizeof(assignPrev));

    AssignCursor cur;
    assignCursorInit(&cur);

    ui.drawTitleBar("Assign Pistons");
    ui.clearDisplaySpace();

    // Two rows of three buttons along the bottom, matching runConfigScreen style.
    BUTTON nextFnBtn  = { "Next",       75, ui.displaySpaceBottomY - 58, 130, 34 };
    BUTTON nextBlkBtn = { "Next Block", 215, ui.displaySpaceBottomY - 58, 130, 34 };
    BUTTON clearBtn   = { "Clear",      285, ui.displaySpaceBottomY - 58, 100, 34 };
    BUTTON startBtn   = { "Start Over", 75,  ui.displaySpaceBottomY - 18, 130, 34 };
    BUTTON saveBtn    = { "Save",       215, ui.displaySpaceBottomY - 18, 100, 34 };
    BUTTON cancelBtn  = { "Cancel",     315, ui.displaySpaceBottomY - 18, 100, 34 };
    ui.drawButton(nextFnBtn); ui.drawButton(nextBlkBtn); ui.drawButton(clearBtn);
    ui.drawButton(startBtn);  ui.drawButton(saveBtn);    ui.drawButton(cancelBtn);

    char lastLine[72] = "";
    while (true) {
        // Pump both input sources so hardware and virtual-chain presses land.
        scanAllChains();
        serialMidiProcess();
        ui.getTouchEvents();

        uint16_t slotAddr = assignCursorSlotAddr(&cur);

        // Capture a physical press onto the current slot.
        uint16_t pressed = captureFirstPress();
        if (ADDR_VALID(pressed) && pressed != slotAddr) {
            char lbl[40]; slotLabel(cur, lbl, sizeof(lbl));
            if (remapAdd(pressed, slotAddr)) {
                Serial.print("DBG: assign ");
                Serial.print(lbl);
                Serial.print("  <- CWB ");
                Serial.print(ADDR_CHAIN(pressed)); Serial.print('/');
                Serial.print(ADDR_WORD(pressed));  Serial.print('/');
                Serial.print(ADDR_BIT(pressed));
                Serial.print("  (0x"); Serial.print(pressed, HEX); Serial.println(")");
            } else {
                Serial.println("DBG: assign rejected (table full or invalid)");
            }
        }

        // Live label + count, repainted only on change.
        char base[40]; slotLabel(cur, base, sizeof(base));
        char line[72];
        snprintf(line, sizeof(line), "%-22s buttons: %u",
                 base, (unsigned)countForSlot(slotAddr));
        if (strcmp(line, lastLine) != 0) {
            ui.lcdDrawFilledRectangle(6, 40, ui.displaySpaceWidth - 12, 28, LCD_BLACK);
            ui.lcdSetFontColor(LCD_WHITE);
            ui.lcdSetCursorXY(6, 44);
            ui.lcdPrint(line);
            strcpy(lastLine, line);
        }

        if (ui.checkForButtonClicked(nextFnBtn)) {
            assignCursorNextFunction(&cur);
            memset(assignPrev, 0, sizeof(assignPrev));   // fresh debounce on move
        }
        if (ui.checkForButtonClicked(nextBlkBtn)) {
            assignCursorNextBlock(&cur);
            memset(assignPrev, 0, sizeof(assignPrev));
        }
        if (ui.checkForButtonClicked(clearBtn)) {
            remapClearSlot(slotAddr);
            lastLine[0] = '\0';   // force count redraw
        }
        if (ui.checkForButtonClicked(startBtn)) {
            remapClearAll();
            lastLine[0] = '\0';
        }
        if (ui.checkForButtonClicked(saveBtn)) {
            remapStoreSave();
            return;
        }
        if (ui.checkForButtonClicked(cancelBtn)) {
            memcpy(liveRemapFrom, savedFrom, sizeof(uint16_t) * savedCount);
            memcpy(liveRemapTo,   savedTo,   sizeof(uint16_t) * savedCount);
            liveNumRemaps = savedCount;
            return;
        }
    }
}

#endif // ORGANCORE_HAS_REMAP_STORE
