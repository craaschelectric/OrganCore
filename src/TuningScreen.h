// TuningScreen.h
// Dedicated tuning/temperature screen for this instrument, reached from its own
// entry in the config menu (separate from Expression Calibration). Shows live
// temperature, temperature offset, manual trim, and the resulting total /
// frequency, plus the reported-vs-target readout from the GrandOrgue feedback
// loop. Trim +/- and Reset drive PitchManager; Back returns to the menu.
//
// Blocking by design, like ExpressionCalScreen: only reached while the main scan
// loop is paused (SCREEN_CONFIG). Temperature stays live because the loop polls
// tempSensorPoll() inside the screen.

#ifndef TUNINGSCREEN_H
#define TUNINGSCREEN_H

#include <Arduino.h>

void tuningScreenRun();

#endif // TUNINGSCREEN_H
