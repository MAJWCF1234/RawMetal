# Investigator direct Mixamo sources

Downloaded on the Investigator's existing verified 65-bone rig: nine packs,
28 individual clips, 241 animation FBXs and one static character. Counts include
pack overlaps. Raw animation FBXs and ZIPs remain local and ignored; manifests,
SHA-256 hashes, the clip catalog and original source rig are tracked.

Requested FBX Binary, 24 fps, uniform key reduction. 192 animation files have
24 fps timelines; Pro Rifle's 49 animations and static character retain 30 fps.
Use elapsed source time when resampling to 24 fps to preserve playback speed.

Coverage includes pistol/rifle and unarmed locomotion, aim/combat/reloads,
cover/evasion, injury, awareness/corner scans, pickups/device/door interactions,
melee, get-up recovery, ladder transitions, deaths, prone movement/combat and
squad gestures. Generic axe equip reaches are adaptation references, not finished
gun-draw animations. Pack and individual names are listed in manifest.json.

All 242 FBXs passed skeleton, source timing and finite sampled pose checks.
Selected native renders preserve evaluated bone matrices across FBX bind poses
and show both hands with bent fingers. The long coat folds sharply in crouch
and prone; integration must address coat deformation and prop contacts.

This collection is pending game bake. Slicing, waist aiming, prop alignment,
events/transitions and in-game visual verification remain outstanding.

[Verification report](../../../docs/investigator-mixamo-intake-20261014.md)
