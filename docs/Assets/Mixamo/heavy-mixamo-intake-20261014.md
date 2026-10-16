# Heavy rig and animation intake

The user placed Heavy's body rig markers. Both original hands are present,
with 2,268 calibrated corners per side. Mixamo returned 57 bones, preserving
thumb/index/middle/ring chains but omitting both pinkies. The repair adds two
fitted pinky chains while preserving all 57 original bone positions and every
mesh vertex. Only pinky/web skin weights are reassigned. Ten independent
finger bend checks passed, and the repaired 65-bone FBX was reuploaded.

The original FBX, fixed FBX and JSON report are retained under
`source-assets/cleanup-repairs`. Reproduce with:

```
blender -b --python-exit-code 1 --python tools/RigEngineerPinkies.py -- --character heavy --source source-assets/cleanup-repairs/heavy-original-mixamo.fbx --output diagnostics/heavy-pinky-rebuild
```

The shared pinky tool also passed fresh regeneration on Engineer's original
source after extending it for Heavy.

## Animation collection

Seven packs and eight individual clips supply 192 FBXs: 191 animation files
and one character. All passed 65-bone/30-finger-joint, timeline and finite
sampled-pose checks. File/archive copies are hash-verified. Pack overlaps
are retained, so the count does not represent distinct moves.
After verifying retained hashes, 16 download duplicates were removed. All
catalogued FBXs, seven archives and the original body rig remain local.

Coverage includes rifle combat and aiming, directional movement, crouching,
reloads, cover, injury movement, hits, deaths, squad gestures, two rifle blocks,
a rifle-butt punch, get-up from back/stomach and ladder start/climb/exit.
The sources support Heavy's existing suppression role; no weapon assignment
or gameplay behavior was changed during intake.

The requested export was 24 fps with uniform reduction. 142 animations use
24 fps. Pro Rifle's 49 motions and static character retain 30 fps. Actual
clocks are recorded; the later 24 fps bake must preserve elapsed source time.

Original downloaded aiming, crouched aiming, walking and blocking poses were
rendered, including a hand close-up showing bent fingers. The source renderer
preserves full evaluated bone matrices across FBX bind poses. These renders
contain no weapon; final grip alignment remains necessary.

![Heavy source poses and fingers](images/heavy-mixamo-source-poses-20261014.png)

## Runtime boundary

These are verified sources before slicing. Selected renders and structural
audits do not prove visual quality for every clip. The current game assets
and executable remain unchanged. A fresh asset-packer comparison confirmed
all 228 embedded resources still match source exactly. Final weapon grip,
aim pivots, animation events, transitions and in-game verification remain
pending for the later bake.
