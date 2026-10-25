---
name: ashfall-zone
description: Expand or change the Ashfall Exclusion Zone custom campaign (the open-world STALKER/Fallout-style testbed) — places, vegetation, NPCs, monsters, quests, ground cover, sky. Use when asked to add to, fix or restyle the zone, or to test new tech in it.
---

# Ashfall Exclusion Zone

The zone is a **testbed**: an open outdoor custom campaign for trying new
engine tech on real content. It must never be merged into, or linked from, the
main campaign.

## Files

| What | Where |
| --- | --- |
| Generator (edit this) | `tools/ashfall/build_zone.py` |
| Generated campaign (do not hand-edit) | `src/assets/campaigns/RawMetal-Ashfall-Exclusion-Zone.txt`, `custom maps/RawMetal-Ashfall-Exclusion-Zone.txt` |
| Record parser | `src/world/CustomCampaign.cpp` |
| Terrain, ground paint, sun | `src/world/World.cpp` / `World.h` (`authoredTerrainHeight`, `authoredGroundKind`, `zoneSky()`, `sun()`) |
| Sky, sun shafts, steam | `src/renderer/shaders/post.frag` (`zoneSkyColor`, `sunShafts`, `steamAmount`) |
| Painterly Horizon LOD | `src/renderer/Impostors.cpp`, `scene.vert`/`scene.frag` impostor paths, `Scene3D.cpp` fixture loop |
| Model catalogue | `src/assets/library/catalog.json` (see the `library-import` skill) |

## Layout

- The zone is 5 × 4 maps of 24 m (120 × 96 m). Map index = `cy*5+cx`.
- Everything is placed in **world metres**; the generator splits it into maps.
- `TERRAIN` is a seeded value-noise heightfield. `Terrain` in the generator is an exact port of the engine's, so `t.ground(x,y)` predicts the walking height.
- `t.flat(x1,y1,x2,y2,height,blend)` levels pads and roads.
- `t.paint(x1,y1,x2,y2,kind,edge)` writes `GROUND` cover. Kinds:
  - 0 short grass, 1 leafy grass, 2 clover, 3 wild, 4 dry dirt
  - 5 forest litter, 6 gravel, 7 mud, 8 meadow (the default noise mix)
- `skybox=zone_overcast` on every MAP turns on the open-zone sky. That means a broken overcast, a warm low sun, god rays, low steam and wet patches. It also keeps the authored terrain.

## Generator helpers

- **Models:** `z.model(name, x, y, yaw, scale, solid, core)` places a library model from the catalogue.
  - Facing is `(sin yaw, cos yaw)`.
  - `core` shrinks the collision box; trees use `.1` so only the trunk blocks.
- **Places:** `z.struct`, `z.light`, `z.terminal`, `z.sign`, `z.zone_name`.
- **Danger and loot:** `z.hazard` (4 fire, 7 anomaly), `z.emitter` (smoke and steam), `z.pickup`, `z.creature`.
- **NPCs:** `z.actor(x, y, visual, yaw, mode, faction, squad, patrol, ...)`. Every patrol point must stay in the actor's own map; the generator asserts it.
- **Clearings:** `z.clear(x, y, r)` keeps vegetation away.
- **Vegetation:** `scatter()` plants vegetation in per-biome patches (`TREES`, `PATCHES`). It respects roads, flats and clearings.

## Audio

- **Ambience beds** (`AudioEngine::update`) follow the ground under the player:
  - wind is always present, and louder on high ground;
  - birds are strongest in the forest litter;
  - crickets play in the open.
- **Positional loops:** camp fires (hazard kind 4) crackle, anomalies (kind 7) hum, and water in the zone carries frogs.
- **Footsteps** pick dirt, grass or gravel from the painted `GROUND` kind (`Game::stepSoundAt`). Guards use the same rule. `footstepSound()` keeps hearing working for every surface.
- **Sound files** live in `src/assets/library/audio/` (gitignored) as resources 720+, in `Sound` enum order.
  - Ambience loops are mono MP3, because miniaudio here has no Vorbis.
  - Footsteps are PCM16 WAV at 44.1 kHz.
  - Convert from the packs with ffmpeg.
- **NPC voices:** `ACTOR_VOICE|map|actor|role` picks a voice-bank role. The generator's `z.actor(..., voice=N)` writes it. Roles:
  - 8 checkpoint military
  - 9 scrapyard bandits
  - 10 camp guide (escort)
- **New lines:**
  1. Add them to `source-assets/cleanup-voices/catalog.json`, using ElevenLabs v3 audio tags (`[shouting]`, `[whispers]`, `[grunts]`...).
  2. Run `python -I tools/BakeNpcVoices.py --generate --key-file H:/Documents/Private/keys.txt`. The script reads the key itself; never print it.
  3. Check that no tag was spoken by transcribing the masters with ElevenLabs speech-to-text.
  4. Per-role filter profiles and the shared mastering chain are in the bake tool.

## Loop

1. Edit `tools/ashfall/build_zone.py`, then run `python tools/ashfall/build_zone.py`.
2. Build with `Build.cmd`. Kill a running `RawMetal.exe` first.
3. Inspect: `RawMetal.exe --zone-shots=list.txt`.
   - Each list line is `name map x y yawDeg pitch [lift]`; pitch is in game units, where 140 ≈ 1 rad.
   - Frames are written to `.build/shot/<name>.ppm`.
   - Add `--no-impostors` for an A/B comparison without Painterly Horizon LOD.
   - Atlases are written to `.build/shot/impostor-<model>.ppm`.
4. Put views on roads or in clearings. The camera drops from the sky onto the highest surface, which may be a roof or a canopy.
5. If the game refuses to load, `RawMetal-error.txt` names the bad record. Delete it before each run, because it is not cleared.
6. Run the exclusion and streaming tests before committing (see `ExclusionTest.cpp`).
