// StopHandler.h
// Stop state management: sense inputs, SAM coil control, MIDI communication
//
// For SAM stops: physical sense is truth. Hauptwerk commands fire coils
//   only when commanded state differs from physical state.
//   After coil pulse ends, sense is checked. If mismatch, coil re-fires
//   up to SAM_RETRY_MAX times with increasing pulse duration.
// For Light stops: commanded state is truth. Light output reflects state.

#ifndef STOP_HANDLER_H
#define STOP_HANDLER_H

#include "OrganCore.h"
#include "ScanChain.h"

// ============================================================
// State
// ============================================================

// What Hauptwerk has commanded each stop to be (true=ON, false=OFF)
extern bool stopCommandedState[MAX_STOPS];

// When true, the commanded-is-truth stop sends (processStopInputs / stopSetState)
// are suppressed because the blind crescendo owns the engine's stop state and
// emits the OR'd effective state via stopSendToEngine(). SAM sense reporting is
// never suppressed. Set/cleared by the crescendo module.
extern bool stopEngineSuppressed;

// Authoritative per-stop on/off MIDI send that ignores stopEngineSuppressed.
// The crescendo overlay uses this to drive the effective (base OR level) state.
void stopSendToEngine(uint16_t stopIndex, bool on);

// Active coil tracking
struct ActiveCoil {
    uint16_t coilAddr;
    uint32_t endTime;
    uint8_t  stopIndex;    // Which stop this coil belongs to (0xFF = no retry tracking)
};
extern ActiveCoil activeCoils[MAX_ACTIVE_COILS];
extern uint8_t numActiveCoils;

// ============================================================
// Functions
// ============================================================

// Initialize: clear commanded state, fire OFF coils for all SAMs
// with retry until sense confirms all stops are off.
void stopInit();

// Process stop inputs: compare inputBuffer to inputBufferPrev for all stops.
// For SAM stops: if sense changed, send MIDI to Hauptwerk.
// For Light stops: if toggle input pressed, flip commanded state and send MIDI.
void processStopInputs();

// Command a stop to a target state by index (used by local-SD combination
// recall and cancel). Sets commanded state; for a SAM stop it fires the coil
// only if sense differs (the tested pulse/retry path), and the resulting sense
// change is what reports the new state to the PC engine over MIDI. For a screen
// or light stop (commanded-is-truth) it sends the stop MIDI directly and lets
// buildStopOutputs() drive any lamp.
void stopSetState(uint16_t stopIndex, bool on);

// Handle incoming MIDI from Hauptwerk for a stop.
// Determines stop index from channel/note, updates commanded state,
// and fires coils or updates lights as needed.
void onStopMidiReceived(uint8_t channel, uint8_t note, bool on);

// Update coil pulse timing. Call from main loop.
// Turns off coils whose pulse time has expired.
void updateCoilPulses();

// Check stops with active retries. Call from main loop after updateCoilPulses().
// If a coil pulse has ended and sense doesn't match expected state,
// re-fires with increased pulse duration. Non-blocking.
void checkStopRetries();

// Build stop outputs into outputBuffer.
// Sets coil and light bits based on current state.
void buildStopOutputs();

#endif // STOP_HANDLER_H
