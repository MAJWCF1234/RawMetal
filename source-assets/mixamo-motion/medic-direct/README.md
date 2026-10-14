# Medic direct Mixamo sources

The user placed the body rig markers on the original Medic model. Mixamo
returned 33 bones with one index chain per hand despite Standard Skeleton
(65). The repair preserves both authored hands, all mesh vertices and the
body rig, fitting five finger chains to each hand. All ten independent curl
checks passed. Animations were downloaded after reuploading the fixed rig.

Six packs and 22 individual clips provide 121 animation FBXs and one static
character FBX. The packs overlap. Local FBXs and archives are ignored;
`manifest.json` preserves their relative paths, byte counts and SHA-256 hashes.
`clip-catalog.json` records each file's actual source timing and audit results.

The browser requested FBX Binary, 24 fps and uniform key reduction.
101 animations use 24 fps; 20 pistol pack animations and the static character
retain 30 fps. Resample by elapsed source time to 24 fps for the final bake.

## Role selection

- Pistol locomotion, strafing, crouch transitions, jumps, firing and whip.
- Medkit case opening/closing, pickup and kneeling interactions.
- Authored Administering CPR and Receiving CPR, plus assistance/carrying.
- Unarmed locomotion, injury movement, cover, landing/roll, get-up recovery,
  three death variants, squad gestures and ladder transitions.
- Generic axe equip reaches as adaptation references, not finished pistol
  draw or holster animations.

CPR is a specific source action, not a substitute for all treatment. The
later in-game bake needs patient/kit alignment, prop events, finger grip
adjustment and action selection appropriate to the conscious or downed target.
Pistol remains Medic's weapon; these sources do not introduce a shotgun.

Use the common auditor and source renderer:

```
blender -b --python-exit-code 1 --python tools/InspectAssaultMixamo.py -- --library source-assets/mixamo-motion/medic-direct
blender -b --python-exit-code 1 --python tools/InspectEngineerMotion.py -- --character medic --library source-assets/mixamo-motion/medic-direct --output diagnostics/medic-mixamo-20261013/source-poses
```

Do not transfer raw animation F-curves between imported FBX bind poses. The
source renderer preserves full evaluated bone matrices and checks the result.
The source intake milestone left the game assets unchanged.

## Integration status — 14 October 2026

Seven clips are now prepared: the six core motions and the full **8.9167 s**
`Opening A Lid` take for case opening/treatment (clip 6). RMA3 retains actual
source timing, bending fingers and matching wrist anchors with per-clip
24 fps samples. Medic retains the pistol. The rest of this library remains
available for later action selection. Native build, case/patient alignment
review and live healing verification are pending; see the
[direct bake report](../../../docs/direct-squad-animation-20261014.md).
