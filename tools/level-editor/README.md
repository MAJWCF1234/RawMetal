# Depthworks Level Editor

Run `LevelEditor.cmd` from the repository root. No build step is required.

The editor is intended for artists and designers, not programmers. The default asset browser shows **Prefabs** instead of loose source files. A prefab combines the real model, its game texture, a friendly name, and a useful default footprint. Click a prefab and then click the 2D map to place it. Materials work the same way: click a material and paint tiles.

## Main workflow

1. Run `LevelEditor.cmd`.
2. Stay in **2D EDIT** for layout work.
3. Paint Floor, Wall, Void, or Deck tiles.
4. Open **PREFABS**, click Pump / Shelf / Crate / Computer / etc., and click the map to place it.
5. Select placed objects to move, rotate, resize, duplicate, or delete them.
6. Open **MATERIALS**, click a texture, and paint it onto floor/wall/deck tiles.
7. Switch to **3D PREVIEW** at any time to inspect the same layout with real models and textures.
8. Export the project JSON when ready.

The **FILES** tab still exposes the underlying asset library for inspection, but artists should normally work from Prefabs and Materials.

The launcher caches the pinned Three.js editor runtime into `tools/level-editor/vendor` on first use. After that, the editor can reuse the local copy. If the 3D runtime cannot be downloaded, the 2D editor still works and reports the 3D problem instead of leaving the whole editor dead.

RawMetal remains authoritative for exact rendering, collision, animation, lighting and gameplay.
