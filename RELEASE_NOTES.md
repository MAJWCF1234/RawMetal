# RawMetal Release Notes

Changelog of shipped updates. Newest first. Each section is one release.

---

## v0.11.0 - Ashfall Exclusion Zone update (2026-10-24)

The Ashfall Exclusion Zone custom campaign is rebuilt as an open,
STALKER-style testbed: twenty maps of woods, marsh, ruins and roads to roam.
It stays a separate custom campaign; the main campaign is unchanged.

- **An open zone.** Rookie camp, fuel station, military checkpoint,
  scrapyard, radio tower, pine forest, swamp, creek, anomaly field, old
  mine, industrial zone and village ruins, with a three-step quest from the
  camp elder to the radio tower.
- **No walls.** The zone is edged by dense forest and wasteland, and beyond
  it the land rolls into wooded hills out to the horizon.
- **Sky and ground.** A broken overcast sky with a low warm sun, god rays
  through trees and ruins, low steam over the ground and wet patches. PSX
  grass, clover, dirt, gravel, forest litter and mud painted over the
  terrain.
- **Lush plants.** Tall grass, goldenrod, aster, bracken, sumac, bushes,
  leafy trees, aspens and pines from the purchased nature packs.
- **Painterly Horizon LOD (experimental).** Distant trees and plants are
  drawn as shared multi-angle impostors with depth and lighting, handing
  over smoothly to the real models up close; distant grass folds into the
  ground colour.
- **Sound.** Wind, birds, crickets, frogs, a creek, crackling camp fires and
  humming anomalies; footsteps on dirt, grass and gravel.
- **Voices.** Every NPC line re-recorded with directed delivery, plus new
  voices for the checkpoint soldiers, the scrapyard bandits and the camp
  guide, who will follow you. Speech is now muffled by what each character
  wears: a sealed hazmat hood, a gas mask, a face plate or nothing, and
  squad callouts come over the radio.
- **Performance.** The zone holds the 60 fps frame cap; small ground plants
  no longer render into the sun shadow maps.
- **Fixes.** An NPC crossing into another map no longer crashes the game;
  crashes now leave RawMetal-crash.txt for diagnosis.

---

## v0.10.0 - Rebuilt facility update (2026-10-24)

Every campaign map from the Foundry to the Deep Junction is rebuilt from
scratch. Each keeps its name, its purpose and its story beats; everything
else is new.

- **The facility (maps 0-9).** Foundry, Pressure Works, Turbine Gantry with
  its catwalks, the Reactor Complex stitched across three chunks, Service
  Gallery, Coolant Return, Cable Vaults, Pump Annex, Utility Junction and
  Waste Handling.
- **The freight district (maps 10-31).** Freight Access, the lift machinery
  and auxiliary lift, the receiving floor and lanes, the high-bay warehouse
  (A-B, C, D and upper racks), the manifest offices and routing wing, the
  transfer conveyors, sorting and rail yard, Platform 3, the rolling-stock
  depot, the maintenance bore and the deep junction.
- **Purchased-library dressing.** Shipping containers on flat wagons, drums,
  storage tanks, gas cylinders and road blocks, and a full subway car (Train
  27) with platform furniture, ticket machines and a vending machine.
- **Offices built like offices.** Glazed partitions, fabric cubicles with
  workstations that face their seats, suspended ceilings with troffers,
  meeting, records, archive and switch rooms.
- **A continuous rail network** from the platform through the depot and the
  bore to the junction, with no tracks into walls.
- **Smarter Cleanup patrols.** Guards walk real beats and scan from their
  posts instead of pacing.
- **Rendering.** Auto exposure, the AgX-free display curve, coloured blackbody
  lamps, wet reflections, combat effects, crater bullet holes and a visible
  autofocus rack on the cinematic lens.
- **Neural textures, round 3.** 45 more textures ship as x2 neural textures.
- **27,915,264-byte EXE / 26.62 MiB**, **5.38 MiB free** under the 32 MiB ceiling.
- All 27 test suites pass and every rebuilt map passes the placement audit.

---

## v0.9.0 - Light update (2026-10-24)

A rendering update on top of v0.8.0's Night Shift content.

- **Per-pixel lamps with cube shadow maps.** Every lamp lights the static
  world per pixel with normal-mapped diffuse and GGX specular, and casts real
  shadows from a six-face shadow map (props, railings and grates all occlude).
- **Sun and sky for Ashfall.** A directional sun with three cascaded shadow
  maps, sky-hemisphere ambient, and a procedural overcast sky with a sun disc
  and drifting cloud deck.
- **Temporal anti-aliasing.** Stable edges and texture detail in motion, on top
  of 4x MSAA. `r_taa off` returns to MSAA only.
- **Close-up quality.** Walking up to a wall no longer collapses it into flat
  blocks: a detail layer keeps relief and lighting at any distance, parallax and
  texel snapping fade with magnification, and the ghost copies the occlusion
  pass painted on near walls are gone.
- **Volumetric light.** Lamp shafts are shadow-mapped, so light pours through
  gaps and stops at walls; haze is continuous from the eye.
- Mailbox presentation (no tearing), per-frame shadow budgeting for streamed
  chunks, and the renderer survives walking into every Security Office wall at
  1080p in the new windowed-path harness.
- **27,827,712-byte EXE / 26.54 MiB**, **5.46 MiB free** under the 32 MiB ceiling.
- Vulkan, performance, prologue, Exclusion and world isolation tests pass.

Console switches for A/B: `r_pixel`, `r_taa`, `r_post`, `r_texels`.

SHA-256: `E4A465B463B12D3ECEB6E6CC92136FEF8AE1178A53B65C1568957167CC9D2FD2`

## v0.8.0 - Night Shift update (2026-10-23)

A content and presentation update, not a patch.

- **Security Office prologue** now opens the main campaign: a shift at your desk,
  a friend messaging you on the workstation, the alarm, the run to the service lift
  and the drop into the Foundry. The workstation opens full screen, and you start
  unarmed.
- **Maps 0-2 rebuilt** (Foundry, Pressure Works, Turbine Gantry) with industrial
  set pieces, new wall and floor surfaces and a second lamp grid. The freight lift
  cab is remodelled, and Map 3 now reads as a second failed escape.
- **New renderer post pass**: ambient occlusion with one-bounce light, denoised
  screen-space lamp shadows (creatures and props now block lamps), depth haze,
  volumetric lamp scattering, wide bloom and a filmic grade with a real black
  point.
- **Redesigned UI**: the title screen, HUD, pause menu, death screen and
  inventory now use anti-aliased type and translucent panels instead of the
  5-pixel font.
- Missing textures now show a pink and black checker and are logged, instead of
  stopping the game.
- Headless frame readback is about 5x faster (104 ms to 21 ms per frame).
- **27,698,688-byte EXE / 26.42 MiB**, **5.58 MiB free** under the 32 MiB ceiling.
- Prologue, world isolation, Exclusion and Vulkan smoke tests passed.

SHA-256: `08B3F110CFAC4E4E27FF206C6D39233D7D2B1BBC94652D63E9075FCD0E39954A`

## v0.7.2-preview.13 - Spatial audio patch (2026-10-19)

Approved spatial audio first pass, integrated with all sixteen reduced textures.

- Walls now smoothly muffle treble as well as volume using existing geometry checks.
- Positional stereo recordings resolve to a single emitter; music and listener-centered cues retain stereo.
- All 80 NPC takes, voice triggers, cue ordering and lift transitions remain intact.
- **24,171,008-byte EXE / 23.05 MiB**, **8.95 MiB free** below the unchanged 32 MiB ceiling. Native trained-PGO shipping build, no new dependencies or sound assets.
- Full Vulkan smoke, audio regression, live Windows device playback and 231 packaged-resource checks passed.
- Initial candidate mixer stress: 48 spatial loops in 0.25-0.28 ms per 11.61 ms output block. Final PGO shipping validation measured 0.537 ms/block. These short runs vary, exclude game-thread geometry and are not a gameplay FPS comparison.

This first slice adds spectral occlusion and point-source mixing; HRTF, room reflections and audio-thread contention work remain for later passes.

SHA-256: `1092F66712970439633C65925E0418B692FC86C9457A97ADBB79B6D12F65967F`

[Audio validation and remaining work](docs/Performance/audio-spatial-pass1-20261019.md)

## v0.7.2-preview.12 - Neural texture reduction patch (2026-10-19)

Nine more textures reduced, **sixteen total**, using the existing shared decoder and weights. Textures reconstruct once at load time and remain cached at their authored dimensions. Scissors is the largest new saving; human textures, hands, weapons, UI, normal maps, animations and AI code are unchanged. Fine grain is slightly softer.

- **23.05 MiB EXE (24,173,568 bytes)**: **1.235 MiB / 5.08% smaller than preview.11**.
- **2.156 MiB / 8.55% smaller than procedural preview.10**.
- **8.95 MiB free** under the unchanged 32 MiB ceiling.
- Exclusion lab: **309.6 aggregate FPS**, **3.23 ms average frame time**, **4.77 ms p99**, **196.7 FPS 1% low**, across 14,400 patch frames on RTX 5060 Ti at 640x360 Vulkan HDR/4x MSAA. Run averages ranged 292-321 FPS. Baseline variability was substantial; these measurements do not establish a speed gain or a stable low-FPS improvement.
- **750 decoder checks**, **231 resource checks**, full **Vulkan smoke**, **28 visibility views**, twelve Exclusion and 23 freight-detail captures passed/inspected.
- **Zero reconstruction during gameplay** in the 240-frame cache test or atlas scale changes.

The release asset matches the tested root executable.

SHA-256: `54E771DF0DBE6F3F08584B01B6BC4CC5D9E1A6BE01E341D51245BE9E7DC7509B`

[Full statistics and methodology](docs/Performance/preview12-stats-20261019.md)
[Screening and visual comparisons](docs/Assets/neural-textures-round2-20261019.md)

## v0.7.2-preview.11 - Performance and bloat reduction patch (2026-10-18)

Seven reduced surface/prop textures reconstruct once with a small shared neural model and remain cached at their authored dimensions. Odd-sized atlases preserve their UV layout. Retired OpenKTG/recipe experiments are archived; no new animation banks or runtime frameworks are added. Fine grain is slightly softer.

- **24.29 MiB EXE (25,468,416 bytes)**: **943 KiB / 3.65% smaller** than preview.10.
- **7.71 MiB free** under the unchanged 32 MiB ceiling.
- Matched trained-PGO ABBA comparison: **315.28 versus 318.69 FPS** (1.07% lower FPS for the size reduction); frame time 3.1718 versus 3.1378 ms. This patch does not claim an FPS gain over preview.10.
- **750 decoder checks**, **231 embedded-resource checks**, full **Vulkan smoke** and **28 visibility views** passed. Twelve Exclusion and eight service captures inspected.
- Zero reconstruction during 240 tested gameplay frames or atlas scale changes.

The release asset is byte-identical to the tested root executable.

SHA-256: `7C96C14B434CA63F3D71BD2A96EA72C947A47871327BE3A923BF53D470F10D42`

[Measurements and visual comparisons](docs/Assets/neural-textures-expanded-20261018.md)

## v0.7.2-preview.10 - Procedural textures (2026-10-18)

Replaces five photographic environment materials with OpenKTG-backed **RMP1**
recipes that expand to ordinary **RMT1** RGBA at load. Resource IDs **125 /
131 / 132 / 149 / 150** (concrete floor, sign rust, panel metal, service floor,
weathered metal). Original PNGs archived under `src/assets/texture-old/`.

Package: five PNGs (~1.13 MiB) ? 160 bytes of recipes; root EXE **27,236,352 ?
26,434,048 bytes (?783 KiB)** vs preview.9. Runtime atlases keep the same
dimensions. Visual HF energy improved vs the first procedural previews;
accepted for in-game look. All **25** release regression checks PASS; procedural
texture unit test PASS; pack verifier reports **5** procedural recipes among
**230** packaged resources.

Executable SHA-256
`8AF412E7214016E025B7FEDEF937A618AD9B15368AE329362FF292FDC3A9970C`.
**7.12 MiB** headroom under 32 MiB.

[Procedural texture report](docs/Assets/procedural-textures-20261018.md).

## v0.7.2-preview.9 - Performance hotfix (2026-10-18)

Promotes the tested round-ten SIMD animation winner unchanged. Same clip samples,
waist/hand behavior, graphics, AI cadence and 2015-compatible x64 baseline.
Interleaved preview.8 comparison: **308.4 to 314.2 FPS** (+1.86%); 1% lows
**187.8 to 199.9 FPS**. All nine runs match gameplay fingerprints. **330 FPS
remains unmet.** All **25 regression checks PASS**, including full smoke.

Executable **27,236,352 bytes / 25.975 MiB**; **6.025 MiB headroom** under 32 MiB.
All 230 packaged resources unchanged; winner is 7 KiB smaller than preview.8.
SHA256 `75774028EE7F71F6F1C94775ABDDA08CE83A50723F4936013DEDF9249735B812`.

[Round-ten results](docs/Performance/Competitive%20Round%2010%2020261018.md).

## v0.7.2-preview.7 - Performance hotfix (2026-10-17)

Promotes the validated round-four winner to the canonical root executable: compact
Vulkan actor triangle stream (**152 ? 92 bytes**, full float precision) with the
original lighting ABI via unused W channels, plus MSVC **/Ob3** inlining. Same
meshes, animation, materials, AI rates and save rules � render/compiler savings only.

Fresh interleaved promote check (this session): root preview.6 median **189.8 FPS**
vs winner **205.9 FPS** (+8.5%); scene **2.83 ? 2.48 ms**. Identical AI queries
**14,029,424** and final health **99158**. Smoke / exclusion / friendly / stealth PASS.
Competition batch met the **200 FPS** average target (median **202.3 FPS**).

Executable: **27,405,312 bytes** (26.14 MiB), **5.86 MiB** under the unchanged 32 MiB
ceiling. SHA256:
`661b557e7d73275327d2ed81edca4d2f6c9b9a9d9bff1ec0a40c3d02c7d69d34`.

[Full round-four results](docs/Performance/Competitive%20Round%204%2020261017.md).

## v0.7.2-preview.6 - Performance hotfix (2026-10-17)

Promotes the validated round-three winner unchanged to the canonical root executable.
Exact NPC state-key prefix hashing and validated sorted-state lookup hints remove
repeated bookkeeping without changing AI behavior, state IDs, save data or visuals.

Three-run Exclusion median: 160.6 -> 186.5 FPS; 1% lows 94.6 -> 107.7 FPS.
Measured at 640x360, Vulkan HDR/4x MSAA; CPU wall time including presentation.
This is an average throughput improvement, not a minimum FPS guarantee.

Validation: state-key/property and cached/reference replay tests, 24 regression/profile
commands, twelve exact cross-build encounter views, 28 visibility views, GPU actor
comparisons, packaged resources, full Vulkan smoke and save checks all passed.
Executable: 26,939,904 bytes (25.69 MiB), below the unchanged 32 MiB ceiling.
SHA256: 1d4c6fd9da0d471d9fb1924ef0230606ec22cb493b443577f4782b92a27536e9

## v0.7.2-preview.5 - Performance hotfix

Promotes the validated round-two support/door broad-phase winner to the root EXE.
Fresh three-run Exclusion medians: average **161.74 -> 163.51 FPS** (+1.1%),
1% lows **95.41 -> 105.14 FPS** (+10.2%), P99 **9.471 -> 8.997 ms**,
and frame-time spread **8% narrower**. RTX 5060 Ti, 640 x 360, Vulkan HDR / 4x MSAA.

- Reject irrelevant door footprints before terrain-height queries
- Use existing spatial indices for prop/terminal support, retaining exact live tests
- 21 regression commands passed, including full Vulkan smoke
- 28 visibility/reference views and 12 direct EXE comparison captures: zero changed pixels
- 56 GPU actor comparisons passed; nine combat replays kept identical gameplay counters
- No additional runtime cache, no reduced visuals/behavior, unchanged 32 MiB ceiling

EXE: **26,896,896 bytes**. SHA-256:
`35c12d64d450e86271eba3365fe4eb24ec4c9fe1c85a354046299d33dfc2534d`.

[Full round-two results](docs/Performance/Competitive%20Round%202%2020261017.md).

---

## v0.7.2-preview.4 - Performance hotfix

Promotes the validated competitive winner to the canonical root executable.
Fresh three-run Exclusion medians on RTX 5060 Ti at 640 x 360 improve average
FPS **140.2 -> 163.6** (+16.7%), 1% lows **92.2 -> 103.6** (+12.4%) and P99 frame
time **10.108 -> 9.039 ms**. Frame-time spread remains essentially unchanged;
this release does not claim hitch elimination or 500 FPS.

- Exact terrain-height reuse, retained minimap occupancy and decorative panel pixels
- Includes the validated decision clock, retained route queries and GPU actor preparation
- Preserves the walk-start optimization and existing gameplay/content edits
- No reduced animation samples, lighting, geometry or AI decision rates in this round
- Hard **32 MiB** executable ceiling unchanged

**Validation:** 26 commands passed, including full `--smoke-test --vulkan`;
28 cache reference views had zero changed pixels, and 56 GPU actor comparisons passed.
The final six competitive replays retained identical gameplay counters.
Full results: [competitive round](docs/Performance/Competitive%20Round%2020261017.md).

**Canonical EXE:** 26,896,896 bytes (25.65 MiB; 6.35 MiB headroom).
SHA-256: `17059affba29ab4e694d7e0177cdee69c35829c86510d1ec3985813c04277c0a`.

**Play:** Custom Maps -> ASHFALL EXCLUSION ZONE, or `RawMetal.exe --pc --exclusion-zone`.

---

## v0.7.2-preview.3

Exclusion yard CPU pass + docs vault reorg. Lab `--lab-performance` on RTX 5060 Ti settled ~**8.4 ms** (~**119 FPS**) after hull-scale walk pitch and static crate/prop VBO bake; calm-patrol think LOD ships for idle friendlies (combat yard totals noise-flat). GoldSrc cost-model notes live in the Obsidian vault under `docs/`.

- Indoor/terrain `aiWalkSamplePitch` coarsened; static C/B + props baked into scene VBO (cache rev 4)
- Calm patrol AI think LOD + `think_debt_us` bank; stealth/guard/tactics follow-ons from the same arc
- Depthworks branding resources in `src/branding/`; docs flattened to topic folders with `Home.md`
- Hard **32 MiB** ceiling unchanged

**Play:** Custom Maps ? ASHFALL EXCLUSION ZONE, or `RawMetal.exe --pc --exclusion-zone`.

**Validation:** EXE 26,665,984 / 33,554,432 bytes (6.57 MiB headroom). `--smoke-test --vulkan`, `--exclusion-test`, `--stealth-test`, `--friendly-ai-test` PASS.

---

## v0.7.2-preview.2

Synced tester build of the NPC radio polish from preview.1. Repo root hygiene: ignore ashfall performance dumps; no gameplay change.

Same radio behavior as preview.1 (Search bark, AllyDown cooldown, reply timing, channel holds). Confirm in play, then cut **v0.7.2**.

**Validation:** EXE 26,533,888 / 33,554,432 bytes (6.70 MiB headroom). `--npc-voice-test` PASS.

---

## v0.7.2-preview.1

Preview toward the next complete NPC radio patch. Builds on 0.7.1 chatter election with Search cues, AllyDown cooldown, longer reply timing, and channel holds so delayed copies do not expire while someone else is talking. Superseded for testers by preview.2.

- Lost-contact **Search** elects one Investigate bark; Investigate?Search stays quiet
- Reply delay stretched (~1.1�2.2 s) so the optional copy lands after the contact call
- Pending tactical / work / AllyDown claims hold the channel while `npc_voice_busy` instead of timing out mid-sentence
- AllyDown picks the nearest living voice, arms a squad cooldown immediately, priority above patrol chatter
- `--npc-voice-test` covers Search bark, Investigate?Search silence, AllyDown election/cooldown

Save format still 14. Hard **32 MiB** ceiling unchanged. Confirm in play, then cut **v0.7.2** when radio feels done.

**Validation:** EXE 26,533,888 / 33,554,432 bytes (6.70 MiB headroom). `--npc-voice-test`, `--exclusion-test`, `--stealth-test`, `--guard-roles-test` PASS.

---

## v0.7.1

Patch on the 0.7 line: Cleanup / Witness NPCs were already in the Exclusion loop; this fixes their placeholder radio behavior so squad contact sounds like chatter instead of a roll call. Exits the `0.7.0-preview.N` series � same playable loop, versioned as a normal patch.

- Shared Alert / Investigate / Retreat elect one radio voice per squad (spotter wins contact); at most one delayed copy answers
- Cue cooldowns keep the rest quiet after a call; pending claims no longer drain as a sequential queue
- Multi-chunk ambient audio uses resident world/enemy views instead of copying whole chunks each frame
- Optional `--ashfall-performance` frame profile harness and cheaper nav connector selection for profiling passes
- Native `--npc-voice-test` covers one-speaker contact + optional reply

No new maps or animation banks. Save format still 14. Hard **32 MiB** ceiling unchanged. Play: Custom Maps -> ASHFALL EXCLUSION ZONE, or `RawMetal.exe --pc --exclusion-zone`.

**Validation:** EXE 26,530,304 / 33,554,432 bytes (6.70 MiB headroom). `--npc-voice-test` PASS.

---

## v0.7.0-preview.16

Same VO chatter fix shipped under the preview label before graduating to **v0.7.1**. Superseded for testers by 0.7.1.

- Shared Alert / Investigate / Retreat elect one radio voice per squad (spotter wins contact); at most one delayed copy answers
- Cue cooldowns keep the rest quiet after a call; pending claims no longer drain as a sequential queue
- Multi-chunk ambient audio uses resident world/enemy views instead of copying whole chunks each frame
- Optional `--ashfall-performance` frame profile harness and cheaper nav connector selection for profiling passes
- Native `--npc-voice-test` covers one-speaker contact + optional reply

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
