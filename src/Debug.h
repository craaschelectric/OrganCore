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

// Enable/disable debug output at compile time
#define DEBUG_ENABLED 1

#if DEBUG_ENABLED

void debugInit();
void debugPrintInputs();
void debugPrintOutputs();
void debugReportChanges();     // Print CWB of every changed input bit
void debugCheckOutputChanges(); // Compare outputBuffer to saved copy, print if changed

#else

inline void debugInit() {}
inline void debugPrintInputs() {}
inline void debugPrintOutputs() {}
inline void debugReportChanges() {}
inline void debugCheckOutputChanges() {}

#endif

#endif // DEBUG_H
