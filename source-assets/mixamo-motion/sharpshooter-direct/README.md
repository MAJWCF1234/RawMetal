# Sharpshooter direct Mixamo sources

Downloaded after repairing and reuploading the user's marker-placed body rig.
Both original hands and every mesh vertex remain intact. Mixamo returned
33 bones despite Standard Skeleton (65); the repaired source has five fitted
finger chains per hand and 65 bones. All ten independent finger curls passed.

Seven packs and 13 individual clips contain 196 animation FBXs and one static
character FBX. Pack overlaps are preserved for selection. Local FBXs/archives
are ignored; the manifest records relative paths, sizes and SHA-256 hashes.
The clip catalog records each imported file's actual timing and audit results.

Requested export: FBX Binary, 24 fps, uniform key reduction. 147 animation
files use 24 fps. Pro Rifle's 49 animations and character retain 30 fps.
Resample to 24 fps using elapsed source time; do not change playback speed.

## Role coverage

- Rifle aiming, standing/crouched movement, directional running and sprinting.
- Firing, reloads, hit reactions, deaths, cover and squad gestures.
- Prone idle, fire, reload, crawling forward, turns and crouch/prone transitions.
- Unarmed movement, injury locomotion, fall/landing recovery, get-up from
  back and stomach, and ladder start/climb/exit.

No result was returned for the site's sniper query. Rifle aiming clips are
the source for later scoped-weapon alignment, not a claimed sniper-specific
animation. Weapon grip, sight alignment, waist aiming and transitions need
the later in-game bake.

Audit and selected original skinning renders:

```
blender -b --python-exit-code 1 --python tools/InspectAssaultMixamo.py -- --library source-assets/mixamo-motion/sharpshooter-direct
blender -b --python-exit-code 1 --python tools/InspectEngineerMotion.py -- --character sharpshooter --library source-assets/mixamo-motion/sharpshooter-direct --output diagnostics/sharpshooter-mixamo-20261013/source-poses
```

These are source assets. Current game mesh/bindings/animation bank remain
unchanged until a matching bake and native visual verification are complete.
