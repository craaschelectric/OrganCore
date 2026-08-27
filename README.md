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

## Builder piston assignment

A touchscreen flow (`Config → Assign Pistons`) that lets a builder assign the console's
pistons and control buttons **in the field, without recompiling**, by parking on a logical
function and pressing the physical button(s) that should trigger it. Compiled only for
local-capture on SD-card media (`ORGAN_COMBINATION_MODE == COMBINATION_MODE_SD` and
`ORGAN_COMBINATION_MEDIA != COMBINATION_MEDIA_SPIFLASH`) **and** when the feature is enabled
by `ORGAN_ENABLE_PISTON_ASSIGN` (default `1`) — the three together define the guard
`ORGANCORE_HAS_REMAP_STORE`. Setting `ORGAN_ENABLE_PISTON_ASSIGN 0` in `CombinationConfig.h`
(or `-D` on the build) compiles out the whole feature — store, screen, menu entry, and
REMAP.DAT loading — and `applyRemaps()` uses only the const `remapFrom[]`/`remapTo[]` from
`OrganConfig.h`. Choose that on consoles whose input map is fully defined in config data
(e.g. Opus 62), where field reassignment is neither needed nor wanted. `ORGANCORE_HAS_REMAP_STORE` is computed in `CombinationConfig.h` and every
file that uses the feature guards its `#include`s on it, so a disabled build
does not need the feature's source files (`RemapStore.*`, `PistonAssignScreen.*`,
`PistonAssignSlots.h`) present at all. In HW mode or on
SPI-flash media the feature also compiles out and `applyRemaps()` reads the const arrays
exactly as before.

**Canonical virtual slots.** Every assignable function has one canonical address on a reserved
virtual chain (`REMAP_SLOT_CHAIN`, index 11 — the top of the widened `MAX_CHAINS = 12` space,
so real chains growing upward from 0 never collide). Calibration never rewrites the piston list;
it only appends remap entries `{pressed physical addr → slot addr}`. `applyRemaps()` then funnels
each press onto its slot before any handler runs, so the runtime piston/handler code is
unchanged and simply reads the canonical slots it always read. Multiple physical buttons can
funnel onto one slot (a General Cancel on both the rail and a toe stud). Captures come from
**all input chains including virtual ones** (a MIDI pedalboard's embedded pistons report through
a virtual chain and must be assignable); only the slot chain is excluded, being a destination.

**The slot layout is frozen**, exactly like the combination file's record layout: stored `to`
addresses are computed from it, so changing a cap or stride renumbers slots and silently
corrupts stored assignments. Layout on the slot chain (211 of 256 bits used), walked in order:
7 known controls (Set, General Cancel, Next, Previous, Mem+, Mem−, Shift), 64 generals, 128
divisionals (8 divisions × 16, division-major, stride 16, frozen order **0 Pedal, 1 Great,
2 Swell, 3 Choir, 4 Solo**, then further divisions), 12 spare controls (reserved, renameable,
walked last). To grow later: append a **new** block at a fresh range, never widen one in place,
and bump `REMAP_FORMAT_VERSION`.

**Screen controls.** *Next* advances one function; *Next Block* jumps to the next division (or
region) and is always available with no validation, so a builder who wants, say, the pedal
divisional positions as extra generals leaves the Pedal block empty, advances to the Generals
block, and assigns those physical pedal buttons there. *Clear* drops the current function's
assignments; *Start Over* empties the table; *Save* writes `REMAP.DAT`; *Cancel* discards.
Presses capture on the rising edge, one per press (per-address release-debounce). Each capture
prints a serial line naming the function and the captured CWB address; the builder sees only the
function name and a live count of buttons assigned to it.

**Storage.** `REMAP.DAT` on the same card as `COMB.DAT` (magic `OCRM`, versioned, with the slot
chain and `MAX_REMAPS` cap validated). Loaded by `remapStoreInit()`, called at the end of
`combinationInit()` once the card is mounted — no new sketch ordering. **A missing file means
never-calibrated** → the const remap defaults stay in effect; **a valid zero-count file means
deliberately-cleared** (Start Over) → the live empty table wins; a foreign/old/truncated file is
blanked to zero-count.

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

- **1.4.0** — **Builder piston assignment** (see the section above): a touchscreen
  `Config → Assign Pistons` flow that funnels builder-pressed physical pistons onto frozen
  canonical virtual slots via an SD-backed remap table (`REMAP.DAT`), for local-capture on
  SD-card media, gated by the new `ORGAN_ENABLE_PISTON_ASSIGN` switch (default `1`; set `0`
  to compile the feature out entirely on fully-config-defined consoles like Opus 62). The
  three conditions — local-capture mode, SD-card media, and the enable flag — together
  define the guard `ORGANCORE_HAS_REMAP_STORE` that all the feature's files key on. Enabling
  constants changed, all
  backward-compatible in the append-only sense (grow RAM/format caps, never move a stored
  record): `MAX_CHAINS` 8 → 12 with the CWB chain field widened to 4 bits; `ADDR_DISABLED`
  `0x800` → `0xF00` (0x800 became a real address — chain 8 — once chains 8–11 exist);
  `MAX_REMAPS` 16 → 256 (now `uint16_t`); new frozen `REMAP_SLOT_*` layout constants; new
  `RemapStore`, `PistonAssignScreen`, and pure/host-tested `PistonAssignSlots`. **Sketch
  migration** (`ConfigData.cpp`): define `NUM_REMAPS` as `uint16_t`, add `NUM_DIVISIONS`,
  replace any raw `0x800` disabled-address literals with `ADDR_DISABLED`, and add
  `static_assert(NUM_CHAINS <= REMAP_SLOT_CHAIN, "...")`. `applyRemaps()` selects the live vs
  const source at compile time; HW and SPI-flash builds are unaffected. The
  `ORGANCORE_HAS_REMAP_STORE` guard is computed in `CombinationConfig.h` and every
  consumer guards its `#include`s on it, so a disabled build needs none of the
  feature's source files present.

  Also fixes three config-screen bugs found on first hardware bring-up, independent
  of the piston feature: (a) **Expression Calibration** and **Tuning / Temperature**
  touch was unresponsive because both screens repainted their full readout every
  loop iteration, flooding the SPI bus and starving touch sampling — they now repaint
  only on change (tuning) or on a timer (expression, whose ADC readout jitters), and
  sample touch every loop, matching the run/crescendo reactive-repaint discipline;
  (b) Tuning's −/+/Reset buttons overlapped the text block and were erased by the
  readout blit — moved below it; (c) Expression's readout ran across the Min buttons —
  the Min/Max buttons moved right of a wider text field; (d) the **Crescendo Program**
  full paint didn't clear the tab-grid region, so config-menu text bled through the
  gaps between stop tabs — it now clears that region first. And a second layout pass
  after further bring-up: the title-bar height constant `TITLE_H` was 24 but the TUI
  draws a 32px bar, so the run/crescendo memory band and everything derived from it sat
  under the bar — corrected to 32, which cascades to every derived coordinate and
  auto-fits the tabs; the run and crescendo full paints now clear the ENTIRE display
  space on entry so nothing from the previous screen survives in the gaps; the
  config-menu and Expression buttons (which are center-anchored) were re-anchored to
  `displaySpaceTopY` so their top edges clear the bar; and the crescendo control band
  (`CR_BTN_Y`, previously a hardcoded 30) now derives from `MEM_BAND_Y` so it clears the
  bar and tracks `TITLE_H`.

- **1.3.0** — First-boot format now shows a "Preparing Memory" screen with a progress
  bar (the QSPI-flash format blanks the full 8 MB and can take ~1-3 min). A new
  `displayReady` flag makes the progress draw a no-op until `displayInit()` has run, so
  it is safe regardless of order. **To see the bar, the sketch must call `displayInit()`
  before `combinationInit()`.**

- **1.2.0** — Storage medium is now selectable with `ORGAN_COMBINATION_MEDIA`
  (`COMBINATION_MEDIA_SD` default, or `COMBINATION_MEDIA_SPIFLASH` for the Teensy 4.1
  on-board QSPI flash via LittleFS). The combination file layout is byte-identical on
  both media; only `begin()` and the filesystem handle differ. Assumes a 16 MB QSPI
  part, which holds the full 8 MB file. Combination logic unchanged.

- **1.1.0** — `MAX_PISTONS` 48 → 128 (== `COMBO_PISTON_CAP`) and `MAX_SEQUENCER_PISTONS`
  24 → 64, so a virtual console can carry up to 64 generals per memory level plus divisionals.
  Backward-compatible: only grows RAM arrays; never moves a stored SD record.
- **1.0.0** — Baseline (Opus 62 line): HW/SD combination back-ends, SAM + light + screen stops,
  crescendo, expression, keyboards, tuning, startup handshake.

## Dependencies

Arduino `SD` library (Teensy build) for the SD combination store. Treated as an external
dependency, not vendored here.
