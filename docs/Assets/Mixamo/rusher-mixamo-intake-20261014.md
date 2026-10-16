# Rusher rig and animation intake

Rusher's original hands are intact, with 2,298 calibrated corners per side.
The user placed its body rig markers with Standard Skeleton (65), but Mixamo
exported 33 bones and only index chains. The repair retains every mesh vertex
and body bone, fitting all five digit chains to each authored hand. Ten
independent bend checks passed, and hand renders were inspected before the
fixed 65-bone rig was reuploaded.

The original export, repaired FBX and JSON report are preserved under
`source-assets/cleanup-repairs`. Reproduce with:

```
blender -b --python-exit-code 1 --python tools/RigAssaultFingers.py -- --character rusher --source source-assets/cleanup-repairs/rusher-original-mixamo.fbx --output diagnostics/rusher-finger-rebuild
```

The shared rig tool also passed fresh Assault regeneration after extending
it for Rusher. Geometry is checked against the authored corner order.

## Collection and verification

Seven packs and 12 individual clips provide 157 FBXs: 156 animation files
and one character. All passed skeleton, all 30 finger-joint, timeline and
finite sampled-pose checks. Hashes verify the retained file/archive copies.
Pack overlaps remain available; the count is not distinct moves.
After verifying retained hashes, 20 download duplicates were removed. All
catalogued FBXs, seven archives and the original body rig remain local.

Sources include armed/unarmed movement, jumps, cover/evasion, falls/landing,
injury locomotion, rifle combat and reloads, squad signals, cross punch,
elbow strike, front kick, knee strike, back/stomach get-up, unarmed death
variants and ladder transitions. They support Rusher's existing aggressive
advance/flanking role without changing its runtime behavior or loadout.

The browser requested 24 fps with uniform reduction. 107 animations use
24 fps; Pro Rifle's 49 motions and static character retain 30 fps. The
catalog preserves actual timing for later resampling without speed changes.

Three punch phases, armed/unarmed running, a front kick and a finger close-up
were rendered from downloaded source skinning. The shared renderer preserves
evaluated bone matrices across FBX bind poses, including packs without their
own character FBX. Punch renders show bent fingers rather than rigid hands.

![Rusher source melee and movement](images/rusher-mixamo-source-poses-20261014.png)

## Runtime boundary

This pass verifies sources before slicing. Structural checks and selected
renders do not prove visual quality for every animation. Contact reach and
damage timing, weapon alignment, aim pivots, animation events, transitions
and native game verification remain pending. The current game assets and
executable were not replaced. A fresh asset-packer check confirmed all 228
embedded resources still match their source assets exactly.
