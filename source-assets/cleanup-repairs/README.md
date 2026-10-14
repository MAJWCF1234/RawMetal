# Cleanup source repairs

`assault.obj` replaces the missing Assault hand with an exact reflection of
the intact authored hand. The replacement keeps donor UVs and reverses
triangle winding. All 13 shared cuff positions match the existing sleeve;
the body and intact hand are preserved. `assault-arms.bin` carries the
matching per-corner calibration, with the mirrored hand assigned to its new
side. Regenerate with `py -3 tools/RepairAssaultHand.py`.

This is the source submitted to Mixamo for character-specific rigging and
animation downloads. It is not yet the in-game mesh: its changed topology
requires a matching animated bake before publishing the mesh, binding and
animation bank together. The existing game assets remain a consistent set.

## Finger rig

The first Mixamo auto-rig returned only 33 bones: one index chain on each
hand, with no thumb, middle, ring or pinky chains. `assault-full-fingers.fbx`
retains the user-placed body rig and supplies five fitted finger chains per
hand, with normalized weights and 65 total bones. `tools/RigAssaultFingers.py`
reproduces the repair from the original downloaded character FBX. It checks
all ten fingers independently and exports a rest rig with normalized scale.
The FBX was reuploaded to Mixamo; the downloaded Basic Shooter animation
was rendered to check its actual finger grip, rather than accepting only a
local procedural curl. See the diagnostics in `assault-finger-rig-20261013`.

Keep the corrected rig and the downloaded character-specific animations
together when baking. The game's existing rigid-hand attachment must not
replace these finger poses during the later integration.

## Engineer fingers

Engineer keeps both authored hands. The user's marker placement produced a
57-bone Mixamo rig with four finger chains per hand and no pinkies.
`engineer-original-mixamo.fbx` preserves that original export.
`engineer-full-fingers.fbx` adds only the two missing pinky chains and fits
their skin weights, retaining every original bone and mesh vertex. All ten
independent finger curl checks passed; details are in
`engineer-finger-rig-report.json`.

Reproduce using Blender:

```
blender -b --python-exit-code 1 --python tools/RigEngineerPinkies.py -- --source source-assets/cleanup-repairs/engineer-original-mixamo.fbx --output diagnostics/engineer-pinky-rebuild
```

The fixed rig was reuploaded to Mixamo before collecting the Engineer
library in `source-assets/mixamo-motion/engineer-direct`. These remain source
assets pending a matching in-game bake.
