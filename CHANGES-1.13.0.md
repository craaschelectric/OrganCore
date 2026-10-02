<style>
body, p, li { font-size: 10pt; line-height: 1.15; }
</style>

# OrganCore 1.13.0 — Tutti as a blind overlay, and field diagnostics

Two independent changes, committed separately. Both were prompted by Opus 62 (the Rodgers 760 retrofit), which is being moved to this release from 1.9.2.

## 1. TUTTI is a blind toggle overlay

From 1.11.0 to 1.12.0, a `PISTON_TYPE_TUTTI` press ran `combinationRecall()`: it replaced the base registration with the stored tutti, moved every drawknob lamp, and had no "off". That is a level-independent general, not a tutti. A tutti adds to whatever is drawn without disturbing it, and taking it off restores what was there.

In 1.13.0 a TUTTI press toggles a blind overlay, exactly like the crescendo. While engaged, the stored tutti registration is OR'd onto the base for the engine only — `stopCommandedState` is never touched, so no drawknob lamp moves. The second press releases it and the base registration sounds again. General Cancel also releases it. SET+TUTTI still stores the visible registration as the tutti (fixed level 0, `CB_0000_<addr>.DAT`, as before). If a tutti is engaged when it is re-stored, the live overlay updates immediately. An unset tutti engages as an empty overlay, adding nothing — it does not cancel the registration, which a recall of an unset tutti would have done.

The tutti is implemented as a second term in the crescendo's existing overlay calculation, not as a parallel mechanism: effective = base OR crescendo level OR tutti. One calculation means the two overlays compose (both can be engaged) and never fight over `stopEngineSuppressed` or the last-sent state. Manual stop changes, recalls and General Cancel are picked up every loop while either is engaged, and capture still stores the base, never the overlay. Entering crescendo programming releases the tutti as well as the crescendo, so programming starts from a visible state.

**Breaking behavior change** for any console already using `PISTON_TYPE_TUTTI` (Opus 57): the piston now toggles an overlay instead of recalling. Stored tutti registrations keep working; only what a press does changes.

**Sketch change required for every console:** `crescendoPoll()` must be called every loop, not inside `if (EXPR_ENABLED)`. The overlay engine now lives there, and it must run on consoles with no crescendo shoe at all. The shoe is still only read when an `EXPR_CRESCENDO` slot exists.

New contract value:

| Symbol | Type | Meaning |
|---|---|---|
| `TUTTI_INDICATOR_ADDR` | `uint16_t` | Output bit lit while the tutti is engaged. `ADDR_DISABLED` for none. Must not be any stop's `stopLightAddr`. |

The run screen shows TUTTI while engaged, in the same place as the crescendo level: "TUTTI", "CRESCENDO n", or "TUTTI + CRESC n" when both are engaged. Without local combination memory (`COMBINATION_MODE_HW`) TUTTI is unchanged and still goes to the host.

Files: `Crescendo.cpp/.h`, `CombinationSD.cpp`, `DisplayManager.cpp`, `CoreConfig.h`, `OrganConfig.h`.

## 2. Field diagnostics (OrganDiag)

Opus 62 went grey-screened and silent mid-service, and nothing on the console could say why afterwards. The new `OrganDiag` module records what would have answered it in `DIAG.LOG` on the console's own storage medium (QSPI or SD, whichever OrganStorage mounted), where it survives the power-cycle that ends every such incident.

**What is logged.**
- Every boot, with a boot number and the CPU reset cause read from `SRC_SRSR`: power-on, lockup/software, watchdog, over-temperature, JTAG. A brownout shows as power-on; the i.MX RT has no separate flag.
- The full Teensy `CrashReport`, if the previous run faulted.
- Every USB link drop and recovery after boot completes, with the outage length.
- Every display re-initialization.
- A startup wait that timed out.

Lines are stamped with boot number and time since boot (`B47 +01:23:05 USB link lost`); there is no wall clock, since the Teensy RTC only keeps time across power-off with a coin cell. The log rolls over to `DIAG.OLD` past 4 KB, so writes stay small appends. LittleFS never overwrites in place, so a write cut off by a brownout loses at most that line. The boot counter is a separate 4-byte `DIAG.CNT`.

**USB link drop.** When the host re-enumerates the Teensy, GrandOrgue does not re-open the MIDI port, so the console keeps running while the engine stops listening. After a drop and recovery the title bar reads "MIDI LINK LOST - RESTART ORGAN" until the next power-up, and the panel is re-initialized. A USB-powered panel browns out on the same dip.

**Panel health readback.** With `DISPLAY_READBACK_ENABLED`, every 2 s the ILI9341 power-mode register is read through `ui.lcd->readcommand8(ILI9341_RDMODE)`. If the panel reports it is asleep or its display is off — the grey screen — it is re-initialized. That requires the panel's MISO line. If a re-initialization does not change the reading, the check concludes the readback is meaningless (no MISO) and switches itself off for that boot, logging why, rather than re-initializing in a loop. Re-initialization happens only at the run screen; one detected inside a menu waits until the console is back there.

**Startup-wait timeout.** `STARTUP_WAIT_TIMEOUT_MS` ends the wait for the engine's handshake. After a mid-service reset the engine is already running and will never send it again; with 0 (wait forever, the old behavior) the console would sit on "Starting Up" until everything was power-cycled.

**Reading the log.**
- On a piston-driven console: SET + General Cancel → Diagnostics shows the newest entries, paged with the list pistons, read-only.
- From any console: send `D` over USB serial and it prints `DIAG.OLD` then `DIAG.LOG`. With the Teensy plugged into a Pi, this works over SSH:
  ```
  stty -F /dev/ttyACM0 115200 raw -echo
  cat /dev/ttyACM0 &
  printf D > /dev/ttyACM0
  ```
- The touch-driven config menu does not have a Diagnostics item yet; touch consoles read the log over serial.

**API.** `diagInit()` second in `setup()`, right after `powerInit()` and `Serial.begin()`; `diagBootComplete()` last, beside `powerBootComplete()`; `diagPoll(true)` in `loop()`. Every blocking screen in the library calls `diagPoll(false)` in its pump, following the rule that a blocking loop pumps what `loop()` pumps. `displayReinit()` is new in DisplayManager.

New contract values:

| Symbol | Type | Meaning |
|---|---|---|
| `STARTUP_WAIT_TIMEOUT_MS` | `uint32_t` | Give up on the handshake after this long. 0 = wait forever. |
| `DISPLAY_READBACK_ENABLED` | `bool` | Periodic ILI9341 status read and automatic re-initialization. Needs MISO. |

Files: new `OrganDiag.h/.cpp`; `DisplayManager.cpp/.h`, `PistonMenu.cpp`, `StartupScreen.cpp`, `TuningScreen.cpp`, `ExpressionCalScreen.cpp`, `OrganConfig.h`, `OrganCore.h`.

## Migrating a sketch to 1.13.0

- Add `TUTTI_INDICATOR_ADDR`, `STARTUP_WAIT_TIMEOUT_MS` and `DISPLAY_READBACK_ENABLED` to ConfigData. A missing one is a link error naming it.
- Move `crescendoPoll()` out of any `if (EXPR_ENABLED)` block.
- Add `diagInit()`, `diagBootComplete()` and `diagPoll(true)` as described above.

## Not verified

Not compiled on the Teensy toolchain. Bench checks before installing:
- SET+TUTTI stores and TUTTI toggles with no drawknob lamp moving.
- General Cancel drops an engaged tutti.
- Pulling and reinserting the USB cable logs a drop and recovery and shows the title-bar warning.
- With MISO connected, the readback reports healthy (no spurious re-inits in `DIAG.LOG`).
- With `STARTUP_WAIT_TIMEOUT_MS` set, the startup wait gives up on time.
- The first boot's reset cause reads "power-on". The Teensy core is believed not to clear `SRC_SRSR` before `setup()`; if it does, every boot will read "unknown".
