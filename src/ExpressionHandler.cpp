// ExpressionHandler.cpp

#include "ExpressionHandler.h"
#include "MidiOut.h"
#include "ExpressionCalibration.h"
#include "Debug.h"

static uint8_t exprLastSent[MAX_EXPRESSIONS];
static uint16_t exprLastRawBucket[MAX_EXPRESSIONS];

void expressionInit() {
    memset(exprLastSent, 0xFF, sizeof(exprLastSent));  // Force first send
    
    for (uint8_t i = 0; i < NUM_EXPRESSIONS; i++) {
        if (exprType[i] == EXPR_ANALOG || exprType[i] == EXPR_CRESCENDO) {
            pinMode(exprAnalogPin[i], INPUT);
        }
        // Discrete inputs are already configured by scanInit()
    }
}

void processExpressions() {
    for (uint8_t i = 0; i < NUM_EXPRESSIONS; i++) {
        uint8_t value;

        // A crescendo shoe drives stop recall (crescendoPoll), not a CC — skip.
        if (exprType[i] == EXPR_CRESCENDO) continue;

        if (exprType[i] == EXPR_ANALOG) {
            uint16_t raw = analogRead(exprAnalogPin[i]);

            // Debug: print raw value only when it moves 8+ counts away from last printed value
            if ((uint16_t)abs((int16_t)raw - (int16_t)exprLastRawBucket[i]) >= 8) {
                exprLastRawBucket[i] = raw;
                Serial.print("DBG: Expr ");
                Serial.print(i);
                Serial.print(" raw=");
                Serial.println(raw);
            }

            // Scale raw ADC value within [min, max] to 0-31. The live min/max
            // come from the EEPROM-backed calibration (defaults seeded from the
            // ConfigData exprAnalogMin/Max on first boot), set on the config
            // screen and persisted there.
            uint16_t lo = calibratedExprMin[i];
            uint16_t hi = calibratedExprMax[i];
            if (hi <= lo) {
                // Degenerate calibration (Min captured >= Max): avoid divide by
                // zero, treat the pedal as fully closed until re-calibrated.
                value = 0;
            } else {
                uint16_t clamped = raw < lo ? lo : (raw > hi ? hi : raw);
                value = (uint8_t)(((uint32_t)(clamped - lo) * 31) / (hi - lo));
            }
            
        } else {
            // EXPR_DISCRETE: count HIGH inputs in the CWB range
            uint16_t startAddr = exprDiscreteStart[i];
            uint16_t endAddr   = exprDiscreteEnd[i];
            if (!ADDR_VALID(startAddr)) continue;
            
            uint8_t chain = ADDR_CHAIN(startAddr);
            uint16_t startBit = ADDR_WORD(startAddr) * 16 + ADDR_BIT(startAddr);
            uint16_t endBit   = ADDR_WORD(endAddr) * 16 + ADDR_BIT(endAddr);
            
            uint8_t activeCount = 0;
            for (uint16_t bitIdx = startBit; bitIdx <= endBit; bitIdx++) {
                uint8_t word = bitIdx / 16;
                uint8_t bit  = bitIdx % 16;
                if ((inputBuffer[chain][word] >> bit) & 1) {
                    activeCount++;
                }
            }
            value = activeCount;
        }
        
        // Hysteresis: require value to move at least exprDeadband steps before sending
        if (abs((int16_t)value - (int16_t)exprLastSent[i]) < exprDeadband[i]) continue;
        
        // Send all intermediate values between old and new
        uint8_t ch = exprMidiChannel[i];
        uint8_t cc = exprMidiCC[i];
        
        if (exprLastSent[i] == 0xFF) {
            // First send after init — just send current value
            midiSendControlChange(cc, value, ch);
        } else if (value > exprLastSent[i]) {
            for (uint8_t v = exprLastSent[i] + 1; v <= value; v++) {
                midiSendControlChange(cc, v, ch);
            }
        } else {
            for (uint8_t v = exprLastSent[i] - 1; v != (uint8_t)(value - 1); v--) {
                midiSendControlChange(cc, v, ch);
            }
        }
        
        exprLastSent[i] = value;
    }
}
