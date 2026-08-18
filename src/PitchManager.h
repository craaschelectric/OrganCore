// PitchManager.h
// Pitch offset + tuning. Total offset = temperature offset (cents, from
// TempSensor) + manual trim (cents, EEPROM-backed). On any change it drives the
// configured tuning transport(s) and updates the tuning screen's readout.
//
// GrandOrgue owns the pitch: the pulse-feedback loop nudges GO up/down and reads
// its reported offset back (via SysExParser -> pitchManagerOnReportedOffset),
// retrying until reported == target. A once-per-second startup bootstrap nudge
// (from Opus 65) makes GO emit its first report so the loop can arm. For a
// Hauptwerk host the MTS SysEx is sent open-loop alongside. Both are selected in
// TuningConfig.h.
//
// Ported from Opus 65 (feedback + bootstrap) and Opus 62 (manual reset, target
// frequency). Two changes from those: the manual trim persists in EEPROM (not
// ESP32 Preferences), and MIDI goes out through MidiOut (usbMIDI + pipe mirror).

#ifndef PITCHMANAGER_H
#define PITCHMANAGER_H

#include <Arduino.h>

// eepromManualOffsetAddr: EEPROM address of the int8 manual trim (validated by
// range on load; no separate signature). Supplied by the sketch's Config.h, same
// injection pattern as expressionCalibrationInit().
void pitchManagerInit(int eepromManualOffsetAddr);

void pitchManagerSetup();       // load trim, push initial tuning
void pitchManagerPoll();        // call from loop(); drives pulse timing/retry/bootstrap

void pitchManagerOnTempChange();                      // from TempSensor
void pitchManagerOnReportedOffset(int reportedCents); // from SysExParser (GO report)
void pitchManagerOnUsbMounted();                      // host (re)connected: resync tuning

// Tuning screen controls
void pitchManagerManualUp();
void pitchManagerManualDown();
void pitchManagerManualReset();

// Tuning screen / display accessors
int   getManualOffsetCents();
int   getTotalTargetCents();
int   getReportedOffsetCents();
bool  pitchHaveReport();
float getTargetFrequencyHz();

#endif // PITCHMANAGER_H
