// TuningConfig.h
// The instrument-config contract for the pitch/temperature subsystem.
//
// There is no compile-time gate here any more. ORGAN_HAS_TUNING used to be a
// code-elimination switch living in this header, which meant a builder with
// several consoles in flight had to edit a LIBRARY file to move between them --
// the library stopped being common. Arduino build flags can't fix that either:
// platform.local.txt is global to the machine, not per sketch.
//
// So the tuning code now always compiles, and whether a console HAS pipes is an
// ordinary contract value like every other per-instrument fact:
// ORGAN_TUNING_PRESENT. A pipeless console sets it false, never calls
// pitchManagerInit() or tempSensorAttach(), and gets no "Tuning / Temperature"
// entry in the config menu. It carries the tuning code as dead flash -- a few KB
// on a part with eight megabytes -- and nothing else.
//
// A pipeless sketch does not have to write out the whole contract below by hand:
// include <TuningDefaults.h> once from its ConfigData.h and every symbol here is
// defined at an inert value.

#ifndef TUNING_CONFIG_H
#define TUNING_CONFIG_H

#include <Arduino.h>

// ---- Instrument tuning contract (define these in ConfigData.h/.cpp) ----

// Does this console have pipes to keep in tune? False disables the tuning
// screen's menu entry and makes pitchManagerInit() / tempSensorAttach() no-ops,
// so a stray call can't arm anything.
extern const bool     ORGAN_TUNING_PRESENT;

// Transport selection: pulse-feedback keeps GrandOrgue the source of truth;
// the MTS SysEx is the open-loop Hauptwerk path. Either or both may be on.
extern const bool     PITCH_PULSE_ENABLED;       // GrandOrgue feedback loop
extern const bool     PITCH_SEND_TUNING_SYSEX;   // Hauptwerk MTS alongside

// Offsets
extern const int8_t   MANUAL_OFFSET_MIN;         // cents
extern const int8_t   MANUAL_OFFSET_MAX;         // cents
extern const float    TEMP_REFERENCE_DEGC;       // 0-cents reference temperature
extern const uint8_t  CENTS_PER_DEGREE;          // pitch drift per degree C
extern const float    PITCH_A_REFERENCE_HZ;      // Hz = A_REF * 2^(cents/1200)

// Pulse-feedback transport (GrandOrgue)
extern const uint8_t  PITCH_UP_MIDI_NOTE;
extern const uint8_t  PITCH_DOWN_MIDI_NOTE;
extern const uint8_t  PITCH_PULSE_MIDI_CH;       // 0-based, like every other channel
                                                 // (renamed from PITCH_PULSE_MIDI_CHANNEL
                                                 // at 1.8.0 when it stopped being 1-based)
extern const uint8_t  PITCH_PULSE_ON_MS;         // note-on to note-off
extern const uint16_t PITCH_PULSE_TIMEOUT_MS;    // no-response retry window

// LCD number GrandOrgue reports its current pitch offset on. Must be distinct
// from the displayLineLCD[] values so the report isn't taken for a status line;
// SysExParser routes a match here to pitchManagerOnReportedOffset().
extern const uint8_t  PITCH_SYSEX_LCD_NUM;

#endif // TUNING_CONFIG_H
