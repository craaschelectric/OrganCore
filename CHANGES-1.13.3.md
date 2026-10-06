<style>
body, p, li { font-size: 10pt; line-height: 1.15; }
</style>

# OrganCore 1.13.3 — stops moved by a recall now reach the host

One bug fix in `StopHandler`. No contract change and no sketch change.

## The bug

On a console with SAM stops (a drawknob with a coil and a sense contact), a combination recall or General Cancel moved the knobs but sent the host **no stop notes**. A stop moved by hand was reported; a stop moved by a piston was not, so the host kept sounding the old registration while the knobs showed the new one.

`stopSetState()` fires the coil and relied on `processStopInputs()` to report the move when the sense contact changed. But `processStopInputs()` deliberately ignores a sense change on a stop whose coil is energized, to keep contact bounce away from the engine, and the contact always changes inside the 200 ms pulse. The report was dropped every time. The comment in `stopSetState()` that said the change "is reported on a later scan" was wrong.

## The fix

`stopSetState()` now marks a SAM stop it has fired a coil for. When `checkStopRetries()` confirms the move, which happens after the pulse has ended, it sends that stop's note to the host. If the retries are exhausted and the knob never reached its position, it sends where the knob really is (sense is truth), after the existing warning.

- Nothing is sent for a stop that was already in position.
- A stop moved by a host command is not echoed back; the host already knows.
- A report made from a sense edge (a hand-drawn stop) clears the mark, so a stop is never reported twice.
- Stops with no coil (screen and lamp stops) are unchanged: `stopSetState()` already sent their note directly.
- The crescendo's engine suppression is unchanged. SAM sense reporting was never suppressed by it, and the new report follows the same rule.

Files: `StopHandler.cpp`, `StopHandler.h` (comment), `library.properties`.

## Checked

The real `StopHandler.cpp` and `ScanChain.cpp` were run on a host against a model of the three chains and 49 SAM stops, with the Opus 52 `ConfigData.h` (a low is on, a high is off):

- Before: a recall of all 49 stops and a General Cancel each sent the host 0 notes.
- After: each sent exactly one note per stop (49 note-ons, then 49 note-offs). Stops already in position sent nothing. A host-commanded move sent nothing back. A stop with a dead ON coil sent one note-off giving its actual state after the five attempts. A hand-drawn stop is still reported.

## Not verified

Not built with the Teensy toolchain and not run on a console. The model has the sense contact change while the coil is energized, as the hand-drawn evidence suggests; a contact slower than the pulse would already be re-fired by the existing retry logic.
