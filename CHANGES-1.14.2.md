<style>
body, p, li { font-size: 10pt; line-height: 1.15; }
</style>

# OrganCore 1.14.2 — the blind crescendo no longer writes into the drawn registration

Bug fix in `StopHandler`, `Crescendo`, `CoreConfig.h`. No contract change and no sketch change.

## The bug

On a console whose stops live on the host (Opus 57), moving the crescendo shoe made stops flip on and off, and stops the crescendo had added stayed on after the shoe was closed.

The crescendo overlay sends its stops to Hauptwerk with `stopSendToEngine()`. Hauptwerk reports every stop change back out on the same note, including a change it made because the controller told it to. `onStopMidiReceived()` took each of those echoes for the organist drawing the stop and wrote it into `stopCommandedState[]`, the drawn registration. So:

- A stop added by the crescendo became "drawn", and releasing the shoe left it on.
- An echo that arrived after the overlay had already sent the opposite state (a shoe moved faster than the round trip) re-added a stop the crescendo had just removed, which sent it again, and so on.
- Set stored the overlay, not the drawn registration, while a level was engaged.

The stored crescendo levels were never the cause: a level is only a bitmap of stops added on top of the drawn registration.

## The fix

- `stopSendToEngine()` records when it last sent each stop. `onStopMidiReceived()` ignores a stop note that arrives within `STOP_ECHO_IGNORE_MS` (new in `CoreConfig.h`, 300 ms) of that send. The echo changes nothing: not the drawn registration, and not a SAM knob.
- A stop note outside that window is the organist. It updates the drawn registration as before, and the new `crescendoEngineStopChanged()` records that the host is now in that state, so the overlay resends if it still wants the stop otherwise. A touch cannot remove a stop the crescendo or tutti is adding; it snaps back.
- The crescendo logs `DBG: Crescendo level N` on the debug serial port each time the level changes, so a noisy shoe is visible.

The ignore applies only to stops the overlay sent. A SAM stop moved by hand is still reported and still updates the commanded state from the host's reply, as before.

## What the screens do

Hauptwerk's stop screen shows the same switches the crescendo operates, so the screens still show the stops the crescendo adds while it is engaged. What no longer changes is the drawn registration. A truly invisible crescendo needs the sample set to expose a second set of switches that engage ranks without moving the stop switches.

Files: `src/StopHandler.cpp`, `src/Crescendo.cpp`, `src/Crescendo.h`, `src/CoreConfig.h`, `library.properties`, `README.md`, `docs/OPERATING-MODES.md`.

## Checked

The real `StopHandler.cpp` and `Crescendo.cpp` were run on a host against a model of Hauptwerk that holds stop state and echoes every change back after 20 ms, with six virtual stops and two stored levels. Thirteen checks, in four scenarios: the shoe up to a level and back (the engine ends on exactly the drawn stops, and the drawn registration is unchanged); the shoe flapped 1, 0, 2, 0 faster than the echo; a touch on a crescendo-added stop (it snaps back), on an unused stop (it is drawn) and on a drawn stop (it goes off); and a host that does not echo. On 1.14.1 the same test fails eight of the thirteen checks, with the crescendo stops left on and in the drawn registration; on 1.14.2 all thirteen pass.

## Not verified

Not built with the Teensy toolchain and not run on a console. The 300 ms window assumes the host's echo returns well inside it; a touch on the same stop inside the window after a crescendo change is dropped. A shoe that is electrically noisy at a level boundary can still step between two adjacent levels; the new level log line shows whether that is happening.
