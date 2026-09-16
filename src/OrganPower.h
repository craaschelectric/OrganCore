// OrganPower.h
// Console power control: the supply keep-alive, the console power switch, and
// an orderly shutdown of the whole instrument including a host computer.
//
// This exists because a retrofit console often does not own its own power. The
// Rodgers 760 that prompted it latches its supply through a relay that the
// control computer has to hold in, and its sample engine runs on a Raspberry Pi
// whose mains is switched by the same supply. Cut that supply while the Pi is
// writing and you corrupt its card. So "switch the organ off" is a sequence,
// not a contact.
//
// THE SEQUENCE
//   1. General cancel, so nothing is left speaking while the amplifiers die.
//   2. POWER_SHUTDOWN_SYSEX, if the console has one -- a message to whatever
//      else needs telling. On Opus 62 that is the pipe driver's blower relay.
//   3. Lamps shifted out, so the console goes visibly dark.
//   4. POWER_HOST_SHUTDOWN_PIN driven LOW, asking the host to halt.
//   5. Wait for the host to finish: either POWER_HOST_READY_PIN falling, or
//      POWER_HOST_HALT_MS elapsing, whichever comes first.
//   6. POWER_KEEPALIVE_PIN driven LOW. The relay releases, the supply dies, and
//      everything downstream of it goes with it.
//
// EVERY PART IS OPTIONAL. A console with no supply relay, no power switch, no
// host and no SysEx sets the sentinels and this module does nothing at all.
//
// WHY powerPoll() GOES IN EVERY BLOCKING SCREEN'S PUMP
// The console power switch must work whenever the console is powered, not only
// when loop() happens to be running. The library's blocking screens -- the
// startup wait, the config menu, expression calibration, piston assignment --
// already pump usbMIDI.read(), tempSensorPoll() and pitchManagerPoll() for
// exactly this reason. powerPoll() joins that list. Without it the organist
// cannot switch the instrument off while it sits waiting for the sample engine,
// which is precisely when they most want to.
//
// THE READINESS LINE, AND WHY IT MATTERS MORE THAN IT LOOKS
// Asking a host to halt before it is listening is worse than not asking. The
// request goes nowhere, the timeout expires anyway, and the console cuts mains
// from a machine in the middle of booting -- the exact failure the orderly
// shutdown exists to prevent.
//
// POWER_HOST_READY_PIN is the host saying "a shutdown request will be honoured
// now". It is ACTIVE LOW and read with INPUT_PULLUP: the host pulls it down to
// claim ready, and releasing it -- at halt, at a crash, or by never being wired
// at all -- lets our pull-up report not-ready. A floating input would report
// ready at random, and a random ready cuts mains from a machine that was never
// listening.
//
// It gates the power switch: while the line is high the switch is ignored and a
// debug line says so. It also serves at the other end, because a host that
// releases it as it halts tells the console the halt has begun, which is a real
// signal where POWER_HOST_HALT_MS is only a guess.
//
// Set POWER_HOST_READY_PIN to 255 and there is no wire and no pin is read. The console then treats
// "ready" as "setup() has finished", which means the power switch does nothing
// until the startup wait has been satisfied and the shutdown always runs the
// full POWER_HOST_HALT_MS. Safe, and no worse than having no host at all -- but
// it is the reason to run the wire.
#ifndef ORGANPOWER_H
#define ORGANPOWER_H

#include <Arduino.h>

// Call as the FIRST statement in setup(), before anything that can block.
// Drives the keep-alive pin high, taking over from whatever momentary switch
// the organist is holding, and parks the host shutdown pin hi-Z. Everything
// after it in setup() -- storage mount, a first-boot combination format, the
// startup wait -- may block for a long time, and the supply has to stay up
// through all of it.
void powerInit();

// Call as the LAST statement in setup(). Tells this module the console is
// running. With no POWER_HOST_READY_PIN wired this is what enables the power
// switch; with one wired it is recorded but the pin decides.
void powerBootComplete();

// Call from loop() AND from every blocking screen's pump. Reads the power
// switch, applies the arming latch and the hold timer, and runs the shutdown
// when both are satisfied. Returns immediately when there is no power switch.
void powerPoll();

// Run the shutdown sequence directly, without the switch. For a console that
// wants to power down from a piston, a MIDI command or a menu entry. Does not
// return: the last thing it does is release the supply.
void powerShutdownNow();

#endif // ORGANPOWER_H
