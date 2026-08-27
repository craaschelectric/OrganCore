// ============================================================
// ExpressionCalScreen.cpp - TUI expression-pedal calibration screen
// ============================================================
// NOTE: TeensyUserInterface is not compilable in the host harness; verify font
// names and method signatures against your TUI version on the Teensy.
//
// One row per analog expression input, each with a live raw readout and Min/Max
// capture buttons. Save writes EEPROM via expressionCalibrationSave(); Cancel
// discards. Blocking by design — only reached from the config menu while
// scanning is paused.
//
// Repaint discipline (matches the run/crescendo screens): touch is sampled every
// loop, but the raw readouts are repainted at most every READOUT_MS. Blitting the
// readouts every loop floods the SPI bus and starves touch sampling — that, plus
// a too-narrow text field that ran the readout across the Min buttons, is what
// made this screen's buttons unresponsive. The Min/Max buttons now sit to the
// right of a text field wide enough for the readout, so text never overwrites
// them.

#include "ExpressionCalScreen.h"
#include "OrganCore.h"
#include "ExpressionCalibration.h"
#include "Display.h"            // shared ui instance

#include <stdio.h>
#include <string.h>

void expressionCalScreenRun() {
    // Work on a copy so Cancel discards changes.
    uint16_t wMin[MAX_EXPRESSIONS];
    uint16_t wMax[MAX_EXPRESSIONS];
    for (uint8_t i = 0; i < NUM_EXPRESSIONS; i++) {
        wMin[i] = calibratedExprMin[i];
        wMax[i] = calibratedExprMax[i];
    }

    ui.drawTitleBar("Expression Calibration");
    ui.clearDisplaySpace();

    // Rows start below the title bar. The Min/Max buttons are center-anchored at
    // y+4 (height 30), so their top edge is y+4-15 = y-11; with rowY0 =
    // displaySpaceTopY + 16 the first row's buttons clear the 32px title bar.
    const int rowY0   = ui.displaySpaceTopY + 16;
    const int rowH    = 40;
    const int readX   = 6;
    const int readW   = 165;      // wide enough for "P0 1023 [0-1023]"; ends well left of Min
    const int capMinX = 205;      // button centers, to the RIGHT of the readout field
    const int capMaxX = 268;

    // One row of Min/Max per analog input; discrete shoes have nothing to calibrate.
    BUTTON minBtn[MAX_EXPRESSIONS];
    BUTTON maxBtn[MAX_EXPRESSIONS];
    for (uint8_t i = 0; i < NUM_EXPRESSIONS; i++) {
        if (exprType[i] != EXPR_ANALOG) continue;
        int y = rowY0 + i * rowH;
        minBtn[i] = (BUTTON){ "Min", capMinX, y + 4, 54, 30 };
        maxBtn[i] = (BUTTON){ "Max", capMaxX, y + 4, 54, 30 };
        ui.drawButton(minBtn[i]);
        ui.drawButton(maxBtn[i]);
    }

    BUTTON saveBtn   = { "Save",    80, ui.displaySpaceBottomY - 18, 90, 30 };
    BUTTON cancelBtn = { "Cancel", 240, ui.displaySpaceBottomY - 18, 90, 30 };
    ui.drawButton(saveBtn);
    ui.drawButton(cancelBtn);

    // Throttled readout: raw ADC jitters, so repaint on a timer rather than every
    // loop. Touch is still sampled every loop, so buttons stay responsive.
    const uint32_t READOUT_MS = 120;
    uint32_t lastReadout = 0;
    char shown[MAX_EXPRESSIONS][40];
    for (uint8_t i = 0; i < MAX_EXPRESSIONS; i++) shown[i][0] = '\0';

    while (true) {
        ui.getTouchEvents();        // every loop -> responsive touch

        // Capture buttons: checked every loop (a tap must never be missed).
        for (uint8_t i = 0; i < NUM_EXPRESSIONS; i++) {
            if (exprType[i] != EXPR_ANALOG) continue;
            if (ui.checkForButtonClicked(minBtn[i])) wMin[i] = (uint16_t)analogRead(exprAnalogPin[i]);
            if (ui.checkForButtonClicked(maxBtn[i])) wMax[i] = (uint16_t)analogRead(exprAnalogPin[i]);
        }

        // Readouts: throttled, and only the rows whose text changed are blitted.
        uint32_t now = millis();
        if (now - lastReadout >= READOUT_MS) {
            lastReadout = now;
            for (uint8_t i = 0; i < NUM_EXPRESSIONS; i++) {
                if (exprType[i] != EXPR_ANALOG) continue;
                int raw = analogRead(exprAnalogPin[i]);
                char buf[40];
                snprintf(buf, sizeof(buf), "P%u %4d [%u-%u]", i, raw, wMin[i], wMax[i]);
                if (strcmp(buf, shown[i]) == 0) continue;
                int y = rowY0 + i * rowH;
                ui.lcdDrawFilledRectangle(readX, y, readW, rowH - 8, LCD_BLACK);
                ui.lcdSetFontColor(LCD_WHITE);
                ui.lcdSetCursorXY(readX, y);
                ui.lcdPrint(buf);
                strcpy(shown[i], buf);
            }
        }

        if (ui.checkForButtonClicked(saveBtn)) {
            expressionCalibrationSave(wMin, wMax);
            return;
        }
        if (ui.checkForButtonClicked(cancelBtn)) {
            return;
        }
    }
}
