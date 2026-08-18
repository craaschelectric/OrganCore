# OrganCore

Shared embedded firmware core for Craasch Electric organ consoles (Teensy 4.1, Arduino/C++).
One library, many instruments: a console is a thin sketch that supplies `Config.h` +
`ConfigData.cpp` and wires up USB-MIDI; everything else lives here.

## Layout

The repo root is the Arduino library. Drop it in your sketchbook's `libraries/OrganCore/`.

- `src/` — the library modules (`.h`/`.cpp` pairs)
- `CoreConfig.h` — instrument-**invariant** constants: array caps, address macros
  (`0xCWB`, `ADDR_DISABLED`), chain/piston/stop enums, and the combination-mode ids.
- `OrganConfig.h` — the per-instrument **contract**: every instrument symbol declared
  `extern const`, defined by the sketch's `ConfigData.cpp`. Static arrays are sized by the
  `MAX_*` caps in `CoreConfig.h`, so `ConfigData` values need not be compile-time constants.
  **Include-order rule:** `ConfigData.cpp` must include the contract header *before* it
  defines the consts, or they won't get external linkage.

## How a console is built

A sketch supplies `Config.h` (build options), `ConfigData.cpp` (the instrument tables), and
the `usbMIDI` NoteOn/Off/SysEx handlers, then calls each module's `xxxInit()` in `setup()` and
`xxxPoll()`/process step in `loop()`. The library never names a serial port or calls `begin()`;
the sketch owns every `begin()` and passes ports in via `xxxAttach()`.

Key modules: `ScanChain` (CWB input/output buffers + `readInput`/`inputChanged`/`setOutput`),
`StopHandler` (stop state, SAM coils, MIDI), `PistonHandler` / `CombinationSD` (pistons +
combination action), `KeyboardHandler`, `ExpressionHandler`, `Crescendo`, `MidiOut` (the single
USB-MIDI transmit path; `midiOutNoteOn(note, vel, channel)` with a **1-based** channel),
`SerialMidi`, `DisplayManager`, `StartupScreen`, `PersistentConfig`, plus optional
`PitchManager`/`TempSensor` behind `ORGAN_HAS_TUNING`.

## Combination action

Selected at compile time by `ORGAN_COMBINATION_MODE` in `CombinationConfig.h`:

- `COMBINATION_MODE_HW` — pistons are Hauptwerk-side (piston → MIDI note → the PC engine builds
  the registration and commands stops back).
- `COMBINATION_MODE_SD` — pistons are **local**. Capture/recall run on the controller against a
  fixed-record SD file (`COMB.DAT`): 1024 memory levels × 128 piston records × 64-byte
  (512-bit) stop bitsets, at `offset(level, piston)`. SET+piston captures; the piston alone
  recalls. A Next/Prev sequencer walks the general list and wraps by stepping the memory level.

**Capture scope is computed, never stored.** A general captures every stop flagged
`STOP_IN_GENERALS`; a divisional captures `STOP_IN_DIVISIONALS` stops whose `stopDivision`
matches the piston's division. `stopDivision` (control scope) and the capture flags (mask
membership) are independent — a stop can be in a division yet excluded from its divisional.

## Stop truth

`stopCommandedState[]` holds each stop's state. A stop with a valid coil address is a **SAM**
stop (physical sense is truth; coils fire on mismatch with pulse/retry). A stop with **no coil
address** is commanded-is-truth: `stopSetState()` sends its MIDI directly. Incoming stop MIDI
from Hauptwerk is dispatched to `onStopMidiReceived(channel, note, on)`, which resolves
`(channel, note) → stop` via `midiToStopIndex` and updates state.

## Fully-virtual (Hauptwerk-touch) console recipe

No new engine code — it's configuration:

- Every stop has **no** coil/sense/light address (all `ADDR_DISABLED`) → commanded-is-truth.
  Its input arrives over MIDI, not the scan. (`STOP_SCREEN` is a defined-but-unused flag; the
  no-coil condition is what matters.)
- `stopMidiChannel`/`stopMidiNote` across the two stop channels; `STOP_IN_GENERALS` /
  `STOP_IN_DIVISIONALS` + `stopDivision` as the masks; pistons in the CWB table;
  `ORGAN_COMBINATION_MODE = COMBINATION_MODE_SD`.
- Stops on Hauptwerk's own touch page, so `NUM_SCREEN_STOPS = 0` (the local TFT shows only
  combination status).
- Incoming touches are **not** echoed (Hauptwerk already knows; echoing risks a feedback loop).
  Outbound stop MIDI happens only when the combination system recalls/cancels, which reports
  every in-scope stop. This is loop-safe by structure: `onStopMidiReceived` never emits, and the
  only emitter (`stopSetState`) is called only by local recall/cancel.

## Changelog

- **1.1.0** — `MAX_PISTONS` 48 → 128 (== `COMBO_PISTON_CAP`) and `MAX_SEQUENCER_PISTONS`
  24 → 64, so a virtual console can carry up to 64 generals per memory level plus divisionals.
  Backward-compatible: only grows RAM arrays; never moves a stored SD record.
- **1.0.0** — Baseline (Opus 62 line): HW/SD combination back-ends, SAM + light + screen stops,
  crescendo, expression, keyboards, tuning, startup handshake.

## Dependencies

Arduino `SD` library (Teensy build) for the SD combination store. Treated as an external
dependency, not vendored here.
