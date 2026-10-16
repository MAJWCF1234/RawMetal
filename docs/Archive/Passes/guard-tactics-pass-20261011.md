# Guard cover repositioning — October 11, 2026

An armed guard previously stopped whenever the player was within weapon range.
If the player's head was visible above low cover while the barrel/body line was
blocked, it remained stationary in Alert and never found a firing position.
Guards now sidestep to a safe firing position and resume their deliberate attack.

## Shared engine behavior

Candidate weapon poses and real firing share calibrated torso-to-grip,
grip-to-muzzle and first-hit traces. A guard tests at most eight nearby poses per
attempt, requiring a traversable full-hull route and a shot that reaches the
player before any wall, door or other character. Candidate searches share the
existing four-search frame budget and retry after 600–780 ms. When no direct
sidestep succeeds, ordinary navigation can approach the remembered position;
impassable geometry and locked doors retain their restrictions.

The guard keeps facing the visible target while sidestepping. It lowers the
weapon during movement, stops once the firing line clears, raises the weapon
and completes the authored aiming delay. Moving or losing the shot line cancels
accumulated aim. Repositioning goals and retry clocks use the existing saved
state store and stable actor identity; invalid values are rejected on load.
Unarmed guards retain their existing patrol behavior.

## Runtime and visual verification

The same temporary Ash Custom Map gained one supported 1.3 m metal panel at
X20.6–20.8, Y7.1–7.9. All twelve authored runtime records and four patrol points
remain unchanged. The actual JSON/TXT pair exactly matches the clean HEAD
editor export with the current runtime-record helper, with zero warnings.
Unrelated local editor changes were preserved.

`--guard-tactics-inspection` loads that TXT, places the encounter player behind
the panel, and advances normal Game updates to the selected phase. It verifies
initial visible-head/blocked-body conditions, movement of at least 0.4 m without
damage, then actual firing with health 82. The fixed observer is positioned at
(22, 10.5) after simulation; HUD/hands and fullscreen damage flash are hidden for
inspection, while the real damage and weapon state are retained.

All three Vulkan captures were inspected. The first camera hid most of the gun;
the clearer angle exposed premature weapon raising during movement. The guard
was corrected to remain low-ready while moving and recaptured with attached
hands, visible barrel and firing effects. This is a behavior/pose improvement;
the existing Worker rig remains prototype art and the rendering pipeline is
unchanged.

`tools/CompareGuardTacticsRenders.py diagnostics/guard-tactics-20261011` converts
the captures losslessly and compares the same actor/cover region at an RGB
difference threshold of 12. Covered-to-moving changes 35,156 pixels, and
moving-to-firing changes 51,029 pixels out of 357,500 region pixels. These show
visible feature changes, not a percentage improvement in rendering quality.
The normal 30-second patrol, recognition and distraction inspection also passed
with the new panel in place.

## Validation

The targeted suite adds **27 checks**. Together with **61 combat/ballistics** and
**55 patrol/authoring** checks, all **143 passed**. Tests use real Game updates
at 60/120 Hz and cover reachable detours, continuous impassable cover,
invulnerable bystander safety, movement interrupted by save/load, nine-guard
navigation fairness, unarmed compatibility and malformed saved-state rejection.

In the final low-cover fixture, the first shot occurs at 2.08333 s at 60 Hz and
2.09167 s at 120 Hz, including movement, raising and aiming. Loaded and
uninterrupted movement resumes its first shot on the same frame (81 frames after
the save), with final position error about 0.0000015 m. All nine unreachable
guards receive a search; the remaining navigation budget stays between zero and
four, and no blocked shot is emitted during the ten-second wall fixture.

The physics/AI, controls, saves, offline VR, campaign maps, mechanisms, streaming,
audio, world isolation and Vulkan smoke suites passed. AI/save tests and actual
Vulkan captures were repeated after the final weapon-pose correction. Offline
VR tests do not substitute for a physical headset playtest.

The rebuilt EXE is **23,657,472 bytes**, below 28,000,000. All **185 embedded
resources** match their sources losslessly. Build diagnostics retain existing
asset-packer, legacy test conversion and vendored image-library warnings; this
pass introduced no source warnings. Logs, captures and pixel results are under
`diagnostics/guard-tactics-20261011/`.

Launch `RawMetal.exe`, choose **Custom Maps → RawMetal // Ashfall Guard Lab** and
restart for a clean encounter. Stand near (21.8, 7.5) to observe the low-cover
detour. No new campaign maps or external assets were added. Recognition-memory
retention and recovery from permanently unreachable patrol points remain future
AI work; this pass does not claim a completed tactical/faction system.
