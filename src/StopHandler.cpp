// StopHandler.cpp
// Stop state management with non-blocking coil retry.
//
// Retry flow:
//   1. Coil fires → retry tracker loaded (retriesRemaining, expectedState, lastPulseMs)
//   2. Each loop: updateCoilPulses() expires finished coils
//   3. Each loop: checkStopRetries() checks stops with retriesRemaining > 0:
//      - If coil still active for this stop → skip (wait for it to finish)
//      - If sense matches expected → clear retry (success)
//      - If mismatch and retries remain → re-fire with longer pulse
//      - If mismatch and retries exhausted → log warning, clear retry

#include "StopHandler.h"
#include "MidiOut.h"
#include "Debug.h"

// ============================================================
// State
// ============================================================

bool stopCommandedState[MAX_STOPS];
ActiveCoil activeCoils[MAX_ACTIVE_COILS];
uint8_t numActiveCoils = 0;

// While the blind crescendo is engaged it becomes the sole authority for stop
// on/off MIDI to the engine: the commanded-is-truth sends here are suppressed
// (commanded state and lamps still update) so a manual-off of a crescendo-
// commanded stop can't desync the engine. The crescendo emits the OR'd
// "effective" state itself via stopSendToEngine(). Default off, so instruments
// with no crescendo are unaffected. Never suppresses SAM sense reporting.
bool stopEngineSuppressed = false;

// Lamp-to-sense settling. On a console where the lamp drive couples into that
// stop's own sense line, a lamp that has just changed can fake a contact
// closure on the next scan, which processStopInputs() would read as a real
// draw and toggle the stop straight back off. Every commanded stop change
// (recall, cancel, engine-driven) stamps this forward by
// STOP_INPUT_SETTLE_MS; stop contact edges are ignored until it expires.
// STOP_INPUT_SETTLE_MS = 0 leaves the check permanently inert.
static uint32_t stopInputSettleUntil = 0;

// ============================================================
// Retry Tracking (per stop)
// ============================================================

struct StopRetry {
    uint8_t  retriesRemaining;  // 0 = not retrying
    bool     expectedState;     // what sense should read when correct
    uint16_t lastPulseMs;       // pulse duration used on last attempt
    uint32_t debounceEndTime;   // millis() value after which sense may be read (0 = no debounce)
};
static StopRetry stopRetry[MAX_STOPS];

// ============================================================
// Helpers
// ============================================================

static inline bool isSAMStop(uint8_t i) {
    return ADDR_VALID(stopOnCoilAddr[i]);
}

static inline bool getSenseState(uint8_t i) {
    return readInput(stopSenseAddr[i]);  // 1 = ON
}

static uint16_t getBasePulseMs(uint8_t i) {
    return stopPulseMs[i] ? stopPulseMs[i] : SAM_PULSE_MS;
}

// Check if a coil is currently active for a given stop index
static bool isCoilActiveForStop(uint8_t stopIdx) {
    for (uint8_t j = 0; j < numActiveCoils; j++) {
        if (activeCoils[j].stopIndex == stopIdx) return true;
    }
    return false;
}

static void startCoilPulse(uint16_t coilAddr, uint16_t pulseMs, uint8_t stopIdx) {
    if (!ADDR_VALID(coilAddr)) return;
    if (numActiveCoils >= MAX_ACTIVE_COILS) return;

    setOutput(coilAddr, true);
    activeCoils[numActiveCoils].coilAddr = coilAddr;
    activeCoils[numActiveCoils].endTime = millis() + pulseMs;
    activeCoils[numActiveCoils].stopIndex = stopIdx;
    numActiveCoils++;
}

// Fire a coil and set up retry tracking
static void fireCoilWithRetry(uint8_t stopIdx, bool targetState) {
    uint16_t coilAddr = targetState ? stopOnCoilAddr[stopIdx] : stopOffCoilAddr[stopIdx];
    uint16_t pulseMs = getBasePulseMs(stopIdx);

    startCoilPulse(coilAddr, pulseMs, stopIdx);

    stopRetry[stopIdx].retriesRemaining = SAM_RETRY_MAX;
    stopRetry[stopIdx].expectedState = targetState;
    stopRetry[stopIdx].lastPulseMs = pulseMs;

    // Debounce: don't check sense until this long after the pulse ends
    if (stopDebounceMs[stopIdx] > 0) {
        stopRetry[stopIdx].debounceEndTime = millis() + pulseMs + stopDebounceMs[stopIdx];
    } else {
        stopRetry[stopIdx].debounceEndTime = 0;
    }

    Serial.print("DBG: Stop ");
    Serial.print(stopIdx);
    Serial.print(targetState ? " ON" : " OFF");
    Serial.print(" coil fired, pulse=");
    Serial.print(pulseMs);
    Serial.println("ms");
}

static void sendStopMidi(uint8_t stopIdx, bool on) {
    uint8_t ch   = stopMidiChannel[stopIdx];
    uint8_t note = stopMidiNote[stopIdx];
    if (note == 0xFF) return;
    if (on) midiOutNoteOn(note, 127, ch + 1);
    else    midiOutNoteOff(note, 0, ch + 1);
}

// Authoritative stop MIDI send that bypasses stopEngineSuppressed. Used by the
// crescendo overlay to emit the OR'd effective state while it owns the engine.
void stopSendToEngine(uint16_t stopIndex, bool on) {
    if (stopIndex >= NUM_STOPS) return;
    sendStopMidi((uint8_t)stopIndex, on);
}

static int16_t midiToStopIndex(uint8_t channel, uint8_t note) {
    for (uint16_t i = 0; i < NUM_STOPS; i++) {
        if (stopMidiChannel[i] == channel && stopMidiNote[i] == note) return (int16_t)i;
    }
    return -1;
}

// ============================================================
// Initialization
// ============================================================

void stopInit() {
    memset(stopCommandedState, 0, sizeof(stopCommandedState));
    memset(stopRetry, 0, sizeof(stopRetry));
    numActiveCoils = 0;

    // Fire OFF coils for all SAM stops with retry tracking
    for (uint8_t i = 0; i < NUM_STOPS; i++) {
        if (isSAMStop(i)) {
            fireCoilWithRetry(i, false);  // Target: OFF
        }
    }
}

// ============================================================
// Process Stop Inputs (called after each scan)
// ============================================================

void processStopInputs() {
    for (uint8_t i = 0; i < NUM_STOPS; i++) {
        if (!ADDR_VALID(stopSenseAddr[i])) continue;
        if (!inputChanged(stopSenseAddr[i])) continue;

        if (isSAMStop(i)) {
            // Suppress sense reporting while coil is active or debounce window hasn't elapsed.
            // Prevents reed bounce during/after actuation from being sent to Hauptwerk.
            if (isCoilActiveForStop(i)) continue;
            if (stopRetry[i].debounceEndTime != 0 && millis() < stopRetry[i].debounceEndTime) continue;

            bool physicalState = getSenseState(i);
            sendStopMidi(i, physicalState);
        } else {
            // A lamp that just changed can couple into its own sense line and
            // look exactly like a contact closure. Ignore edges while settling.
            if ((int32_t)(millis() - stopInputSettleUntil) < 0) continue;

            bool currentInput = readInput(stopSenseAddr[i]);
            bool prevInput = readInputPrev(stopSenseAddr[i]);
            if (currentInput && !prevInput) {
                stopCommandedState[i] = !stopCommandedState[i];
                // Lamp still follows via buildStopOutputs(); the engine send is
                // suppressed while the crescendo owns the effective state.
                if (!stopEngineSuppressed) sendStopMidi(i, stopCommandedState[i]);
            }
        }
    }
}

// ============================================================
// Command a stop by index (local-SD combination recall / cancel)
// ============================================================

void stopSetState(uint16_t stopIndex, bool on) {
    if (stopIndex >= NUM_STOPS) return;
    uint8_t i = (uint8_t)stopIndex;

    stopCommandedState[i] = on;
    stopInputSettleUntil = millis() + STOP_INPUT_SETTLE_MS;

    if (isSAMStop(i)) {
        // SAM: fire the coil only if physical sense disagrees. The sense change
        // is reported to the PC engine by processStopInputs() on a later scan,
        // so no separate MIDI send is needed here.
        if (on != getSenseState(i)) {
            fireCoilWithRetry(i, on);
        }
    } else {
        // Screen/light stop: commanded is truth. Mirror to the PC engine now
        // (unless the crescendo owns the engine — it will emit the effective
        // OR'd state, which includes this change); buildStopOutputs() reflects
        // any lamp on the next output build regardless.
        if (!stopEngineSuppressed) sendStopMidi(i, on);
    }
}

// ============================================================
// MIDI Reception from Hauptwerk
// ============================================================

void onStopMidiReceived(uint8_t channel, uint8_t note, bool on) {
    int16_t idx = midiToStopIndex(channel, note);
    if (idx < 0) return;

    uint8_t i = (uint8_t)idx;
    stopCommandedState[i] = on;
    stopInputSettleUntil = millis() + STOP_INPUT_SETTLE_MS;

    if (isSAMStop(i)) {
        bool physicalState = getSenseState(i);
        if (on != physicalState) {
            fireCoilWithRetry(i, on);
        }
    }
}

// ============================================================
// Coil Pulse Timing
// ============================================================

void updateCoilPulses() {
    uint32_t now = millis();
    for (uint8_t i = 0; i < numActiveCoils; ) {
        if (now >= activeCoils[i].endTime) {
            setOutput(activeCoils[i].coilAddr, false);
            activeCoils[i] = activeCoils[--numActiveCoils];
        } else {
            i++;
        }
    }
}

// ============================================================
// Retry Check (non-blocking, call each loop after updateCoilPulses)
// ============================================================

void checkStopRetries() {
    for (uint8_t i = 0; i < NUM_STOPS; i++) {
        if (stopRetry[i].retriesRemaining == 0) continue;
        if (!isSAMStop(i)) continue;

        // Don't check sense while a coil is still pulsing for this stop
        if (isCoilActiveForStop(i)) continue;

        // Don't check sense until debounce period has elapsed
        if (stopRetry[i].debounceEndTime != 0 && millis() < stopRetry[i].debounceEndTime) continue;

        // Coil pulse has ended — check sense
        bool senseNow = getSenseState(i);
        if (senseNow == stopRetry[i].expectedState) {
            // Success
            Serial.print("DBG: Stop ");
            Serial.print(i);
            Serial.print(" sense confirmed ");
            Serial.print(stopRetry[i].expectedState ? "ON" : "OFF");
            Serial.print(" (");
            Serial.print(SAM_RETRY_MAX - stopRetry[i].retriesRemaining);
            Serial.println(" retries used)");
            stopRetry[i].retriesRemaining = 0;
            continue;
        }

        // Mismatch — retry if attempts remain
        stopRetry[i].retriesRemaining--;

        if (stopRetry[i].retriesRemaining == 0) {
            // Exhausted
            Serial.print("DBG: WARNING Stop ");
            Serial.print(i);
            Serial.print(" sense mismatch after ");
            Serial.print(SAM_RETRY_MAX);
            Serial.print(" retries. Expected=");
            Serial.print(stopRetry[i].expectedState ? "ON" : "OFF");
            Serial.print(" Actual=");
            Serial.println(senseNow ? "ON" : "OFF");
            continue;
        }

        // Re-fire with increased pulse duration
        uint16_t newPulseMs = stopRetry[i].lastPulseMs + SAM_RETRY_PULSE_INCREMENT_MS;
        stopRetry[i].lastPulseMs = newPulseMs;

        uint16_t coilAddr = stopRetry[i].expectedState
                            ? stopOnCoilAddr[i] : stopOffCoilAddr[i];

        Serial.print("DBG: Stop ");
        Serial.print(i);
        Serial.print(" retry ");
        Serial.print(SAM_RETRY_MAX - stopRetry[i].retriesRemaining);
        Serial.print("/");
        Serial.print(SAM_RETRY_MAX);
        Serial.print(" pulse=");
        Serial.print(newPulseMs);
        Serial.println("ms");

        startCoilPulse(coilAddr, newPulseMs, i);
    }
}

// ============================================================
// Build Stop Outputs
// ============================================================

void buildStopOutputs() {
    for (uint8_t i = 0; i < NUM_STOPS; i++) {
        if (ADDR_VALID(stopLightAddr[i])) {
            setOutput(stopLightAddr[i], stopCommandedState[i]);
        }
    }
}
