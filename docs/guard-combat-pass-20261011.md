# Shared armed guard combat

The evolving Ash Custom Map now equips its existing guard through
`ACTOR_WEAPON|0|0|1|650|1000|9|18`. This is shared authoring and engine behavior;
no campaign maps, new assets, special Ash AI cases or separate human subsystem
were added. The existing Worker suit remains prototype art.

Guards recognize the player, raise their weapon, aim deliberately, fire, recover
and reload a finite six-round tube. Breaking visibility or blocking the muzzle's
line cancels an interrupted aim. They search the last observed position after
losing the player. Other guards hear a captured shot on their next perception
frame, while the shooter ignores its own shot. Guard patrol, attack and saved
clock fractions retain their authored identity across loaded chunks.

## Engine changes

- Player, VR and NPC shots share an explicit-origin first-hit resolver. Firing
  from a controller no longer temporarily rewrites the player's view/body pose.
- Guard aim also traces torso-to-grip and grip-to-muzzle clearance. A barrel
  protruding through thin cover cannot shoot from the far side of that cover.
- Static grid solids use cached BVH/slab intersections. Current bulkhead and
  swinging leaves, glass, moving lifts/platforms and live character hulls block
  shots. A guard fires only when the first target is the player; intervening
  characters do not take accidental friendly fire or consume guard ammunition.
- Shaped terrain/water ground uses bounded 4 cm sampling with entry refinement;
  thin authored structures remain exact. Character hitboxes are hull proxies,
  not per-triangle body-part damage. Faction tactics and takedowns are not added.
- Destructible tiles invalidate occlusion caches. A structure hit above a crate
  cannot destroy that unrelated crate. Floor/ceiling decals preserve Z normals.
- Unrelated chunk bounds are rejected before BVH queries, avoiding a first-shot
  cache-build spike across every resident map. Idle/recovering guards skip firing
  traces; actual aim checks the current line each frame.
- Guard clocks carry fractional milliseconds. In the same real-update fixture,
  the first shot occurs at 1.56667 s at 60 Hz and 1.575 s at 120 Hz.
- Armed guards count toward contacts and authored enemy-clear gates. Bullet
  damage bypasses bare-handed melee blocking. Existing unarmed guards stay unarmed.
- Save format 13 is retained; partial aim, recovery, reload, rounds and fractions
  restore and reject invalid values before calculations.

## Visual verification

The existing shotgun renders at 75 cm for a 1.6 m actor, with two attached sleeves
and gauntlets. A lazy immutable `Mesh::bindTriangles()` supports standing upper
bodies while keeping authored moving legs. Initial Vulkan captures exposed a
hunched idle/weapon overlap; source joints were checked, the grip moved forward,
and the raised weapon was recaptured from both oblique and side views. Barrel,
stock and flash now clear the hood. Original models/materials remain unchanged.

`--guard-combat-inspection` loads the actual exported custom pack and advances it
to aiming/firing through normal Game updates. It then positions a viewer for
inspection. The firing image retains the real 18-point damage result; only the
fullscreen damage flash is suppressed to isolate NPC visuals. A local unarmed
control uses the same camera/geometry. The side view is a separate camera check.

`tools/CompareGuardCombatRenders.py diagnostics/guard-combat-20261011` compares
the fixed NPC region with a max-channel difference threshold of 12. Weapon/body
attachment changes affect 39,046 pixels; aiming-to-firing affects 14,909 pixels.
These demonstrate visible feature changes and are not overall quality percentages.
Captures and differences are kept in that diagnostics folder.

## Validation and playtesting

Release build passed: **23,621,120 bytes**, below 28,000,000. All **185** embedded
resources match their sources losslessly. **61 combat/ballistics checks** and
**55 patrol/authoring checks** passed, including real 60/120 Hz updates, muzzle
inside cover, intervening/invulnerable bodies, thin solids, glass, both door types,
cross-chunk guard fire, ammunition/reloads, contacts/gates and interrupted saves.
Eight checks specifically cover protruding barrels near the body and muzzle in
both local and neighboring chunks.

Physics/AI, controls, saves, offline VR interaction, campaign maps, mechanisms,
streaming, audio, world isolation and Vulkan smoke suites passed. Offline VR
tests do not claim a physical headset session. The actual Ash four-point patrol
inspection also passed after equipping the weapon.

Seven editor runtime-record tests and the original payload checks passed against
clean HEAD editor code with the current helper. All **12** Ash records are retained
exactly, and TXT matches that clean export with zero warnings. Unrelated local
editor modifications were preserved and are not required for this feature.

Start the rebuilt `RawMetal.exe`, choose **Custom Maps**, then **RawMetal // Ashfall
Guard Lab**. Restart for a clean encounter. Stay behind the panel to observe
patrols; sustained exposure now starts combat. See [playground controls and
authoring](ashfall-playground.md).
