// TempSensor.cpp
// ASCII-degrees-C temperature input, ported from Opus 62. Reads the shared
// Serial8 Rx and drives PitchManager on offset change.

#include "TempSensor.h"
#include "TuningConfig.h"
#include "PitchManager.h"
#include "Debug.h"
#include <stdlib.h>   // atof
#include <math.h>     // lround

#if ORGAN_HAS_TUNING

static HardwareSerial* tempPort = nullptr;
static char    lineBuf[16];
static uint8_t lineLen = 0;
static float   currentDegC      = 0.0f;   // set to TEMP_REFERENCE_DEGC in tempSensorAttach()
static int     currentCentOffset = 0;

void tempSensorAttach(HardwareSerial& port) {
    tempPort = &port;        // sketch already called begin() on the shared Serial8
    currentDegC = TEMP_REFERENCE_DEGC;   // runtime init avoids static-init-order across TUs
}

// Parse one completed line ("23.5") into degrees and update the offset.
static void handleLine() {
    if (lineLen == 0) return;
    lineBuf[lineLen] = '\0';

    // Require the line to actually look like a number: at least one digit and
    // nothing but digits / sign / decimal point / spaces. Without this a garbled
    // line makes atof() return 0.0, which reads as a real 0 C (a huge cold offset).
    bool hasDigit = false;
    for (uint8_t i = 0; i < lineLen; i++) {
        char ch = lineBuf[i];
        if (ch >= '0' && ch <= '9') hasDigit = true;
        else if (ch != '.' && ch != '-' && ch != '+' && ch != ' ') {
            if (DEBUG_ENABLED) { Serial.print("TempSensor: ignoring non-numeric \""); Serial.print(lineBuf); Serial.println("\""); }
            return;
        }
    }
    if (!hasDigit) return;

    float degC = (float)atof(lineBuf);
    if (degC < -40.0f || degC > 85.0f) {
        if (DEBUG_ENABLED) { Serial.print("TempSensor: ignoring out-of-range \""); Serial.print(lineBuf); Serial.println("\""); }
        return;
    }

    currentDegC = degC;
    int newOffset = (int)lround((degC - TEMP_REFERENCE_DEGC) * CENTS_PER_DEGREE);

    if (newOffset != currentCentOffset) {
        if (DEBUG_ENABLED) {
            Serial.print("TempSensor: "); Serial.print(degC, 1);
            Serial.print(" C, offset "); Serial.print(currentCentOffset);
            Serial.print(" -> "); Serial.print(newOffset); Serial.println(" cents");
        }
        currentCentOffset = newOffset;
        pitchManagerOnTempChange();
    }
}

void tempSensorPoll() {
    if (!tempPort) return;
    while (tempPort->available()) {
        char c = (char)tempPort->read();
        if (c == '\n' || c == '\r') {
            handleLine();
            lineLen = 0;
        } else if (lineLen < sizeof(lineBuf) - 1) {
            lineBuf[lineLen++] = c;
        } else {
            lineLen = 0;   // overrun - drop the garbled line
        }
    }
}

int   getTempOffsetCents() { return currentCentOffset; }
float getTempDegC()        { return currentDegC; }

#endif // ORGAN_HAS_TUNING
