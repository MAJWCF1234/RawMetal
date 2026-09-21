# Depthworks Level Editor

Run `LevelEditor.cmd` from the repository root.

The launcher starts a localhost-only Python server, opens the editor, and scans `src/assets` dynamically. New models and textures therefore appear in the asset browser without maintaining a second hard-coded manifest.

Current scope:

- multi-chunk 24 x 24 top-down editing
- multiple vertical layers per chunk
- floor, wall, void and deck painting
- box, stairs, door, light, terminal, hazard and spawn placement
- live browsing of every file under `src/assets`
- texture thumbnails and full preview
- OBJ, FBX, GLB and GLTF model preview
- model placement into the map and 3D scene
- orbitable real-time 3D preview
- JSON import/export for editor projects

The browser preview is for layout and composition. RawMetal remains authoritative for exact game rendering, collision, animation, lighting and gameplay behavior.

Three.js is loaded from a pinned CDN version for the 3D editor. If that dependency is unavailable, the 2D editor, asset browser, texture preview and JSON workflow still remain useful.
