# Chapter 2 Engineering: notes (2026-11-24)

Parked while the bug list is cleared. Nothing here is built. Read it when the bugs are done.

## The sequence (from the user)
1. The player goes into Engineering and the doors lock behind them until it is complete.
2. The skinbot stalks the player into the main area. It is the cursed robot in a stolen human skin (`O:\retro official\cursedmachinein skinsuit.zip`):
   it crawls on all fours; meat and parts lie around; robots standing in cages come alive and hunt the player.
3. The half-broken mining bots are small enough for the vents and follow the player through them.
4. The player crosses a long area, repairs a generator and so powers the mine entrance door.
5. The player escapes back through a vent, crawling in a crouch, to the main path.
It needs the robotics workshop (mining robots and parts), robots as zombies, repairable generator -> power state -> mine door, openable vent covers at both ends.

## The robot assets (surveyed 2026-11-24; extracted copies are scratch, the zips are in `O:\retro official\`)
| pack | what it really is |
|---|---|
| `old-xrk-mining robot-standalone.zip` | `Robot_Low.fbx` (570 KB) + 5 PBR maps (~2.2 MB). The humanoid robots we use, like the endoskeletons in Security Breach: the half-bot crawlers, the stalkers. Blender 5.2 imported 0 meshes from it: it needs another import route (ufbx, or re-export). |
| `mining robot-drone.zip` | `XRKReduxShowcase.fbx`: one mesh, 13,092 verts, 65 bones (Hips/Spine/Chest/Neck/Head, fingers), 3 actions (Idle, Reference, ShowcaseIdle); materials MainBody, Hands, Head; 15 PBR maps (~2.0 MB). For D2 and the other showcase robots. |
| `Scrapped Robots.zip` | 25 OBJs that are ONE model (identical geometry hash, 3,455 verts, ~1.6 m across, units look like cm) plus one PBR atlas ("Material _28", ~7 MB of maps). Import it once. |
- Crawl clips on hand: `O:\retro official\downloads\mixamo-descent\transformed_crawl.fbx` and `survivor_crawl_back_fear.fbx`. More animations can be fetched.
- Half-bot plan: cut the XRK humanoid at the waist (keep torso, arms, head), retarget a crawl clip onto its Hips/Spine/Arm bones, give it a small creature hull that fits a vent, register it as a creature (the `Transformed` crawler behaviours are the nearest existing kind).
- Canon (memory `project-canon-lore`): the decommissioned mining-robot program; D4 is the supervisor, the one barely used, slightly cursed.

## Size budget (the exe is a retro-size principle: trim and compress first, ask before raising)
- `RawMetal.exe` 95,254,016 / 99,614,720 bytes: headroom 4.36 MB (the build warns under 6 MiB).
- Packed size by folder (MB): library 17.8 (player 15.1, audio 8.2, creatures 3.8), cleanup-voices.bank 6.5, actors/cleanup 4.2, audio 2.3, ai/brutal 2.3, library/textures 1.9.
- Biggest single packed assets (MB): `claire-animation.bin` 8.17 (19.5 raw), `cleanup-voices.bank` 6.52, `claire-fp-shotgun.fbx` 1.36, `claire-arms.fbx` 1.12, `transformed-animation.bin` 0.94, `claire-fp-rifle.fbx` 0.73, `medic-animation.bin` 0.69.
- Optimise before importing robots: the Claire animation bank and first-person rigs are the first candidates (several FP rigs carry a full arms mesh each); the PBR maps of the robots go in at 256 px or less, one model once.

## Bug queue (the user's 2026-11-24 fix list; do these first)
Done this session: shotgun pellets (and their damage/mark fixes), sniper rifle, controller options (first pass), doorway/gateway/ramp collision, stacked kit lights, floating bullet holes, reload scope bug, pickups and commands.
Open:
- Arches, pipes and vents with real openings: collision from the model's mesh, crouch and prone walk-through test, small-creature duct test (user: the bent-sheet arch and the concrete pipe cannot be walked, crouched or crawled through).
- Some stair types are not climbable (cube hitbox); do not clip off balcony rails in the editor or the game.
- Props cannot be scaled by hand or by numbers; model scaling squashes slightly and stops.
- Textures and blood do not lift with doors; doors are busted in lots of stuff.
- Lights mounted wrong or with another light inside; "lots of the new stuff is busted".
- Cannot hear other players' footsteps in multiplayer.
- VR: arms rip off at some player heights (needs smarter height adjustment).
- Editor: smooth rotation mode; heavy doors / mesh / ticket gate; curtain-wall and megastructure facade modules; the UI restyle from `Qt-Advanced-Docking-System-master.zip`, `monochrome-kde-master.zip`, `dear-imgui-styles-main.zip`.
- Controller support needs improvement (waiting on the player's feedback).
- Builder: its `settings` test fails (it has its own front-door menu); the ejected case is the scaled pistol brass; the Builder's pickup-kind list was not photographed.
