# OrganCore — Operating Modes

Reference for OrganCore **v1.7.0** (`github.com/craaschelectric/OrganCore`).

OrganCore is one Arduino library serving every console. A console is a thin sketch that
supplies `Config.h` + `ConfigData.cpp` and wires the USB-MIDI handlers; the library holds
the behaviour. What differs between consoles is expressed as *modes* — a small number of
selectable behaviours, almost all of them ordinary `extern const` config values rather than
build flags.

Only **two** compile-time switches remain in the whole library:

| Switch | Where | Why it can't be config data |
|---|---|---|
| `ORGAN_COMBINATION_MODE` | `CombinationConfig.h` | `PistonHandler.cpp` and `CombinationSD.cpp` define the same symbols; compiling both is a duplicate-symbol link error |
| `DEBUG_ENABLED` | `Debug.h` | The prints and their format strings have to not exist |

Everything else below is decided at run time from `ConfigData.cpp`, so moving between
consoles never means editing a library header.

---

## 1. Combination action mode

The primary mode axis: **who owns the combination action.** Selected by
`ORGAN_COMBINATION_MODE` in `CombinationConfig.h` (default `COMBINATION_MODE_SD`, or
`-DORGAN_COMBINATION_MODE=`). Exactly one back-end links; there is no runtime dispatch.

### 1a. `COMBINATION_MODE_HW` — host-owned combinations

`PistonHandler.cpp` compiles. Pistons are pure input: a press sends a MIDI NoteOn on
`PISTON_MIDI_CHANNEL` and the PC engine (Hauptwerk) builds the registration and commands
stops back over MIDI. `combinationInit()` is a no-op and `combinationAvailable` is always
false. No card, no local storage.

Behaviour in this mode:

- **SHIFT** is a modifier. While held, `SHIFT_NOTE_OFFSET` is added to the next piston's
  note; the note actually sent is remembered so the NoteOff matches even if SHIFT was
  released in between. Pressed and released alone, SHIFT sends its own note.
- **SET** sends NoteOn on press / NoteOff on release and raises `setHeld` for the display.
- **Sequencer.** Next/Previous walk `sequencerPistonList[]`, pulsing each general's note
  (NoteOn then NoteOff — there is no physical press to release). At either end it sends
  MEM+/MEM− MIDI, waits `SEQUENCER_WRAP_DELAY_MS` (blocking, so the host can change level),
  then fires the wrapped-to general. `SEQUENCER_DEBOUNCE_MS` rate-limits Next/Prev.

A HW-mode console may still have a full physical stop action — coils, sense, lamps all
work as in section 3. The mode says where combinations live, not whether stops are real.

### 1b. `COMBINATION_MODE_SD` — local combinations

`CombinationSD.cpp` compiles and supplies **both** the combination API and the piston
processing (`pistonInit`/`processPistons`), since the HW piston file is compiled out.
Capture and recall run on the Teensy and drive stops through `stopSetState()`, reusing the
tested coil pulse/retry path.

- **Gestures.** SET held + a general or divisional piston captures. The piston alone
  recalls. GC cancels every stop not flagged `STOP_GC_IMMUNE`. Recall is *absolute*:
  in-scope stops are set to the stored state; out-of-scope stops are left untouched.
- **Scope is computed, never stored.** A general covers every stop flagged
  `STOP_IN_GENERALS`. A divisional covers `STOP_IN_DIVISIONALS` stops whose `stopDivision`
  matches the piston's `pistonDivision`. `stopDivision` (control scope) and the capture
  flags (mask membership) are independent — a stop can belong to a division and still be
  excluded from that division's divisional.
- **Memory levels.** `combinationMemStep(±1)` wraps and persists; `combinationMemZero()`
  returns to level 0. The level is remembered in EEPROM via `PersistentConfig` and restored
  at boot.
- **Sequencer.** Next/Previous walk the general list, recalling each; at the ends they step
  one memory level and wrap — the local equivalent of the HW-mode behaviour.
- **File.** `COMB.DAT`, one flat fixed-record binary: 1024 memory levels × 128 piston
  records × 64-byte (512-bit) stop bitsets, LSB-first, at
  `offset = 16 + (level × 128 + piston) × 64` = 8 MB. Header magic `OCMB`. Caps are fixed,
  not the live `NUM_STOPS`/`NUM_PISTONS`, so growing the stop or piston tables never moves
  an existing record — **provided growth is append-only.** Inserting or reordering indices
  silently corrupts every stored registration.
- **First boot.** A missing file, or a magic/version/cap mismatch, blanks the whole 8 MB.
  See the "Preparing Memory" screen in section 6.

---

## 2. Combination storage medium

Runtime, from `COMBINATION_USE_SPIFLASH` in the config contract. Both back-ends are always
compiled; `combinationInit()` picks one at mount and points `comboFS` at it. The file
layout is byte-identical on both media — only `begin()` differs.

- **`false` — SD card.** `SD.begin(COMBINATION_SD_CS)`, defaulting to `BUILTIN_SDCARD`
  (override with `-DCOMBINATION_SD_CS=<pin>` for an external SPI card).
- **`true` — on-board QSPI flash.** `LittleFS_QSPIFlash` on the Teensy 4.1 back-side pads.
  Assumes a 16 MB part, which holds the full 8 MB file with no geometry change.

The setting covers **every** file the console keeps — `COMB.DAT`, `CRESC.DAT` and
`REMAP.DAT` — so a flash console keeps its crescendo and its builder piston assignment.
(Before 1.7.0 the latter two were stranded on the card and a flash console silently lost
both.) The mount itself lives in `OrganStorage`: `organStorageMount()` is idempotent and
`organFS` points at whichever medium came up, so `combinationInit()` and `crescendoInit()`
can run in either order and only the first one actually mounts.

Switching an existing console from card to flash does **not** migrate anything — it starts
with fresh, blank files on the new medium.

### Degraded mode

If the medium won't mount or the file won't open, `combinationAvailable` goes false and
`combinationErrorText` names the fault (`SD CARD MISSING`, `SPI FLASH MISSING`,
`COMB FILE ERROR`). This is a real operating mode, not a crash: stops, keyboards, expression
and MIDI all keep working; only capture/recall is out, and the display shows the fault.

---

## 3. Stop truth models

Per-stop, decided at run time by address validity — not by a flag and not by a build
option. The single test is `isSAMStop(i) = ADDR_VALID(stopOnCoilAddr[i])`.

### 3a. SAM stop — physical sense is truth

The stop has a valid on-coil address.

- A command (from the host or from a local recall) fires a coil **only** if sense disagrees.
- The coil pulses for `stopPulseMs[i]` (falling back to `SAM_PULSE_MS`). When the pulse
  ends, and after `stopDebounceMs[i]`, sense is checked; on mismatch the coil re-fires up
  to `SAM_RETRY_MAX` times, each with pulse lengthened by `SAM_RETRY_PULSE_INCREMENT_MS`.
  Exhaustion logs a warning and gives up. The whole retry path is non-blocking.
- The console never announces a SAM stop's state directly. The **sense change** is what
  reports to the engine, so what the engine sees is always what the drawknob physically did.
- Sense reporting is suppressed while a coil is active or its debounce window is open, so
  reed bounce during actuation never reaches the engine.

### 3b. Commanded-is-truth stop — lamp or screen stop

No valid on-coil address. `stopCommandedState[]` is the authority.

- `stopSetState()` sends the stop's MIDI directly.
- A sense input, if the stop has one, acts as a **toggle** on the rising edge (a touch tab,
  a momentary tab) rather than as a position report.
- `buildStopOutputs()` drives `stopLightAddr[]` from commanded state, so a lamp follows the
  stop including after a combination recall.
- `STOP_INPUT_SETTLE_MS` guards the case where a lamp driver couples into that stop's own
  sense line and fakes a contact closure: contact edges are ignored for that long after any
  commanded change. Set it to 0 on a console with no observed coupling.

> `STOP_SCREEN` (0x10) is defined in `CoreConfig.h` but is **vestigial** — no library logic
> reads it. The no-coil condition is what makes a stop commanded-is-truth.

### 3c. Fully-virtual console (Hauptwerk-touch stops)

Not new code — a configuration of 3b. Every stop has `ADDR_DISABLED` for coil, sense and
light, so stop input arrives over MIDI rather than the scan, and the stops themselves live
on Hauptwerk's own touch page (`NUM_SCREEN_STOPS = 0`; the local TFT shows combination
status only).

Incoming touches are **not echoed** — Hauptwerk already knows what it just touched, and
echoing risks a feedback loop. This is loop-safe by structure: `onStopMidiReceived()` never
emits, and the only emitter (`stopSetState()`) is called only by local recall and cancel,
which report every in-scope stop at once.

**Where the state and the combinations live.** Stop state is held locally in
`stopCommandedState[]`, and combinations are captured to `COMB.DAT` on whichever medium
`COMBINATION_USE_SPIFLASH` selects — the console owns both, not the engine. A recall or a
cancel walks the in-scope stops and emits NoteOn/NoteOff over `usbMIDI` for each one whose
state it set, which is the entire conversation with Hauptwerk in this mode.

**Pistons.** Physical pistons come either from `pistonAddr[]` in `ConfigData.cpp` or from
field discovery (section 7). Discovery assigns *physical addresses to canonical slots*, not
MIDI notes: in local-combination mode a piston has no MIDI identity at all, so
`pistonMidiNote[]` is read only by the HW-mode `PistonHandler.cpp` and is dead data here.
Screen stops touched on the local TFT are a third input path, writing virtual chain bits.

### Stop MIDI identity

Every stop carries `stopMidiChannel[]` (0-based; `sendStopMidi()` adds 1 for `usbMIDI`) and
`stopMidiNote[]`. Incoming messages resolve back through `midiToStopIndex()`, a linear
search for the first stop matching both fields.

**The library imposes no allocation scheme.** Any channel, any note, in any order, with gaps
— the reverse lookup doesn't care. What it does care about is that the `(channel, note)`
pair is unique: a duplicate silently shadows, since the first match wins. `stopMidiNote[i]`
of `0xFF` means send nothing, for a stop that exists locally but is never reported.

`MIDI_CH_STOPS_1` and `MIDI_CH_STOPS_2` are declared in the contract but **read by no
library code**. They exist so the sketch's `usbMIDI` NoteOn/NoteOff handlers know which
channels to route into `onStopMidiReceived()`. Fixed stop channels are therefore a house
convention in `ConfigData.cpp` and the sketch, not a rule the library enforces. Two of them
is the natural number: 128 notes per channel against `MAX_STOPS = 256` makes two channels
exactly enough to address every stop one-to-one.

### Couplers and anything else outside a division

There is no coupler concept and no reserved note block. A coupler is an ordinary stop with
an ordinary index and whatever note you give it; what makes it behave like a coupler is two
independent fields:

- `stopDivision[i]` — **control scope**. `STOP_DIVISION_NONE` (`0xFF`) means no divisional
  will ever touch it.
- `stopFlags[i]` — **mask membership**: `STOP_IN_GENERALS` (`0x01`), `STOP_IN_DIVISIONALS`
  (`0x02`), `STOP_GC_IMMUNE` (`0x04`).

The usual coupler is `STOP_IN_GENERALS` with division `NONE`: captured and recalled by
generals, untouched by every divisional. If a Swell-to-Great coupler should instead answer
to Great divisionals, give it `stopDivision = Great` and add `STOP_IN_DIVISIONALS` — the two
fields are independent, so a stop can sit in a division for control purposes and still be
excluded from that division's capture mask, or the reverse. A tremulant or blower control
that shouldn't move on General Cancel adds `STOP_GC_IMMUNE`.

**One tension to know about.** The crescendo's scope is `STOP_IN_GENERALS`, the same flag,
so a stop cannot be excluded from the crescendo sweep without also excluding it from
generals. If a coupler shouldn't be swept, that flag is the only lever, and it costs you
general capture too.

> `STOP_IN_SFZ` (`0x08`) is declared alongside these but, like `STOP_SCREEN`, is read by no
> library code.

---

## 4. Crescendo modes

Present when an expression slot is typed `EXPR_CRESCENDO`. Thirty-one levels
(1..31; 0 = off) stored in `CRESC.DAT` on the SD card, reusing the combination record
layout. Capture/recall scope is `STOP_IN_GENERALS`, matching a general.

### 4a. Operation — blind overlay

Lamps and drawstops never move. On any change (the shoe moving, or the organist changing
the base registration), the module computes `effective = stopCommandedState OR levelRecord[N]`
and sends only the stops whose effective state changed.

While engaged it is the **sole** sender of stop MIDI: `stopEngineSuppressed` silences the
commanded-is-truth sends so a manual-off of a crescendo-commanded stop can't desync the
engine, and the overlay emits the OR'd state itself through `stopSendToEngine()`. SAM sense
reporting is never suppressed. At level 0 the effective state is just the base, so releasing
the shoe removes exactly the stops the crescendo added.

### 4b. Programming — not blind

The `SCREEN_CRESCENDO` screen (section 6) recalls into commanded state and lamps so the
organist can see and edit a level. Draw a registration, press SET (on-screen or the console
piston), and the in-scope registration stores to the displayed level and auto-increments,
clamped at 31. Navigating up or down recalls a stored level; an all-zero (never-set) level
leaves the console unchanged, so a progressive crescendo is built by adding to the previous
level.

If `CRESC.DAT` is missing or unreadable, `crescendoAvailable` is false and both operation
and programming are disabled.

---

## 5. Tuning modes

`ORGAN_TUNING_PRESENT` (a runtime contract value, no longer a build gate) says whether the
console has pipes to keep in tune. The tuning code always compiles; on a pipeless console it
is dead flash.

- **Pipeless.** `ORGAN_TUNING_PRESENT = false`: no "Tuning / Temperature" entry in the
  config menu, `pitchManagerInit()` and `tempSensorAttach()` no-op if called anyway, and the
  SysEx pitch report is never matched. A pipeless sketch includes `<TuningDefaults.h>`
  **once** to define the whole contract at inert values instead of writing thirteen dummy
  constants. A console with pipes must **not** include it.
- **Pipes.** Total offset = temperature offset (from `TempSensor`, `CENTS_PER_DEGREE` about
  `TEMP_REFERENCE_DEGC`) + a manual trim held in EEPROM and bounded by
  `MANUAL_OFFSET_MIN`/`MAX`. Target frequency is `PITCH_A_REFERENCE_HZ × 2^(cents/1200)`.

Two transports, independently selectable — either, both, or neither:

| Transport | Config | Behaviour |
|---|---|---|
| Pulse feedback (GrandOrgue) | `PITCH_PULSE_ENABLED` | **Closed loop.** Nudges GO up/down with `PITCH_UP_MIDI_NOTE`/`PITCH_DOWN_MIDI_NOTE` pulses on `PITCH_PULSE_MIDI_CHANNEL`, reads GO's reported offset back through SysEx on `PITCH_SYSEX_LCD_NUM`, and retries until reported == target. A once-per-second startup bootstrap nudge makes GO emit its first report so the loop can arm. |
| MTS SysEx (Hauptwerk) | `PITCH_SEND_TUNING_SYSEX` | **Open loop.** Sent alongside; no acknowledgement. |

`pitchManagerOnUsbMounted()` re-pushes the tuning when the host reconnects.

> **Migration trap.** A sketch that still carries its own `#if ORGAN_HAS_TUNING` guards
> fails *silently*: the macro no longer exists, evaluates to 0, and the tuning calls vanish
> from a clean build. The undefined-reference safety net only applies to the `ConfigData`
> symbols.

---

## 6. Screen / UI modes

`currentScreen` holds one of three states; several screens are blocking sub-modes entered
from the config menu.

| State | Scanning | Notes |
|---|---|---|
| `SCREEN_OPERATIONAL` (2) | runs | Run screen: memory band (−32/−1/level/+1/+32), last-general name, and up to 8 screen-stop tabs in a 4×2 grid. Tab touches write the stop's virtual chain bit, so the tested `processStopInputs()` path does the toggle and MIDI; the tab lamp paints from `stopCommandedState[]`. |
| `SCREEN_CONFIG` (3) | **paused** | Blocking menu. `displayScanChainsActive()` returns false while it is open, so the main loop stops scanning. |
| `SCREEN_CRESCENDO` (4) | runs | Non-blocking crescendo programming — the config menu hands off and returns, so drawstops and tabs stay live while you program. |

Config menu entries, in order, appearing or not according to config so nothing overlaps:

1. **Expression Calibration** — blocking; captures per-shoe analog min/max into EEPROM
   (`calibratedExprMin/Max` become the live values, the const `exprAnalogMin/Max` arrays are
   just the first-boot defaults).
2. **Crescendo Program** — hands off to `SCREEN_CRESCENDO` and returns.
3. **Assign Pistons** — only when the feature is compiled in *and* `PISTON_ASSIGN_ENABLED`
   (section 7).
4. **Tuning / Temperature** — only when `ORGAN_TUNING_PRESENT`.
5. **Back.**

### Orientation and touch inversion

Three contract values, all applied once in `displayInit()`.

**Display: `TFT_ORIENTATION`**, an `ORIENT_*` value from `CoreConfig.h`, passed to
`ui.begin()`. **Landscape only** — the run screen's layout is fixed 320×240
(`SCREEN_W`/`SCREEN_H` are compile-time consts, the memory band lays five elements across the
width, the tab grid is a fixed 4×2) — so use `ORIENT_LANDSCAPE_4PIN_LEFT` or
`ORIENT_LANDSCAPE_4PIN_RIGHT`. The two differ by exactly 180°, which is the flip an inverted
mount needs, and both give the same 320×240 space so nothing in the layout moves. The
portrait values exist for numbering fidelity, not as supported settings. The `ORIENT_*`
constants mirror TeensyUserInterface's `LCD_ORIENTATION_*` so a `ConfigData.cpp` needn't
include the TUI header; `static_assert`s in `DisplayManager.cpp` catch drift.

**Touch: `TOUCH_INVERT_X` and `TOUCH_INVERT_Y`**, one flag per axis, applied on top of that
orientation. Panels vary in how the touch layer is wired relative to the glass — some are
mirrored on a single axis, some are end-for-end on both. Inverting both axes *is* a 180°
touch rotation, so the pair covers every case a second orientation value could, plus the
single-axis mirrors it couldn't (an orientation rotates; a mirror is not a rotation of
anything).

The mechanism needs no touch code of its own and no TUI change. `ui.begin()` loads TUI's
touch calibration for the display orientation, which maps a raw reading as
`lcd = raw / scaler - offset`, so reversing an axis end-for-end is a negated scaler with a
re-derived offset:

```
(span-1) - (raw/S - O)  ==  raw/(-S) - ( -((span-1) + O) )
```

Two more screens exist outside that state machine:

- **Startup handshake.** With `STARTUP_WAIT_ENABLED`, the end of `setup()` shows a blocking
  "Starting Up" splash with a seconds counter and waits **solely** for one NoteOn matching
  `STARTUP_WAIT_MIDI_CHANNEL`/`_NOTE` over USB-MIDI — the sample engine telling the console
  it is ready. No timeout, no touch-to-skip. That note means nothing else to the console.
- **"Preparing Memory."** Shown during the first-boot combination-file format, which matters
  on QSPI flash (blanking 8 MB is roughly 1–3 minutes; seconds on SD). A `displayReady` flag
  makes the progress draw a no-op until the display is up — **so to actually see the bar, the
  sketch must call `displayInit()` before `combinationInit()`.**

---

## 7. Builder piston assignment mode

Lets a builder assign the console's pistons and control buttons **in the field, without
recompiling**: park on a logical function, press the physical button(s) that should trigger
it.

Availability is two conditions:

- Compiled in only in local-combination mode — `ORGANCORE_HAS_REMAP_STORE` is defined when
  `ORGAN_COMBINATION_MODE == COMBINATION_MODE_SD`. Every consumer guards its `#include`s on
  it, so a HW-mode build doesn't need `RemapStore.*`, `PistonAssignScreen.*` or
  `PistonAssignSlots.h` present at all.
- Offered at run time when `PISTON_ASSIGN_ENABLED`. False is the usual case for a console
  whose input map is fully defined in config data: no menu entry, no `REMAP.DAT` load, and
  `applyRemaps()` uses only the const `remapFrom[]`/`remapTo[]`. `REMAP.DAT` follows
  `COMBINATION_USE_SPIFLASH` like every other file, so a flash console gets the feature too.

**Canonical slots.** Every assignable function has one fixed address on a reserved virtual
chain (`REMAP_SLOT_CHAIN` = 11, the top of the widened `MAX_CHAINS = 12` space, so real
chains growing upward from 0 never collide). Calibration never rewrites the piston list; it
appends remap entries `{pressed physical addr → slot addr}`, and `applyRemaps()` funnels each
press onto its slot before any handler runs. Several physical buttons can funnel onto one
slot (a General Cancel on both the rail and a toe stud). Captures come from all input chains
including virtual ones — a MIDI pedalboard's embedded pistons must be assignable — and only
the slot chain itself is excluded, being a destination.

**The layout is frozen**, exactly like the combination record layout: stored `to` addresses
are computed from it, so changing a cap or stride renumbers slots and silently corrupts
saved assignments. In walk order: 7 known controls (Set, GC, Next, Prev, Mem+, Mem−, Shift),
64 generals, 128 divisionals (8 divisions × 16, division-major, frozen order 0 Pedal,
1 Great, 2 Swell, 3 Choir, 4 Solo), 12 spare controls — 211 of 256 bits. To grow: append a
new block at a fresh range, never widen one in place, and bump `REMAP_FORMAT_VERSION`.

**Storage semantics.** `REMAP.DAT`, on the same medium as `COMB.DAT` (magic `OCRM`, versioned, slot chain and `MAX_REMAPS`
validated), loaded at the end of `combinationInit()`. A **missing file means
never-calibrated** → the const defaults stay in effect. A **valid zero-count file means
deliberately cleared** (Start Over) → the empty table wins. A foreign, old or truncated file
is blanked.

---

## 8. Scan chain modes

Per chain, from `chainType[]` and `chainDir[]`. Addressing is CWB throughout — `0xCWB`,
chain 0–11 in the top nibble, `ADDR_DISABLED = 0xF00` (moved up from `0x800` when chains
grew past 8, since `0x800` became a real address).

| Type | Use |
|---|---|
| `CHAIN_TYPE_MULTIDROP` (0) | SAM sense bus, synchronised by a HIGH→LOW pulse (`SYNC_PULSE_US`, `SYNC_SETTLE_US`) |
| `CHAIN_TYPE_SHIFTREG` (1) | 74HC595 / 74HC597 / CD4021 / CD4094 shift registers |
| `CHAIN_TYPE_VIRTUAL` (2) | No hardware — fed from serial MIDI or from touch |
| `CHAIN_TYPE_SERIAL_SAM` (3) | Coils driven as `0xCn` frames over a UART (`scanSerialSamAttach()`; the sketch owns the `begin()`) |

Shift-register chains have two further per-chain modes: `chainStrobeActiveHigh[]` (the active
level of the parallel-load or output-latch pulse — HIGH for CD4021/CD4094/74HC595, LOW for a
74HC597 `PL`) and `chainMsbFirst[]` (MSB-first makes the buffer bit index equal the physical
bit number in the data-chain list; LSB-first is clock order). Both are ignored on other chain
types.

**Virtual chain input.** `SerialMidi` injects messages arriving on `VIRTUAL_CHAIN_MIDI_CH`
between `VIRTUAL_CHAIN_BASE_NOTE` and `VIRTUAL_CHAIN_MAX_NOTE` into
`VIRTUAL_CHAIN_INDEX` as scan bits; every other NoteOn/NoteOff/CC on that port is forwarded
straight to USB MIDI. Screen-stop tab touches write the same kind of virtual bit.

---

## 9. Expression modes

Per slot, from `exprType[]`:

- **`EXPR_ANALOG`** — reads `exprAnalogPin[]`, scales against the calibrated min/max to a
  0–127 CC on `exprMidiCC[]` / `exprMidiChannel[]`, with `exprDeadband[]` hysteresis.
- **`EXPR_DISCRETE`** — counts HIGH inputs across a CWB range
  (`exprDiscreteStart[]`..`exprDiscreteEnd[]`); the CC value is the count. On a change it
  sends every intermediate value in one burst, so a jump doesn't step the engine abruptly.
- **`EXPR_CRESCENDO`** — an analog shoe that drives the blind crescendo (0–31) and sends no
  CC of its own. See section 4.

---

## 10. Build-time diagnostics

`DEBUG_ENABLED` (default 1) in `Debug.h` gates the scan diagnostics — input/output buffer
dumps, CWB change reporting, output-change detection. It is a genuine code-elimination gate,
so it must stay a macro and cannot come from `Config.h`: the Arduino IDE compiles the library
separately from the sketch. Override with a build flag, noting that `platform.local.txt` is
global to the machine rather than per sketch.

---

## Mode selection summary

| Mode | Selected by | When |
|---|---|---|
| Combination action (HW / local) | `ORGAN_COMBINATION_MODE` | compile |
| Debug prints | `DEBUG_ENABLED` | compile |
| Storage medium, all files (SD / QSPI) | `COMBINATION_USE_SPIFLASH` | boot |
| Piston assignment offered | `PISTON_ASSIGN_ENABLED` | boot / run |
| Display orientation | `TFT_ORIENTATION` | boot |
| Touch axis inversion | `TOUCH_INVERT_X`, `TOUCH_INVERT_Y` | boot |
| Tuning present | `ORGAN_TUNING_PRESENT` | run |
| Tuning transport | `PITCH_PULSE_ENABLED`, `PITCH_SEND_TUNING_SYSEX` | run |
| Startup handshake | `STARTUP_WAIT_ENABLED` | boot |
| Stop truth (SAM / commanded) | `stopOnCoilAddr[]` validity | run, per stop |
| Crescendo present | an `EXPR_CRESCENDO` slot + `CRESC.DAT` | boot |
| Chain transport | `chainType[]`, `chainDir[]` | run, per chain |
| Expression behaviour | `exprType[]` | run, per slot |
| Screen state | `currentScreen` | run |

### Declared but not acted on

Three contract symbols exist in `OrganConfig.h` with no library code reading them —
`HIDE_CONFIG_SCREEN`, `SCREEN1_BACKLIGHT_SECONDS`, and the `STOP_SCREEN` flag. Define them,
but don't expect them to change behaviour.
