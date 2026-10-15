# RawMetal Release Notes

Changelog of shipped updates. Newest first. Each section is one release.

---

## v0.7.0-preview.16

Squad tactical VO stops roll-calling � contact sounds like chatter instead of every Cleanup repeating the same cue in actor order.

- Shared Alert / Investigate / Retreat elect one radio voice per squad (spotter wins contact); at most one delayed copy answers
- Cue cooldowns keep the rest quiet after a call; pending claims no longer drain as a sequential queue
- Multi-chunk ambient audio uses resident world/enemy views instead of copying whole chunks each frame
- Optional `--ashfall-performance` frame profile harness and cheaper nav connector selection for profiling passes
- Native `--npc-voice-test` covers one-speaker contact + optional reply

No new maps or animation banks. Hard **32 MiB** ceiling unchanged. Play the same Exclusion loop as preview.15.

**Validation:** EXE 26,530,304 / 33,554,432 bytes (6.70 MiB headroom). `--npc-voice-test` PASS; `--exclusion-test` PASS.

---

## v0.7.0-preview.15

Ashfall Exclusion Zone becomes a short authored stealth/combat loop on the existing three chunks and six Cleanup roles.

**Play:** Custom Maps -> ASHFALL EXCLUSION ZONE, or `RawMetal.exe --pc --exclusion-zone`.

- Arrival cache, north scrap-rig distraction, facility relay, east winch, South Wash departure; optional witness escort / Scrap Basin
- Bounded custom objective/event/action records for interlocks, item grants, objectives, checkpoints, and completion
- Rear **Q knockout** / **E grab** (Fire kills while held); sound-only Search keeps a narrow yaw sweep; north lure second clatter
- Procedural flashlight housing after cache pickup; CRT status lamp stays on the bezel, not the phosphor
- Terrain anchoring: props/terminals keep local floor offsets; cover follows footprint
- Readable cover materials, hazard caps, numbered signs, task lamps and reused industrial props
- East winch west-flank rear KO physically replayed; north checked at three approach timings
- Receiving 02 containment detail is overrun after two player kills; real entities kill its remaining fighters while one survivor physically retreats into Receiving 03
- Save format 14 preserves living and dead script-spawned creatures; legacy versions 1-13 remain readable
- 80 ElevenLabs voice takes for the six Cleanup roles and Witness, with model-specific mask effects, positional playback, priorities and saved cooldowns
- Script exit-42 and console tick-1 fixtures corrected; full Vulkan smoke passes without quarantine
- Native `--exclusion-test` 47/47, `--stealth-test` 36/36, `--exclusion-authoring-test` 38/38

No new areas or animation banks. Hard **32 MiB** ceiling unchanged. See [README](README.md) and [encounter notes](docs/exclusion-encounter-slice-20261015.md).

**Validation:** EXE 26,510,336 / 33,554,432 bytes (6.72 MiB headroom), 230 verified embedded resources. Full Vulkan smoke passes. [Receiving breach evidence](docs/receiving-breach-20261015.md) and [voice validation](docs/cleanup-voices-20261015.md).

---

## v0.7.0-preview.14

Lossless animation packaging frees cartridge headroom for the next content pass.

- New frame-to-frame animation predictor (transform 10) before LZMS; RMA2/RMA3 round-trip byte-exact
- Duplicate packed payloads alias to canonical resources (textures + arm/skin pairs) without changing resource IDs
- Release size check warns under 6 MiB reserve; hard OoT 32 MiB ceiling unchanged
- Changelog consolidated into this single `RELEASE_NOTES.md`

**Validation:** EXE 25,499,648 / 33,554,432 bytes (~8.0 MiB headroom); 229 embedded resources verify; animation codec + split/aim + engineer smokes PASS. See [packaging evidence](docs/space-optimization-20261014.md).

**Play at release:** Same Exclusion Zone / Custom Maps content as preview.13 � packaging only. Superseded for testers by preview.15.

---

## v0.7.0-preview.13

Ashfall Exclusion Zone replaces Guard Lab / Horror OpenWorld as the wasteland feature lab.

- Continuous Surface Nets terrain for stitched Exclusion chunks (world-space heights, soft 1–3 m undulation, flat facility pad)
- Stealth kill on unaware NPCs from behind; crouch reduces sight range; multi-point patrols boot walking instead of spawn-idling
- Throw distract and lose-interest (Investigate ? Search ? Patrol) remain wired
- Exclusion Zone campaign embedded in the EXE (resource 346); Custom Maps, `MAP EXCLUSION`, or `--exclusion-zone`

**Play:** Custom Maps -> ASHFALL EXCLUSION ZONE.

---

## v0.7.0-preview.12

- Ashfall Guard Lab embedded in the EXE for one-file playtester builds under **Custom Maps**
- Squad/escort AI no longer freezes under the shared four-search nav budget: flank/cover/portal backoff, deferred Move stall charging, Deploy/Repair timeouts, and committed reload resume after Danger
- Direct Mixamo squad bake and glue-gun/tank aim from the previous bake pass remain in this build

**Playtester:** Download `RawMetal.exe`, choose **Custom Maps ? RawMetal // Ashfall Guard Lab**. No sidecar `custom maps/` folder required.

**Validation:** Guard Lab loads from packed resource 346; AI smokes (friendly, danger, schedule, tactics, roles, patient, policy); release build under OoT 32 MiB cart ceiling.

---

## v0.7.0-preview.11

- Cleanup NPC idle / walk / hit / crouch retargeted from Mixamo (rifle aiming idle, walking, hit reaction, pistol kneeling idle)
- Punch and death remain Quaternius UAL1; Medic treatment clip stays UAL2
- New bake: `tools/BakeCleanupMixamo.py` (anatomical limb retarget through Mixamo world-scale matrices)

**Validation:** Blender bake evidence `diagnostics/cleanup-mixamo-bake.json`; `--friendly-ai-test` + kill-squad stage 0; EXE 24,044,032 / 33,554,432 bytes.

---

## v0.7.0-preview.10

- Enable-gated squad staging publishes complete authored XYZ (and restores it) before `saved=1`, so activation no longer teleports from origin
- Kill-squad / danger / engineer inspections resolve cargo roles by authored role instead of hard-coded actor indices
- Medic inspection leaves the bay after wounding so treatment can complete; pursuit observer uses hull-safe south-chunk refuges
- Cleanup mesh validation requires head+body material parts; mesh material-part ID ranges cover all six cleanup role resources
- Mixamo packs parked under `source-assets/mixamo-motion/` as local donors (not baked yet)

**Validation:** `--friendly-ai-test` all PASS; kill-squad stages 0–13 PASS; EXE 24,045,056 / 33,554,432 bytes.

---

## v0.7.0-preview.9

- Reuse creature pose scratch buffer in `poseCreature` (no per-frame allocation for every humanoid)
- Skip armed bind-pull / sleeve IK work when influence is negligible
- Enable-gated guards only clear schedule/danger state while that state is still dirty
- Idle patrol guards, escorts, civilians, and pre-enable squads track visible threats via `friendlyAwareness`
- Awareness considers enemies on the NPC's resident streamed chunk, not only the player's level

**Validation:** `--friendly-ai-test`, `--smoke-test`, `--performance-test` PASS; EXE 24,025,600 / 33,554,432 bytes.

---

## v0.7.0-preview.8

- Guard Lab / enable-gated squads keep cycling UAL idle instead of freezing on bind (sample 0)
- Armed draw softens bindPull so posed idle and legs stay readable (cleanup ×0.18, Worker idle ×0.55, walk/crouch ×0.4)
- Ashfall Guard Lab primary guard uses Cleanup Assault mesh for the stronger idle cycle
- Idle phase rate raised to 0.45 so staged and armed idles read as alive

**Validation:** `--friendly-ai-test` PASS (disabled-squad idle advance and armed phase cycling); EXE 24,023,552 / 33,554,432 bytes.

---

## v0.7.0-preview.7

- Store-low / run-high Vulkan texture present scale (`TEX_UP 1|2|4`, default 2×)
- Half-res packed atlases for skybox, service van, remington, scissors (cart size)
- Denser ceiling lighting samples so fixture corona no longer stairs at 1 m tile seams

**Notes:** Console commands `TEX_UP`, `R_SCALE`, `FPS`. Guard-lab NPC animation fix still outstanding at this tag.

---

## v0.7.0-preview.6 — Rendering and transfer performance

Diffuse textures filter in linear light using sRGB images and complete area-weighted mip chains. Fine patterns retain average brightness instead of darkening as they recede. Original embedded assets remain lossless.

Normal-map mip levels and hardware interpolation preserve the mean normal and its variance; GGX uses that variance to soften unresolved highlights. Ceiling light scattering follows room density and lamp intensity, clips to the finite volume, and uses a smooth brightness shoulder.

Vulkan batches immutable texture transfers through a reusable staging buffer. Fixed ceiling fixtures reuse geometry across frames. In a focused Warehouse cache check, CPU scene preparation used 45% less time with identical compared pixels.

Campaign still contains 32 chunks. EXE limit strictly below 28,000,000 bytes. EXE **23,360,000 bytes**; 185 embedded resources verified lossless. Evidence: [docs/render-quality-pass6.md](docs/render-quality-pass6.md).

---

## v0.7.0-preview.5 — Freight construction and material stability

Freight Access gains a human-scale service ceiling, recessed extraction fans, mounted cage lamps and connected painted pipework. Receiving, warehouse, and inspection pit use working-space arrangements. No maps added.

- Imports complete forklift (`forklift.zip`), extraction fans, packing crate, and physical switch/CRT console; original source pixels/geometry retained
- Open steel stairs with optional side rails; shared render/collision/optical geometry
- Narrows inspection pit into a real service trench; develops transfer/sorting/platform construction
- Green status displays on native computer screens; generic wall-mounted lights with authored facing
- Fixes GPU material caching across texture reallocations; linear emission mip chains

EXE **23,312,384 / 25,000,000 bytes**; 185 resources. Evidence: [docs/freight-reference-pass.md](docs/freight-reference-pass.md).

---

## v0.7.0-preview.4 — Freight overlooks and material shading

Manifest offices overlook the real stitched warehouse with protected glazing and authored visibility residency. Vulkan shader fixes a scale-dependent rejection that disabled GGX highlights and parallax on valid close surfaces. Still 32 campaign chunks.

- Generic `VISIBLE_GROUP` authored record for observation windows
- Real warehouse/Manifest window with retaining wall, posts, sill, header and glazing
- Wooden table/chair from PSX Bunkers; terminal facing direction in authored packs
- Cargo Tunnel cables/grilles; Pump Annex motors and drop housings

EXE **22,124,032 / 25,000,000 bytes**; 168 resources. Evidence: [docs/industrial-visual-pass.md](docs/industrial-visual-pass.md).

---

## v0.7.0-preview.3 — Controller interaction and depot rail equipment

VR doors and terminals use the gripping controller's reach and direction instead of requiring head facing. Depot locomotives and suspended inspection chassis developed further; no map chunks added.

- Either hand can touch nearby or point at distant controls; second-hand grip can operate while first remains held
- Locomotives: round steel wheels, axles, hubs, suspension, louvers, stripe
- Suspended chassis: open longitudinal beams/crossmembers; collinear UV fix on cylinder caps
- Precomputed cylinder ring reuse

EXE 22,082,560 bytes. Offline VR/freight/physics/Vulkan checks passed; live headset comfort still playtest-pending.

---

## v0.7.0-preview.2 — Tracking recovery and freight detail

Fixes VR tracking recovery and develops warehouse/depot maps. No campaign chunks added.

- Lost headset tracking clears movement, grips, firing, jump/reload and wrist pointer; recovery starts room-scale from newly tracked position
- Disconnected controllers cannot feed stale SteamVR actions
- Warehouse shelves: varied pallet loads, empty slots, rear bracing
- Inspection-pit workshop: track-height benches, parts, cabinets, tool-bay sign

Executable below 25,000,000-byte limit; embedded assets verified lossless.

---

## v0.7.0-preview.1 — SteamVR controls and wrist computer

Adds PC/VR launch selection and native SteamVR Vulkan stereo rendering while retaining the HDR/material pipeline. VR preview, not finished headset coverage.

- Black/green terminal in a 13.9 cm steel wrist computer; right-controller pointing/clicks/sliders
- Corrects backward shotgun orientation in VR
- Either-hand grip pickup, grip-release drop/throw, shoulder shotgun stow/equip, gripped fist strikes
- Desktop right-eye spectator for streaming
- Fixes SteamVR/Vulkan shutdown ordering
- EXE limit raised to 25,000,000 bytes

EXE 22,072,320 bytes; 166 packaged assets lossless. See [docs/vr.md](docs/vr.md).

---

## v0.6.6 — Eye-Level Carry Fix

Carried objects were capped at 0.48 m above the feet (near knee height). Their centre now follows the camera aim 0.9 m from the current eye, including standing, crouching and looking up/down. Ground support, obstruction, drop and punt remain active. Rendering quality and v0.6.5 performance improvements retained.

Regression checks cover seven carryable shapes in both stances at three aim angles, shelf placement, seam transfer, throw damage and settling. EXE below 22,000,000 bytes; 166 assets lossless.

---

## v0.6.5 — Performance and Frame Pacing

Stationary pipework reuses a world-space Vulkan vertex buffer so camera turns no longer rebuild cylinders. Collision selects nearby fixture/prop/terminal candidates from a spatial grid before exact tests. Arm rig performs one final skinning pass after IK. Vulkan surface capabilities queried on window change, not twice every frame.

Resolution, 4× MSAA, HDR, bloom, parallax and materials unchanged. Local Cable Vaults p95 fell from 58.4 to 7.3 ms; Waste Handling from 36.9 to 9.9 ms (RTX 5060 Ti). See [docs/performance-quality.md](docs/performance-quality.md).

---

## v0.6.4 — Windows Compatibility Patch

Explicitly targets Windows 10/11 x64 in build configuration and embedded application manifest. Renderer fixes from v0.6.3 included unchanged.

Install [MSVC x64 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/) 14.44+ and a Vulkan-capable GPU driver. `--software` selects the CPU renderer. See [docs/windows-compatibility.md](docs/windows-compatibility.md).

---

## v0.6.3 — Renderer Lighting Repair

Fixture lights previously stopped at 5.48 m; influence now fades over 12 m. Lower interior ambient lets direct light and occlusion define surfaces. Cached tangent-space normals use the physical surface hemisphere. Moving actors/doors and arms/weapon receive material lighting from visible fixtures.

Shared static-light visibility accelerates exact box intersections. HDR bloom samples small emitters more closely. Receiving worker encounter, soundtrack and saves remain compatible. 166 resources verified; EXE below 22,000,000 bytes. See [docs/renderer-lighting-repair.md](docs/renderer-lighting-repair.md).

---

## v0.6.2 — Freight Worker Encounter

Receiving stages the first human encounter: radiation worker wanders until observed from the overlook, then walks to a fight spot and is attacked by two wasps. Faster duck/strike/hit/death animations; turns toward the active bug; pipe uses hand anchors; persistent body left behind cargo.

`map receiving` / `map 13` place the player at the overlook. Encounter state survives save/load. Shared authored actor timelines support native and custom campaigns. Vulkan lighting/transparency improvements. EXE 21,956,096 bytes; 166 resources.

---

## v0.6.1 — Freight Repairs

Service van wheels restored at correct axles (FBX assembly rebuild). Freight Access and Lift Machinery get clearer signs, cargo dressing and shaft guards. Auxiliary lift release opens its gate; lower landing connects for walk-off. Shared boundary-door headers close openings above entry/transfer doors. Terminal lore renders as green text on recessed black screens.

EXE 20,710,912 bytes; save format 13 unchanged.

---

## v0.6.0 — Freight District Playtest

Main campaign continues from Waste Handling into eight freight areas across 22 connected chunks: Freight Access, Receiving, Warehouse, Manifest Offices, Transfer, Empty Platform, Depot and Cargo Maintenance Tunnels.

District plays **Pressure** (Echoes Audio Super Kit) with music cursor continuity across stitched boundaries. Exactly two mutated humans (Warehouse C, Depot Trackside) with supplied MonsterPSX model and baked animations. Save version 13; versions 1–12 remain readable.

EXE 20,663,808 bytes (under 22,000,000 cap); 157 embedded resources verified.

---

## v0.5.9.7 — Service Map Rendering

Maps 6–9 ambient raised from 0.18–0.26 to 0.48–0.55 while retaining fixture visibility shadows. Mipmapped surface normals on concrete pressure floor; 4× MSAA (2×/1× fallback). EXE 18,649,600 bytes.

---

## v0.5.9.6 — HDR Light Pipeline

Vulkan scene renders into blended RGBA16F when supported (RGBA32F secondary, LDR fallback). Surface light, emission, additives and water stay linear through composition; bloom and filmic tone-map once before HUD. Service-map ambient tuned to 0.18–0.26 for Cable Vaults through Waste Handling. EXE 18,643,968 bytes.

---

## v0.5.7 — Ashfall Coast

Ashfall extends east into three connected coastal chunks. Walkable rock-and-sand shelf slopes into terrain beneath the sea. Coastal water, sky and atmosphere; original wasteland intact. `map coast` jumps to the middle coastal chunk. Existing Ashfall chunk IDs and saves remain compatible.

---

## v0.5.6 — Camera depth and focus

Native Vulkan presentation uses scene depth for distance-aware lens softness (2.10 cm focal length, f/4.8). Distant rooms soften up to seven screen pixels; nearby geometry, weapon and HUD stay sharp. Depth-aware sampling prevents foreground bleed. ACES tone mapping applied once.

---

## v0.5.5 — Explosive barrel fire

Shooting a chemical barrel produces a visible world-space fire burst with rising flames and smoke, occluded by geometry. Tile destroy, nearby damage, camera shake and impact sound retained. Flame/smoke masks generated in-EXE.

---

## v0.5.4 — Square fixture light shafts

Ceiling-lamp fog shafts begin across the 0.8 m square luminous fixture and widen gently downward. Vulkan ray-marched volume and software fallback use matching proportions.

---

## v0.5.3 — Atmospheric renderer update

Vulkan scene depth and material response improved while keeping low-resolution texture art. Restrained contact shading, consistent local lighting, subtler fog shafts, gloss, filmic color and small bright-pixel bloom. Static geometry caching and first-frame lighting fix. Terrain occlusion probes limited to indoor worlds. EXE 18,607,104 bytes.

---

## v0.5.0 — Utility campaign expansion

Continue through the Cable Vaults bulkhead in Coolant Return (map index 5). Four connected chapters:

- **Cable Vaults:** low service passages, damaged electrical trench detours, pulsing arcs, disconnect, flashlight pickup
- **Pump Annex:** three elevations, large machinery, manifold stairs, observation route, pump restart
- **Utility Junction:** elevated bridge, control booth, waste dispatch, checkpoint, locked future connections
- **Waste Handling:** sorting deck, disposal basin, scrap, animated conveyor/compactor

Save version 11 retains prior six-chunk campaign saves and adds the new route. Freight Access remains a future chapter; route ends at Waste Handling dispatch.
