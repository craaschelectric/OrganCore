<style>
body, p, li { font-size: 10pt; line-height: 1.15; }
</style>

# OrganCore 1.14.0 — an About screen in the config menu

Every console now has an About screen as the last item in its config menu. It shows the OrganCore version linked into the firmware, the sketch's firmware version, and a description of the instrument such as the venue and opus. **Sketch migration is required:** two new contract constants, with no defaults.

## What changed

- **Piston-driven consoles** (`TOUCH_ENABLED` false): hold SET and press GENERAL CANCEL to open the config menu, then choose *About* with SET. GENERAL CANCEL goes back to the menu. The menu now has up to five items, so the buttons are 30 px tall on a 34 px pitch (they were 34 on 42) to keep the last one clear of the hint line.
- **Touch consoles:** a new *About* button above *Back* in the config screen, with its own *Back* button.
- `ORGANCORE_VERSION` is a constant in `CoreConfig.h`, kept equal to `version=` in `library.properties`. It lives in the library rather than the sketch so the screen reports what was actually linked in and cannot drift.
- New module `AboutScreen` (`AboutScreen.h`, `AboutScreen.cpp`) paints the screen. Each menu supplies its own way out, so the exit handling is written inline in `PistonMenu.cpp` and `DisplayManager.cpp`.

## Sketch migration

Add two constants to `ConfigData.cpp`, after the contract header is included, like every other contract constant:

```cpp
const char* const FIRMWARE_VERSION  = "2026-10-21 07:51 PDT";
const char* const ABOUT_DESCRIPTION = "St. John's Lutheran, Orange\nOpus 62";
```

A sketch that omits either one fails to link, which is the intended failure. `FIRMWARE_VERSION` is shown as given; the convention is the date and time the firmware was made, to the minute, in Pacific time with the zone label (PDT or PST), which fits on the line. In `ABOUT_DESCRIPTION`, `\n` starts a new line. Up to four lines of about 44 characters are shown; a longer line wraps at the screen edge and anything past the fourth line is not shown. An empty string shows no description.

Files: `src/AboutScreen.h`, `src/AboutScreen.cpp` (new), `src/CoreConfig.h`, `src/OrganConfig.h`, `src/PistonMenu.cpp`, `src/PistonMenu.h`, `src/DisplayManager.cpp`, `library.properties`, `README.md`, `docs/OPERATING-MODES.md`.

## Checked

The description splitting in `AboutScreen.cpp` was compiled on a host against a stub display and run on seven strings: a normal two-line description, an empty one, a blank line in the middle, a 70-character line, a 44-character line followed by a break, six lines (the last two are dropped), and a trailing newline. All came out as described above. Not built with the Teensy toolchain and not run on a console, so the button spacing and text positions are first-cut pixel values, like the rest of `PistonMenu.cpp`.
