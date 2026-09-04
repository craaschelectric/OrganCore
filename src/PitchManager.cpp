// PitchManager.cpp

#include "PitchManager.h"
#include "TuningConfig.h"
#include "TempSensor.h"
#include "MidiOut.h"
#include "Debug.h"
#include <EEPROM.h>
#include <math.h>

// ---- Persistence ----
static int eepromManualOffsetAddr = -1;   // set by pitchManagerInit()

// ---- State ----
static int8_t manualOffset      = 0;      // cents, EEPROM-backed
static int    totalTargetCents  = 0;      // temperature + manual
static int    reportedCents     = 0;      // last offset GrandOrgue reported
static bool   haveReceivedReport = false;

// ---- Pulse-feedback state (GrandOrgue) ----
static bool     pulseActive         = false;   // in a target-seeking nudge sequence
static bool     waitingForResponse  = false;
static bool     pulseNoteIsOn       = false;   // note-on sent, awaiting its note-off
static uint8_t  pulseNoteSent       = 0;
static uint32_t pulseNoteOnTime     = 0;
static uint32_t lastPulseSentTime   = 0;
static uint8_t  consecutiveRetries  = 0;
static uint32_t lastBootstrapTime   = 0;       // startup nudge cadence until first report

// ------------------------------------------------------------
// EEPROM manual trim
// ------------------------------------------------------------
static void loadManualOffset() {
    int8_t stored = 0;
    EEPROM.get(eepromManualOffsetAddr, stored);
    if (stored < MANUAL_OFFSET_MIN || stored > MANUAL_OFFSET_MAX) {
        stored = 0;
        EEPROM.put(eepromManualOffsetAddr, stored);
        if (DEBUG_ENABLED) Serial.println("Pitch: EEPROM trim out of range, reset to 0");
    }
    manualOffset = stored;
}

static void saveManualOffset() {
    EEPROM.put(eepromManualOffsetAddr, manualOffset);
}

// ------------------------------------------------------------
// Hauptwerk transport: MIDI Master Fine Tuning SysEx (open-loop)
// ------------------------------------------------------------
// F0 7F 7F 04 03 LSB MSB F7. 14-bit value 0x2000 = center (A440),
// 0x0000..0x3FFF spans -/+100 cents (8192 units per 100 cents).
static void sendTuningSysEx(int cents) {
    if (cents < -100) cents = -100;
    if (cents >  100) cents =  100;

    int32_t value = 0x2000 + (int32_t)cents * 8192 / 100;
    if (value < 0)      value = 0;
    if (value > 0x3FFF) value = 0x3FFF;

    uint8_t msb = (value >> 7) & 0x7F;
    uint8_t lsb = value & 0x7F;
    uint8_t sysex[] = { 0xF0, 0x7F, 0x7F, 0x04, 0x03, lsb, msb, 0xF7 };
    midiOutSysEx(sizeof(sysex), sysex);

    if (DEBUG_ENABLED) {
        Serial.print("Pitch: MTS SysEx cents="); Serial.print(cents);
        Serial.print(" MSB=0x"); Serial.print(msb, HEX);
        Serial.print(" LSB=0x"); Serial.println(lsb, HEX);
    }
}

// ------------------------------------------------------------
// GrandOrgue transport: pitch up/down nudge notes + reported-offset feedback
// ------------------------------------------------------------
static void sendPulseNote(bool up) {
    pulseNoteSent = up ? PITCH_UP_MIDI_NOTE : PITCH_DOWN_MIDI_NOTE;
    midiSendNoteOn(pulseNoteSent, 127, PITCH_PULSE_MIDI_CH);
    pulseNoteIsOn      = true;
    pulseNoteOnTime    = millis();
    waitingForResponse = true;
    lastPulseSentTime  = pulseNoteOnTime;
    if (DEBUG_ENABLED) { Serial.print("Pitch: pulse "); Serial.println(up ? "UP" : "DOWN"); }
}

static void startPulseSequence() {
    if (!PITCH_PULSE_ENABLED) return;
    if (totalTargetCents == reportedCents) return;   // already there
    pulseActive        = true;
    consecutiveRetries = 0;
    waitingForResponse = false;
    pulseNoteIsOn      = false;
    sendPulseNote(totalTargetCents > reportedCents);
}

// ------------------------------------------------------------
// Recompute target and drive whichever transport(s) are enabled
// ------------------------------------------------------------
static void recalcAndApply() {
    int old = totalTargetCents;
    totalTargetCents = getTempOffsetCents() + manualOffset;

    if (totalTargetCents != old) {
        if (PITCH_SEND_TUNING_SYSEX) sendTuningSysEx(totalTargetCents);
        if (PITCH_PULSE_ENABLED && haveReceivedReport && !pulseActive) startPulseSequence();
    }

    if (DEBUG_ENABLED) {
        Serial.print("Pitch: temp="); Serial.print(getTempOffsetCents());
        Serial.print(" manual="); Serial.print(manualOffset);
        Serial.print(" total="); Serial.print(totalTargetCents);
        Serial.print(" ("); Serial.print(getTargetFrequencyHz(), 1);
        Serial.println(" Hz)");
    }
}

// ------------------------------------------------------------
// Public
// ------------------------------------------------------------
void pitchManagerInit(int addr) {
    if (!ORGAN_TUNING_PRESENT) return;   // no pipes on this console
    eepromManualOffsetAddr = addr;
}

void pitchManagerSetup() {
    loadManualOffset();
    totalTargetCents = getTempOffsetCents() + manualOffset;

    // Push an initial tuning. MTS is open-loop so it applies immediately; the
    // pulse loop can't start until GO has reported an offset, which the poll()
    // bootstrap nudge brings about.
    if (PITCH_SEND_TUNING_SYSEX) sendTuningSysEx(totalTargetCents);
    if (DEBUG_ENABLED) Serial.println("Pitch: initialized");
}

void pitchManagerPoll() {
    if (!PITCH_PULSE_ENABLED) return;

    uint32_t now = millis();

    // Startup bootstrap: GO does not emit its pitch report until nudged. Send a
    // pitch-up nudge once per second until the first report arrives; that report
    // sets haveReceivedReport and arms the feedback loop, stopping these nudges.
    // The note-off is handled by the shared block below.
    if (!haveReceivedReport && !pulseNoteIsOn) {
        if (now - lastBootstrapTime >= 1000) {
            lastBootstrapTime = now;
            pulseNoteSent   = PITCH_UP_MIDI_NOTE;
            midiSendNoteOn(pulseNoteSent, 127, PITCH_PULSE_MIDI_CH);
            pulseNoteIsOn   = true;
            pulseNoteOnTime = now;
            if (DEBUG_ENABLED) Serial.println("Pitch: bootstrap UP (awaiting first GO report)");
        }
    }

    // End the current nudge note after its on-time (bootstrap or feedback).
    if (pulseNoteIsOn && (now - pulseNoteOnTime >= PITCH_PULSE_ON_MS)) {
        midiSendNoteOff(pulseNoteSent, 0, PITCH_PULSE_MIDI_CH);
        pulseNoteIsOn = false;
    }

    // No report came back in time - resend the nudge.
    if (pulseActive && waitingForResponse && !pulseNoteIsOn) {
        if (now - lastPulseSentTime >= PITCH_PULSE_TIMEOUT_MS) {
            consecutiveRetries++;
            if (DEBUG_ENABLED) { Serial.print("Pitch: pulse timeout, retry #"); Serial.println(consecutiveRetries); }
            sendPulseNote(totalTargetCents > reportedCents);
        }
    }
}

void pitchManagerOnTempChange() {
    recalcAndApply();
}

void pitchManagerOnReportedOffset(int newReportedCents) {
    if (!PITCH_PULSE_ENABLED) return;

    if (DEBUG_ENABLED) { Serial.print("Pitch: GO reported "); Serial.print(newReportedCents); Serial.println(" cents"); }

    reportedCents = newReportedCents;
    haveReceivedReport = true;

    if (pulseActive && waitingForResponse) {
        consecutiveRetries = 0;
        waitingForResponse = false;
        if (reportedCents == totalTargetCents) {
            pulseActive = false;
            if (DEBUG_ENABLED) Serial.println("Pitch: target reached");
        } else {
            sendPulseNote(totalTargetCents > reportedCents);   // keep nudging
        }
    } else if (!pulseActive && reportedCents != totalTargetCents) {
        startPulseSequence();
    }
}

void pitchManagerOnUsbMounted() {
    // Host just appeared; make sure it has the current tuning.
    if (PITCH_SEND_TUNING_SYSEX) sendTuningSysEx(totalTargetCents);
    if (PITCH_PULSE_ENABLED && haveReceivedReport && !pulseActive) startPulseSequence();
}

void pitchManagerManualUp() {
    if (manualOffset >= MANUAL_OFFSET_MAX) return;
    manualOffset++;
    saveManualOffset();
    recalcAndApply();
}

void pitchManagerManualDown() {
    if (manualOffset <= MANUAL_OFFSET_MIN) return;
    manualOffset--;
    saveManualOffset();
    recalcAndApply();
}

void pitchManagerManualReset() {
    manualOffset = 0;
    saveManualOffset();
    pulseActive        = false;
    waitingForResponse = false;
    consecutiveRetries = 0;
    recalcAndApply();
}

int  getManualOffsetCents()   { return manualOffset; }
int  getTotalTargetCents()    { return totalTargetCents; }
int  getReportedOffsetCents() { return reportedCents; }
bool pitchHaveReport()        { return haveReceivedReport; }

float getTargetFrequencyHz() {
    return PITCH_A_REFERENCE_HZ * powf(2.0f, (float)totalTargetCents / 1200.0f);
}

