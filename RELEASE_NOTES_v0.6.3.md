# RawMetal v0.6.3 — Renderer Lighting Repair

Fixture lights previously stopped at 5.48 metres, leaving much of the tall
warehouse on uniform ambient. Their influence now fades smoothly over 12 metres.
Lower interior ambient lets the direct light and occlusion define the surfaces.
Static shadow budgets count complete visibility queries instead of individual
march steps, and exhausted budgets no longer imply that a lamp is visible.

Cached tangent-space normals now use the physical surface hemisphere. Loading
geometry from behind no longer removes its front-side PBR highlight. Moving
actors and doors receive material lighting, and the arms and weapon respond to
visible room fixtures instead of a fixed brightness. The wooden cargo atlas is
calibrated for linear lighting so its board detail remains readable.

A shared static-light visibility system accelerates exact box intersections,
including oriented fixtures, rack posts, shelf decks, and guardrail bars. Light
passes through openings that the player collision envelope intentionally blocks.
Terrain and shaped water beds retain their volumetric visibility trace. Static
lighting remains cached during door movement. The final HDR bloom pass samples
small emitters more closely and uses a modestly stronger glow; it leaves the
base image and HUD sharp. Renderer diagnostics now identify linear HDR or LDR.

No maps were added. The Receiving worker encounter, original dying voice,
soundtrack and existing saves remain compatible. All 166 embedded resources
are verified losslessly; the executable remains below 22,000,000 bytes.

Validation and fixed-camera pixel comparisons are documented in
[docs/renderer-lighting-repair.md](docs/renderer-lighting-repair.md).
