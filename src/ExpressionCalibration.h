// ExpressionCalibration.h
// EEPROM-backed per-expression analog min/max calibration.
// Replaces the const exprAnalogMin/Max arrays from ConfigData as the LIVE
// source read by ExpressionHandler. Those const arrays remain the defaults.
//
// On first boot (blank/invalid signature): defaults to 0/1023 for all inputs
// and writes them to EEPROM. On subsequent boots: loads the stored values.
// The config screen saves updated values via expressionCalibrationSave().
//
// The library compiles separately from the sketch, so it can't see the
// sketch's Config.h EEPROM addresses. The sketch passes them in at init time
// (same pattern as scanSerialSamAttach / serialMidiAttach). The signature is
// written LAST so a partial write leaves it absent and forces a re-default.

#ifndef EXPRESSION_CALIBRATION_H
#define EXPRESSION_CALIBRATION_H

#include "OrganCore.h"

// Runtime calibration values — read by ExpressionHandler instead of the const
// ConfigData arrays. Index matches exprAnalogPin[], exprMidiCC[], etc.
extern uint16_t calibratedExprMin[MAX_EXPRESSIONS];
extern uint16_t calibratedExprMax[MAX_EXPRESSIONS];

// Load calibration from EEPROM. If no valid signature is found, seeds the live
// arrays from the ConfigData defaults (exprAnalogMin/Max) and writes them.
// Call before expressionInit().
//   signatureAddr  EEPROM address of the uint16 validity signature
//   signatureValue expected signature value
//   dataAddr       EEPROM address of the first min/max pair
void expressionCalibrationInit(int signatureAddr, uint16_t signatureValue, int dataAddr);

// Write new min/max values to EEPROM and update the live calibratedExpr* arrays.
// newMin[] and newMax[] must be MAX_EXPRESSIONS long.
void expressionCalibrationSave(const uint16_t newMin[], const uint16_t newMax[]);

#endif // EXPRESSION_CALIBRATION_H
