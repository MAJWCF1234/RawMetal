# Heavy direct Mixamo sources

The user's marker-placed rig exported 57 bones despite Standard Skeleton
(65). Both original hands are intact. The repair adds only the missing pinky
chains, preserving the other 57 bone positions and all mesh vertices.
All ten independent finger bend checks passed. Sources were downloaded only
after the repaired 65-bone rig was reuploaded.

Seven packs and eight individual clips contain 191 animation FBXs and one
static character. Pack overlaps remain available for selection. Local FBXs
and archives are ignored; manifest paths, sizes and SHA-256 hashes preserve
provenance. The clip catalog records actual source FPS and durations.

Requested export: FBX Binary, 24 fps, uniform key reduction. 142 animation
files use 24 fps; Pro Rifle's 49 animations and character retain 30 fps.
Use elapsed source time when resampling the final bake to 24 fps.

Coverage: rifle aiming/combat, directional movement, crouching, reloads,
cover, injury locomotion, hit reactions, deaths, squad gestures, rifle blocks,
a rifle-butt punch, get-up recovery and ladder transitions. These are source
motions for Heavy's suppression role, not a new runtime weapon assignment.

```
blender -b --python-exit-code 1 --python tools/InspectAssaultMixamo.py -- --library source-assets/mixamo-motion/heavy-direct
blender -b --python-exit-code 1 --python tools/InspectEngineerMotion.py -- --character heavy --library source-assets/mixamo-motion/heavy-direct --output diagnostics/heavy-mixamo-20261014/source-poses
```

The native source renderer preserves evaluated bone matrices across FBX
bind poses. The source intake milestone left the game animation bank unchanged.

## Integration status — 14 October 2026

Six core clips are now prepared from this character's evaluated skin: idle,
walk, fire, hit, death and crouch. RMA3 retains source timing with per-clip
24 fps samples, bending fingers and full-hand upper-layer ownership. The rest
of this library remains available for later action selection. Native build,
weapon/aim review and gameplay verification are pending; see the
[direct bake report](../../../docs/direct-squad-animation-20261014.md).
