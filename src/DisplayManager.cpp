// DisplayManager.cpp
// Run screen + config screen for the Op62-MVUMC console, on TeensyUserInterface.
//
// Run screen (top to bottom):
//   - Title bar "Op62-MVUMC" with a Config button at its right end.
//   - Memory control band: [-32] [-1]  MEM nnn  [+1] [+32]. The buttons call
//     combinationMemStep(), which wraps 0..255 and persists the level.
//   - Last-general line: the name of the last general piston pressed. Divisional
//     pistons never write lastGeneralName, so only generals show here; GC clears
//     it. When the combination card is unavailable, the error text shows here in
//     yellow instead.
//   - An 4x2 grid of the first 8 screen-stop tabs. Each tab label is up to three
//     lines, taken from screenStopName[] / screenStopNameLine2[] /
//     screenStopNameLine3[] and centred independently, so the strings need no
//     padding spaces. Tabs paint from stopCommandedState[] so a lamp follows the
//     actual stop state. A tab touch writes the stop's virtual chain-3 input bit;
//     the tested processStopInputs() path then does the toggle + MIDI on the next
//     scan.
//
// At most 8 tabs are drawn/handled: if NUM_SCREEN_STOPS is larger the remaining
// screen stops still exist in the config, they just have no touch tab; if it is
// zero the grid is left empty and the run screen is memory status only.
//
// Config screen: a small blocking menu (Calibration + Back), room to grow. While
// it is open, currentScreen == SCREEN_CONFIG and displayScanChainsActive()
// returns false, so the main loop pauses scanning and the blocking screen owns
// the CPU. That is fine during config -- the instrument is not being played.

#include "DisplayManager.h"
#include "Display.h"            // shared ui instance
#include "OrganConfig.h"        // TFT_CS_PIN/TFT_DC_PIN/TOUCH_CS_PIN contract
#include "Debug.h"              // debugPrintTouch()
#include "ScanChain.h"         // inputBuffer + bit-address macros
#include "StopHandler.h"       // stopCommandedState[]
#include "PistonHandler.h"     // lastGeneralName, generalDisplayDirty
#include "Combination.h"       // combinationAvailable/MemoryLevel/ErrorText, combinationMemStep
#include "CombinationConfig.h" // ORGANCORE_HAS_REMAP_STORE (governs the assign-screen include below)
#include "Crescendo.h"         // crescendo overlay level + programming screen API
#include "ExpressionCalScreen.h"
#ifdef ORGANCORE_HAS_REMAP_STORE
#include "PistonAssignScreen.h"   // builder piston assignment (only when the feature is compiled in)
#endif
#include "TuningConfig.h"
#include "TuningScreen.h"

#include <stdio.h>
#include <string.h>

// The one shared UI instance (declared extern in Display.h).
TeensyUserInterface ui;

uint8_t currentScreen = SCREEN_OPERATIONAL;   // set again in displayInit()
bool    displayReady  = false;                // set true at the end of displayInit()

// ------------------------------------------------------------
// Pin assignments for the ILI9341 + XPT2046 (TeensyUserInterface owns the SPI
// bus) come from the OrganConfig.h contract -- TFT_CS_PIN/TFT_DC_PIN/
// TOUCH_CS_PIN, defined per-instrument in ConfigData.cpp -- so each console's
// wiring is sketch data, not a library edit.
// ------------------------------------------------------------

// The ORIENT_* values in CoreConfig.h are mirrors of TeensyUserInterface's, so
// a sketch's ConfigData.cpp needn't include the TUI header. Catch any drift here,
// where both headers are visible.
static_assert((int)ORIENT_PORTRAIT_4PIN_TOP    == LCD_ORIENTATION_PORTRAIT_4PIN_TOP,
              "ORIENT_* drifted from TeensyUserInterface's LCD_ORIENTATION_*");
static_assert((int)ORIENT_LANDSCAPE_4PIN_LEFT  == LCD_ORIENTATION_LANDSCAPE_4PIN_LEFT,
              "ORIENT_* drifted from TeensyUserInterface's LCD_ORIENTATION_*");
static_assert((int)ORIENT_PORTRAIT_4PIN_BOTTOM == LCD_ORIENTATION_PORTRAIT_4PIN_BOTTOM,
              "ORIENT_* drifted from TeensyUserInterface's LCD_ORIENTATION_*");
static_assert((int)ORIENT_LANDSCAPE_4PIN_RIGHT == LCD_ORIENTATION_LANDSCAPE_4PIN_RIGHT,
              "ORIENT_* drifted from TeensyUserInterface's LCD_ORIENTATION_*");

// ------------------------------------------------------------
// Run-screen layout (320x240 landscape -- both landscape orientations give the
// same 320x240 space, so TFT_ORIENTATION only changes which way up it reads)
// ------------------------------------------------------------
static const int SCREEN_W = 320;
static const int SCREEN_H = 240;
static const int TITLE_H  = 32;                  // TUI title bar height (standard 32px; was mistakenly 24, which slid the band under the bar)

// Up to 8 screen stops get a tab, in a 4-wide, 2-tall grid. A console with no
// screen stops at all (NUM_SCREEN_STOPS == 0 -- e.g. one whose stops live on
// the sample engine's own touch page) draws no tabs and never reads
// screenStopIndex[] / screenStopName[], which it then need not populate.
// numTabs is set once in displayInit(); MAX_TABS bounds the grid geometry.
static const int MAX_TABS  = 8;
static const int GRID_COLS = 4;
static const int GRID_ROWS = 2;
static uint8_t   numTabs   = 0;

// Config button: a tappable rect at the right end of the title bar.
static const int CFG_BTN_W = 60;
static const int CFG_BTN_H = TITLE_H - 4;
static const int CFG_BTN_X = SCREEN_W - CFG_BTN_W - 2;
static const int CFG_BTN_Y = 2;

// Memory control band, just below the title bar.
static const int MEM_BAND_Y   = TITLE_H + 2;     // 34 (just below the 32px title bar)
static const int MEM_BAND_H   = 40;              // 26..66
static const int MEM_BTN_Y    = MEM_BAND_Y + 4;  // buttons inset in the band
static const int MEM_BTN_H    = MEM_BAND_H - 8;
static const int MEM_BTN_W    = 50;
static const int MEM_M32_X    = 4;
static const int MEM_M1_X     = MEM_M32_X + MEM_BTN_W + 4;   // 58
static const int MEM_P32_X    = SCREEN_W - MEM_BTN_W - 4;    // 266
static const int MEM_P1_X     = MEM_P32_X - MEM_BTN_W - 4;   // 212
// Readout sits centered between the -1 and +1 buttons.
static const int MEM_READ_X   = MEM_M1_X + MEM_BTN_W;        // 108
static const int MEM_READ_W   = MEM_P1_X - MEM_READ_X;       // 104

// Last-general line, below the memory band.
static const int GEN_Y = MEM_BAND_Y + MEM_BAND_H + 2;        // 76
static const int GEN_H = 20;                                 // 76..96

// Tab grid fills the rest of the screen.
static const int GRID_TOP = GEN_Y + GEN_H;                   // 96
static const int TAB_GAP  = 3;
static const int TAB_LINE_GAP = 2;   // vertical space between tab label lines
static const int TAB_W    = (SCREEN_W - (GRID_COLS + 1) * TAB_GAP) / GRID_COLS;
static const int TAB_H    = (SCREEN_H - GRID_TOP - (GRID_ROWS + 1) * TAB_GAP) / GRID_ROWS;

// Colors (set in displayInit once ui exists).
static uint16_t COLOR_TAB_ON;
static uint16_t COLOR_TAB_OFF;
static uint16_t COLOR_TAB_FRAME;
static uint16_t COLOR_TAB_TEXT_ON;
static uint16_t COLOR_TAB_TEXT_OFF;
static uint16_t COLOR_STATUS_BG;
static uint16_t COLOR_STATUS_TEXT;
static uint16_t COLOR_ERROR_TEXT;

// ------------------------------------------------------------
// Repaint bookkeeping -- the run screen repaints reactively, not every loop,
// so SPI writes don't starve the scan/coil loop.
// ------------------------------------------------------------
static bool    lastTabOn[MAX_STOPS];       // last painted lamp state per tab
static uint16_t lastMemLevel;              // last painted memory level
static bool    lastCombinationAvailable;   // last painted availability
static char    lastPaintedGeneral[8];      // last painted general name
static bool    runScreenNeedsFullPaint;    // force a full repaint (e.g. on entry)
static uint8_t lastPaintedCrescLevel;      // last painted operational crescendo level (0 = none)

// Crescendo programming screen (SCREEN_CRESCENDO): a control band above the same
// 8-tab grid the run screen uses (tab coords are identical), plus a Done button
// in the title bar. Level readout + Down/Up + SET live in the band.
static uint8_t lastPaintedProgLevel;       // last painted programming level
static bool    crescScreenNeedsFullPaint;

static const int CR_BTN_Y   = MEM_BAND_Y;   // 34; align with the run-screen memory band, below the 32px title bar (was hardcoded 30, which overlapped it)
static const int CR_BTN_H   = 40;
static const int CR_DOWN_X  = 4;
static const int CR_DOWN_W  = 44;
static const int CR_READ_X  = 52;
static const int CR_READ_W  = 116;
static const int CR_UP_X    = 172;
static const int CR_UP_W    = 44;
static const int CR_SET_X   = 220;
static const int CR_SET_W   = 96;

// Per-tab pending virtual-bit clear: when a tab is tapped we set its chain-3
// input bit; one loop later we clear it, giving a single clean rising edge
// (one toggle) per tap -- the virtual equivalent of a piston press-and-release.
static bool tabBitPendingClear[MAX_STOPS];

// ------------------------------------------------------------
// Small helpers (multi-call: used by both full and reactive repaint)
// ------------------------------------------------------------

// Geometry of tab t (0..numTabs-1) in row-major order.
static void tabRect(uint8_t t, int& x, int& y, int& w, int& h) {
    int col = t % GRID_COLS;
    int row = t / GRID_COLS;
    x = TAB_GAP + col * (TAB_W + TAB_GAP);
    y = GRID_TOP + TAB_GAP + row * (TAB_H + TAB_GAP);
    w = TAB_W;
    h = TAB_H;
}

// Draw one tab reflecting the stop's commanded (lamp) state.
static void paintTab(uint8_t t) {
    uint16_t stopIdx = screenStopIndex[t];
    bool on = stopCommandedState[stopIdx];

    int x, y, w, h;
    tabRect(t, x, y, w, h);

    uint16_t fill = on ? COLOR_TAB_ON : COLOR_TAB_OFF;
    uint16_t txt  = on ? COLOR_TAB_TEXT_ON : COLOR_TAB_TEXT_OFF;

    ui.lcdDrawFilledRectangle(x, y, w, h, fill);
    ui.lcdDrawRectangle(x, y, w, h, COLOR_TAB_FRAME);

    ui.lcdSetFont(Arial_9_Bold);
    ui.lcdSetFontColor(txt);

    // Up to three label lines, each centred on its own so no padding spaces are
    // needed in the strings. Empty or null lines are skipped and the remaining
    // lines are centred in the tab as a block, so a one-line name sits exactly
    // where it always did.
    const char* labelLine1 = screenStopName[t];
    const char* labelLine2 = screenStopNameLine2[t];
    const char* labelLine3 = screenStopNameLine3[t];

    int lineHeight = ui.lcdGetFontHeightWithoutDecenders() + TAB_LINE_GAP;
    int lineCount = 0;
    if (labelLine1 && labelLine1[0]) lineCount++;
    if (labelLine2 && labelLine2[0]) lineCount++;
    if (labelLine3 && labelLine3[0]) lineCount++;

    int lineY = y + (h - (lineCount * lineHeight - TAB_LINE_GAP)) / 2;

    if (labelLine1 && labelLine1[0]) {
        ui.lcdSetCursorXY(x + w / 2, lineY);
        ui.lcdPrintCentered((char*)labelLine1);
        lineY += lineHeight;
    }
    if (labelLine2 && labelLine2[0]) {
        ui.lcdSetCursorXY(x + w / 2, lineY);
        ui.lcdPrintCentered((char*)labelLine2);
        lineY += lineHeight;
    }
    if (labelLine3 && labelLine3[0]) {
        ui.lcdSetCursorXY(x + w / 2, lineY);
        ui.lcdPrintCentered((char*)labelLine3);
    }

    lastTabOn[t] = on;
}

// Shared tab machinery — the run screen and the crescendo programming screen use
// the same grid coords, so both drive screen stops identically (virtualized
// chain-3 toggle). Kept in one place so the two screens can't drift.

// Clear any virtual bit set on the previous tap (one clean rising edge per tap).
static void retireTabBits() {
    for (uint8_t t = 0; t < numTabs; t++) {
        if (tabBitPendingClear[t]) {
            uint16_t addr = stopSenseAddr[screenStopIndex[t]];
            inputBuffer[ADDR_CHAIN(addr)][ADDR_WORD(addr)] &= ~(1 << ADDR_BIT(addr));
            tabBitPendingClear[t] = false;
        }
    }
}

// A release inside a tab writes that stop's virtual chain-3 input bit for one
// scan; processStopInputs() sees the edge and toggles + sends MIDI.
static bool processTabTouch() {
    for (uint8_t t = 0; t < numTabs; t++) {
        int x, y, w, h;
        tabRect(t, x, y, w, h);
        if (ui.checkForTouchEventInRect(TOUCH_RELEASED_EVENT, x, y, x + w, y + h)) {
            uint16_t addr = stopSenseAddr[screenStopIndex[t]];
            inputBuffer[ADDR_CHAIN(addr)][ADDR_WORD(addr)] |= (1 << ADDR_BIT(addr));
            tabBitPendingClear[t] = true;
            return true;    // one tab per touch
        }
    }
    return false;
}

// Repaint any tab whose lamp (commanded) state changed.
static void repaintChangedTabs() {
    for (uint8_t t = 0; t < numTabs; t++) {
        uint16_t stopIdx = screenStopIndex[t];
        if (stopCommandedState[stopIdx] != lastTabOn[t]) {
            paintTab(t);
        }
    }
}

// Draw a flat button (config + memory controls share this look).
static void paintFlatButton(int x, int y, int w, int h, const char* label) {
    ui.lcdDrawFilledRectangle(x, y, w, h, COLOR_TAB_OFF);
    ui.lcdDrawRectangle(x, y, w, h, COLOR_TAB_FRAME);
    ui.lcdSetFont(Arial_9_Bold);
    ui.lcdSetFontColor(COLOR_TAB_TEXT_OFF);
    ui.lcdSetCursorXY(x + w / 2, y + (h - ui.lcdGetFontHeightWithoutDecenders()) / 2);
    ui.lcdPrintCentered((char*)label);
}

// Draw just the "MEM nnn" readout between the -1 and +1 buttons.
static void paintMemoryLevel() {
    ui.lcdDrawFilledRectangle(MEM_READ_X, MEM_BAND_Y, MEM_READ_W, MEM_BAND_H, COLOR_STATUS_BG);
    char buf[16];
    snprintf(buf, sizeof(buf), "MEM %u", combinationMemoryLevel);
    ui.lcdSetFont(Arial_10_Bold);
    ui.lcdSetFontColor(COLOR_STATUS_TEXT);
    ui.lcdSetCursorXY(MEM_READ_X + MEM_READ_W / 2,
                      MEM_BAND_Y + (MEM_BAND_H - ui.lcdGetFontHeightWithoutDecenders()) / 2);
    ui.lcdPrintCentered(buf);
    lastMemLevel = combinationMemoryLevel;
}

// Draw the last-general line, or the combination error text if unavailable.
static void paintGeneralLine() {
    ui.lcdDrawFilledRectangle(0, GEN_Y, SCREEN_W, GEN_H, COLOR_STATUS_BG);
    ui.lcdSetFont(Arial_10_Bold);
    if (!combinationAvailable && combinationErrorText != NULL) {
        ui.lcdSetFontColor(COLOR_ERROR_TEXT);
        ui.lcdSetCursorXY(6, GEN_Y + 3);
        ui.lcdPrint((char*)combinationErrorText);
    } else {
        ui.lcdSetFontColor(COLOR_STATUS_TEXT);
        ui.lcdSetCursorXY(6, GEN_Y + 3);
        ui.lcdPrint((char*)lastGeneralName);
    }

    // Blind-crescendo indicator: "CRESCENDO nn" in yellow, right-justified on
    // this same line, whenever the crescendo is engaged (level > 0).
    if (crescendoLevel > 0) {
        char cbuf[16];
        snprintf(cbuf, sizeof(cbuf), "CRESCENDO %u", crescendoLevel);
        ui.lcdSetFont(Arial_10_Bold);
        ui.lcdSetFontColor(COLOR_ERROR_TEXT);           // yellow
        ui.lcdSetCursorXY(SCREEN_W - 116, GEN_Y + 3);   // fixed right-side field
        ui.lcdPrint(cbuf);
    }
    lastPaintedCrescLevel = crescendoLevel;

    lastCombinationAvailable = combinationAvailable;
    strncpy(lastPaintedGeneral, lastGeneralName, sizeof(lastPaintedGeneral) - 1);
    lastPaintedGeneral[sizeof(lastPaintedGeneral) - 1] = '\0';
}

// Full run-screen repaint (title, config button, memory band, general, tabs).
static void paintRunScreenFull() {
    ui.drawTitleBar("Op62-MVUMC");
    paintFlatButton(CFG_BTN_X, CFG_BTN_Y, CFG_BTN_W, CFG_BTN_H, "Config");

    // Clear the ENTIRE display space (below the title bar) before painting, so
    // nothing from the previous screen survives in the gaps between the bands and
    // tabs or below the last tab row. Everything below is painted on top.
    ui.lcdDrawFilledRectangle(0, TITLE_H, SCREEN_W, SCREEN_H - TITLE_H, COLOR_STATUS_BG);

    ui.lcdDrawFilledRectangle(0, MEM_BAND_Y, SCREEN_W, MEM_BAND_H, COLOR_STATUS_BG);
    paintFlatButton(MEM_M32_X, MEM_BTN_Y, MEM_BTN_W, MEM_BTN_H, "-32");
    paintFlatButton(MEM_M1_X,  MEM_BTN_Y, MEM_BTN_W, MEM_BTN_H, "-1");
    paintFlatButton(MEM_P1_X,  MEM_BTN_Y, MEM_BTN_W, MEM_BTN_H, "+1");
    paintFlatButton(MEM_P32_X, MEM_BTN_Y, MEM_BTN_W, MEM_BTN_H, "+32");
    paintMemoryLevel();

    paintGeneralLine();

    for (uint8_t t = 0; t < numTabs; t++) {
        paintTab(t);
    }
    runScreenNeedsFullPaint = false;
}

// ------------------------------------------------------------
// Crescendo programming screen (SCREEN_CRESCENDO, non-blocking): a control band
// [Down] LEVEL nn [Up] [SET] above the same 8 screen-stop tabs as the run
// screen, with a Done button in the title bar. Scanning keeps running (the loop
// treats this like the run screen), so physical drawstops and the tabs set the
// registration live; SET stores it to the displayed level. Recall here is NOT
// blind -- it moves commanded state + lamps so a level can be seen and edited.
// ------------------------------------------------------------

// Just the "LEVEL nn" readout (reactive repaint on Up/Down/SET auto-increment).
static void paintCrescLevel() {
    ui.lcdDrawFilledRectangle(CR_READ_X, CR_BTN_Y, CR_READ_W, CR_BTN_H, COLOR_STATUS_BG);
    char buf[16];
    snprintf(buf, sizeof(buf), "LEVEL %u", crescendoProgLevel);
    ui.lcdSetFont(Arial_10_Bold);
    ui.lcdSetFontColor(COLOR_STATUS_TEXT);
    ui.lcdSetCursorXY(CR_READ_X + CR_READ_W / 2,
                      CR_BTN_Y + (CR_BTN_H - ui.lcdGetFontHeightWithoutDecenders()) / 2);
    ui.lcdPrintCentered(buf);
    lastPaintedProgLevel = crescendoProgLevel;
}

static void paintCrescendoScreenFull() {
    ui.drawTitleBar("Crescendo Program");
    paintFlatButton(CFG_BTN_X, CFG_BTN_Y, CFG_BTN_W, CFG_BTN_H, "Done");

    // Clear the ENTIRE display space before painting, so neither the config
    // menu's text nor any prior screen survives in the gaps between the control
    // band and tabs (or in the TAB_GAP margins between/around tabs).
    ui.lcdDrawFilledRectangle(0, TITLE_H, SCREEN_W, SCREEN_H - TITLE_H, COLOR_STATUS_BG);
    paintFlatButton(CR_DOWN_X, CR_BTN_Y, CR_DOWN_W, CR_BTN_H, "-");
    paintFlatButton(CR_UP_X,   CR_BTN_Y, CR_UP_W,   CR_BTN_H, "+");
    paintFlatButton(CR_SET_X,  CR_BTN_Y, CR_SET_W,  CR_BTN_H, "SET");
    paintCrescLevel();

    for (uint8_t t = 0; t < numTabs; t++) paintTab(t);
    crescScreenNeedsFullPaint = false;
}

static void crescendoUpdate() {
    if (crescScreenNeedsFullPaint) {
        paintCrescendoScreenFull();
        return;
    }
    if (crescendoProgLevel != lastPaintedProgLevel) {
        paintCrescLevel();
    }
    repaintChangedTabs();   // stops toggled on this screen update their tab lamps
}

// Touch handling for the crescendo screen (called from displayProcessTouch).
static void crescendoHandleTouch() {
    retireTabBits();
    ui.getTouchEvents();

    if (ui.checkForTouchEventInRect(TOUCH_RELEASED_EVENT,
                                    CFG_BTN_X, CFG_BTN_Y,
                                    CFG_BTN_X + CFG_BTN_W, CFG_BTN_Y + CFG_BTN_H)) {
        crescendoProgExit();
        currentScreen = SCREEN_OPERATIONAL;
        runScreenNeedsFullPaint = true;
        return;
    }
    if (ui.checkForTouchEventInRect(TOUCH_RELEASED_EVENT,
                                    CR_DOWN_X, CR_BTN_Y, CR_DOWN_X + CR_DOWN_W, CR_BTN_Y + CR_BTN_H)) {
        crescendoProgNav(-1);
        return;
    }
    if (ui.checkForTouchEventInRect(TOUCH_RELEASED_EVENT,
                                    CR_UP_X, CR_BTN_Y, CR_UP_X + CR_UP_W, CR_BTN_Y + CR_BTN_H)) {
        crescendoProgNav(+1);
        return;
    }
    if (ui.checkForTouchEventInRect(TOUCH_RELEASED_EVENT,
                                    CR_SET_X, CR_BTN_Y, CR_SET_X + CR_SET_W, CR_BTN_Y + CR_BTN_H)) {
        crescendoProgStore();
        return;
    }
    processTabTouch();
}

// ------------------------------------------------------------
// Config screen: a small blocking menu. Room to grow -- add entries here.
// ------------------------------------------------------------
static void runConfigScreen() {
    while (true) {
        ui.drawTitleBar("Configuration");
        ui.clearDisplaySpace();

        // Entries stacked from the TOP of the display space. TUI buttons are
        // center-anchored, so a button's top edge is centerY - height/2; anchor
        // the first center at displaySpaceTopY + margin + height/2 so it clears
        // the title bar. The count varies with compile-time features (piston
        // assign, tuning) without overlapping.
        const int btnH = 32;
        int   row = 0;
        auto  rowY = [&](int n) { return ui.displaySpaceTopY + 6 + btnH / 2 + n * (btnH + 6); };

        BUTTON calBtn   = { "Expression Calibration",
                            ui.displaySpaceCenterX, rowY(row++), 260, btnH };
        ui.drawButton(calBtn);

        BUTTON crescBtn = { "Crescendo Program",
                            ui.displaySpaceCenterX, rowY(row++), 260, btnH };
        ui.drawButton(crescBtn);

#ifdef ORGANCORE_HAS_REMAP_STORE
        // Same rule as the tuning entry: whether this console offers builder
        // piston assignment is instrument config, so the row appears or doesn't
        // and everything below it shifts up.
        BUTTON assignBtn = { "Assign Pistons", 0, 0, 0, 0 };
        if (PISTON_ASSIGN_ENABLED) {
            assignBtn = BUTTON{ "Assign Pistons",
                                ui.displaySpaceCenterX, rowY(row++), 260, btnH };
            ui.drawButton(assignBtn);
        }
#endif
        // The tuning entry is instrument config, not a build option: a console
        // with no pipes simply doesn't get the row, and the entries below it
        // move up.
        BUTTON tuneBtn = { "Tuning / Temperature", 0, 0, 0, 0 };
        if (ORGAN_TUNING_PRESENT) {
            tuneBtn = BUTTON{ "Tuning / Temperature",
                              ui.displaySpaceCenterX, rowY(row++), 260, btnH };
            ui.drawButton(tuneBtn);
        }
        BUTTON backBtn  = { "Back",
                            ui.displaySpaceCenterX, rowY(row++), 160, btnH };
        ui.drawButton(backBtn);

        bool leaveMenu = false;
        while (!leaveMenu) {
            ui.getTouchEvents();

            if (ui.checkForButtonClicked(calBtn)) {
                expressionCalScreenRun();   // blocking; returns here on Save/Cancel
                break;                       // redraw this menu
            }
            if (ui.checkForButtonClicked(crescBtn)) {
                // Hand off to the NON-blocking crescendo screen: set the state and
                // return so the main loop resumes scanning (drawstops/tabs live).
                currentScreen = SCREEN_CRESCENDO;
                return;
            }
#ifdef ORGANCORE_HAS_REMAP_STORE
            if (PISTON_ASSIGN_ENABLED && ui.checkForButtonClicked(assignBtn)) {
                pistonAssignScreenRun();     // blocking; returns here on Save/Cancel
                break;                        // redraw this menu
            }
#endif
            if (ORGAN_TUNING_PRESENT && ui.checkForButtonClicked(tuneBtn)) {
                tuningScreenRun();           // blocking; returns here on Back
                break;                       // redraw this menu
            }
            if (ui.checkForButtonClicked(backBtn)) {
                leaveMenu = true;
            }
        }

        if (leaveMenu) return;
    }
}

// ------------------------------------------------------------
// Public interface
// ------------------------------------------------------------

void displayForceRepaint() {
    // A blocking screen (e.g. the startup wait) overpainted the run screen;
    // request a full repaint on the next displayUpdate().
    runScreenNeedsFullPaint = true;
}

void displayInit() {
    // Backlight: simple always-on. BACKLIGHT_PIN=255 disables (no backlight
    // control on this instrument), matching the POWER_SUPPLY_PIN sentinel
    // convention. SCREEN1_BACKLIGHT_SECONDS is not used by this simple form;
    // it's left in the contract for a possible future timeout/dim feature.
    if (BACKLIGHT_PIN != 255) {
        pinMode(BACKLIGHT_PIN, OUTPUT);
        digitalWrite(BACKLIGHT_PIN, HIGH);
    }

    // Orientation is instrument config, not a library fact.
    ui.begin(TFT_CS_PIN, TFT_DC_PIN, TOUCH_CS_PIN,
             (int)TFT_ORIENTATION, Arial_9_Bold);

    // Touch inversion, per axis, on top of that orientation. begin() has just
    // loaded TUI's touch calibration for TFT_ORIENTATION, which maps a raw
    // reading as  lcd = raw / scaler - offset. Flipping one axis end-for-end is
    // therefore a negated scaler and a re-derived offset:
    //
    //   (span-1) - (raw/S - O)  ==  raw/(-S) - ( -((span-1) + O) )
    //
    // so the axis reverses with no change to TUI and no touch code of our own.
    // Inverting both axes is exactly a 180-degree touch rotation; inverting one
    // is the mirror an orientation value could never express.
    if (TOUCH_INVERT_X) {
        ui.touchScreenToLCDOffsetX = -((ui.lcdWidth  - 1) + ui.touchScreenToLCDOffsetX);
        ui.touchScreenToLCDScalerX = -ui.touchScreenToLCDScalerX;
    }
    if (TOUCH_INVERT_Y) {
        ui.touchScreenToLCDOffsetY = -((ui.lcdHeight - 1) + ui.touchScreenToLCDOffsetY);
        ui.touchScreenToLCDScalerY = -ui.touchScreenToLCDScalerY;
    }
    ui.setColorPaletteGray();

    COLOR_TAB_ON       = ui.lcdMakeColor(6, 40, 10);    // lit green
    COLOR_TAB_OFF      = ui.lcdMakeColor(6, 12, 6);     // dim
    COLOR_TAB_FRAME    = LCD_LIGHTGREY;
    COLOR_TAB_TEXT_ON  = LCD_WHITE;
    COLOR_TAB_TEXT_OFF = LCD_LIGHTGREY;
    COLOR_STATUS_BG    = LCD_BLACK;
    COLOR_STATUS_TEXT  = LCD_WHITE;
    COLOR_ERROR_TEXT   = LCD_YELLOW;

    numTabs = (NUM_SCREEN_STOPS < MAX_TABS) ? NUM_SCREEN_STOPS : (uint8_t)MAX_TABS;

    for (uint16_t i = 0; i < MAX_STOPS; i++) {
        lastTabOn[i] = false;
        tabBitPendingClear[i] = false;
    }
    lastMemLevel = 0xFFFF;              // force first readout paint (beyond any real level)
    lastCombinationAvailable = true;
    lastPaintedGeneral[0] = '\0';

    // Go straight to the operational run screen.
    currentScreen = SCREEN_OPERATIONAL;
    runScreenNeedsFullPaint = true;
    paintRunScreenFull();

    displayReady = true;
}

void displayUpdate() {
    if (currentScreen == SCREEN_CRESCENDO) {
        crescendoUpdate();
        return;
    }
    // The config screen is blocking and paints itself; nothing to do here while
    // it owns the loop.
    if (currentScreen != SCREEN_OPERATIONAL) return;

    if (runScreenNeedsFullPaint) {
        paintRunScreenFull();
        return;
    }

    // Reactive repaint: only redraw what changed.

    // Memory readout: level changed (e.g. a -20/-1/+1/+20 tap).
    if (combinationMemoryLevel != lastMemLevel) {
        paintMemoryLevel();
    }

    // General line: availability, general name, the dirty flag, or the crescendo
    // indicator level changed.
    if (combinationAvailable != lastCombinationAvailable ||
        generalDisplayDirty ||
        crescendoLevel != lastPaintedCrescLevel ||
        strncmp(lastGeneralName, lastPaintedGeneral, sizeof(lastPaintedGeneral)) != 0) {
        paintGeneralLine();
        generalDisplayDirty = false;
    }

    // Tabs: repaint any whose lamp state (commanded) changed -- this covers both
    // a local toggle and a combination recall, since both land in
    // stopCommandedState[].
    repaintChangedTabs();
}

void displayProcessTouch() {
    if (currentScreen == SCREEN_CRESCENDO) {
        crescendoHandleTouch();
        return;
    }
    if (currentScreen != SCREEN_OPERATIONAL) return;

    // Retire any virtual bits set on the previous tap (one clean rising edge).
    retireTabBits();

    ui.getTouchEvents();

#if DEBUG_ENABLED
    // ui.touchEventType/touchEventX/touchEventY are public members TUI sets
    // inside getTouchEvents() -- confirmed against TeensyUserInterface.h.
    // touchEventX/Y are already LCD/screen-pixel coordinates (TUI applies its
    // calibration constants before setting these), not raw ADC counts.
    if (ui.touchEventType == TOUCH_PUSHED_EVENT) {
        debugPrintTouch(ui.touchEventX, ui.touchEventY, false);
    } else if (ui.touchEventType == TOUCH_RELEASED_EVENT) {
        debugPrintTouch(ui.touchEventX, ui.touchEventY, true);
    }
#endif

    // Config button (title bar): enter the config menu. It is blocking; on Back
    // it returns here and we repaint the run screen. If the menu handed off to
    // the crescendo screen, it set currentScreen = SCREEN_CRESCENDO -- honor that
    // and paint the crescendo screen instead of forcing OPERATIONAL.
    if (ui.checkForTouchEventInRect(TOUCH_RELEASED_EVENT,
                                    CFG_BTN_X, CFG_BTN_Y,
                                    CFG_BTN_X + CFG_BTN_W, CFG_BTN_Y + CFG_BTN_H)) {
        currentScreen = SCREEN_CONFIG;    // pauses scanning via displayScanChainsActive()
        runConfigScreen();                // blocking menu (may set SCREEN_CRESCENDO)
        if (currentScreen == SCREEN_CRESCENDO) {
            crescendoProgEnter();
            crescScreenNeedsFullPaint = true;
        } else {
            currentScreen = SCREEN_OPERATIONAL;
            runScreenNeedsFullPaint = true;   // repaint on next displayUpdate()
        }
        return;
    }

    // Memory control buttons: step the level (combinationMemStep wraps + persists).
    if (ui.checkForTouchEventInRect(TOUCH_RELEASED_EVENT,
                                    MEM_M32_X, MEM_BTN_Y,
                                    MEM_M32_X + MEM_BTN_W, MEM_BTN_Y + MEM_BTN_H)) {
        combinationMemStep(-32);
        return;
    }
    if (ui.checkForTouchEventInRect(TOUCH_RELEASED_EVENT,
                                    MEM_M1_X, MEM_BTN_Y,
                                    MEM_M1_X + MEM_BTN_W, MEM_BTN_Y + MEM_BTN_H)) {
        combinationMemStep(-1);
        return;
    }
    if (ui.checkForTouchEventInRect(TOUCH_RELEASED_EVENT,
                                    MEM_P1_X, MEM_BTN_Y,
                                    MEM_P1_X + MEM_BTN_W, MEM_BTN_Y + MEM_BTN_H)) {
        combinationMemStep(1);
        return;
    }
    if (ui.checkForTouchEventInRect(TOUCH_RELEASED_EVENT,
                                    MEM_P32_X, MEM_BTN_Y,
                                    MEM_P32_X + MEM_BTN_W, MEM_BTN_Y + MEM_BTN_H)) {
        combinationMemStep(32);
        return;
    }

    // Screen-stop tabs: a release writes that stop's virtual chain-3 bit for one
    // scan; processStopInputs() sees the edge and toggles + sends MIDI.
    processTabTouch();
}

bool displayPowerShouldBeOn() {
    // Console power supply stays on whenever the display module is running.
    return true;
}

bool displayScanChainsActive() {
    // Scanning runs on the operational run screen AND the crescendo programming
    // screen (which is non-blocking and needs live drawstops/tabs). The config
    // menu is blocking, so we return false there and the main loop skips
    // scanning while the config UI owns the CPU.
    return currentScreen == SCREEN_OPERATIONAL || currentScreen == SCREEN_CRESCENDO;
}
