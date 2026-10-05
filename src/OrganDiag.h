// OrganDiag.h  -  field diagnostics (OrganCore 1.13.0).
//
// Written after a console in service went to a grey screen and stopped playing
// mid-hymn, with nothing afterwards to say why. These are the things that would
// have answered it, recorded where they survive the organist's power-cycle.
//
// WHAT IT RECORDS, in DIAG.LOG on the console's own storage medium:
//   - every boot, with its number and the CPU's reset cause (power-on, software,
//     watchdog, lockup, over-temperature) -- and the Teensy CrashReport if the
//     previous run faulted
//   - every USB link drop and recovery after the console has finished booting.
//     A drop means the host re-enumerated the Teensy; GrandOrgue does not re-open
//     a MIDI port that went away, so after a drop the engine no longer hears the
//     console even though the console is running normally
//   - every display re-initialization (see below)
//   - a startup wait that timed out (STARTUP_WAIT_TIMEOUT_MS)
//
// Each line is stamped with the boot number and the time since that boot, e.g.
//     B47 +01:23:05 USB link lost
// There is no wall-clock time: the Teensy's RTC only survives a power-off with a
// coin cell on VBAT, and these consoles have none.
//
// The log is capped at DIAG_LOG_MAX_BYTES; when full it is renamed DIAG.OLD and a
// fresh DIAG.LOG begun, so the most recent history is always on the console. It
// lives on LittleFS (QSPI) or SD, whichever OrganStorage mounted. LittleFS never
// overwrites in place, so a brownout mid-write loses at most the line being
// written -- which matters, since a brownout is exactly when a line is written.
//
// HOW TO READ IT
//   - On the console: SET + General Cancel -> Diagnostics (piston-driven consoles).
//   - Over USB serial: send 'D'. The Teensy prints DIAG.OLD then DIAG.LOG. With
//     the Teensy plugged into the console's Pi, this works from anywhere you can
//     SSH to the Pi:   stty -F /dev/ttyACM0 115200 raw; cat /dev/ttyACM0 &
//                      printf D > /dev/ttyACM0
//
// DISPLAY RE-INITIALIZATION
// After a USB link recovery the panel is re-initialized (the same supply dip that
// drops USB can brown out a VUSB-powered panel), and the title bar reads "MIDI
// LINK LOST - RESTART ORGAN" until the next power-up, because only restarting the
// engine brings its MIDI input back.
//
// 1.13.0 also had a periodic panel readback (DISPLAY_READBACK_ENABLED). It needed
// TeensyUserInterface's private ILI9341 object and did not compile, so it was
// removed in 1.13.2. The contract value remains and is ignored.

#ifndef ORGANDIAG_H
#define ORGANDIAG_H

#include <Arduino.h>

// Call in setup() right after powerInit() and Serial.begin(). Captures the reset
// cause before anything can clear it, mounts storage, bumps the boot counter and
// logs the boot.
void diagInit();

// Call as the last statement of setup(), beside powerBootComplete(). Arms the USB
// link monitor -- a link that is still enumerating during boot is not a drop.
void diagBootComplete();

// Call every loop with atRunScreen = true, and from every blocking screen's pump
// with atRunScreen = false. The USB monitor and the serial dump run everywhere;
// display re-initialization only happens at the run screen, so a menu screen is
// never wiped out from under itself (a re-init found elsewhere waits until the
// console is back at the run screen).
void diagPoll(bool atRunScreen);

// Append one line to DIAG.LOG (and echo it on Serial).
void diagLog(const char* text);

// True after a USB link drop and recovery this boot. The run screen's title bar
// shows a warning while it is set; it clears only at the next power-up.
extern bool diagLinkLostNotice;

// Boot number of this run (from DIAG.CNT), for the Diagnostics screen.
extern uint32_t diagBootNumber;

// Diagnostics screen support: read the last lines of DIAG.LOG into a caller
// buffer. Returns the number of lines found (most recent last).
uint16_t diagReadLog(char* buffer, uint16_t bufferSize, char** lines, uint16_t maxLines);

#endif // ORGANDIAG_H
