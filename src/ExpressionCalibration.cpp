// ExpressionCalibration.cpp
// Loads and saves expression pedal calibration data from/to EEPROM.
//
// EEPROM layout (addresses supplied by the sketch at init):
//   [signatureAddr]  uint16_t  validity signature
//   [dataAddr]       for each expression slot 0..MAX_EXPRESSIONS-1:
//                      uint16_t  calibratedExprMin[i]
//                      uint16_t  calibratedExprMax[i]
//
// Data is written before the signature so a partial write leaves the signature
// absent, forcing a re-default on the next boot.

#include "ExpressionCalibration.h"
#include "Debug.h"
#include <EEPROM.h>

uint16_t calibratedExprMin[MAX_EXPRESSIONS];
uint16_t calibratedExprMax[MAX_EXPRESSIONS];

// The addresses are captured at init so save() can reuse them without the
// sketch passing them again.
static int      eepromSignatureAddr  = 0;
static uint16_t eepromSignatureValue = 0;
static int      eepromDataAddr       = 0;

void expressionCalibrationInit(int signatureAddr, uint16_t signatureValue, int dataAddr) {
    eepromSignatureAddr  = signatureAddr;
    eepromSignatureValue = signatureValue;
    eepromDataAddr       = dataAddr;

    uint16_t sig = 0;
    EEPROM.get(eepromSignatureAddr, sig);

    if (sig == eepromSignatureValue) {
        int addr = eepromDataAddr;
        for (uint8_t i = 0; i < MAX_EXPRESSIONS; i++) {
            EEPROM.get(addr, calibratedExprMin[i]);
            addr += sizeof(uint16_t);
            EEPROM.get(addr, calibratedExprMax[i]);
            addr += sizeof(uint16_t);
        }
#if DEBUG_ENABLED
        Serial.println("Calibration: loaded from EEPROM");
        for (uint8_t i = 0; i < NUM_EXPRESSIONS; i++) {
            Serial.print("  Expr "); Serial.print(i);
            Serial.print("  min="); Serial.print(calibratedExprMin[i]);
            Serial.print("  max="); Serial.println(calibratedExprMax[i]);
        }
#endif
    } else {
        // No valid stored calibration: default to the full ADC range (0..1023)
        // so the installer must calibrate, and persist that with a signature.
#if DEBUG_ENABLED
        Serial.println("Calibration: no valid EEPROM data, writing defaults");
#endif
        for (uint8_t i = 0; i < MAX_EXPRESSIONS; i++) {
            calibratedExprMin[i] = 0;
            calibratedExprMax[i] = 1023;
        }
        expressionCalibrationSave(calibratedExprMin, calibratedExprMax);
    }
}

void expressionCalibrationSave(const uint16_t newMin[], const uint16_t newMax[]) {
    for (uint8_t i = 0; i < MAX_EXPRESSIONS; i++) {
        calibratedExprMin[i] = newMin[i];
        calibratedExprMax[i] = newMax[i];
    }

    int addr = eepromDataAddr;
    for (uint8_t i = 0; i < MAX_EXPRESSIONS; i++) {
        EEPROM.put(addr, calibratedExprMin[i]);
        addr += sizeof(uint16_t);
        EEPROM.put(addr, calibratedExprMax[i]);
        addr += sizeof(uint16_t);
    }
    // Write signature last so an interrupted write leaves it absent.
    uint16_t sig = eepromSignatureValue;
    EEPROM.put(eepromSignatureAddr, sig);

#if DEBUG_ENABLED
    Serial.println("Calibration: saved to EEPROM");
    for (uint8_t i = 0; i < NUM_EXPRESSIONS; i++) {
        Serial.print("  Expr "); Serial.print(i);
        Serial.print("  min="); Serial.print(calibratedExprMin[i]);
        Serial.print("  max="); Serial.println(calibratedExprMax[i]);
    }
#endif
}
