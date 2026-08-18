// Crescendo.h  -  blind crescendo overlay + level programming.
//
// A crescendo is an analog shoe (an expression slot typed EXPR_CRESCENDO) that
// selects one of 31 stored stop combinations (levels 1..31; 0 = off). Each
// level is a full-registration bitmap on the SD card, in its OWN file
// (CRESC.DAT), reusing the combination record layout so a future PC editor
// agrees.
//
// OPERATION is BLIND: the console lamps/drawstops never move. On every change
// (shoe moving, or the organist changing the base registration) the module
// computes  effective = stopCommandedState OR levelRecord[N]  and sends only
// the stops that changed in that OR'd result to the engine. While engaged it is
// the SOLE sender of stop MIDI (stopEngineSuppressed) so a manual-off of a
// crescendo-commanded stop can't desync the engine; commanded state and lamps
// still track the organist's own draws. At level 0 the effective state is just
// the base, so releasing the shoe removes exactly the crescendo-added stops.
//
// PROGRAMMING (the non-blocking SCREEN_CRESCENDO screen) is NOT blind: recall
// writes commanded state + lamps so the organist can see and edit a level. The
// organist draws a registration, presses SET (on-screen button or the console
// SET piston), which stores the current in-scope registration to the displayed
// level and auto-increments (clamped at 31). Navigating up/down recalls a
// stored level; an all-zero (never-set) level leaves the console unchanged so a
// progressive crescendo can be built by adding to the previous level.
//
// Capture/recall scope is STOP_IN_GENERALS (console-wide), matching a general.

#ifndef ORGANCORE_CRESCENDO_H
#define ORGANCORE_CRESCENDO_H

#include "OrganCore.h"

constexpr uint8_t CRESC_MAX_LEVEL = 31;   // levels 1..31; 0 = crescendo off

// ---- State for the display module ----
extern bool    crescendoAvailable;    // false = SD file missing/unreadable (programming + operation disabled)
extern uint8_t crescendoLevel;        // live operational level 0..31 (0 = off)
extern uint8_t crescendoProgLevel;    // level currently shown on the programming screen (1..31)

// ---- Lifecycle ----
// Find the EXPR_CRESCENDO shoe slot and the console SET piston, then open/
// validate CRESC.DAT (blank on magic/version/cap mismatch, create zero-filled
// if absent). Call after combinationInit() (SD is mounted there).
void crescendoInit();

// ---- Operation (blind overlay) ----
// Read the shoe, track engage/level changes, and emit the OR'd effective state.
// No-op unless currentScreen == SCREEN_OPERATIONAL. Gate the call site by
// EXPR_ENABLED (the shoe must be wired/terminated). Call every loop.
void crescendoPoll();

// ---- Programming screen support ----
void crescendoProgEnter();            // enter programming: reset to level 1, recall if set
void crescendoProgExit();             // leave programming (overlay resumes in operation)
void crescendoProgNav(int8_t delta);  // up/down: step displayed level 1..31 (clamp), recall if set
void crescendoProgStore();            // SET: store current in-scope registration to the displayed level, then auto-increment
// Poll the console SET piston edge while programming (call in the scan block,
// before saveInputState). On-screen SET calls crescendoProgStore() directly.
void crescendoProgrammingPoll();

#endif // ORGANCORE_CRESCENDO_H
