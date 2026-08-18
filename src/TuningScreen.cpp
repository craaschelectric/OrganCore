// TuningScreen.cpp
// NOTE: TeensyUserInterface is not compilable in the host harness; verify font
// name and method signatures against your TUI version on the Teensy.
//
// Adapted from the Opus 62 TempTuneScreen onto OrganCore's shared 'ui'. Adds the
// reported-offset / target readout so the GrandOrgue feedback loop is visible
// while trimming.

#include "TuningScreen.h"
#include "TuningConfig.h"
#include "PitchManager.h"
#include "TempSensor.h"
#include "Display.h"            // shared ui instance

#include <stdio.h>

#if ORGAN_HAS_TUNING

void tuningScreenRun() {
    ui.drawTitleBar("Tuning / Temperature");
    ui.clearDisplaySpace();

    const int lineX  = 10;
    const int lineY0 = 34;
    const int lineH  = 24;

    BUTTON downBtn  = { "-",      40, 150, 55, 40 };
    BUTTON upBtn    = { "+",     110, 150, 55, 40 };
    BUTTON resetBtn = { "Reset", 190, 150, 80, 40 };
    BUTTON backBtn  = { "Back",  ui.displaySpaceCenterX, ui.displaySpaceBottomY - 18, 90, 30 };
    ui.drawButton(downBtn);
    ui.drawButton(upBtn);
    ui.drawButton(resetBtn);
    ui.drawButton(backBtn);

    while (true) {
        ui.getTouchEvents();
        tempSensorPoll();           // keep the temperature reading live

        char buf[44];
        ui.lcdSetFontColor(LCD_WHITE);
        ui.lcdDrawFilledRectangle(lineX, lineY0, 300, lineH * 5, LCD_BLACK);

        snprintf(buf, sizeof(buf), "Temp:        %.1f C", (double)getTempDegC());
        ui.lcdSetCursorXY(lineX, lineY0 + 0 * lineH); ui.lcdPrint(buf);

        snprintf(buf, sizeof(buf), "Temp offset: %+d cents", getTempOffsetCents());
        ui.lcdSetCursorXY(lineX, lineY0 + 1 * lineH); ui.lcdPrint(buf);

        snprintf(buf, sizeof(buf), "Manual trim: %+d cents", getManualOffsetCents());
        ui.lcdSetCursorXY(lineX, lineY0 + 2 * lineH); ui.lcdPrint(buf);

        snprintf(buf, sizeof(buf), "Total: %+d c   %.1f Hz",
                 getTotalTargetCents(), (double)getTargetFrequencyHz());
        ui.lcdSetCursorXY(lineX, lineY0 + 3 * lineH); ui.lcdPrint(buf);

        if (pitchHaveReport()) {
            snprintf(buf, sizeof(buf), "GO reported: %+d cents", getReportedOffsetCents());
        } else {
            snprintf(buf, sizeof(buf), "GO reported: (awaiting)");
        }
        ui.lcdSetCursorXY(lineX, lineY0 + 4 * lineH); ui.lcdPrint(buf);

        if (ui.checkForButtonClicked(upBtn))    pitchManagerManualUp();
        if (ui.checkForButtonClicked(downBtn))  pitchManagerManualDown();
        if (ui.checkForButtonClicked(resetBtn)) pitchManagerManualReset();
        if (ui.checkForButtonClicked(backBtn))  return;
    }
}

#endif // ORGAN_HAS_TUNING
