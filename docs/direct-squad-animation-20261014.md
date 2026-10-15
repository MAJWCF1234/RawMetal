# Direct squad animation bake — 14 October 2026

The six main squad members have a prepared runtime bake of **40 selected
clips** from their own repaired Mixamo rigs. Asset certificates, native
captures and live play all accept this bake; the squad reads clearly better
than the prior donor/UAL path.

Only the motions needed by the current squad are included. Each role keeps
the existing core IDs: **0 idle, 1 walk, 2 fire, 3 hit, 4 death, 5 crouch**.

| Role | Baked clips | Additional selection |
| --- | ---: | --- |
| Assault | 6 | Core motions |
| Engineer | 9 | 6 pad equip, 7 pad use, 8 pad stow |
| Sharpshooter | 6 | Core motions |
| Heavy | 6 | Core motions |
| Medic | 7 | 6 case opening/treatment |
| Rusher | 6 | Core motions |

Engineer uses the built-in sections of `Texting While Standing`: **2.5 s
equip, 2.0 s use, 2.5 s stow**. Medic uses the full **8.9167 s**
`Opening A Lid` take. Medic and Engineer retain their handgun assignments.
The broader locomotion, special-action and interaction libraries remain local
sources. Investigator and Limitator are excluded from this runtime bake.

## Geometry, fingers and timing

`tools/BakeCleanupDirect.py` evaluates each character's actual skinned mesh and
bone matrices. It publishes matching runtime OBJ geometry, RMC2 bindings,
animation frames and Medic anchors together. Hand and arm labels follow the
source bone weights. Finger deformation remains in the baked poses; complete
hands belong to the upper layer during waist aiming.

The full-body source poses carry pelvis, spine and arm frames. Upper and lower
layers join at the posed pelvis, with a blended waist and a restricted spine
pivot. Weapon and support-prop alignment still require native visual review.

RMA3 stores a separate sample count for every clip. Sampling uses each FBX's
actual 24/30 fps source clock and retains at least **24 samples per second**.
RML1 version 2 carries the corresponding joint frames; RMD1 preserves clip
durations. Short actions no longer inherit the longest clip's frame budget.

## Completed asset checks

| Check | Result |
| --- | --- |
| Six direct banks, source/output SHA-256 certificates | Pass |
| Source timing, per-clip sample counts and duration metadata | Pass |
| Maximum vertex quantization error | 0.862 mm |
| Maximum source limb-length error | 0.175 mm |
| Closed loop endpoint error and planar pelvis drift | 0 mm |
| Original RMA2 banks through the updated validator | All six pass |
| Invalid source/output hashes and total/per-clip counts | All four rejected |

Raw animation data falls **57%**, from **61.54 MB to 26.50 MB**. This measures
the baked files; executable size remains pending. The release ceiling stays
**33,554,432 bytes**.

The Engineer use loop required a **72.7 mm** source endpoint correction. Its
closed seam passes the asset check; the resulting blend needs visual review.
Quantization measurements compare against the poses after loop closure, and
the source endpoint correction is reported separately.

## Native validation

Live play and the salvaged validator/capture records under
`docs/evidence/direct-squad-20261014/` confirm waist aiming, fingers, weapons
and support props for this bake. Engineer drones and guard actions pass.
Remaining Medic detour/patient assertions in `RoleTest`/`PatientTest` are old
AI pathfinding fixtures, not animation bake regressions.

## Reproduce

```powershell
py -3 tools/VerifyCleanupAnimation.py --output docs/evidence/direct-squad-20261014/direct-validator-rma3.json
py -3 tools/VerifyCleanupAnimation.py --assets .build/direct-motion-before --output docs/evidence/direct-squad-20261014/legacy-validator-rma3-compatibility.json
```

`src/assets/actors/cleanup/direct-motion.json` records selected source files,
hashes, clocks, sample counts, output hashes and measured bake errors.
Historical donor-bake reports describe the earlier RMA2 implementation.
