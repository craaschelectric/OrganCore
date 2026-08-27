// PistonAssignScreen.h
// TUI builder piston-assignment screen (local-capture / SD mode only).
//
// Walks the frozen virtual-slot list (known controls -> generals -> divisionals
// -> spare controls). The builder parks on a slot and presses physical pistons;
// each press is captured on its rising edge and remapped ONTO the current slot's
// canonical virtual address via the RemapStore. Presses are captured from ALL
// input chains, including CHAIN_TYPE_VIRTUAL (a MIDI pedalboard's embedded
// pistons report through a virtual chain and must be assignable) -- only the
// reserved slot chain (REMAP_SLOT_CHAIN) is excluded, since it is a destination,
// never a source.
//
// Controls: Next function (advance one slot), Next block (jump to head of the
// next region), Clear function (drop this slot's remaps), Start over (empty the
// table), Save (persist REMAP.DAT), Cancel (discard).
//
// Blocking, reached from the config menu while the main scan loop is paused. The
// screen pumps scanAllChains() + serialMidiProcess() itself so live presses from
// hardware and virtual chains both land in inputBuffer.

#ifndef PISTON_ASSIGN_SCREEN_H
#define PISTON_ASSIGN_SCREEN_H

#include "RemapStore.h"   // defines ORGANCORE_HAS_REMAP_STORE
#ifdef ORGANCORE_HAS_REMAP_STORE

void pistonAssignScreenRun();

#endif
#endif // PISTON_ASSIGN_SCREEN_H
