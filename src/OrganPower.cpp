// OrganPower.cpp  -  see OrganPower.h for what this is and why.
#include "OrganPower.h"
#include "OrganConfig.h"
#include "CoreConfig.h"
#include "ScanChain.h"
#include "StopHandler.h"
#include "Combination.h"
#include "MidiOut.h"
#include "Display.h"
#include "DisplayManager.h"
#include "Debug.h"

// 255 is the "not fitted" sentinel for a pin, matching BACKLIGHT_PIN and the
// rest of the contract. ADDR_DISABLED is the equivalent for an input address.
static const uint8_t PIN_NONE = 255;

static bool     switchArmed   = false;   // has the switch been seen released?
static uint32_t pressedSince  = 0;       // start of the current continuous press
static bool     bootComplete  = false;   // powerBootComplete() has been called
static bool     shuttingDown  = false;   // latch: the sequence runs once

// ------------------------------------------------------------
// Assert the keep-alive: whatever "stay powered" means on this console.
// Active-high drives HIGH. Active-low drives LOW, which is what an ATX supply's
// PS_ON# wants.
static void keepAliveAssert() {
    if (POWER_KEEPALIVE_PIN == PIN_NONE) return;
    pinMode(POWER_KEEPALIVE_PIN, OUTPUT);
    digitalWrite(POWER_KEEPALIVE_PIN, POWER_KEEPALIVE_ACTIVE_HIGH ? HIGH : LOW);
}

// Release it. Active-high drives LOW, positively. Active-low goes HI-Z and lets
// the supply's own pull-up take the line -- NOT a driven HIGH, because ATX pulls
// PS_ON# up to +5VSB and a Teensy 4.x pin is not 5V tolerant, and because the
// momentary power switch sits across the same node: two pull-downs, no drivers.
static void keepAliveRelease() {
    if (POWER_KEEPALIVE_PIN == PIN_NONE) return;
    if (POWER_KEEPALIVE_ACTIVE_HIGH) digitalWrite(POWER_KEEPALIVE_PIN, LOW);
    else                             pinMode(POWER_KEEPALIVE_PIN, INPUT);
}

void powerInit() {
    keepAliveAssert();

    // Open-drain idle. Left as an input so the host's own pull-up holds the
    // line high and no shutdown is requested; only ever driven LOW, never
    // HIGH. That way no state of this board -- unpowered, resetting, sitting in
    // the bootloader -- can halt the host by accident.
    if (POWER_HOST_SHUTDOWN_PIN != PIN_NONE) {
        pinMode(POWER_HOST_SHUTDOWN_PIN, INPUT);
    }

    // ACTIVE LOW, with our pull-up. The host pulls it down to say ready and
    // releases it to say halting. INPUT_PULLUP is not optional: a bare INPUT
    // floats, and a floating pin reads whatever is in the air -- an unwired,
    // unpowered or crashed host would report ready at random. With the pull-up
    // every one of those cases reads HIGH, which is "not ready", which is the
    // safe answer.
    if (POWER_HOST_READY_PIN != PIN_NONE) {
        pinMode(POWER_HOST_READY_PIN, INPUT_PULLUP);
    }
}

// ------------------------------------------------------------
void powerBootComplete() {
    bootComplete = true;
}

// ------------------------------------------------------------
// "Is the host in a state where asking it to halt will actually work?"
// With a readiness wire that is a live question with a live answer. Without
// one, the best available answer is "we have finished booting, so the host has
// had at least that long" -- which is why the wire is worth running.
static bool hostReady() {
    // Active low: LOW = the host is pulling it down = ready. HIGH is our own
    // pull-up with nothing on the other end, which covers "not booted yet",
    // "wire not fitted", "host crashed" and "host unplugged" alike.
    if (POWER_HOST_READY_PIN != PIN_NONE) return digitalRead(POWER_HOST_READY_PIN) == LOW;
    return bootComplete;
}

// ------------------------------------------------------------
void powerShutdownNow() {
    if (shuttingDown) return;
    shuttingDown = true;

    if (DEBUG_ENABLED) Serial.println("Power: shutting down");

    // 1. Silence the instrument through the ordinary path, so the engine and
    //    the drawstop lamps end up consistent rather than cut off mid-chord.
    combinationCancel();

    // 2. Tell whatever else needs telling. On Opus 62 this is the pipe driver's
    //    blower relay. midiOutSysEx() writes to usbMIDI and to the pipe mirror,
    //    so both the engine and the downstream driver see it; an engine that
    //    does not recognise the manufacturer ID ignores it.
    if (POWER_SHUTDOWN_SYSEX_LEN > 0) {
        midiOutSysEx(POWER_SHUTDOWN_SYSEX_LEN, POWER_SHUTDOWN_SYSEX);
        if (DEBUG_ENABLED) Serial.println("Power: shutdown SysEx sent");
    }

    // 3. Push the cancelled stops out to the lamps for real, so the console
    //    goes dark rather than freezing with stops apparently drawn.
    buildStopOutputs();
    scanAllChains();

    if (displayReady) {
        ui.drawTitleBar("Shutting Down");
        ui.clearDisplaySpace();
        ui.lcdSetFont(Arial_10_Bold);
        ui.lcdSetFontColor(LCD_WHITE);
        ui.lcdSetCursorXY(ui.displaySpaceCenterX, ui.displaySpaceCenterY);
        ui.lcdPrintCentered("Do not switch off");
    }

    // 4. Ask the host to halt. Held low from here on: this board loses power
    //    before the host comes back, so it releases the line by dying.
    if (POWER_HOST_SHUTDOWN_PIN != PIN_NONE) {
        pinMode(POWER_HOST_SHUTDOWN_PIN, OUTPUT);
        digitalWrite(POWER_HOST_SHUTDOWN_PIN, LOW);
        if (DEBUG_ENABLED) Serial.println("Power: host halt requested");

        // 5. Wait for it to finish. The readiness line falling is the host
        //    telling us it is done, which beats a timer; POWER_HOST_HALT_MS is
        //    the backstop for a host that dies without saying so.
        uint32_t started = millis();
        while (millis() - started < POWER_HOST_HALT_MS) {
            // Released -- our pull-up takes the line HIGH -- means the host has
            // begun halting in earnest. A real signal, where the timeout below
            // is only a guess.
            if (POWER_HOST_READY_PIN != PIN_NONE &&
                digitalRead(POWER_HOST_READY_PIN) == HIGH) {
                if (DEBUG_ENABLED) {
                    Serial.print("Power: host reported halt after ");
                    Serial.print(millis() - started);
                    Serial.println(" ms");
                }
                break;
            }
        }
    }

    // 6. Release the supply. Everything downstream of the relay goes with it,
    //    including, on a console wired this way, this board.
    if (POWER_KEEPALIVE_PIN != PIN_NONE) {
        if (DEBUG_ENABLED) Serial.println("Power: releasing supply");
        keepAliveRelease();
    }

    while (true) { }    // nothing left to do but wait for the dark
}

// ------------------------------------------------------------
void powerPoll() {
    if (POWER_SWITCH_ADDR == ADDR_DISABLED) return;
    if (shuttingDown) return;

    bool pressed = readInput(POWER_SWITCH_ADDR);

    // The arming latch. On a console where the same momentary switch turns the
    // instrument ON by bypassing the supply relay, the organist is holding it
    // down for the whole of setup() and it still reads pressed on the first
    // scans here. Ignore it until it has been seen RELEASED once, or the
    // console shuts down the instant it finishes booting -- which presents as a
    // dead board, because it is one.
    if (!switchArmed) {
        if (!pressed) {
            switchArmed = true;
            if (DEBUG_ENABLED) Serial.println("Power: switch released -- armed");
        }
        return;
    }

    if (!pressed) {
        pressedSince = 0;           // released: restart the hold
        return;
    }

    if (pressedSince == 0) pressedSince = millis();
    if (millis() - pressedSince < POWER_SWITCH_HOLD_MS) return;

    // Refuse while the host is not ready. Asking a booting machine to halt does
    // nothing except guarantee we cut its mains when the timeout expires.
    if (POWER_HOST_SHUTDOWN_PIN != PIN_NONE && !hostReady()) {
        pressedSince = 0;           // make them press again once it is ready
        if (DEBUG_ENABLED) Serial.println("Power: switch ignored -- host not ready yet");
        return;
    }

    powerShutdownNow();
}
