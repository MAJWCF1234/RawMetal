# Service Gallery / Coolant Return geometry repair

Campaign sectors 05 and 06 are engine maps 4 and 5 (`map gallery`, `map coolant`).
They remain one continuous 24-by-48-metre space at reactor elevation, -9 m.

## Corrected defects

- Authored partitions were at z=0 while the playable floor was at z=-9. They now use absolute reactor elevations and populate the collision/occlusion index.
- Coolant Return's final three-metre aperture had no door or continuation. A reinforced, fixed bulkhead closes the full aperture and blocks movement. This is the current prototype endpoint, not an operable transfer into another map.
- Wall cabinets now face into the room and sit against their wall. A mistaken gate prop is replaced with the intended supplied pipe mesh and correctly oriented collision bounds.
- A lower maintenance canopy defines the gallery. Shared deck materials no longer inherit an unrelated rectangular concrete patch from the opening map. Architectural box UVs repeat by metre.
- Coolant Return has four shallow, flooded return channels with a dry central route, plus a visible steam leak. The 18 cm channels are walkable without jumping.
- Old saves overlapping corrected geometry move to the nearest sampled safe floor position. Valid positions and player health remain unchanged.

## Assets

Reuses the already imported purchased pump, compressor, pipe, shelving, wall cabinet, terminal, bulkhead and industrial surface assets. See `ASSET_SOURCES.md` for provenance. Water/ripples and steam particles are procedural effects; this patch does not claim new archive imports.

## Verification

Run diagnostics in a separate working directory:

```
RawMetal.exe --megamap-inspection
RawMetal.exe --megamap-inspection --software
RawMetal.exe --save-test
RawMetal.exe --console-test
RawMetal.exe --vulkan-test
RawMetal.exe --smoke-test --vulkan
```

The inspection command produces seven targeted views and eight compass views of each sector, validates structure elevations/collision, and runs streaming regressions. Those regressions include actual walking across the seam both ways, end-bulkhead collision, and walking out of a flooded channel.

Reported timings are averages of twelve warmed render-only frames per view, excluding simulation and window presentation. They are not a guaranteed minimum gameplay frame rate. These sectors remain environment prototypes without authored combat encounters or a subsequent playable map beyond the sealed endpoint.
