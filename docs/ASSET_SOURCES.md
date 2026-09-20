# Asset source provenance

This file centralizes source/provenance notes that were previously scattered through `src/assets/**/SOURCES.md` and related notes.

## First-person arms
Source archive: `psx-first-person-arms-free-game-assets.zip`.
The supplied `arms_rig.glb` / `arms_rig.fbx` expose 18 clips including grab, guard, jab, knife, relax and rest motions. RawMetal uses the supplied rig data for firearm hand poses and unarmed animation.

## Audio
Source library: `O:/retro official`.
RawMetal audio was copied or converted from the purchased/source library while leaving originals unchanged. Runtime resources include shotgun fire/pump, metal and concrete footsteps, jump/landing, pickup/hurt, creature calls/attacks/deaths, machine ambience, door, melee, clutter impacts, lift motor/creak/snap/crash, and reactor/music loops.

Surface-lift/reactor sources include Rust & Blood v1.1d, ROT v1.9, and Echoes v2.1 assets. Runtime WAV effects are 44.1 kHz PCM16; MP3 music is decoded at runtime.

## Physical clutter
Primary source: `PSX Bunkers v1.8.8.zip` under `O:/retro official`.
Runtime clutter includes trash, MRE, power supply, PCB and floppy-disc meshes/textures. The glass bottle remains from the purchased Street Furniture asset set.

## Weapon effects
`muzzle-flash.png` came from:
`O:/retro official/Weapons & Items/RetroWeaponPack_V1/Assets/RetroWeaponsPack/FX/Textures/MuzzleFlash.png`.

## Environment
Source library: `O:/retro official`.
Imported assets include generator, barrel, crate, concrete/brick/metal surfaces, hazard stripes, chemical/machinery/confined-space signage, terminal panels and warning graphics. Originals were not modified.

## Facility
Purchased sources include Modular Retro FPS Kit v1.5 and PSX Bunkers v1.8.8. RawMetal uses supplied columns, vents, ceiling lamps, generators, shelving, wall cabinets and computer/control-terminal assets. Model UVs and per-face material assignments are retained.

## Architectural materials
Purchased sources: PSX Textures v3.1 and PSX Textures II v1.6.
Runtime material families include wall-grey, wall-painted, bulkhead, grating and concrete-floor with matching normal maps where supplied.

## Pickups
Copied from `O:/retro official`:
- first aid: `Models/FBX/Props/first_aid_kit_hr_1.fbx`
- first-aid texture: `Textures/first_aid_kit_hr_1.png`
- shotgun shells: `Weapons & Items/PSXAmmoBoxes/12g/12g_Green_240.fbx`
- shell texture: `Weapons & Items/PSXAmmoBoxes/12g/Textures/12G_240px.png`

## Pressure Works
Copied from the purchased `O:/retro official` library:
- pump: `Models/FBX/machinery_1.fbx`
- compressor: `Models/FBX/Large Props & Machinery/machinery_mx_1.fbx`
- pipe: `Models/FBX/Large Props & Machinery/pipe_ax_1.fbx`
- gate: PSX Mega Pack II sample `gate_1.fbx`
- wall/floor/metal surfaces: Sewer environment textures

## Reactor instruments
Source: `E:/retro official/PSX Tech v1.2.3.zip`.
- model: `PSX Tech/Models/other-formats/FBX/control_panel_etx_1.fbx`
- texture: `PSX Tech/Textures/electronics_etx_part_1.png`

## Reactor Stalker
- model: `E:/retro official/Characters/Characters_psx/Models/Killers/Character_Monster_03.fbx`, mesh `Character_Monster_03`, 1,386 triangles, with matching texture
- motion: Quaternius Universal Animation Library Standard, `Unity/UAL1_Standard.fbx`
- used clips: `Idle_Loop`, `Walk_Loop`, `Punch_Cross`, `Hit_Chest`, `Death01`
- the supplied animation library declares CC0 1.0

`src/tools/bake_creature.cpp` retargets and bakes the creature animation into RawMetal's compact runtime format.
