# Assault hand rig and animation intake

## Result

The missing hand was replaced with an exact reflection of the intact authored
hand. Donor UVs are preserved, triangle winding is reversed, and all 13 cuff
attachment points match the existing sleeve. The body and donor hand remain
unchanged. The corrected OBJ and calibration are in
`source-assets/cleanup-repairs`.

Mixamo's first auto-rig returned only 33 bones, including one index chain on
each hand and no thumb, middle, ring or pinky chains. The repaired FBX retains
the user-placed body rig and supplies all five fitted finger chains per hand.
All ten fingers were checked independently for actual surface movement. An
FBX export/import check confirmed 65 bones, all 30 finger joints weighted,
and normalized vertex weights. The model was reuploaded to Mixamo. Its
downloaded rifle-aim animation was then rendered to verify the actual grip.

![Downloaded Mixamo rifle grip](images/assault-mixamo-finger-grip-20261013.png)

## Collection and timing

14 packs and 44 individual clips were downloaded on this corrected character:
311 animation FBXs and one character FBX. The selection covers armed and
unarmed movement, standing/crouched/prone combat, reloads, grenade throws,
reactions, deaths, recovery, cover, squad gestures, traversal and interaction.
There are overlapping motions between packs.

The browser requested 24 fps with uniform key reduction. The audit found
295 animation files at 24 fps and 16 legacy Basic Shooter takes at 30 fps.
The character's pose file also declares 30 fps. Keep the final bake at 24 fps
and preserve real source timing during resampling. The collection totals
approximately 578 seconds of animation entries, including overlaps.

Every downloaded FBX was imported with `tools/InspectAssaultMixamo.py`.
**312 files passed**, with the full 65-bone skeleton, correct known source
clocks, valid clip ranges and finite sampled bone transforms. The manifest
and clip catalog are under `source-assets/mixamo-motion/assault-direct`.

## Integration boundary

This pass repairs and verifies source assets. It does not publish the new
hand topology or clips into the game. The later bake must publish matching
mesh, binding and animation data together, and retain animated finger poses
through hand attachment and waist aiming. Current runtime assets and the
installed executable were not changed by this intake.
Full-body skinning and transition QA remain necessary before integration;
finite sampled poses do not establish visual quality for every clip.
Mixamo's animated browser preview intermittently omitted the reuploaded
character. Its rest preview displayed correctly after refresh, and the
downloaded animation was inspected by rendering the actual FBX in Blender.
