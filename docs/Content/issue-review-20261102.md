# Visual issue review — 2026-11-02

## Repairs applied

- Added a usable escape stair from the Switch Hall pit and recessed the two wall pipes.
- Cleared the inspection-pit stairwell by trimming overhead track sleepers; the real player traversal now reaches the upper catwalk.
- Added non-rendered collision proxies to the thin Research Receiving fence panels so the player hull cannot clip through them while preserving the gateway opening.
- Relocated nine Exclusion Zone creature spawns that failed full-hull clearance, updating both authored map copies and the Ashfall generator.
- Tightened the map-audit walkway test so it only treats actual deck tiles as walkable and samples sloped rails at their real height.

## Verification

- `gameplay-fixes`: PASS
- `deep-exclusion`: PASS (all flagged spawns now clear)
- `deep-exclusion-authoring`: PASS
- Map 37: PASS
- Map 42: PASS
- Map 64: PASS
- Map 28: the targeted real-player stair traversal passes. The approximate flood audit still reports a large disconnected lower-floor region; this is retained as a known audit limitation/follow-up rather than suppressed.
- Rebuilt `RawMetal.exe` successfully; the only build diagnostic was a non-fatal existing MSVC alignment warning during whole-program optimization.

## Visual evidence

The rebuilt map captures are in `.build/issue-review-20261102/.build/shot/` and the contact sheet is `sheet-maps.png`. The receiving fence remains visible, the Switch Hall pit is clear, and the repaired inspection stairs are visible from the player route.

The Python issue-hunt runner and evidence scripts remain external tooling under `tools/` and `.build/`; they are not part of the shipped asset set. Native regression code is compiled into the executable, as before.
