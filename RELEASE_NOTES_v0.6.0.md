# v0.6.0 — Freight District Playtest

The main campaign continues from Waste Handling into eight freight areas spread
across 22 connected chunks: Freight Access, Receiving, Warehouse, Manifest
Offices, Transfer, Empty Platform, Depot and Cargo Maintenance Tunnels.
These are native campaign maps, available through normal traversal and the
console map commands. The final deep-junction handoff marks the current end
of the freight section; the following district remains under construction.

The district now plays **Pressure**, an industrial loop from Echoes Audio Super
Kit, across all 22 maps. Music fades between the original campaign and freight
section and keeps its playback position across stitched boundaries. The original
reactor music and music-volume controls remain supported.

Exactly **two mutated humans** appear: Warehouse C (chunk 17, displayed map 18)
and Depot Trackside (chunk 27, displayed map 28). The supplied MonsterPSX model
and texture use its authored idle, nervous-walk, lunge and recovery animations,
baked into interpolated vertex samples to avoid per-frame FBX evaluation. Each
has 180 health, a 1.85 m standing hull and a telegraphed melee attack. Creature
calls reuse the existing fiend sounds. The model has no supplied death clip;
the baked recovery pose is rotated and grounded for death.

Shared authored cargo lifts, timed events, residency groups, material choices,
signs, music cues and creatures work in native and custom campaigns. Cargo
pallets, service vans, office carpet, location signs and structural lighting
supports give the freight locations more distinct contents. Fixture geometry
participates in the static cache; doors retain the shared moving-door path.

Save version 13 persists the new creature type. Versions 1–12 remain readable;
the existing population migration adds these encounters to older saves while
preserving matched actors' damage, deaths and collected items. Back up saves
before switching back to an older executable, which cannot read version 13.

The executable is **20,663,808 bytes**, below the **22,000,000-byte** cap.
All 157 embedded resources passed exact source verification: texture RGBA
pixels and dimensions, model/animation bytes, and embedded audio bytes. The
freight MP3 is a 128 kbps conversion; its original OGG remains in the source
tree. Binary predictor and whole-resource LZMS candidates improve packing
without changing decoded resources. No runtime DLL or loose asset folder is
required.

Validation passed:

- Release build and Vulkan smoke test, including animation, combat/audio events,
  AI movement, attack damage, audio mixing, volume/mute and original lift music.
- Campaign traversal, floor/standing clearance, dedicated music in all freight
  maps, exactly two mutants and damaged-mutant save/load.
- Freight music cursor continuity across all 22 chunk views and original-OST
  return; one freight voice, with no duplicate soundtrack instance.
- Shared mechanisms, world isolation, streaming, saves, console and Vulkan tests.
- Actual Vulkan captures of walking, lunging, death and both authored encounters.
  Matched-camera pixel comparisons confirm the visible creature changes;
  these comparisons measure coverage, not artistic quality.

Local visual evidence: `diagnostics/freight-mutant-release/encounters.png` and
`pixel-comparison.json`. Previous freight layout/material comparisons are in
`diagnostics/freight-visual-pass/`. Generated mutant PPM captures were converted
to PNG and removed.
