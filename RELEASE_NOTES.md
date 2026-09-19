# RawMetal v0.3.5 — Animated Melee Stalker

## Changes

- Replaced the antlered Warden with a different masked horror creature from the supplied PSX character library (Character_Monster_03, 1,386 triangles).
- Added articulated idle, walk, melee swing, hit reaction and death animation. Five Quaternius clips are retargeted onto the creature's skeleton, skinned offline and stored as compact shared-vertex samples with runtime interpolation.
- Corrected source/rest-mesh orientation, exact clip-name matching and ground alignment. Damage occurs at the melee animation's reach peak.
- Removed the ranged strike, targeting stripe and glowing charge. The Stalker must approach to melee distance; its committed windup can be dodged, and walls/closed doors block damage.
- Existing save enemy kind 3 now loads the melee Stalker. No save-format change or fresh save is required. Prior movement, physics, reload, weapon-wheel and menu changes are retained.

## Verification

The targeted Stalker test checks that all five clips deform the mesh (not just translate it), remain finite and stay above the floor. It also checks melee contact, backstep dodges, door blocking and no attack at three units. Twenty-five reactor pose captures cover the clips.

Full Vulkan/software smoke tests, save/load, physics/AI and Vulkan checks passed. The binary stays below the 19,800,000-byte limit.

The final 1,560-frame Intel Graphics benchmark passed the 50 ms update/audio/render budget at 640x360. Reactor active AI averaged 17.30 ms (57.8 FPS), with a worst measured frame of 21.59 ms (46.3 FPS); all six culling/reference comparisons matched. Startup and OS presentation are excluded.

## Known limitations

Earlier benchmarks showed intermittent elevator rendering hitches; one passing run is not a universal minimum-20-FPS guarantee. These animations are retargeted skeletal clips baked to vertex samples, not a new runtime animation graph or ragdoll system.

## Download

Extract RawMetal.zip and run RawMetal.exe. Use the backtick console and map reactor to inspect the replacement, or load an existing reactor save. Vulkan is default; --software enables fallback.

The EXE is signed with the existing self-signed RawMetal Development Build certificate. Self-signing does not establish public publisher trust, SmartScreen reputation, or antivirus clearance. The EXE inside the ZIP matches the separately uploaded executable.
