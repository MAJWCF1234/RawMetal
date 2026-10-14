# Engineer direct Mixamo sources

Downloaded on the repaired `engineer-full-fingers` character after the user
placed its body rig markers. Both original hands are retained. The original
Mixamo export omitted the pinkies; the repaired source has all 65 bones.

Six packs and 19 individual downloads contain 118 animation FBXs and one
character FBX. Packs overlap; this is a file count, not 118 distinct actions.
The original downloads and extracted FBXs are local, ignored assets. The
manifest preserves paths, sizes and SHA-256 hashes; the clip catalog records
each imported file's actual FPS and duration.

Requested export: FBX Binary, 24 fps, uniform key reduction. Most files are
24 fps. The 20 pistol pack animations retain 30 fps, as does its static
character FBX. Resample by elapsed source time to 24 fps when baking.

## Engineer behavior requested by the user

- Pad equip/use/unequip: use the built-in pocket reach, texting section and
  return-to-pocket sequence of `Texting While Standing`. Source renders
  confirm all three sections. Loop only the use section during support idle.
- Briefcase walk and turns were rejected by the user and removed from this
  selected source library. Do not use them for Engineer locomotion.
- Pistol draw/stow: use a separate equip gesture, with the pad's own unequip
  section completed first. Never show both props together.
- Before a jump or evasive maneuver, stow the pad. If armed, use pistol
  movement and pistol jump clips. Ladders require both hands free.
- Device work, pickup and case opening/closing support drone deployment,
  repair and supervision. Keep the Engineer's pistol and five-drone limit.

`Unarmed Equip Underarm` and `Unarmed Equip Over Shoulder` are generic axe
equip gestures, not authored pistol draws. Their original skinned poses and
hand trajectories are inspected before adaptation; final grip alignment,
transition blending and prop swap timing still need the in-game bake.
Mixamo's reload search returned rifle motions, so no rifle reload was
mislabelled as a pistol reload.

## Reproduction

Preserve the local `downloads.json` until verified copies are catalogued:

```
py -3 tools/CatalogMixamoDownloads.py --library source-assets/mixamo-motion/engineer-direct --character engineer-full-fingers
blender -b --python-exit-code 1 --python tools/InspectAssaultMixamo.py -- --library source-assets/mixamo-motion/engineer-direct
blender -b --python-exit-code 1 --python tools/InspectEngineerMotion.py -- --library source-assets/mixamo-motion/engineer-direct --output diagnostics/engineer-mixamo-20261013/source-poses
```

The historical auditor filename now supports both character libraries.
The source intake milestone left the game's existing animation bank unchanged.

## Integration status — 14 October 2026

Nine clips are now prepared: the six core motions and `Texting While Standing`
sections for pad equip (clip 6, 2.5 s), use (clip 7, 2.0 s) and stow
(clip 8, 2.5 s). RMA3 retains actual source timing and bending fingers with
per-clip 24 fps samples. Engineer retains the pistol and five-drone limit.
The rest of this library remains available for later action selection.
Native build, prop-transition review and gameplay verification are pending;
see the [direct bake report](../../../docs/direct-squad-animation-20261014.md).
