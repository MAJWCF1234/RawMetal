# Render distance and big open maps

How far an outdoor map is drawn, what limits it, what it costs, and how to build a big map that holds
its frame rate. Code: `Game::viewDistance` (Settings.cpp), `SoftwareRenderer::render` (chunk set),
`drawScene` in `Scene3D.cpp` (far LOD, fixture cells), `GpuRenderer` (cache culling, view fade),
`post.frag` (the fade). Proof: the `open-space` lab tube (`src/lab/tubes/OpenSpace.cpp`, fixtures
`lab/fixtures/open-*.lab`, the generated world in `src/lab/StressZone.cpp`).

## What limited the view (before)

- A custom campaign draws every chunk whose nearest point is within **48 m** of the eye; nothing
  beyond, except the Painterly Horizon strip past the outer edge of the whole map.
- The CPU far plane was **70 m** outdoors (160 m indoors). The GPU has no far clip (infinite
  reverse depth), so the 48 m chunk ring was the real limit.
- Every drawn chunk was drawn whole: its full static cache, every tree mesh, wherever the camera looked.
- Shadows: three sun cascades out to 70 m; every cache with shadow runs is offered to them.
- Custom campaigns keep every chunk resident; resting props in every chunk were simulated every frame.

## The knobs

- **VIDEO > VIEW DISTANCE**: NEAR (x0.7), NORMAL (x1, default), FAR (x2), VERY FAR (x3.5). Saved in the
  settings file (appended field; older files load with NORMAL).
- **The map's own distance**: `TerrainShape::viewDistance` (0 = engine default). The default is 70 m, and
  110 m under the open-zone sky (`skybox=zone_overcast`). A `VIEW|metres` campaign record to set it is
  not parsed yet (the loader belongs to the Builder audit); add it in `CustomCampaign.cpp` and
  `CampaignExport.cpp` next to `HORIZON`.
- `Game::setViewDistance(m)` fixes a distance (the lab's `view` key, captures).
- Final distance = map distance x choice, clamped 20-400 m. It sets the CPU far plane outdoors, the chunk
  ring (`max(48, V - 22)` m to the chunk's nearest point) and the fade.

At NORMAL on a 70 m map nothing changes: same far plane, same 48 m ring, no fade.

## The LOD scheme

- **Full detail** (`Game::fullDetailRange()`, 48 m): chunks as before.
- **Far chunks** (beyond 48 m, back to near inside 44 m so the line does not thrash): one cache per chunk
  (slot `FarChunkCaches`): the terrain mesh, the horizon strip, structures; a tree is its Painterly
  Horizon impostor alone; anything 6 m or wider keeps its real mesh (buildings); rocks, walls and wrecks
  become a textured box; props under 1.2 m are left out; no lamps, doors, signs, props, probes or
  per-frame extras. Far caches cast no sun shadow (the cascades end at 70 m anyway). Switching drops
  the other LOD's caches so memory does not grow while walking.
- **Fixture cells**: a near terrain chunk with 16+ fixtures bakes them in nine 8 m caches
  (`FixtureCellCaches`) instead of inside its main cache. Each cell is quicker to bake and is culled on
  its own. Cells further than 20 m from the eye bake only while the frame's budget (8 ms,
  `setBuildBudget`) lasts; a fresh scene bakes everything for its first 12 frames.
- **Cache culling**: every static cache is skipped when its bounding sphere is outside the view frustum
  (not in VR: the second eye replays the first eye's batches, so VR keeps drawing everything).
- **The fade** (`post.frag`, `LampHeader::viewFade`): beyond 70 m of view distance, surfaces melt into
  the horizon colour (the procedural sky at the horizon, or the haze colour) from 0.62 V to 0.97 V, so the
  edge of the draw is haze, never a cut.
- **Far props asleep**: a resident chunk more than 80 m away whose props all sleep (no breakables) is not
  simulated (`Clutter.cpp`).

## Software renderer

It uses the same chunk ring and far-chunk LOD, but has no impostors: far trees become boxes. It costs
370-1500 ms a frame in these forests at 960x540 even at 70 m, so it is a fallback. Keep it on NORMAL or NEAR.

## Numbers (RTX 5060 Ti, 960x540, Vulkan, lab)

`play` is the lab's serial frame (update + render, the GPU waited on every frame, readback excluded);
the game overlaps the CPU with the GPU, so its frame is closer to `max(CPU, GPU)`.

| scene | view | chunks drawn | cached tris | draws | shadow draws | GPU shadows + scene | update | play |
|---|---|---|---|---|---|---|---|---|
| stress 13x13, before | (48 m ring) | 21 | 4.45 M | 570 | 31,000 | 6.3 + 5.3 ms | 8.0 ms | 28.6 ms |
| stress 13x13, after | 70 m | 21 | 2.09 M | 279 | 31,048 | 6.5 + 3.8 ms | 0.8 ms | 20.6 ms |
| stress 13x13, after | 140 m | 97 | 2.51 M | 567 | 31,048 | 6.6 + 4.4 ms | 1.0 ms | 22.7 ms |
| stress 13x13, after | 250 m | 169 | 2.92 M | 929 | 31,048 | 6.4 + 4.8 ms | 1.0 ms | 23.5 ms |
| stress 8x8, before | (48 m ring) | 21 | 4.37 M | 568 | 29,980 | 6.1 + 6.2 ms | 2.9 ms | 23.9 ms |
| stress 8x8, after | 250 m | 64 | 2.28 M | 478 | 30,025 | 6.1 + 5.5 ms | 0.5 ms | 21.1 ms |
| Ashfall zone, before | (48 m ring) | 18 | 1.05 M | 1185 | 13,519 | 1.1 + 1.9 ms | 5.3 ms | 21.2 ms |
| Ashfall zone, after | 250 m | 20 | 0.57 M | 785 | 14,066 | 1.1 + 1.6 ms | 4.1 ms | 19.4 ms |

The "before" update times include props that had not yet fallen asleep (the tube now lets the world
settle for 4 s first), so read the update column with care. The 13x13 stress world (169 chunks, about
200 fixtures a chunk, 312 m a side; the engine holds 180 chunks) draws to 250 m with the GPU at about
12-13 ms a frame and the CPU at about 10 ms: inside 16.6 ms when overlapped, 23.5 ms serial in the lab.
What limits it now: the sun cascades (31,000 shadow draws, 6.5 ms GPU), which do not grow with distance.

Walking at 7 m/s across chunk borders (`open-walk-8`): the worst frame went from 470-530 ms to about
180 ms, but about 10 frames are still over twice the median, so the walk check still fails.
Every bake is now on the frame budget: a chunk's main cache waits for a frame with budget left (the
other LOD's caches stand in meanwhile, so nothing blinks out), and so do far chunks and fixture cells.
The tube reports the longest single bake: main cache about 3 ms, far chunk about 19 ms, fixture cell
about 24 ms. The remaining spikes are therefore not bakes. Per-hitch breakdowns (the check's detail lists
each one, with its top cost scope and stage times) show them outside the scene and submit stages. One is
`warmChunks` decoding the new map's model textures on the main thread when the player crosses into
it (115 ms). Others coincide with the sun cascades and lamp shadows being redrawn after caches change
(GPU shadows 12-23 ms instead of 6). That is where to look next.

## How to build a big map

- Use stitched terrain chunks with `skybox=zone_overcast` and a `HORIZON` record so the edge of the world
  is land, not void.
- Trees from the library that have impostors (pines, oaks, aspens, leafy) cost almost nothing far away.
- Big buildings (6 m+) keep their mesh at every distance: few of them per chunk.
- Keep props that move (CLUTTER) few; they cost only near the player once they rest.

## Not done yet

- Budgeting the main chunk cache and far chunk bakes (the remaining walk hitches).
- Terrain LOD (far chunks keep the full terrain mesh; it is cheap next to the trees).
- Edge analysis (closed map, open ground, neighbour, nothing per side, a per-direction distance eased
  over a second), the cave darkness tone for underground maps, far emissive lights past the fog,
  occlusion by enclosure, and the km-scale underground city (proxies, instancing of modular pieces,
  thousands of lamps, depth precision at 2-5 km). These were asked for late and are not started.
- A `VIEW` campaign record (loader belongs to the Builder audit).
