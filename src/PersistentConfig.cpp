// PersistentConfig.cpp

#include "PersistentConfig.h"
#include "CombinationConfig.h"   // COMBO_MEM_LEVELS, for the load-time range clamp

uint16_t configCombinationLevel = DEFAULT_COMBINATION_LEVEL;

void configLoad() {
    uint8_t magic = EEPROM.read(EEPROM_ADDR_MAGIC);

    if (magic != EEPROM_MAGIC_VALUE) {
        // EEPROM is blank, corrupted, or an older layout — write defaults.
        configCombinationLevel = DEFAULT_COMBINATION_LEVEL;
        configSave();
        Serial.println("DBG: EEPROM blank/old, wrote defaults");
        return;
    }

    configCombinationLevel = (uint16_t)EEPROM.read(EEPROM_ADDR_COMBO_LEVEL)
                           | ((uint16_t)EEPROM.read(EEPROM_ADDR_COMBO_LEVEL + 1) << 8);
    if (configCombinationLevel >= COMBO_MEM_LEVELS) configCombinationLevel = DEFAULT_COMBINATION_LEVEL;

    Serial.print("DBG: Config loaded - combo level=");
    Serial.println(configCombinationLevel);
}

void configSave() {
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC_VALUE);
    EEPROM.write(EEPROM_ADDR_COMBO_LEVEL,     configCombinationLevel & 0xFF);
    EEPROM.write(EEPROM_ADDR_COMBO_LEVEL + 1, (configCombinationLevel >> 8) & 0xFF);

    Serial.print("DBG: Config saved - combo level=");
    Serial.println(configCombinationLevel);
}

void configSaveCombinationLevel(uint16_t level) {
    configCombinationLevel = level;
    // Ensure magic is present (first-ever save path also handled by configLoad).
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC_VALUE);
    EEPROM.update(EEPROM_ADDR_COMBO_LEVEL,     level & 0xFF);          // update = write only if changed
    EEPROM.update(EEPROM_ADDR_COMBO_LEVEL + 1, (level >> 8) & 0xFF);
    Serial.print("DBG: Combination level saved = ");
    Serial.println(level);
}
