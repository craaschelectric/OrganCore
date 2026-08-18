// StartupScreen.cpp
// Blocking startup handshake screen. Mirrors the ExpressionCalScreen/TuningScreen
// pattern: draws on the shared 'ui' and blocks until the wait is satisfied. Here
// the wait is a single USB-MIDI NoteOn, caught by the sketch's handler.

#include "StartupScreen.h"
#include "OrganCore.h"        // STARTUP_WAIT_* contract symbols
#include "Display.h"          // shared ui instance

#include <stdio.h>

bool startupNoteSeen = false;

void startupWaitScreenRun() {
    startupNoteSeen = false;

    ui.drawTitleBar("Starting Up");
    ui.clearDisplaySpace();

    // Seconds counter, centered. Only repainted when the whole-second value
    // changes, so there's no flicker while we spin on usbMIDI.read().
    ui.lcdSetFont(Arial_10_Bold);

    uint32_t startMs = millis();
    int32_t  lastShownSecs = -1;

    while (!startupNoteSeen) {
        usbMIDI.read();   // dispatch to the sketch handlers; the handshake note sets startupNoteSeen

        int32_t secs = (int32_t)((millis() - startMs) / 1000);
        if (secs != lastShownSecs) {
            lastShownSecs = secs;

            char buf[16];
            snprintf(buf, sizeof(buf), "%ld s", (long)secs);

            int y = ui.displaySpaceCenterY - 8;
            ui.lcdDrawFilledRectangle(0, y - 4, 320, 28, LCD_BLACK);   // clear the line region
            ui.lcdSetFontColor(LCD_WHITE);
            ui.lcdSetCursorXY(ui.displaySpaceCenterX, y);
            ui.lcdPrintCentered(buf);
        }
    }
}
