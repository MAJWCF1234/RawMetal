# Limitator Mixamo intake — 2026-10-14

Nine packs and 14 individual clips saved and catalogued: 228 FBX files,
227 animated and one static character, including overlaps. Source hashes
verify the unchanged original model and both textures. Existing 78-bone rig
accepted by Mixamo without marker placement; all ten finger bend checks passed.

## Verification

All exports passed expected rig size, all 30 finger joints, finite sampled
bone transforms and source timing checks. 178 animations have 24 fps timelines;
49 Pro Rifle animations plus its static character retain 30 fps. Resample by
elapsed source time for the later 24 fps bake.

Native renders show rifle aim, crouch aim, walk, three punch phases, front kick
and a grip close-up. The renderer initially selected an embedded old T-pose
from a skinned FBX. It now selects the direct Mixamo motion action explicitly;
the auditor does the same. Source pose transfer stays within 0.002 local matrix
components for Limitator (centimetre translations, about 20 micrometres), and
0.001 for the other roles. No F-curve copying across different bind poses.
Medic rendering and the Investigator export audit passed regression checks.

![Selected actual source poses](images/limitator-mixamo-source-poses-20261014.png)

## Limits

Armor creases around elbows/hips in deep bends need integration review. Source
renders omit weapons; grip/prop contacts are unverified. Only the selected poses
were visually reviewed, not every exported clip. These downloads are sources,
not a runtime replacement or a GoldSrc parity pass. Preserve full-body/finger
motion in the later bake, implement waist aiming and transitions, then validate
native gameplay. No encounter-frequency change: Limitator remains sparse-use.

Raw motion FBXs/archives remain local and ignored. Git tracks the unchanged
source rig/textures, catalogs, renderer/auditor fixes and this visual report.
