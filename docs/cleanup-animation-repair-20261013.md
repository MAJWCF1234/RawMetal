# Cleanup animation repair — 13 October 2026

The six cleanup characters retain their repaired OBJ geometry, RMC2 arm
calibration, textures and existing save clip IDs. All 26 protected asset hashes
match the pre-repair snapshot. The first-person arm rig hash is also unchanged.

The broken split retained 1.229 m of donor travel in a walk loop while gameplay
also moved the actor. Height-only ownership assigned most fingers to the legs.
Idle substituted bind geometry for its first key; positive aim pitch rotated
downward. Weapon placement followed finger averages independently of hitscan.

The initial repair used 16 in-place samples, closed idle/walk/crouch loops,
full donor forearm orientation and rigid hands. Death sampling follows donor
angular motion to avoid shrinking fingers during collapse. Rifle roles use
rifle motion; Medic and Engineer use pistol motion. Treatment retains UAL2
Chest_Open on immutable Medic geometry.

The current bake uses 32 samples; see the
[sampling follow-up](cleanup-animation-quality-20261013.md).

Optional RML1 frames carry pelvis, spine, shoulder, elbow, wrist and palm
direction beside RMA2 vertices. The runtime aligns the halves at the moving
pelvis, aims around the posed spine and keeps whole hands on the upper layer.
Final attachments use those composed joints and the gameplay grip/barrel.
Short arms can slide along the shotgun receiver/pump; the pistol support hand
smoothly releases toward the hip when a two-hand grip would exceed reach.
The Medic case follows its carrying palm while the opposite hand treats.

NPC kneeling uses a 1.36 m hull at 1.6 m scale. Aimed crouch mesh maxima range
from 1.133 to 1.267 m. The player's crouch is unchanged. Guard grips, collision,
hitscan and perception share the NPC height.

## Initial repair verification

- Release build: **25,604,608 bytes**, below 33,554,432 bytes. All **222** embedded resources match their source losslessly.
- **103** native split checks pass across all six roles: feet, waist, head/hand edges, Fire upper layer, aim signs/clamps, crouch bounds, loops, legacy fallback and Medic anchors.
- **180** native pose/view checks pass. Maximum final hand-edge change: **0.0021 mm**; wrist reach and barrel error are below **0.0002 mm**. These are measurements of the final attachment, separate from baked quantization.
- Independent baked hand-edge checks: maximum **1.55 mm** at keys and **2.34 mm** between keys. Loop endpoint error and planar pelvis drift are zero; treatment anchors match joint wrists exactly.
- Friendly AI, guard combat, guard actions, guard defense/policy and Engineer drone tests pass. Beam tests cover 60/120 Hz, save/restore, a valid 1.4 m passage and rejection of a 0.9 m player-only passage.
- Live Engineer inspection passes deployment, five owned drones, maintenance and pistol support. Native walking, aiming, crouch, death, Medic and terminal views were inspected.

## Reproduce

```powershell
py -3 tools/VerifyCleanupAnimation.py
.\RawMetal.exe --cleanup-split-test
.\RawMetal.exe --cleanup-layer-inspection
.\RawMetal.exe --cleanup-layer-inspection --cleanup-layer-role=4 --cleanup-layer-sample=10
```

The Python check requires NumPy. `--baseline` accepts a saved Name/Sha256 JSON
manifest; `--output` writes its results. Native captures and attachment CSVs
are written under `diagnostics/animation-repair-20261013/native`.

## Remaining failures

Full smoke still exits **38** at the existing authored-save byte-exact check.
Guard-role tests still fail the existing Medic detour/healing fixture. Vulkan
still fails four existing diffuse/emission radiance-filtering checks; hardware
rendering remains active. These are not fixed by this animation repair, and
overall GoldSrc parity remains unproven.

Basic Shooter (16), Pistol/Handgun (20) and Pro Rifle (49) donor packs were
downloaded from Mixamo. Original FBX/ZIP donors remain local and ignored;
the embedded runtime bakes and provenance notes are tracked.
