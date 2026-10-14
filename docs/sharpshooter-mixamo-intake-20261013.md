# Sharpshooter rig and animation intake

The user placed the original Sharpshooter's body rig markers. Both hands were
intact, with 2,382 calibrated hand corners on each side. Mixamo's Standard
Skeleton (65) export returned 33 bones with only index chains. The repair
preserves every mesh vertex and body bone, fitting all five digit chains to
this character's hand geometry. All ten independent bend checks passed.

The source export, repaired 65-bone FBX and finger report are retained under
`source-assets/cleanup-repairs`. The rig repair clears the imported animation's
leftover hip translation before export. Reproduce with:

```
blender -b --python-exit-code 1 --python tools/RigAssaultFingers.py -- --character sharpshooter --source source-assets/cleanup-repairs/sharpshooter-original-mixamo.fbx --output diagnostics/sharpshooter-finger-rebuild
```

The repaired source was reuploaded before acquiring animations. The extended
rig tool also passed fresh Medic and Assault regeneration checks.

## Collection and verification

Seven packs and 13 individual clips provide 197 FBXs: 196 animation files
and one character. All 197 passed skeleton, finger-joint, timeline and finite
sampled-pose checks. All copied files and archives are hash-verified.
Pack overlaps are retained, so this is a file count rather than distinct moves.
After verifying retained hashes, 21 download duplicates were removed. All
catalogued FBXs, seven archives and the original body rig remain local.

Coverage includes rifle combat, eight-direction locomotion, standing/crouched
aiming, reloads, hit reactions, deaths, cover, squad gestures, injury movement,
prone aim/fire/reload/turns/forward crawl, prone transitions, get-up recovery and
ladders. The site's sniper query had no results; rifle aiming clips supply
the source motion for later scoped-weapon alignment.

The browser requested 24 fps and uniform key reduction. 147 animation files
use 24 fps. Pro Rifle's 49 motions and static character retain 30 fps. The
catalog preserves actual timing so a 24 fps bake can keep correct speed.

Standing, crouched and prone aiming and a finger close-up were rendered from
the downloaded skinning. Full evaluated bone matrices are retained when
applying pack motion to its character skin, avoiding FBX bind-pose curve
mismatch. Finger geometry remains authored; the renders contain no weapon.

![Sharpshooter original source aiming and grip](images/sharpshooter-mixamo-source-poses-20261013.png)

## Integration boundary

This pass verifies sources before slicing. Structural audits and selected
renders do not establish visual quality for every animation. Final weapon
grip/sight alignment, aim pivots, root movement, animation events, transitions
and native game verification remain pending. Current game assets and the
executable were not replaced by this intake.
A fresh asset-packer check confirmed all 228 embedded resources still match
the current game source assets exactly.
