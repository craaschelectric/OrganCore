<style>
body, p, li { font-size: 10pt; line-height: 1.15; }
</style>

# OrganCore 1.15.0 — combination mode at run time, a No Storage setting, and a pulse keep-alive

Contract change: every console's `ConfigData.cpp` needs the four lines listed at the end. Written for Opus 73 (Rodgers 770, Hauptwerk in control, no SD or QSPI).

## Combination mode is ConfigData now

`ORGAN_COMBINATION_MODE` is gone. The Hauptwerk-side piston code (`PistonHandler.cpp`) and the local code (`CombinationSD.cpp`) used to define the same symbols, so exactly one could link. The local piston entry points are now `localPistonInit()` and `localProcessPistons()`; both files always build, and `pistonInit()`/`processPistons()` pick one from `COMBINATION_HOST_OWNED`. The combination state the display reads (`combinationAvailable` and friends) is defined once, in `CombinationSD.cpp`.

A build that still passes `-DORGAN_COMBINATION_MODE` stops with an `#error` that says what to do, because silently ignoring it would run the wrong back-end.

## No Storage

`STORAGE_ENABLED` false means no SD card and no QSPI chip. `organStorageMount()` returns false at once, without a debug line and without setting `organStorageError`, so no fault line is drawn. `OrganDiag` logs to Serial only (it already did when the mount failed), `crescendoAvailable` stays false so the piston menu has no Crescendo entry, and nothing else changes. Local combinations with no storage set `combinationErrorText` to "STORAGE DISABLED".

## Keep-alive as a pulse train

`POWER_KEEPALIVE_PULSE_ADDR` and `POWER_KEEPALIVE_PULSE_MS` toggle an output chain bit from `powerPoll()`. The Rodgers 770's supply relay is held by pulses on output chain 6 bit 0, not by a level, and it drops about 50 ms after they stop. `powerPoll()` and `scanAllChains()` are both pumped by every blocking screen, so the pulse keeps running in the config menu and the calibration screens. During a shutdown halt wait the pulse continues, so a pulse-fed relay does not drop in the middle of the host's halt. It stops when the supply is released.

## ConfigData lines to add to every existing console

For a console that behaves exactly as before:

```cpp
const bool     COMBINATION_HOST_OWNED         = false;   // true when Hauptwerk owns the combinations
const bool     STORAGE_ENABLED                = true;
const uint16_t POWER_KEEPALIVE_PULSE_ADDR     = ADDR_DISABLED;
const uint32_t POWER_KEEPALIVE_PULSE_MS       = 0;
```

A console that was built with `-DORGAN_COMBINATION_MODE=COMBINATION_MODE_HW` sets `COMBINATION_HOST_OWNED = true` and drops the flag.
