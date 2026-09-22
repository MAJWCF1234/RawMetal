# World content and reusable systems

## What caused the campaign/custom overlap

The original custom-map path replaced map arrays but left campaign identity as
the chunk number. Renderer branches emitted foundry signs and reactor geometry;
floor queries, lift logic, audio, spawn tables and scripts also used that number.
Streaming used campaign offsets. A process-wide environment variable selected
the replacement world, and save format 8 stored no world identity. These were
content ownership bugs, not meshes mysteriously merging on disk.

## Implemented boundary

- `WorldId` belongs to each Game and World instance. Constructors, streamed
  reloads, menu selection and save version 9 all preserve it. There is no global
  environment switch. Save versions 1–8 are treated as campaign saves because
  they did not contain sufficient information to identify a custom world.
- `ChunkDefinition` supplies the origin, player spawn, environment and lift
  capability. Audio, movement and rendering use the same chunk origins.
- Outdoor chunks have no automatically drawn ceiling, underground floor heights,
  foundry signs, extraction paint, pipe galleries or reactor/lift machinery.
  Legacy campaign decoration is explicitly restricted to campaign chunks.
- `CreatureSpawn` and `CreatureKind` are content data. Custom creatures use the
  existing shared spawn, combat, movement and AI code; navigation selects the
  stacked path based on layers, not a magic chunk number.
- `ParticleEmitter` supplies position, drift, rise, rate, count and size. Steam
  rendering consumes emitters; Coolant Return authors its own emitter. Creating
  a steam effect elsewhere does not require changing the renderer's level tests.
- `ScriptDefinition` contains shared trigger/action types. Chunks author their
  events; `seedScripts` collects them and the shared executor processes them.
  The reactor checkpoint event now belongs to reactor content, not engine startup.
- Lighting caches are invalidated when world identity or session changes.

New systems should consume authored data/capabilities, not `level == N`. A level
is a local chunk index. Persistent references must include the owning world.

## What is still legacy or missing

This is a separation repair, not a completed general-purpose map framework.
The game still has two compiled world definitions and six chunks per world.
The title menu lists a built-in custom map; it does not scan or load the JSON in
`custom maps`. That JSON is an editor document, not the runtime's source of truth.
Do not advertise arbitrary downloaded maps as playable yet.

The campaign still contains procedural geometry in Scene3D.cpp, bespoke floor
queries and encounter tables. These must migrate to ordinary authored records
before removing the campaign adapter. The lift remains a campaign-specific
component behind an explicit capability. Steam is the first emitter type; a
general particle preset library and friendly NPC/dialogue system do not exist.
The skybox selector does not yet load the purchased skybox textures at runtime.

The next content pipeline needs a versioned map schema and validated loader for
chunks, layers, placements, spawns, emitters, triggers and asset references. Both
the editor export and runtime must use that same schema. Validate files before
changing the active session; list errors in the custom-map browser rather than
crashing. Ship maps as data packages; retain the engine and purchased raw assets
outside them as appropriate to their licenses.

## Verification

`RawMetal.exe --world-isolation-test` checks coexistence, custom/campaign save
switching, geometry reload ownership, independent triggers, full-hull spawn
clearance, path reachability, and actual movement over all five seams in both
directions. It writes six render captures. Add `--vulkan` for GPU captures.
`--smoke-test` exercises the existing campaign systems and renderer.

On Windows, wait for the GUI executable to exit and read that process's exit
code. Invoking it directly from PowerShell and checking an old `$LASTEXITCODE`
can report success before the test has run. Test outputs belong in `.build`.
