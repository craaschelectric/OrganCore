// TuningDefaults.h
// The tuning contract, defined at inert values, for a console with no pipes.
//
// Since the tuning subsystem stopped being a compile-time gate, every sketch
// defines the TuningConfig.h contract -- but a pipeless console has nothing
// meaningful to say about pipe temperature or pitch drift, and writing thirteen
// dummy constants into each ConfigData.h by hand is exactly the kind of
// boilerplate that goes stale. Include this instead, ONCE, from the same file
// that defines the rest of the contract:
//
//     #include <OrganCore.h>
//     #include <TuningDefaults.h>     // no pipes on this console
//
// It defines every symbol, with ORGAN_TUNING_PRESENT false: no "Tuning /
// Temperature" entry in the config menu, pitchManagerInit() and
// tempSensorAttach() no-op if something calls them anyway, and the SysEx pitch
// report is never matched. The tuning code still links; it just never runs.
//
// A console that DOES have pipes must not include this file. It defines the
// contract itself in ConfigData.h with real values and ORGAN_TUNING_PRESENT
// true -- see Opus 62 / Opus 65.
//
// These are definitions, not declarations, so this header belongs in exactly
// one translation unit. Including it twice is a duplicate-symbol link error,
// which is the failure you want rather than a silent one.

#ifndef ORGANCORE_TUNINGDEFAULTS_H
#define ORGANCORE_TUNINGDEFAULTS_H

#include "TuningConfig.h"   // the extern declarations these definitions satisfy

const bool     ORGAN_TUNING_PRESENT     = false;   // no pipes: the whole subsystem stays asleep

const bool     PITCH_PULSE_ENABLED      = false;   // no GrandOrgue feedback loop
const bool     PITCH_SEND_TUNING_SYSEX  = false;   // no Hauptwerk MTS

const int8_t   MANUAL_OFFSET_MIN        = 0;
const int8_t   MANUAL_OFFSET_MAX        = 0;
const float    TEMP_REFERENCE_DEGC      = 20.0f;
const uint8_t  CENTS_PER_DEGREE         = 0;       // no drift to model
const float    PITCH_A_REFERENCE_HZ     = 440.0f;

const uint8_t  PITCH_UP_MIDI_NOTE       = 0;
const uint8_t  PITCH_DOWN_MIDI_NOTE     = 0;
const uint8_t  PITCH_PULSE_MIDI_CHANNEL = 1;
const uint8_t  PITCH_PULSE_ON_MS        = 0;
const uint16_t PITCH_PULSE_TIMEOUT_MS   = 0;

// 0xFF is not a legal Hauptwerk LCD number, so no incoming SysEx can be taken
// for a pitch report even if the guards above were somehow bypassed.
const uint8_t  PITCH_SYSEX_LCD_NUM      = 0xFF;

#endif // ORGANCORE_TUNINGDEFAULTS_H
