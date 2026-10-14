# Limitator direct Mixamo sources

Nine packs and 14 individual clips downloaded on its preserved existing
78-bone rig: 227 animation FBXs plus one static character, including pack
overlaps. Raw animation FBXs/ZIPs remain local and ignored; manifest hashes,
clip catalog, original rig/textures and visual evidence are tracked.

Requested FBX Binary, 24 fps, uniform reduction. 178 animations use 24 fps;
Pro Rifle retains 49 animations and its static character at 30 fps. Preserve
elapsed source time when resampling to the target 24 fps.

Coverage: rifle/pistol/unarmed movement, combat, reloads, crouch/cover/evasion,
injury, squad gestures, awareness/corner scan, punch, elbow, front kick,
rifle door kick, back/stomach get-up, ladder entry/climb/exit and deaths.
Source collection does not mandate that every action is used in gameplay.

All 228 exports passed the 78-bone/finger-joint, timing and finite pose audit.
Selected native renders show actual rifle, crouch, walk, punch and kick poses,
with bent fingers. Skinned FBXs also contain old embedded takes: explicitly
select the direct `Armature|mixamo.com|Layer0` action, not the nested T-pose.
Sharp armor creases at deep elbow/hip bends need review during game integration.
Weapons/props are not present in these renders; contacts remain unverified.

[Visual report](../../../docs/limitator-mixamo-intake-20261014.md).
Game slicing, waist pivot, prop alignment, events/transitions, baking and native
gameplay verification remain pending. Keep this actor's encounters sparse.
