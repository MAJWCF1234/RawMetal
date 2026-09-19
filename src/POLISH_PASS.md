# Weapon, audio and environment polish

Build: `N:/rawmetal/RawMetal.exe`.

## Controls and combat

- Settings use draggable grooved handles. Mouse capture and row ownership keep a drag on its original slider until release. Keyboard arrows still adjust values.
- Fast looking drives angular-velocity-based weapon springs, shared roll and recoil. Boot takeoff and landing drive a separate vertical spring. The wrist/socket diagnostic exercises fast turns, recoil and jumps.
- The last shell triggers a lowering animation. Left mouse then alternates the supplied skeletal jab clips; an impact at 220 ms requires range, facing, height and an unobstructed path.
- Right mouse holds guard while unarmed, cancels a pending jab and prevents starting another. Frontal melee damage is reduced by 70%; rear damage is unchanged. Shell pickups restore the shotgun.
- The supplied muzzle-flash texture is rendered additively with animated scale, rotation and decay; nearby weapon surfaces brighten briefly. Ejected shells follow a gravity curve.

## Sound and environment

Generator stereo pan is clamped before square roots to prevent invalid gains at full pan. One emitter represents each generator model. Three occlusion rays feed a smoothed envelope, and machinery loops crossfade over 50 ms. Jump and landing sounds combine supplied boot impacts with quieter equipment movement; landing volume responds to impact speed.

The entire 24×24 facility remains in render distance, with gentler distance dimming. Ceiling transitions have closure faces. Bulkheads have fixed jambs, motor headers and wall-mounted control housings on both sides; their height follows the adjacent floor. Header clearance participates in player and enemy collision. Sector labels preserve their texture proportions. Tutorial banners, route paint and imperative sign text were removed; physical safety labels and nearby interaction prompts remain.

## Verification

The smoke suite covers all four slider drags, empty-ammo switching, melee obstruction, frontal/rear guard damage, ammo restoration, generator orbit gain stability, fast-turn motion at 60/120 Hz, wrist attachment, movement, enemy navigation and the extraction route. Inspection frames include six door viewpoints, four unarmed rig angles, guard, jab, muzzle-flash phases and low/high slider positions. The Windows audio playback test verifies that the device consumes samples.

Rendering remains CPU based. This pass improves effect quality and visibility; it does not replace the renderer with a GPU backend.

