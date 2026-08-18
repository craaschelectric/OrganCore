// PersistentConfig.h
// EEPROM-backed persistent configuration.
// Currently stores just the local-SD combination memory level, guarded by a
// magic byte that detects blank/uninitialized (or old-layout) EEPROM.

#ifndef PERSISTENT_CONFIG_H
#define PERSISTENT_CONFIG_H
#include "OrganCore.h"

#include <Arduino.h>
#include <EEPROM.h>

// ============================================================
// Defaults (used when EEPROM is blank)
// ============================================================

constexpr uint16_t DEFAULT_COMBINATION_LEVEL = 0;  // SD combination memory level at first boot

// ============================================================
// EEPROM Layout
// ============================================================
// PersistentConfig owns bytes 0..2. The expression-calibration block
// (Config.h in the sketch) starts after it at byte 3.
//
//   [0]     uint8   magic (EEPROM_MAGIC_VALUE)
//   [1..2]  uint16  combination memory level (0..1023, LSB first)
//
// The magic value is bumped whenever this layout changes, so a board carrying
// an older layout (e.g. the previous one-byte level) fails validation once and
// re-defaults into the new one.

constexpr uint16_t EEPROM_ADDR_MAGIC       = 0;
constexpr uint16_t EEPROM_ADDR_COMBO_LEVEL = 1;   // SD combination: last memory level (u16, LSB at 1, MSB at 2)
constexpr uint8_t  EEPROM_MAGIC_VALUE      = 0xA7;

// ============================================================
// Runtime State (loaded at boot)
// ============================================================

extern uint16_t configCombinationLevel;  // SD combination: last memory level (0..1023)

// ============================================================
// Functions
// ============================================================

void configLoad();   // Read from EEPROM (writes defaults if blank/old-layout)
void configSave();   // Write current values to EEPROM

// Persist just the combination memory level (called on every level change).
// Updates configCombinationLevel and writes only the two level bytes that
// changed (EEPROM.update is wear-friendly: it skips bytes already equal).
void configSaveCombinationLevel(uint16_t level);

#endif // PERSISTENT_CONFIG_H
