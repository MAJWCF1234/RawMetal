# Rusher direct Mixamo sources

The user's marker-placed rig returned 33 bones despite Standard Skeleton
(65), with only index chains. The repaired rig retains both authored hands,
every mesh vertex and the body rig, fitting five digit chains per hand.
All ten independent finger bend checks passed. These sources were downloaded
only after reuploading the repaired 65-bone rig.

Seven packs and 12 individual clips contain 156 animation FBXs and one static
character. Pack overlaps are retained. Local FBXs/archives are ignored; the
manifest preserves relative paths, sizes and SHA-256 hashes. The clip catalog
records actual source timing and audit results.

Requested export: FBX Binary, 24 fps, uniform reduction. 107 animations use
24 fps; Pro Rifle's 49 animations and static character retain 30 fps.
Resample to 24 fps using elapsed source time to preserve playback speed.

Coverage: armed and unarmed locomotion, jumps, cover/evasion, falls/landing,
injury movement, squad gestures, rifle combat/reloads, cross punch, elbow
strike, front kick, knee strike, get-up recovery, unarmed deaths and ladders.
These are sources for the aggressive advance/flanking role; actual contact
timing, weapon use and role behavior are not changed by this intake.

```
blender -b --python-exit-code 1 --python tools/InspectAssaultMixamo.py -- --library source-assets/mixamo-motion/rusher-direct
blender -b --python-exit-code 1 --python tools/InspectEngineerMotion.py -- --character rusher --library source-assets/mixamo-motion/rusher-direct --output diagnostics/rusher-mixamo-20261014/source-poses
```

The source renderer preserves evaluated bone matrices across FBX bind poses,
using the character's verified skin when a motion pack has no character FBX.
The source intake milestone left the game animation bank unchanged.

## Integration status — 14 October 2026

Six core clips are now prepared from this character's evaluated skin: idle,
walk, fire, hit, death and crouch. RMA3 retains source timing with per-clip
24 fps samples, bending fingers and full-hand upper-layer ownership. The rest
of this library remains available for later action selection. Native build,
contact/aim review and gameplay verification are pending; see the
[direct bake report](../../../docs/direct-squad-animation-20261014.md).
