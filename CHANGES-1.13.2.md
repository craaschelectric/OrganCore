<style>
body, p, li { font-size: 10pt; line-height: 1.15; }
</style>

# OrganCore 1.13.2 — three compile errors on the Teensy toolchain

1.13.0 and 1.12.0 were never built with the Teensy toolchain; the first real build of 1.13.1 (Teensyduino 1.59.0) stopped on three errors, all in library code. No contract change and no sketch change is required.

## The errors and the fixes

**`DisplayManager.cpp`: `lcdInitialize()` is private.** `displayReinit()` called `ui.lcdInitialize(...)` directly. In the TeensyUserInterface that ships with Teensyduino it is a private method. `displayReinit()` now calls the public `ui.begin(TFT_CS_PIN, TFT_DC_PIN, TOUCH_CS_PIN, TFT_ORIENTATION, Arial_9_Bold)` again, then `setColorPaletteGray()` as before. `begin()` re-runs the panel init and re-initializes the touch controller with its default calibration for the orientation; touch inversion is applied to finished coordinates in `uiGetTouchEvents()`, so it is unaffected. If `begin()` allocates its LCD and touch objects on every call, each re-initialization leaks a small block; re-initializations happen only after a USB link recovery.

**`OrganDiag.cpp`: `TeensyUserInterface` has no member `lcd`.** The periodic panel readback (`ui.lcd->readcommand8(ILI9341_RDMODE)`) needs the ILI9341 object, which TeensyUserInterface keeps private. Reaching it any other way would mean driving the SPI bus underneath TeensyUserInterface, so the readback is **removed**. `DISPLAY_READBACK_ENABLED` stays in the contract so existing `ConfigData` files still link; it is ignored, and setting it true logs one line saying so. The display is still re-initialized after a USB link recovery, which is unchanged.

**`OrganQSPIFlash.h`: `constexpr` constructor calls a non-`constexpr` base.** `LittleFS::LittleFS()` is not `constexpr` in Teensyduino 1.59, so `constexpr OrganQSPIFlash() { }` is ill-formed. The `constexpr` is dropped on both the Teensy 4.x class and the fallback class.

Files: `DisplayManager.cpp`, `OrganDiag.cpp`, `OrganDiag.h`, `OrganConfig.h`, `OrganQSPIFlash.h`, `library.properties`.

## Not verified

Checked on a host compiler against stand-ins that reproduce each constraint (a `LittleFS` whose constructor is not `constexpr`; a TeensyUserInterface-shaped class with a private `lcdInitialize()` and no `lcd` member; stubbed `FS` and USB headers). Not built with the Teensy toolchain. Any further errors that only that toolchain reveals will need its output.
