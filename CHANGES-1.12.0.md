<style>
body { font-size: 10pt; line-height: 1.15; }
p, li { font-size: 10pt; line-height: 1.15; }
</style>

# OrganCore 1.12.0 — own QSPI flash driver, piston-driven display, piston assignment removed

## Summary

Three changes. QSPI flash storage no longer needs a patched LittleFS: OrganCore carries its own QSPI back-end, `OrganQSPIFlash`, with the Boya BY25Q128ES in its chip table. `TOUCH_ENABLED = false` now switches the display to a piston-driven mode, grown out of the Opus 57 `DisplayLocal` module, so the config menu, shoe calibration, crescendo programming and tuning are all reachable without touch. Builder piston assignment is removed.

## QSPI flash: OrganQSPIFlash

The stock `LittleFS_QSPIFlash` only mounts chips listed in LittleFS.cpp's private table, which cannot be extended from outside the file and lacks the Boya BY25Q128ES (JEDEC 68 40 18). Builds therefore needed a hand-patched LittleFS, which a Teensyduino update or a build on another computer silently undid. New `src/OrganQSPIFlash.h/.cpp` is the stock QSPI NOR code copied from PJRC's LittleFS (MIT licence notice retained) with its own table: every row of the stock table plus the Boya, at 3000 us page program and 3000000 us block erase (the datasheet's worst case, rounded up). It derives from the stock `LittleFS` base class, so the file system itself is unchanged. `OrganStorage.cpp` mounts it in place of `LittleFS_QSPI`. A failed mount now prints the JEDEC ID that answered. NOR only; QSPI NAND, which `LittleFS_QSPI` also probed, is not supported. Upstream fixes to PJRC's QSPI code will not arrive automatically.

## Piston-driven display (TOUCH_ENABLED false)

`TOUCH_ENABLED` used to make the panel inert while leaving the touch screens drawn. It now selects piston-driven mode. New `src/PistonMenu.h/.cpp`; `DisplayManager.cpp`, `CombinationSD.cpp` and `PistonHandler.cpp` change.

- The run screen drops the Config button and the memory buttons. There are no on-screen stops: tabs whose stop is `STOP_SCREEN` are not drawn. Tabs that mirror a real console stop are drawn read-only. With no tabs, the expanded layout moves up into the space the buttons used.
- Hold SET and press GENERAL CANCEL to open the menu. That press does not cancel (in HW mode it is not sent to the host). The back-end sets the new `displayMenuRequested`; `displayUpdate()` runs the menu. No sketch change.
- SET acts, GENERAL CANCEL goes back, MEM+/MEM- move in a list (falling back to NEXT/PREV), NEXT/PREV step a value (falling back to MEM+/MEM-).
- Items: Calibrate Shoes (every analog shoe, the crescendo shoe included), Crescendo (when present), Tuning (when `ORGAN_TUNING_PRESENT`). All blocking. The crescendo screen runs the stop handlers, so drawknobs work while programming.
- On leaving the menu `setHeld` is re-read from the SET piston. Its release happens inside the menu, unseen by the back-end, which would otherwise treat the next general press as a capture. In HW mode the SET note-off is sent to the host for the same reason.

## Touch Expression Calibration includes the crescendo shoe

`src/ExpressionCalScreen.cpp`. The touch screen listed only swell shoes (`EXPR_ANALOG`), so a crescendo shoe (`EXPR_CRESCENDO`) could not be calibrated by touch. It now gets a row too, marked C on its readout (swell shoes stay P). The screen also now seeds all `MAX_EXPRESSIONS` working entries before Save. It used to seed only `NUM_EXPRESSIONS`, and since `expressionCalibrationSave()` writes all `MAX_EXPRESSIONS`, the unused slots were written to EEPROM from uninitialised memory. Those slots are never read, so nothing misbehaved, but the write is now clean.

## Builder piston assignment removed

Deleted: `src/PistonAssignScreen.h/.cpp`, `src/PistonAssignSlots.h`, `src/RemapStore.h/.cpp`. Removed: the `REMAP_SLOT_*` layout from `CoreConfig.h`, `ORGANCORE_HAS_REMAP_STORE` from `CombinationConfig.h`, `PISTON_ASSIGN_ENABLED` from the contract, the Assign Pistons menu entry, and the REMAP.DAT load in `combinationInit()`. `applyRemaps()` uses only the const `remapFrom[]`/`remapTo[]`. `MAX_CHAINS` stays 12 and `MAX_REMAPS` stays 256.

## Migration

- Delete any patched LittleFS from your sketchbook's `libraries/LittleFS`. The stock one that comes with Teensyduino is all that is needed.
- A `ConfigData.cpp` that still defines `PISTON_ASSIGN_ENABLED` builds unchanged; the line can be deleted at leisure. A REMAP.DAT left on a card or flash is ignored.
- A console with `TOUCH_ENABLED = false` changes behaviour: it gets the piston-driven run screen and menu, and loses any `STOP_SCREEN` tabs (which it could not operate anyway). A console with `TOUCH_ENABLED = true` is unchanged.
- A console that used its own button-driven display (Opus 57's `DisplayLocal`) can delete it: set `TOUCH_ENABLED = false` and call `displayInit()` / `displayUpdate()` / `displayProcessTouch()` as the touch consoles do.
- A console that carried QSPI NAND must stay on 1.11.x or move to NOR.
