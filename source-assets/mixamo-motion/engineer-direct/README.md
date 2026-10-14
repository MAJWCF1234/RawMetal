# Engineer direct Mixamo sources

Downloaded on the repaired `engineer-full-fingers` character after the user
placed its body rig markers. Both original hands are retained. The original
Mixamo export omitted the pinkies; the repaired source has all 65 bones.

Six packs and 23 individual downloads contain 122 animation FBXs and one
character FBX. Packs overlap; this is a file count, not 122 distinct actions.
The original downloads and extracted FBXs are local, ignored assets. The
manifest preserves paths, sizes and SHA-256 hashes; the clip catalog records
each imported file's actual FPS and duration.

Requested export: FBX Binary, 24 fps, uniform key reduction. Most files are
24 fps. The 20 pistol pack animations retain 30 fps, as does its static
character FBX. Resample by elapsed source time to 24 fps when baking.

## Engineer behavior requested by the user

- Standing support idle: `Texting While Standing`, operating a data pad.
- Walking with the pad: male `Walk With Briefcase`, with its matching turns.
- Draw/stow: reach behind, hiding the pad and showing the pistol at a hand
  attachment event. Never show both props together.
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
This source intake does not replace the game's existing animation bank.
