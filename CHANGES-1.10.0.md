<style>
body { font-size: 10pt; line-height: 1.15; }
p, li { font-size: 10pt; line-height: 1.15; }
</style>

# OrganCore 1.10.0 — per-file combination and crescendo storage

## Summary

Combinations and crescendo levels are now each stored as their own small 64-byte file, created on demand, instead of as records inside one large preallocated file. This fixes a ~45-second console freeze on every combination capture on QSPI-flash consoles, removes the boot-time format step, and makes stored combinations survive edits to the piston table.

Four files change: `src/CombinationSD.cpp`, `src/Crescendo.cpp`, `src/CombinationConfig.h`, and `library.properties`. No contract symbols change, so no sketch needs migrating — a console already on 1.9.x rebuilds against 1.10.0 unchanged.

## The problem this fixes

The original design stored every memory-level × piston combination as a fixed-size record inside one 8 MB `COMB.DAT`, addressed by a computed byte offset, and seeked into that file on each capture. On LittleFS (the filesystem used on the on-board QSPI flash), a seek-and-write into a large file walks the file's block chain up to the seek offset on every `flush()`. For an 8 MB file that meant each 64-byte capture rewrote almost the whole file: measured at ~45 seconds, which works out to 182 KB/s — exactly full-file-rewrite speed. The data still landed at the correct offset, so recall always returned the right registration, but the console was frozen for the entire write and did not respond to anything, including the power-down signal, until the write completed.

SD cards do not have this cost, which is why the defect was invisible until the combination file moved to QSPI flash. The 1.9.2 `FILE_WRITE_BEGIN` change removed a separate `O_APPEND` full-rewrite on top of this, but not the underlying block-chain walk — that required abandoning the single-file layout.

## What changed

### Combinations

Each combination is now its own file, `CB_<level>_<pistonAddr>.DAT`, a bare 64-byte stop bitmap with no header (bit *i* = stop *i* on; byte i/8, bit i%8, LSB-first). A 64-byte file has no block chain to walk, so capture and recall are effectively instant on both flash and SD.

The filename is keyed on the piston's **input address** (`pistonAddr[i]`), not its position in the piston table. The address is a fixed hardware fact — a given physical piston has the same address for the life of the instrument — whereas the table index shifts whenever a piston is inserted or removed. The old offset model was safe only for append-only growth of the piston table; inserting or reordering a piston silently rebound every stored combination past that point to the wrong button. Keying on the address makes stored combinations survive any edit to the table.

Dual-input pistons — a thumb piston and a toe piston that fire the same registration — are handled by the existing **remap table**, not by two piston-table entries. Remap the toe's input address to the thumb's, and give only the thumb a piston-table entry. `applyRemaps()` moves the toe's bit onto the thumb address and clears the toe bit before any handler runs, so pressing either contact fires the thumb piston, and both resolve to one canonical file. Giving the toe its own piston-table entry would instead produce a second address and a second file, silently splitting one logical piston into two half-registered combinations — thumb captures never recalled by the toe, and the reverse. This requirement is documented at the filename builder in the source.

### Crescendo

Crescendo had the same single-file design (`CRESC.DAT`: a header plus 31 records at computed offsets) and therefore the same latent defect, smaller only because the file is far smaller than the 8 MB combination file. Each level 1–31 is now its own file, `CR_<level>.DAT`, the same 64-byte record shape, created on demand when that level is programmed.

### Both

There is no boot-time format any more: nothing needs to be pre-zeroed, because a piston or level that was never set simply has no file. Recall of a missing file leaves the console blank — the correct "nothing stored here" result, and exactly what the old preformatted all-zero record produced. On first boot after upgrading, `combinationInit()` and `crescendoInit()` each remove the stale single-file `COMB.DAT` / `CRESC.DAT` once if a previous firmware left one, printing `removed legacy COMB.DAT` and `removed legacy CRESC.DAT`. This reclaims the 8 MB and is expected, not an error.

**Old stored registrations do not carry into the per-file scheme and must be re-set** after upgrading. On a QSPI console they were taking 45 seconds each to store, so there are unlikely to be many worth keeping.

### CombinationConfig.h

The header comment describing the offset-into-one-file model is replaced with the per-file description. `COMBO_HEADER_SIZE`, `COMBO_PISTON_CAP`, `COMBO_MAGIC_*` and `COMBO_FORMAT_VERSION` are now vestigial — no per-file record has a header — and are kept only so external tooling still compiles. `COMBO_STOP_CAP` remains live because it defines `COMBO_RECORD_SIZE` (the 64-byte record shape), and `COMBO_MEM_LEVELS` remains live as the memory-level wrap bound.

## Compatibility

No contract symbols added, removed, or changed, so every existing sketch compiles against 1.10.0 with no edit. The one console-side action is that `COMBINATION_USE_SPIFLASH` can be returned to `true` on any console that was temporarily moved to SD to dodge the capture stall — the per-file scheme makes QSPI fast, so the SD workaround is no longer needed.

`snprintf` is now used in both `CombinationSD.cpp` and `Crescendo.cpp`; `<stdio.h>` is included in each.

## Not yet verified

This has not been compiled on the Teensy toolchain. After building, confirm a combination capture and a crescendo level program are both instant on QSPI, and power-cycle to confirm the per-file records persist across a cold boot.
