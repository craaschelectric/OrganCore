// Debug.h
// USB Serial debug output for scan chain diagnostics
//
// Provides:
//   debugInit()          - Print startup banner
//   debugPrintInputs()   - Print full inputBuffer for all active chains
//   debugPrintOutputs()  - Print full outputBuffer for all active chains
//   debugReportChange()  - Print CWB address of changed input bit
//   debugCheckOutputs()  - Detect and print outputBuffer changes

#ifndef DEBUG_H
#define DEBUG_H

#include "OrganCore.h"
#include "ScanChain.h"

// Enable/disable debug output at compile time. Default ON (1) so debug
// output "just works" for anyone who doesn't override it. Per-instrument
// override belongs in that sketch's build flags (e.g. platform.local.txt
// build.extra_flags -DDEBUG_ENABLED=0), same pattern as ORGAN_HAS_TUNING --
// this can't come from Config.h/ConfigData.cpp because the Arduino IDE
// compiles the library separately from the sketch.
#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED 1
#endif

#if DEBUG_ENABLED

void debugInit();
void debugPrintInputs();
void debugPrintOutputs();
void debugReportChanges();     // Print CWB of every changed input bit
void debugCheckOutputChanges(); // Compare outputBuffer to saved copy, print if changed
void debugPrintTouch(int screenX, int screenY, bool released); // Print a touch event's resolved screen coords

#else

inline void debugInit() {}
inline void debugPrintInputs() {}
inline void debugPrintOutputs() {}
inline void debugReportChanges() {}
inline void debugCheckOutputChanges() {}
inline void debugPrintTouch(int, int, bool) {}

#endif

#endif // DEBUG_H
