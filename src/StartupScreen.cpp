// StartupScreen.cpp
// Blocking startup handshake screen. Mirrors the ExpressionCalScreen/TuningScreen
// pattern: draws on the shared 'ui' and blocks until the wait is satisfied. Here
// the wait is a single USB-MIDI NoteOn, caught by the sketch's handler.

#include "StartupScreen.h"
#include "OrganCore.h"        // STARTUP_WAIT_* contract symbols
#include "Display.h"          // shared ui instance
#include "ScanChain.h"        // scanAllChains() - clear the lamps on entry
#include "StopHandler.h"      // buildStopOutputs()
#include "OrganPower.h"       // powerPoll() - the power switch works on every screen

#include <stdio.h>

bool startupNoteSeen = false;

void startupWaitScreenRun() {
    startupNoteSeen = false;

    // Clear the drawstop lamps before we block. Nothing has shifted the output
    // chain out yet at this point in setup() -- the sketch's first
    // scanAllChains() comes AFTER this screen returns -- so the CD4094s are
    // still holding whatever random state they powered up in, and on a console
    // with many lamps that reads as "mostly on" for as long as the organist
    // waits for the engine.
    //
    // No general cancel is needed and none is wanted here. stopCommandedState[]
    // is already all-false this early, so a cancel would change no state; it
    // would only fire a note-off per stop at an engine that by definition is not
    // listening yet. The lamps are wrong because the hardware was never written,
    // not because the state is wrong. Writing it is the whole fix.
    //
    // The input half of this scan is harmless: the sketch primes edge detection
    // with its own scanAllChains()/applyRemaps()/saveInputState() after we
    // return, so no drawstop that happens to be drawn will register as a change.
    buildStopOutputs();
    scanAllChains();

    ui.drawTitleBar("Starting Up");
    ui.clearDisplaySpace();

    // Seconds counter, centered. Only repainted when the whole-second value
    // changes, so there's no flicker while we spin on usbMIDI.read().
    ui.lcdSetFont(Arial_10_Bold);

    uint32_t startMs = millis();
    int32_t  lastShownSecs = -1;

    while (!startupNoteSeen) {
        scanAllChains();  // refresh inputs so powerPoll() sees the power switch
        powerPoll();      // the organist can switch off while waiting for the engine
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
