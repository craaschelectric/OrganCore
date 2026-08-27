// TuningScreen.cpp
// NOTE: TeensyUserInterface is not compilable in the host harness; verify font
// name and method signatures against your TUI version on the Teensy.
//
// Adapted from the Opus 62 TempTuneScreen onto OrganCore's shared 'ui'. Adds the
// reported-offset / target readout so the GrandOrgue feedback loop is visible
// while trimming.
//
// Repaint discipline (matches the run/crescendo screens): the readout lines are
// redrawn ONLY when their text changes, never every loop. Redrawing the whole
// block every iteration floods the SPI bus and starves touch sampling, which is
// what made the -/+/Reset buttons feel unresponsive. Buttons are drawn once and
// live BELOW the text block so a line repaint can never erase them.

#include "TuningScreen.h"
#include "TuningConfig.h"
#include "PitchManager.h"
#include "TempSensor.h"
#include "Display.h"            // shared ui instance

#include <stdio.h>
#include <string.h>

#if ORGAN_HAS_TUNING

void tuningScreenRun() {
    ui.drawTitleBar("Tuning / Temperature");
    ui.clearDisplaySpace();

    const int lineX  = 10;
    const int lineY0 = ui.displaySpaceTopY + 4;   // below the 32px title bar
    const int lineH  = 24;
    const int NLINES = 5;

    // Buttons live below the text block (text occupies lineY0 .. lineY0+NLINES*lineH
    // = 34..154). Drawn once; never erased by the per-line readout repaint.
    BUTTON downBtn  = { "-",      45, 180,  60, 40 };
    BUTTON upBtn    = { "+",     115, 180,  60, 40 };
    BUTTON resetBtn = { "Reset", 210, 180, 100, 40 };
    BUTTON backBtn  = { "Back",  ui.displaySpaceCenterX, ui.displaySpaceBottomY - 18, 90, 30 };
    ui.drawButton(downBtn);
    ui.drawButton(upBtn);
    ui.drawButton(resetBtn);
    ui.drawButton(backBtn);

    // Cached last-printed text per line; a line repaints only when it changes.
    char shown[NLINES][44];
    for (int i = 0; i < NLINES; i++) shown[i][0] = '\0';

    while (true) {
        ui.getTouchEvents();        // sampled every loop -> responsive touch
        tempSensorPoll();           // keep the temperature reading live

        char line[NLINES][44];
        snprintf(line[0], sizeof(line[0]), "Temp:        %.1f C", (double)getTempDegC());
        snprintf(line[1], sizeof(line[1]), "Temp offset: %+d cents", getTempOffsetCents());
        snprintf(line[2], sizeof(line[2]), "Manual trim: %+d cents", getManualOffsetCents());
        snprintf(line[3], sizeof(line[3]), "Total: %+d c   %.1f Hz",
                 getTotalTargetCents(), (double)getTargetFrequencyHz());
        if (pitchHaveReport())
            snprintf(line[4], sizeof(line[4]), "GO reported: %+d cents", getReportedOffsetCents());
        else
            snprintf(line[4], sizeof(line[4]), "GO reported: (awaiting)");

        // Redraw only the lines that changed; when idle, nothing repaints and the
        // loop spins fast enough to catch every tap.
        for (int i = 0; i < NLINES; i++) {
            if (strcmp(line[i], shown[i]) == 0) continue;
            int y = lineY0 + i * lineH;
            ui.lcdDrawFilledRectangle(lineX, y, 300, lineH, LCD_BLACK);
            ui.lcdSetFontColor(LCD_WHITE);
            ui.lcdSetCursorXY(lineX, y);
            ui.lcdPrint(line[i]);
            strcpy(shown[i], line[i]);
        }

        if (ui.checkForButtonClicked(upBtn))    pitchManagerManualUp();
        if (ui.checkForButtonClicked(downBtn))  pitchManagerManualDown();
        if (ui.checkForButtonClicked(resetBtn)) pitchManagerManualReset();
        if (ui.checkForButtonClicked(backBtn))  return;
    }
}

#endif // ORGAN_HAS_TUNING
