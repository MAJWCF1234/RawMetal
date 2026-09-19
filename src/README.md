# RawMetal

A native C++20 Windows FPS prototype with textured triangle rendering, perspective-correct UVs, frustum clipping and a shared world depth buffer.

Main walls use the supplied PSX Textures concrete materials with matching normal
maps. Riveted square bulkheads and metal grating also use normal-map lighting.
See `assets/materials/SOURCES.md` for the exact maps and rendering details.
Generators use the purchased generator mesh with its separate body/metal
materials and original proportions. Every machine tile is covered, including
four-tile banks; fixture bounds drive both collision and rendering. Imported
thin panels render from both sides to prevent missing faces. Run
`RawMetal.exe --environment-inspection` from a diagnostics directory to inspect
machinery from multiple angles without enemies obscuring it.

## Play

Run `N:/rawmetal/RawMetal.exe`, or the root `RawMetal.cmd` launcher.

All builds now publish the single root `RawMetal.exe`. Close the game before rebuilding.

See `POLISH_PASS.md` for the weapon physics, punching/guard, audio fixes, extended visibility, rebuilt door controls and inspection coverage in this build.

- WASD: move; Shift: sprint
- Mouse: aim; hold left mouse: fire
- With no shells: the shotgun lowers and left mouse punches; hold right mouse to guard. Guard reduces frontal melee damage by 70%; rear attacks still deal full damage. Collecting shells draws the shotgun again.
- Space: jump; C or Ctrl: crouch (including during a jump)
- E: open/close a bulkhead, read/dismiss a log, or lift/drop nearby clutter; left click punts held clutter for 5 damage
- R: restart; Esc: open settings / resume
- M: mute/unmute all audio; N: toggle the soundtrack

Esc pauses gameplay and releases the cursor into a rusted-metal settings menu. Use arrow keys and Enter, or click the rows and volume bars. Adjust master/music/effects levels, mouse sensitivity and inverted vertical aim; Resume returns to the game and Quit Game exits. Music continues for volume preview while combat sounds pause. Settings save to `%LOCALAPPDATA%/RawMetal/settings.ini` when you resume or quit.

Volume and sensitivity use grooved slider handles on tracks. Click or drag a handle; dragging stays attached to that slider even outside its row. The window captures the mouse until release. All four sliders are exercised across multiple values in the settings check.

Rendering now uses 640×360, up from 480×270. Window resizing preserves the 16:9 view and uses whole-pixel scaling where space permits, with black borders instead of stretching text. Menu hit targets follow the same viewport. In-world signs and markings use worn pack materials, mounted warning plates and filtered decal mip levels. Chemical diamonds sit upright on the coolant vessels. Local ceiling lights use directional surface shading, interpolated vertex lighting, obstacle occlusion and distance falloff.

Clear Foundry, transfer to Pressure Works, then reach Turbine Gantry. Clear the third map and activate its upper control terminal to unlock extraction. The intake loading bay has imported freight crates and barrels; the foundry has a 1.2-metre raised deck reached by six steps, generators and six coolant vessels; containment has a raised storage landing and a marked extraction bay. Four animated bulkheads connect the sectors and open with E. A low service passage requires crouching. Three terminals tell the night crew's story through short shift logs. Overhead pipes, girders, wall bands and physical sector signs distinguish the spaces. The HUD includes a local map, nearby contacts, target health and an incoming-attack warning. Collect ammunition and health boxes along the way.

Movement uses ground acceleration and friction, preserved jump momentum, limited air acceleration, buffered jumps and a short coyote window. Ducking in flight raises the feet without raising the top of the collision hull; releasing crouch only stands up when there is room. The hull climbs 20-centimetre steps and lands on raised surfaces. This is an HL2-inspired movement pass, with its own scale and tuning.

Supplies use the imported first-aid kit and labeled 12-gauge shell-box models. Nearby labels identify them, and collection messages report the amount gained. Health kits remain when health is full; ammo adds 16 shells. The next planned improvements are tracked in `ROADMAP.md`.

## Audio

The stereo mixer plays up to 48 simultaneous voices through Windows waveOut on its own thread. All 34 samples are included in the verified RawMetal.assets bundle, copied and converted from the supplied audio library. The soundtrack is the supplied `ost_melting_spine_mx_1_short_loop`, with quieter machinery ambience around the generators. Music and effects play together; firing no longer interrupts other sounds. Bulkheads use the opening section of the supplied heavy-door sound.

Jump takeoff and landing layer boot impacts with quieter equipment movement; landing volume follows impact speed. Each generator has one emitter. Stereo panning clamps floating-point roundoff before square roots, three occlusion rays feed a smooth envelope, and machinery loops crossfade over 50 ms. The generator orbit check verifies finite gains without sudden volume changes.

Metal/concrete footsteps alternate with distance travelled, get louder when sprinting and quieter when crouching, and stop in the air or when pushing against a wall. Jump gear rustle, landing impact, shotgun fire and pump handling, empty clicks, pickups and player damage have separate cues. Each monster type has its own call, attack and death sounds; surviving hits pitch the call upward. Wasps have a flight loop, and brutes have heavy footfalls. Enemy and machinery sounds pan with camera direction, fade with distance and attenuate behind obstacles. Dead wasps stop their flight loop. Audio fades out when the window loses focus; M and N give independent master/music controls.

Audio source paths and copied license documents are in `assets/audio`. `tools/import_audio.ps1` reproduces the PCM conversions without modifying the library.

## Combat and animation

Huntsmen have 110 health, Xenowasps 85, and Scissor Fiends 280. Scissor Fiends remain slow but take more punishment. Damage falls with distance. Enemies signal attacks with a windup before striking, react to hits, and navigate doorways. Dead monsters collapse, shrink away and stop rendering after 2.4 seconds.

Enemies use a view cone and height-aware sight checks. Closed bulkheads block their sight and path. Gunfire and nearby footsteps attract investigation; after losing sight they pursue the last known position, search, then return home. Navigation respects elevation, ceiling clearance and open doors. Separation reduces crowding, wasps strafe at attack range, bugs track nearby targets during most of their windup, while slow brutes commit to a dodgeable attack. Gravity applies to live enemies and bodies, and huntsmen can climb crate-height cover.

The hands attach to grip sockets in the imported shotgun's coordinates. Every frame, the upper-arm and forearm bones solve toward the animated sockets while a damped elbow spring supplies follow-through. Finger poses come from the supplied grab.L/grab.R clips. Weapon recoil, aiming sway, bolt cycling and shell ejection share the gun transform; the hands stay constrained while the elbows swing and settle. See assets/arms/ANIMATION_NOTES.md for the source clip inventory and implementation details.

The HUD now uses the supplied rusted-metal material with recessed gauges, rivets, chipped borders and a muted amber/brown palette.

## Imported models

The game decodes the supplied FBX files with ufbx at startup and renders their actual triangles and UVs:

- Remington-870: 1,170 triangles, with its matching texture from PSX-Weapon-Pack.zip.
- First-person arms: 1,176 triangles and the supplied bones and skin weights from psx-first-person-arms-free-game-assets.zip. Two-bone IK poses the arms toward the shotgun. Bone overrides and skinning drive idle/recoil deformation.
- Huntsman: 2,416 triangles with HuntsmanSpider_2.png; alternating leg movement is driven by travelled distance and stops when idle.
- Xenowasp: 1,424 triangles, with separate insect and wing materials; it hovers and flaps the actual wing meshes.
- Scissor Fiend: 5,960 triangles from Creature/scissors.fbx with Material_Base_color.png; it uses a slow sway, attack anticipation and a lunge.

These supplied creatures are unrigged. Their walking, flying, attack and death motion is procedural, not an authored skeletal animation library.

Models and textures reside in RawMetal.assets beside the small game executable. The complete ZIP contains both files, and the game validates the bundle SHA-256 before loading it. Arms and weapon have their own depth pass; monsters share depth with the world. The old raycaster, sprite enemies and shotgun sprite are no longer active rendering paths. Look pitch rotates the camera, and shots check vertical aim.

Assets were copied from `O:/retro official`; originals were not changed. Texture-pack licences are retained in `assets`. Copied source models remain under `assets/models`.

## Build

The current build is `N:/rawmetal/RawMetal.exe`. Foundry, Pressure Works and Turbine Gantry connect through traversable bulkheads. Adjacent geometry loads when the door opens and unloads only after it fully closes, preserving progress. See [CHUNKS.md](CHUNKS.md) for layout, persistence and validation details.

Run the root `Build.cmd`, or run these commands from the repository root with Visual Studio 2022 C++ tools and CMake:

```bat
cmake -S src -B .build -G "Visual Studio 17 2022" -A x64
cmake --build .build --config Release
```

Obsolete build output folders have been removed. Compiler intermediates live only in `.build`; launch the root **RawMetal.exe**.

## Verification

Run `RawMetal.exe --smoke-test` from a writable working directory. It returns zero on success and emits model-report.txt, isolated model previews, smoke-frame.ppm, firing-frame.ppm and jump-look-frame.ppm.

Checks cover imported geometry, arm bone count, wrist attachment, skinned vertex deformation, near-plane clipping, depth occlusion, navigation reachability, the number of close-range shots required for all three enemy types, attack windup and corpse removal. Encounter frames cover all three types in idle, attack, death and removed states, plus firing, jumping and upward camera rotation.

The smoke test also validates movement/combat audio events, stereo direction, distance attenuation, overlapping voices and mute behavior. It writes a 12-second `audio-preview.wav`, `audio-test.txt`, `map-test.txt`, and four map inspection frames. Every walkable cell, pickup, enemy and extraction must be reachable. `RawMetal.exe --audio-device-test` performs a brief audible hardware playback test and records the device's played sample count in `audio-device-test.txt`.

Movement checks cover crouch-jumping, airborne ducking onto 1.1-metre cover, 60/120 Hz consistency, all six foundry steps, the low passage and blocked standing. Progression checks drive the actual player controller from spawn through three bulkheads to extraction, and verify log interaction. AI checks cover closed-door sight blocking, gunshot investigation, pursuit after a door opens, climbing stairs with all three species and dodging behind a committed melee strike. The smoke test also captures the raised deck, tunnel sign, vessel diamonds, closed/open doors, shift log and settings at 640×360.

`RawMetal.exe --render-benchmark` measures three representative views and writes `render-benchmark.txt`. Geometry outside the view is rejected before lighting; imported props and monsters use local object lighting, while architectural surfaces interpolate cached samples. Moving doors invalidate the cache. This is still a CPU renderer, and render-only timings do not include window presentation or gameplay.

This remains a prototype: collision supports heightfield floors, props and ceilings, but not overlapping floors or bridges with traversable space underneath. The supplied clips include grabs, punches, knife movements and rest poses, but no shotgun animation; its firing cycle is procedural and no full reload sequence is implemented. No external API or credential is required to run it.





