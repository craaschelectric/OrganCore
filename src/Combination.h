// Combination.h  -  combination action API, shared by both back-ends.
//
// Both back-ends are always compiled; COMBINATION_HOST_OWNED (ConfigData) picks
// one at run time (see PistonHandler.cpp):
//   host-owned: combinationInit() does nothing and the capture/recall/etc. calls
//               are unused — pistons are handled Hauptwerk-side in PistonHandler.cpp.
//   local:      these run the local combination action, reading/writing the
//               storage file and driving stops through StopHandler. The local
//               piston processing (CombinationSD.cpp) calls them.
//
// The state below is defined in CombinationSD.cpp in either mode, so a display
// module can link against it unconditionally.
#ifndef ORGANCORE_COMBINATION_H
#define ORGANCORE_COMBINATION_H

#include "OrganCore.h"

// ---- State for the display module ----
// false = SD missing/unreadable, combination disabled (stops/keys/expression
//         still work). When false, combinationErrorText names the fault.
extern bool        combinationAvailable;
extern uint16_t    combinationMemoryLevel;   // current memory level 0..1023
extern const char* combinationErrorText;     // non-null only when unavailable

// ---- Lifecycle ----
// Host-owned: no-op. Local: restore remembered level, mount the card, open/validate the
// combination file (blank it on a magic/version/cap mismatch, create it
// zero-filled if absent). Leaves combinationAvailable set accordingly.
void combinationInit();

// ---- Local back-end actions (used only when the console owns its combinations) ----
void combinationCapture(uint8_t pistonIndex);  // SET+piston: snapshot in-scope stops
void combinationRecall(uint8_t pistonIndex);   // piston: apply stored in-scope stops
void combinationCancel();                       // GC: cancel all non-immune stops
void combinationMemStep(int16_t delta);         // change level (wrap), persist
void combinationMemZero();                       // level 0, persist

#endif // ORGANCORE_COMBINATION_H
