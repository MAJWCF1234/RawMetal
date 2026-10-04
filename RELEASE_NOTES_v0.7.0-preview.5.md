# v0.7.0-preview.5 — Freight construction and material stability

Freight Access now has a human-scale service ceiling, recessed extraction fans, mounted cage lamps and connected painted pipework. Receiving controls, warehouse equipment and the inspection pit use actual working-space arrangements. The 32 campaign chunks are preserved; this update adds no maps.

- Imports the supplied complete forklift from `forklift.zip`, with all eight original texture atlases, four grounded wheels and a correctly oriented mast/forks. The source mesh has 373 triangles and uses the shared fixture importer and collision bounds.
- Imports extraction fans from PSX Bunkers, a packing crate from PSX Mega Pack II and a physical switch/CRT control console from PSX Tech. Original source pixels and source geometry are retained; paint/status variants are prepared at runtime.
- Replaces receiving bay's wall-like supports with corner posts and low guards. Adds an assembled hoist, connected control services, staged freight and a sheltered warehouse dispatch alcove.
- Adds generic open steel stairs with optional side rails. Treads and guards share rendering, movement collision and optical geometry, preserving clearance below the flight. Custom campaigns and both editor export formats support the same records.
- Narrows the inspection pit into a real service trench with masonry cheeks, jack supports, local cage lamps and painted services. Develops transfer supports, sorting apparatus, platform piers and the collapsed cargo service route.
- Adds green status displays to native computer screens without altering their casing, keyboard or normal maps. Manifest records get chairs, desk trays, stocked shelves and a task light.
- Adds generic wall-mounted lights with authored facing, intensity and range. Existing ceiling light records retain their defaults.
- Fixes GPU material caching when a newly loaded texture reuses an old pixel allocation: texture generations distinguish material lifetimes and preserve identities across moves.
- Filters emission atlases in linear radiance with complete mip chains, preventing small glowing glyphs from flickering or losing average brightness at distance. HDR, 4x MSAA, GGX normal shading and PC/VR post processing remain enabled.
- Reduces repeated normal-generation work and merges equal-height ceiling optical solids without changing their union. Light candidate pruning preserves exact visibility results.

Validation and matching-camera image comparisons are recorded in [the freight reference pass](docs/freight-reference-pass.md). Pixel difference is a change measurement, not a quality score. This remains an incremental preview: the full freight brief is not finished, and live headset comfort/controller testing remains pending.

Final executable: **23,312,384 bytes / 25,000,000 bytes**; 185 source-matching embedded resources. Vulkan, traversal, save, streaming, offline VR and editor checks pass. The 1080p RTX 5060 Ti freight samples range from 115.4 to 167.5 FPS, with zero static rebuilds; several scenes are slower after added construction, and cold warehouse cache construction remains a known hitch. Live headset, GTX 1660 and Windows 10 machine testing remain pending.
