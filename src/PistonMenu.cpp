// PistonMenu.cpp  -  piston-driven config menu and screens. See PistonMenu.h.
//
// Drawn with the shared TeensyUserInterface 'ui' (the same calls the Opus 57
// DisplayLocal module used on hardware). Pixel positions are first-cut; nudge
// them once seen on a panel.

#include "PistonMenu.h"
#include "Display.h"               // ui, fonts
#include "DisplayManager.h"        // currentScreen
#include "ScanChain.h"             // scanAllChains, saveInputState, readInput, inputChanged
#include "InputRemap.h"            // applyRemaps
#include "PistonHandler.h"         // setHeld
#include "StopHandler.h"           // processStopInputs, buildStopOutputs, updateCoilPulses, checkStopRetries
#include "Crescendo.h"             // crescendoAvailable, crescendoProgLevel, crescendoProg*
#include "ExpressionCalibration.h" // calibratedExprMin/Max, expressionCalibrationSave
#include "TuningConfig.h"
#include "PitchManager.h"          // pitchManagerPoll, manual trim, readouts
#include "TempSensor.h"            // tempSensorPoll, getTempDegC
#include "OrganPower.h"            // powerPoll
#include "OrganDiag.h"             // diagPoll, diagReadLog, diagBootNumber
#include "CombinationConfig.h"     // ORGAN_COMBINATION_MODE
#include "MidiOut.h"               // midiSendNoteOff (HW mode SET release)

#include <stdio.h>
#include <string.h>

// ============================================================
// Control pistons, looked up once by type in pistonMenuInit()
// ============================================================
static uint16_t setAddr    = ADDR_DISABLED;
static uint8_t  setPistonIndex = 0xFF;   // for the HW-mode SET note-off on exit
static uint16_t cancelAddr = ADDR_DISABLED;

// List movement prefers MEM+/MEM-; value stepping prefers NEXT/PREV. Each
// falls back to the other pair when its own piston is missing.
static uint16_t listForwardAddr = ADDR_DISABLED;
static uint16_t listBackAddr    = ADDR_DISABLED;
static uint16_t stepUpAddr      = ADDR_DISABLED;
static uint16_t stepDownAddr    = ADDR_DISABLED;

// Names of whichever pistons ended up in each role, for the on-screen hints.
static const char* listForwardName = "MEM+";
static const char* listBackName    = "MEM-";
static const char* stepUpName      = "NEXT";
static const char* stepDownName    = "PREV";

// Press edge: changed this scan AND now down.
static bool pressEdge(uint16_t addr) {
    return ADDR_VALID(addr) && inputChanged(addr) && readInput(addr);
}

// The start of every pass of every blocking screen here: fresh inputs, the
// duplicate-button merge (so a toe stud for NEXT works here too), and everything
// loop() would otherwise have kept alive -- the power switch, Hauptwerk/GO
// messages (stop reports, pitch reports), the temperature reading, and the
// pitch loop's note-offs and retries.
static void beginPass() {
    scanAllChains();
    applyRemaps();
    powerPoll();
    usbMIDI.read();
    tempSensorPoll();
    pitchManagerPoll();
    diagPoll(false);   // USB link monitor + serial dump; display re-init waits for the run screen
}

// ============================================================
// Init
// ============================================================
void pistonMenuInit() {
    uint16_t memUpAddr   = ADDR_DISABLED;
    uint16_t memDownAddr = ADDR_DISABLED;
    uint16_t nextAddr    = ADDR_DISABLED;
    uint16_t prevAddr    = ADDR_DISABLED;

    // First piston of each type wins. Duplicates (toe studs etc.) reach the
    // primary address through applyRemaps(), so one address per role is enough.
    for (uint8_t i = 0; i < NUM_PISTONS; i++) {
        uint8_t  type = pistonType[i];
        uint16_t addr = pistonAddr[i];
        if (type == PISTON_TYPE_SET      && !ADDR_VALID(setAddr))     { setAddr = addr; setPistonIndex = i; }
        if (type == PISTON_TYPE_GC       && !ADDR_VALID(cancelAddr))  cancelAddr  = addr;
        if (type == PISTON_TYPE_MEM_UP   && !ADDR_VALID(memUpAddr))   memUpAddr   = addr;
        if (type == PISTON_TYPE_MEM_DOWN && !ADDR_VALID(memDownAddr)) memDownAddr = addr;
        if (type == PISTON_TYPE_NEXT     && !ADDR_VALID(nextAddr))    nextAddr    = addr;
        if (type == PISTON_TYPE_PREV     && !ADDR_VALID(prevAddr))    prevAddr    = addr;
    }

    if (ADDR_VALID(memUpAddr))   { listForwardAddr = memUpAddr;   listForwardName = "MEM+"; }
    else                         { listForwardAddr = nextAddr;    listForwardName = "NEXT"; }
    if (ADDR_VALID(memDownAddr)) { listBackAddr    = memDownAddr; listBackName    = "MEM-"; }
    else                         { listBackAddr    = prevAddr;    listBackName    = "PREV"; }
    if (ADDR_VALID(nextAddr))    { stepUpAddr      = nextAddr;    stepUpName      = "NEXT"; }
    else                         { stepUpAddr      = memUpAddr;   stepUpName      = "MEM+"; }
    if (ADDR_VALID(prevAddr))    { stepDownAddr    = prevAddr;    stepDownName    = "PREV"; }
    else                         { stepDownAddr    = memDownAddr; stepDownName    = "MEM-"; }

    if (!ADDR_VALID(setAddr) || !ADDR_VALID(cancelAddr)) {
        Serial.println("DBG: piston menu needs SET and GENERAL CANCEL -- menu unreachable");
    }
    if (!ADDR_VALID(listForwardAddr) && !ADDR_VALID(listBackAddr)) {
        Serial.println("DBG: piston menu has no MEM+/MEM-/NEXT/PREV -- cannot move in lists");
    }
}

// ============================================================
// Calibrate Shoes
// ============================================================
// Every analog shoe gets a row, the crescendo shoe included (a discrete shoe has
// nothing to calibrate). Cells, walked by the list pistons: row 0 Min, row 0
// Max, row 1 Min, ... then Save. SET on a Min/Max cell captures that shoe's live
// reading into the working copy; SET on Save writes EEPROM and leaves. GENERAL
// CANCEL leaves without saving.
static void runCalibrateShoes() {
    uint8_t calSlot[MAX_EXPRESSIONS];
    uint8_t rowCount = 0;
    for (uint8_t i = 0; i < NUM_EXPRESSIONS; i++) {
        if (exprType[i] == EXPR_ANALOG || exprType[i] == EXPR_CRESCENDO) {
            calSlot[rowCount] = i;
            rowCount++;
        }
    }

    // expressionCalibrationSave() reads the full MAX_EXPRESSIONS arrays, so seed
    // all of them; only the listed rows are changed here.
    uint16_t workingMin[MAX_EXPRESSIONS];
    uint16_t workingMax[MAX_EXPRESSIONS];
    for (uint8_t i = 0; i < MAX_EXPRESSIONS; i++) {
        workingMin[i] = calibratedExprMin[i];
        workingMax[i] = calibratedExprMax[i];
    }

    const uint8_t cellCount = 2 * rowCount + 1;   // Min, Max per row, then Save
    const int left  = ui.displaySpaceLeftX + 6;
    const int top   = ui.displaySpaceTopY + 8;
    const int rowH  = 34;
    uint8_t  selected   = 0;
    bool     needsPaint = true;
    uint32_t nextLiveMs = 0;
    char     text[40];

    while (true) {
        beginPass();

        if (needsPaint) {
            ui.drawTitleBar("Calibrate Shoes");
            ui.clearDisplaySpace();
            ui.lcdSetFont(Arial_9_Bold);
            for (uint8_t row = 0; row < rowCount; row++) {
                uint8_t slot = calSlot[row];
                int y = top + row * rowH;

                ui.lcdSetFontColor(LCD_WHITE);
                ui.lcdSetCursorXY(left, y);
                snprintf(text, sizeof text, exprType[slot] == EXPR_CRESCENDO ? "Shoe %u cresc" : "Shoe %u",
                         slot + 1);
                ui.lcdPrint(text);

                ui.lcdSetFontColor(selected == 2 * row ? LCD_YELLOW : LCD_LIGHTGREY);
                ui.lcdSetCursorXY(left + 150, y);
                snprintf(text, sizeof text, "Min %u", workingMin[slot]);
                ui.lcdPrint(text);

                ui.lcdSetFontColor(selected == 2 * row + 1 ? LCD_YELLOW : LCD_LIGHTGREY);
                ui.lcdSetCursorXY(left + 230, y);
                snprintf(text, sizeof text, "Max %u", workingMax[slot]);
                ui.lcdPrint(text);
            }
            char saveLabel[] = "Save";
            ui.drawButton(saveLabel, selected == 2 * rowCount,
                          ui.displaySpaceCenterX, ui.displaySpaceBottomY - 42, 120, 30);
            ui.lcdSetFont(Arial_9_Bold);
            ui.lcdSetFontColor(LCD_LIGHTGREY);
            ui.lcdSetCursorXY(left, ui.displaySpaceBottomY - 14);
            snprintf(text, sizeof text, "%s/%s move  SET capture/save  GC exit",
                     listForwardName, listBackName);
            ui.lcdPrint(text);
            needsPaint = false;
            nextLiveMs = 0;   // live readings repaint with the screen
        }

        if (pressEdge(listForwardAddr)) { selected = (selected + 1) % cellCount;             needsPaint = true; }
        if (pressEdge(listBackAddr))    { selected = (selected + cellCount - 1) % cellCount; needsPaint = true; }

        if (pressEdge(setAddr)) {
            if (selected == 2 * rowCount) {                  // Save
                expressionCalibrationSave(workingMin, workingMax);
                saveInputState();
                return;
            }
            uint8_t  slot    = calSlot[selected / 2];
            uint16_t reading = (uint16_t)analogRead(exprAnalogPin[slot]);
            if (selected & 1) workingMax[slot] = reading;    // odd cell = Max
            else              workingMin[slot] = reading;    // even cell = Min
            needsPaint = true;
        }

        if (pressEdge(cancelAddr)) {                         // leave without saving
            saveInputState();
            return;
        }

        // Live reading per row, at most every 150 ms so the SPI writes don't
        // starve the input loop.
        if (millis() >= nextLiveMs) {
            ui.lcdSetFont(Arial_9_Bold);
            for (uint8_t row = 0; row < rowCount; row++) {
                int y = top + row * rowH;
                ui.lcdDrawFilledRectangle(left + 96, y, 50, 14, LCD_BLACK);
                ui.lcdSetFontColor(LCD_WHITE);
                ui.lcdSetCursorXY(left + 96, y);
                snprintf(text, sizeof text, "%u", (unsigned)analogRead(exprAnalogPin[calSlot[row]]));
                ui.lcdPrint(text);
            }
            nextLiveMs = millis() + 150;
        }

        saveInputState();
    }
}

// ============================================================
// Crescendo
// ============================================================
// Entering recalls level 1 to the console (NOT blind here, so it can be seen
// and edited). The step pistons move between levels, recalling each; SET stores
// the current registration to the shown level and moves to the next one;
// GENERAL CANCEL leaves. The stop handlers run every pass, so drawknobs, and
// stops changed on the host's screen, set the registration live.
static void runCrescendoProgram() {
    currentScreen = SCREEN_CRESCENDO;   // crescendoPoll()'s overlay stays off
    crescendoProgEnter();

    uint8_t shownLevel = 0xFF;
    char    text[48];

    while (true) {
        beginPass();

        if (pressEdge(stepUpAddr))   crescendoProgNav(+1);
        if (pressEdge(stepDownAddr)) crescendoProgNav(-1);
        if (pressEdge(setAddr))      crescendoProgStore();   // stores, then auto-increments
        if (pressEdge(cancelAddr))   break;

        processStopInputs();
        buildStopOutputs();
        updateCoilPulses();
        checkStopRetries();

        if (crescendoProgLevel != shownLevel) {
            ui.drawTitleBar("Crescendo Program");
            ui.clearDisplaySpace();

            ui.lcdSetFont(Arial_9_Bold);
            ui.lcdSetFontColor(LCD_LIGHTGREY);
            ui.lcdSetCursorXY(ui.displaySpaceLeftX + 6, ui.displaySpaceTopY + 8);
            ui.lcdPrint("Level");

            ui.lcdSetFont(Arial_40_Bold);
            ui.lcdSetFontColor(LCD_WHITE);
            snprintf(text, sizeof text, "%u", crescendoProgLevel);
            ui.lcdSetCursorXY(ui.displaySpaceCenterX - ui.lcdStringWidthInPixels(text) / 2,
                              ui.displaySpaceTopY + 24);
            ui.lcdPrint(text);

            ui.lcdSetFont(Arial_9_Bold);
            ui.lcdSetFontColor(LCD_LIGHTGREY);
            ui.lcdSetCursorXY(ui.displaySpaceLeftX + 6, ui.displaySpaceBottomY - 14);
            snprintf(text, sizeof text, "%s/%s level  SET store  GC exit", stepUpName, stepDownName);
            ui.lcdPrint(text);

            shownLevel = crescendoProgLevel;
        }

        saveInputState();
    }

    crescendoProgExit();
    saveInputState();
    currentScreen = SCREEN_CONFIG;
}

// ============================================================
// Tuning
// ============================================================
// The same readout as the touch Tuning / Temperature screen. The step pistons
// trim up and down, SET resets the trim, GENERAL CANCEL leaves. Trim changes
// are saved by PitchManager as they are made, as on the touch screen.
static void runTuning() {
    ui.drawTitleBar("Tuning / Temperature");
    ui.clearDisplaySpace();

    const int lineX  = 10;
    const int lineY0 = ui.displaySpaceTopY + 4;
    const int lineH  = 24;
    const int NUM_LINES = 5;
    char shownLine[NUM_LINES][44];
    for (int i = 0; i < NUM_LINES; i++) shownLine[i][0] = '\0';

    char hint[48];
    snprintf(hint, sizeof hint, "%s/%s trim  SET reset  GC exit", stepUpName, stepDownName);
    ui.lcdSetFont(Arial_9_Bold);
    ui.lcdSetFontColor(LCD_LIGHTGREY);
    ui.lcdSetCursorXY(lineX, ui.displaySpaceBottomY - 14);
    ui.lcdPrint(hint);

    while (true) {
        beginPass();

        if (pressEdge(stepUpAddr))   pitchManagerManualUp();
        if (pressEdge(stepDownAddr)) pitchManagerManualDown();
        if (pressEdge(setAddr))      pitchManagerManualReset();
        if (pressEdge(cancelAddr))   { saveInputState(); return; }

        char line[NUM_LINES][44];
        snprintf(line[0], sizeof(line[0]), "Temp:        %.1f C", (double)getTempDegC());
        snprintf(line[1], sizeof(line[1]), "Temp offset: %+d cents", getTempOffsetCents());
        snprintf(line[2], sizeof(line[2]), "Manual trim: %+d cents", getManualOffsetCents());
        snprintf(line[3], sizeof(line[3]), "Total: %+d c   %.1f Hz",
                 getTotalTargetCents(), (double)getTargetFrequencyHz());
        if (pitchHaveReport())
            snprintf(line[4], sizeof(line[4]), "Reported:    %+d cents", getReportedOffsetCents());
        else
            snprintf(line[4], sizeof(line[4]), "Reported:    (awaiting)");

        // Repaint only lines whose text changed, so the loop stays fast.
        ui.lcdSetFont(Arial_10_Bold);
        for (int i = 0; i < NUM_LINES; i++) {
            if (strcmp(line[i], shownLine[i]) == 0) continue;
            int y = lineY0 + i * lineH;
            ui.lcdDrawFilledRectangle(lineX, y, 300, lineH, LCD_BLACK);
            ui.lcdSetFontColor(LCD_WHITE);
            ui.lcdSetCursorXY(lineX, y);
            ui.lcdPrint(line[i]);
            strcpy(shownLine[i], line[i]);
        }

        saveInputState();
    }
}

// ============================================================
// Diagnostics (1.13.0)
// ============================================================
// The tail of DIAG.LOG, newest at the bottom, a page at a time. The list pistons
// page back (older) and forward (newer); GENERAL CANCEL leaves. Read-only: the
// log is never cleared from here, so an organist cannot lose the evidence.
static void runDiagnostics() {
    static char logText[4096];
    char*    lines[160];
    uint16_t lineCount = diagReadLog(logText, sizeof logText, lines, 160);

    const int left      = ui.displaySpaceLeftX + 4;
    const int top       = ui.displaySpaceTopY + 4;
    const int lineH     = 18;
    const int PAGE      = 8;
    const int MAX_CHARS = 46;      // what fits across the panel at Arial 9

    // Start on the last page so the newest entries show first.
    int firstShown = (lineCount > PAGE) ? (int)lineCount - PAGE : 0;
    bool needsPaint = true;
    char text[64];

    saveInputState();   // the SET that chose this item is not a fresh press in here

    while (true) {
        beginPass();

        if (needsPaint) {
            ui.drawTitleBar("Diagnostics");
            ui.clearDisplaySpace();
            ui.lcdSetFont(Arial_9_Bold);

            ui.lcdSetFontColor(LCD_YELLOW);
            ui.lcdSetCursorXY(left, top);
            snprintf(text, sizeof text, "Boot %lu   log lines %u   showing %d-%d",
                     (unsigned long)diagBootNumber, lineCount,
                     lineCount ? firstShown + 1 : 0,
                     firstShown + ((int)lineCount - firstShown < PAGE ? (int)lineCount - firstShown : PAGE));
            ui.lcdPrint(text);

            ui.lcdSetFontColor(LCD_WHITE);
            if (lineCount == 0) {
                ui.lcdSetCursorXY(left, top + lineH);
                ui.lcdPrint("(log is empty)");
            }
            for (int row = 0; row < PAGE && firstShown + row < (int)lineCount; row++) {
                strncpy(text, lines[firstShown + row], MAX_CHARS);
                text[MAX_CHARS] = '\0';
                ui.lcdSetCursorXY(left, top + (row + 1) * lineH);
                ui.lcdPrint(text);
            }

            ui.lcdSetFontColor(LCD_LIGHTGREY);
            ui.lcdSetCursorXY(left, ui.displaySpaceBottomY - 14);
            snprintf(text, sizeof text, "%s older  %s newer  GC exit", listBackName, listForwardName);
            ui.lcdPrint(text);
            needsPaint = false;
        }

        if (pressEdge(listBackAddr) && firstShown > 0) {
            firstShown = (firstShown >= PAGE) ? firstShown - PAGE : 0;
            needsPaint = true;
        }
        if (pressEdge(listForwardAddr) && firstShown + PAGE < (int)lineCount) {
            firstShown += PAGE;
            if (firstShown > (int)lineCount - PAGE) firstShown = (int)lineCount - PAGE;
            if (firstShown < 0) firstShown = 0;
            needsPaint = true;
        }
        if (pressEdge(cancelAddr)) { saveInputState(); return; }

        saveInputState();
    }
}

// ============================================================
// The menu
// ============================================================
void pistonMenuRun() {
    // Build the item list for this console.
    const uint8_t ITEM_CALIBRATE = 0;
    const uint8_t ITEM_CRESCENDO = 1;
    const uint8_t ITEM_TUNING    = 2;
    const uint8_t ITEM_DIAG      = 3;
    uint8_t     itemId[4];
    const char* itemLabel[4];
    uint8_t     itemCount = 0;

    bool hasAnalogShoe = false;
    for (uint8_t i = 0; i < NUM_EXPRESSIONS; i++) {
        if (exprType[i] == EXPR_ANALOG || exprType[i] == EXPR_CRESCENDO) hasAnalogShoe = true;
    }
    if (hasAnalogShoe)        { itemId[itemCount] = ITEM_CALIBRATE; itemLabel[itemCount] = "Calibrate Shoes"; itemCount++; }
    if (crescendoAvailable)   { itemId[itemCount] = ITEM_CRESCENDO; itemLabel[itemCount] = "Crescendo";       itemCount++; }
    if (ORGAN_TUNING_PRESENT) { itemId[itemCount] = ITEM_TUNING;    itemLabel[itemCount] = "Tuning";          itemCount++; }
    itemId[itemCount] = ITEM_DIAG; itemLabel[itemCount] = "Diagnostics"; itemCount++;   // always present

    // The SET still held and the GC that opened the menu are already down, so
    // baseline them now: neither may count as a fresh press in here.
    saveInputState();

    uint8_t selected   = 0;
    bool    needsPaint = true;
    char    hint[48];

    while (true) {
        beginPass();

        if (needsPaint) {
            ui.drawTitleBar("Config");
            ui.clearDisplaySpace();
            const int y0 = ui.displaySpaceTopY + 26;   // 4 items fit above the hint line
            for (uint8_t i = 0; i < itemCount; i++) {
                char label[24];   // a writable copy: TUI's drawButton takes char*
                strncpy(label, itemLabel[i], sizeof label - 1);
                label[sizeof label - 1] = '\0';
                ui.drawButton(label, selected == i, ui.displaySpaceCenterX, y0 + i * 42, 240, 34);
            }
            ui.lcdSetFont(Arial_9_Bold);
            ui.lcdSetFontColor(LCD_LIGHTGREY);
            if (itemCount == 0) {
                ui.lcdSetCursorXY(ui.displaySpaceLeftX + 6, ui.displaySpaceTopY + 20);
                ui.lcdPrint("Nothing to configure on this console");
            }
            ui.lcdSetCursorXY(ui.displaySpaceLeftX + 6, ui.displaySpaceBottomY - 14);
            snprintf(hint, sizeof hint, "%s/%s move  SET select  GC exit", listForwardName, listBackName);
            ui.lcdPrint(hint);
            needsPaint = false;
        }

        if (itemCount > 0 && pressEdge(listForwardAddr)) { selected = (selected + 1) % itemCount;             needsPaint = true; }
        if (itemCount > 0 && pressEdge(listBackAddr))    { selected = (selected + itemCount - 1) % itemCount; needsPaint = true; }

        if (itemCount > 0 && pressEdge(setAddr)) {
            // Only a fresh SET press gets here: the SET held to open the menu
            // was baselined above and is not an edge.
            if (itemId[selected] == ITEM_CALIBRATE) runCalibrateShoes();
            if (itemId[selected] == ITEM_CRESCENDO) runCrescendoProgram();
            if (itemId[selected] == ITEM_TUNING)    runTuning();
            if (itemId[selected] == ITEM_DIAG)      runDiagnostics();
            needsPaint = true;
            continue;   // the screen already saved the input state
        }

        if (pressEdge(cancelAddr)) break;

        saveInputState();
    }

    // Leave with no stale edges, and with setHeld matching the real SET piston:
    // its release happened in here, unseen by the combination back-end, which
    // would otherwise go on believing SET is held and turn the next general
    // press into a capture.
    saveInputState();
    bool setWasHeld = setHeld;
    setHeld = ADDR_VALID(setAddr) && readInput(setAddr);

#if ORGAN_COMBINATION_MODE == COMBINATION_MODE_HW
    // With Hauptwerk owning the combination action, SET's press went to the
    // host as a note-on. Its release happened in here, so send the note-off the
    // piston handler never got the chance to send.
    if (setWasHeld && !setHeld && setPistonIndex != 0xFF &&
        pistonMidiNote[setPistonIndex] != 0xFF) {
        midiSendNoteOff(pistonMidiNote[setPistonIndex], 0, PISTON_MIDI_CHANNEL);
    }
#else
    (void)setWasHeld;
    (void)setPistonIndex;
#endif
}
