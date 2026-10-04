<style>
body, p, li { font-size: 10pt; line-height: 1.15; }
</style>

# OrganCore 1.13.1 — crescendo programming poll no longer fires in normal operation

A one-function bug fix. No contract change, no sketch change required.

## The bug

`crescendoProgrammingPoll()` stores a crescendo level on every press edge of the console SET piston. It had no check for which screen was showing, so a sketch that called it every scan pass (as `Crescendo.h` says to) stored a crescendo level on **every SET press during normal operation**. `crescendoProgStore()` then auto-incremented the programming level (clamped at 31) and recalled it, and each press also rewrote a `CB_`-style crescendo record on the card.

It needs `crescendoAvailable`, which is true whenever the crescendo file opens on the storage medium. A shoe is not required, so a console with no crescendo shoe at all still stored and recalled levels on SET. Seen on Opus 60 (OrganCore 1.9.0 era; the function is unchanged through 1.13.0).

## The fix

`crescendoProgrammingPoll()` now returns immediately unless `currentScreen == SCREEN_CRESCENDO`, the screen the console SET is meant to store from. The piston-driven programming screen (`TOUCH_ENABLED` false) calls `crescendoProgStore()` directly and is unaffected.

Consoles that already call it only while programming see no change. A console with no crescendo may now omit the call entirely.

Files: `Crescendo.cpp`, `Crescendo.h`, `library.properties`.

## Cleaning up a card hit by the bug

Crescendo records written by stray SET presses stay on the card (`CRESC.DAT` on 1.9.x and earlier, the per-level crescendo files from 1.10.0). On a console that has a crescendo, re-program the levels. On one that does not, delete them.
