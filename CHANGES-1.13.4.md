<style>
body, p, li { font-size: 10pt; line-height: 1.15; }
</style>

# OrganCore 1.13.4 — the USB link monitor no longer reports the first enumeration as a lost link

A one-function bug fix in `OrganDiag`. No contract change and no sketch change required.

## The bug

`diagBootComplete()` armed the USB link monitor with whatever USB state existed at that instant. `setup()` normally finishes within a second or two of power-on, before the host has finished enumerating the Teensy, so the monitor recorded "not configured". A few seconds later the host completed enumeration, the monitor saw not-configured become configured, and treated it as a link restored after a loss. Every cold boot showed the title-bar banner "MIDI LINK LOST - RESTART ORGAN", re-initialized the display, and logged a "USB link restored" line with no "USB link lost" line before it. The "restored after N ms" figure was the time since power-on, because the loss time had never been set (seen on Opus 60: restored after 4172 ms at +00:00:04).

## The fix

A loss or a recovery only counts once USB has been configured at least once since boot. The first enumeration is silently accepted as the baseline. A drop after that, and its recovery, are logged and shown exactly as before.

Files: `OrganDiag.cpp`, `library.properties`.

## Checked

The monitor code from `OrganDiag.cpp` was run on a host against scripted USB states: arming before enumeration then enumeration at 4 s gives no banner and no log lines; enumeration before arming then a real 3 s drop gives "lost" and "restored after 3000 ms" and the banner; late enumeration then a real drop gives the same. Not built with the Teensy toolchain and not run on a console.

## Not covered

A host that really resets the link after the console has been running (a driver reload, a script that disables and re-enables the device) is a real link loss and is still reported.
