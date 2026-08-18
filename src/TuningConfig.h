// TuningConfig.h
// The ONE compile-time knob for the pitch/temperature subsystem, plus the
// instrument-config contract for it.
//
// ORGAN_HAS_TUNING gates the whole subsystem as a code-elimination switch: when
// 0, none of the tuning code compiles or links and a pipeless console carries
// zero tuning weight (the common case). Because the Arduino IDE compiles the
// library separately from the sketch, this gate has to live in a library header
// rather than the sketch's Config.h -- same constraint as CombinationConfig.h.
// It is the only tuning value that does; everything else that used to sit here
// is now instrument config, supplied by the sketch's ConfigData.cpp through the
// extern contract below (like exprAnalogPin[] / displayLineLCD[] in
// OrganConfig.h). Set ORGAN_HAS_TUNING to 1 for a pipe instrument and define the
// contract in that instrument's ConfigData.cpp.

#ifndef TUNING_CONFIG_H
#define TUNING_CONFIG_H

#include <Arduino.h>

#ifndef ORGAN_HAS_TUNING
#define ORGAN_HAS_TUNING 0
#endif

#if ORGAN_HAS_TUNING
// ---- Instrument tuning contract (define these in ConfigData.cpp) ----
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
extern const uint8_t  PITCH_PULSE_MIDI_CHANNEL;  // 1-based (usbMIDI convention)
extern const uint8_t  PITCH_PULSE_ON_MS;         // note-on to note-off
extern const uint16_t PITCH_PULSE_TIMEOUT_MS;    // no-response retry window

// LCD number GrandOrgue reports its current pitch offset on. Must be distinct
// from the displayLineLCD[] values so the report isn't taken for a status line;
// SysExParser routes a match here to pitchManagerOnReportedOffset().
extern const uint8_t  PITCH_SYSEX_LCD_NUM;
#endif // ORGAN_HAS_TUNING

#endif // TUNING_CONFIG_H
