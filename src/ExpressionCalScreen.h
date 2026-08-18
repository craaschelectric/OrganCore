#ifndef EXPRESSIONCALSCREEN_H
#define EXPRESSIONCALSCREEN_H

// ============================================================
// ExpressionCalScreen.h - TUI expression-pedal calibration screen
// ============================================================
// Shows the live raw ADC value for each analog input and lets the user capture
// the heel (Min) and toe (Max) endpoints per pedal, then Save (writes EEPROM
// via expressionCalibrationSave) or Cancel. Runs as a self-contained blocking
// screen invoked from the config menu; the main scan loop is paused while it is
// open (displayScanChainsActive() returns false in SCREEN_CONFIG), which is
// fine during calibration since the instrument is not being played.

#include <Arduino.h>

void expressionCalScreenRun();

#endif // EXPRESSIONCALSCREEN_H
