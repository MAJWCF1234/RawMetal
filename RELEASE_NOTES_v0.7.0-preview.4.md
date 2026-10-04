# v0.7.0-preview.4 — Freight overlooks and material shading

Manifest offices now overlook the real stitched warehouse, with protected glazing and authored visibility residency. The PC and VR Vulkan shader fixes a scale-dependent rejection that disabled GGX highlights and parallax relief on valid close surfaces. The campaign still contains 32 chunks; this pass adds no areas.

- Adds a generic `VISIBLE_GROUP` authored record for observation windows and distant connected halls. Runtime custom campaigns can use the same system; leaving the viewing area restores normal streaming rules.
- Opens the warehouse/Manifest facade into a real window with a thin retaining wall, steel posts, sill, header and solid glazing. Both Manifest chunks retain the warehouse vista.
- Imports the supplied wooden table and chair from PSX Bunkers, sharing an existing texture. Records desks have seated-scale furniture and manifest trays; freight/workshop tables use actual tables instead of miniature generators.
- Develops Cargo Tunnel wall cables, fasteners and drainage grilles. The forced service door belongs to a framed signal alcove with a readable log.
- Authored terminals and runtime custom packs can set console facing direction. The level editor exports this orientation; older records retain their existing default.
- Removes unintended ceiling suspension rods from a receiving-floor tool.
- Replaces Pump Annex cargo blocks with supplied motor equipment, grounded soleplates and low connected pipework. Frames the operator and dispatch glazing and marks switchgear clearance.
- Replaces stepped waste chute geometry with compact steel drop housings fixed to the retaining wall and discharge lips inside the bins. Marks the sump edge.

The new GPU regression compares a black dielectric at multiple projected sizes and UV scales, including mirrored and collapsed UVs. The old shader is also rebuilt and tested to demonstrate that it fails the new magnification case. Campaign traversal, visibility streaming, custom parsing, service maps and offline VR regressions are checked, alongside actual rendered views and matching-camera pixel comparisons.

This remains an incremental PC/VR preview. The headset is disconnected, so live VR comfort and controller feel are unverified. Transfer machinery and other remaining brief details still need further development. This release does not claim completion of the full freight brief or a broad performance gain.

Validation: the old shader fails the new magnified-surface GPU test (highlight 58 drops to 0); the fix keeps 58 at every valid scale. Inspected matching-camera 1080p renders show the restored close material relief. Native RTX 5060 Ti freight scenes measured 127.6–181.3 FPS with HDR and 4x MSAA; Manifest offices measured 169.4 FPS while retaining its real warehouse view. All freight and six door-cycle benchmarks had zero static-map rebuilds during motion. Cold geometry construction still costs up to 1.68 seconds in the warehouse and is excluded from these warmed measurements. See [validation details](docs/industrial-visual-pass.md).

Executable: **22,124,032 bytes / 25,000,000**. All 168 embedded resources passed lossless verification. Windows 10/11 x64 binary compatibility audit passed; native Windows 10 and connected-headset playtests remain pending.
