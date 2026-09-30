<style>
body { font-size: 10pt; line-height: 1.15; }
p, li { font-size: 10pt; line-height: 1.15; }
</style>

# OrganCore 1.11.1 — reversed expression and crescendo shoes

## Summary

Expression and crescendo shoes wired reversed — where the closed (CC 0 / crescendo off) position gives the higher ADC reading — now work. Previously such a shoe calibrated with Min above Max, which the mapping treated as a degenerate calibration, so the shoe sat at 0 permanently. Three files change: `src/ExpressionHandler.cpp`, `src/Crescendo.cpp`, and `library.properties`. No contract symbols change.

## Behavior

Calibration keeps its meaning: Min is the reading captured at closed, Max the reading captured at full, whichever voltage each happens to be. When Max is above Min the shoe maps as before. When Max is below Min the reading is clamped to the calibrated span and scaled in reverse, so closed still gives 0 and full still gives 31 (the swell value before CC scaling, and the crescendo level). Only Min equal to Max — no travel captured — is still treated as degenerate and held at 0. The crescendo's hysteresis compares raw movement in either direction, so it is unaffected.

## Migration

None. The calibration procedure is unchanged: set Min with the shoe closed and Max with it open. A reversed shoe that was calibrated under 1.11.0 or earlier already has the right values stored and starts working on the first boot with 1.11.1.
