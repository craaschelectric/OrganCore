// ============================================================
// ExpressionCalScreen.cpp - TUI expression-pedal calibration screen
// ============================================================
// NOTE: TeensyUserInterface is not compilable in the host harness; verify font
// names and method signatures against your TUI version on the Teensy.
//
// Adapted from the Opus 58/62 calibration screen: uses OrganCore's exprAnalogPin[]
// and the shared 'ui' from Display.h. One row per analog expression input, each
// with a live raw readout and Min/Max capture buttons. Save writes EEPROM via
// expressionCalibrationSave(); Cancel discards. Blocking by design — only
// reached from the config menu while scanning is paused.

#include "ExpressionCalScreen.h"
#include "OrganCore.h"
#include "ExpressionCalibration.h"
#include "Display.h"            // shared ui instance

#include <stdio.h>

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

    const int rowY0  = 34;
    const int rowH   = 40;
    const int capMinX = 150;
    const int capMaxX = 218;

    // Capture buttons per analog row. Only analog inputs get a row; discrete
    // shoes have nothing to calibrate.
    BUTTON minBtn[MAX_EXPRESSIONS];
    BUTTON maxBtn[MAX_EXPRESSIONS];
    for (uint8_t i = 0; i < NUM_EXPRESSIONS; i++) {
        if (exprType[i] != EXPR_ANALOG) continue;
        int y = rowY0 + i * rowH;
        minBtn[i] = (BUTTON){ "Min", capMinX, y + 4, 60, 30 };
        maxBtn[i] = (BUTTON){ "Max", capMaxX, y + 4, 60, 30 };
        ui.drawButton(minBtn[i]);
        ui.drawButton(maxBtn[i]);
    }

    BUTTON saveBtn   = { "Save",    80, ui.displaySpaceBottomY - 18, 90, 30 };
    BUTTON cancelBtn = { "Cancel", 220, ui.displaySpaceBottomY - 18, 90, 30 };
    ui.drawButton(saveBtn);
    ui.drawButton(cancelBtn);

    while (true) {
        ui.getTouchEvents();

        // Live per-row readout and endpoint capture.
        for (uint8_t i = 0; i < NUM_EXPRESSIONS; i++) {
            if (exprType[i] != EXPR_ANALOG) continue;
            int raw = analogRead(exprAnalogPin[i]);
            int y = rowY0 + i * rowH;

            char buf[40];
            snprintf(buf, sizeof(buf), "P%u raw:%4d  lo:%4u hi:%4u",
                     i, raw, wMin[i], wMax[i]);
            ui.lcdDrawFilledRectangle(6, y, capMinX - 10, rowH - 8, LCD_BLACK);
            ui.lcdSetFontColor(LCD_WHITE);
            ui.lcdSetCursorXY(6, y);
            ui.lcdPrint(buf);

            if (ui.checkForButtonClicked(minBtn[i])) wMin[i] = (uint16_t)raw;
            if (ui.checkForButtonClicked(maxBtn[i])) wMax[i] = (uint16_t)raw;
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
