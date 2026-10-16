# Medic rig and animation intake

Medic's original model has both hands. The user placed its body rig markers
and selected Standard Skeleton (65). The downloaded rig nevertheless had
33 bones, with one index chain per hand. The repaired source retains every
mesh vertex and the body rig, with five fitted finger chains per hand and
65 bones. All ten fingers passed independent surface movement checks.

The original FBX, repaired FBX and JSON report are retained under
`source-assets/cleanup-repairs`. Reproduce using:

```
blender -b --python-exit-code 1 --python tools/RigAssaultFingers.py -- --character medic --source source-assets/cleanup-repairs/medic-original-mixamo.fbx --output diagnostics/medic-finger-rebuild
```

The shared rig tool was also rerun successfully on the original Assault
source after extending it for Medic. Geometry is checked against the authored
corner order before applying calibration, avoiding FBX rounding mismatch.

## Collection and verification

Six packs and 22 individual clips contain 122 FBX files: 121 animation files
and one character. All passed the 65-bone/30-finger-joint, valid timeline and
finite sampled pose audit. Hashes verify all retained source files and archives.
The collection covers pistol combat and movement, kit handling, actual CPR,
patient assistance, injury locomotion, cover, falls, get-up recovery, deaths,
squad gestures and ladders. Pack overlaps remain available for selection.
After checking every retained file hash, 29 verified duplicates were removed
from Downloads. The source library, six archives and original rig remain local.

The export requested 24 fps with uniform key reduction. 101 animations use
24 fps; the pistol pack's 20 animations and static character retain 30 fps.
The catalog preserves those clocks. Final resampling must preserve elapsed
time, rather than relabelling 30 fps data as 24 fps.

Medkit opening, three CPR phases and a pistol body/hand pose were rendered
from the downloaded source skinning. Inspection exposed a renderer error:
copying raw F-curves between FBX bind poses twisted the pistol arms. The
inspection tool now transfers complete evaluated bone matrices and verifies
the resulting pose within 0.001 matrix-component tolerance. The corrected
render shows both hands forward with bent fingers. Engineer source rendering
also passed after this shared tool change.

![Downloaded Medic kit, CPR and pistol poses](images/medic-mixamo-source-poses-20261013.png)

## Integration boundary

These are verified source assets. Structural checks and the selected renders
do not establish visual quality for every clip. CPR is for a downed patient;
kit handling and generic interactions still require appropriate patient/prop
alignment and conscious-patient treatment authoring. Generic axe equip clips
are adaptation references, not completed handgun draws. The original hands
remain intact.

The game animation bank and executable were not replaced by this intake.
Slicing, final hand/weapon alignment, animation events, transitions and
in-game verification remain pending. Medic keeps a pistol as requested.
A fresh comparison confirmed all 228 current embedded resources still match
the existing game source assets exactly.
