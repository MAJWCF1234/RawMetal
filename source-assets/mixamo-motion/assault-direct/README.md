# Assault character-specific Mixamo collection

Downloaded through Mixamo on October 13, 2026 using the repaired
`assault-full-fingers` character. This collection contains **14 packs and 44
individual clips: 311 animation FBXs plus one character FBX**. Animation
entries overlap between packs; this count does not mean 311 distinct motions.

The download dialog was set to FBX Binary, 24 fps and uniform keyframe
reduction. File inspection confirms 295 animation files at 24 fps. The 16
legacy Basic Shooter takes retain 30 fps, as does the character's pose file.
**Keep the bake target at 24 fps, using each source file's actual clock to
preserve duration.** Do not reinterpret a 30 fps timeline as 24 fps.

## Coverage

- Pro Rifle, Rifle 8-Way, Basic Shooter, Shooter, Lite Rifle and Slim Shooter:
  rifle idle/aim/fire/reload, directional walk/run/sprint, crouch movement,
  jumping, turns, hit reactions, grenade throwing and directional deaths.
- Pistol/Handgun Locomotion: fallback handgun movement, kneeling and transitions.
- Locomotion, Basic Locomotion and Male Locomotion: unarmed movement and turns.
- Male Injured: wounded movement, stumbling and reactions.
- Gestures Basic and Action Adventure: gestures, cover entry/exit and movement,
  falling, landing and stopping. Free Test includes skinning diagnostics.
- Individual clips add rifle butt strikes, punches, damage reactions,
  knockdown recovery, cover peeking, squad signaling, ladders and vaulting,
  armed evasions, prone combat/crawling/transitions, pickups, container lids
  and device operation.

`manifest.json` records every source file and hash. `clip-catalog.json` records
actual frame rates, durations, finger rotations and source groups. Every FBX
was imported and audited: all have the full 65-bone skeleton, with all 30
finger joints present and finite bone transforms at sampled poses. The
corrected skinned character is in `basic-shooter-pack/assault-full-fingers.fbx`.

The FBXs and downloaded archives remain local and are ignored by Git. These
are source assets, not additional embedded runtime resources. The new mesh
and animations still need a coordinated in-game bake. Preserve the source
finger deformation in that bake; the current game's rigid-hand attachment
must not overwrite it.
