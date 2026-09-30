<style>
body { font-size: 10pt; line-height: 1.15; }
p, li { font-size: 10pt; line-height: 1.15; }
</style>

# OrganCore 1.11.0 — TUTTI piston type

## Summary

Adds `PISTON_TYPE_TUTTI` (10), a general-scope piston that stands outside the NEXT/PREV sequence and holds one registration shared by every memory level. Four files change: `src/CoreConfig.h`, `src/CombinationSD.cpp`, `src/PistonHandler.cpp`, and `library.properties`. No contract symbols are added or removed; a console that does not use the new type rebuilds against 1.11.0 unchanged.

## Why a new type

A tutti could already be approximated with an ordinary general left out of `sequencerPistonList[]`, which keeps it off NEXT/PREV. It could not be made level-independent: every general's combination file is `CB_<level>_<addr>.DAT`, keyed on the current memory level, so a general-type tutti would hold a different registration on every level. A tutti is expected to be the same registration everywhere.

## Behavior

With local combination memory (`COMBINATION_MODE_SD`), TUTTI captures with SET held and recalls on press, exactly like a general, with general stop scope (`STOP_IN_GENERALS`). Its file is always written and read at level 0 — `CB_0000_<addr>.DAT` — whatever level is showing, so SET+TUTTI on any level sets the one tutti and TUTTI recalls it from any level. This cannot collide with a level-0 general because the address is part of the filename. Pressing TUTTI shows "TUTTI" as the last general and leaves the sequencer position alone, so NEXT continues from the last general fired.

Without local memory (`COMBINATION_MODE_HW`), TUTTI is sent to the host exactly like a general — NoteOn on press, NoteOff on release, on its `pistonMidiNote` — again showing "TUTTI" and leaving the sequencer position alone.

## Configuration

Give the tutti one piston-table entry with `pistonType = PISTON_TYPE_TUTTI` and its input address. Do not list it in `sequencerPistonList[]` and give it no `generalName[]` entry (that table is indexed by sequencer position). Extra physical buttons for the tutti go in the remap table, as for any dual-input piston.
