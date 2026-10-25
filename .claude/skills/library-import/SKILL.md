---
name: library-import
description: Import a model from the user's purchased asset packs (O:/retro official, PSX Nature, Environments, Radiation Workers...) into the engine as a fixture model. Use when a map or the Ashfall zone needs a new tree, plant, building, prop or vehicle.
---

# Importing purchased models

The user's packs are in `O:/retro official/` (Nature, Environments, Props, Models...) and `O:/retro official/PSX Nature v1.7.3b/`.
Use them freely. Never send them to outside AI services.

Imported files go to `src/assets/library/`. That folder is **gitignored**, because the repository is public.

## 1. Import (Blender 5.2, headless)

```bash
"/c/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --python tools/library/import_model.py -- \
  --src "O:/retro official/.../thing.fbx" --name thing --root "O:/retro official/.../pack" \
  [--pick NAME=REGEX ...] [--objects REGEX] [--exclude REGEX] [--max-tex 256] [--yaw DEG] [--tex-suffix _summer]
```

The importer reads FBX, OBJ and GLB.
- **Geometry:** it joins the meshes, centres the footprint and puts the base at 0.
- **Materials:** it prefixes material names with the model name.
- **Textures:**
  - Textures are found by name under `--root`.
  - Images packed inside GLB files are written out directly.
  - `--tex-suffix` handles seasonal packs (the retro nature pack: `bush1` → `bush1_summer.png`).
  - Anything still missing becomes a 4×4 colour swatch. **Check for 4×4 swatches after every import**; they render as grey sheets.
- **Several models from one scene:** pass `--pick NAME=REGEX` once per model.

## 2. Register

```bash
python tools/library/register.py NAME [NAME ...]
```

This step makes the model usable in the engine.
- It appends `assets.rc` entries.
- It adds a `Mesh{id}` slot in `SoftwareRenderer.h`.
- It raises `FacilityModelCount`.
- It writes `materials.txt` (resource 466) and `catalog.json` (slot, width = OBJ X, depth = OBJ Z, height = OBJ Y).

Names that are already registered are skipped. Re-importing a registered name in place is fine as long as the texture file names stay the same.

## 3. Check

- Build, then look at the model in game, for example with `--zone-shots` (see the `ashfall-zone` skill).
- Check its facing: some packs face −y at yaw 0.
- Check the size budget: `Build.cmd` prints the EXE size against the 40 MiB cap. Keep textures at 256 px or less unless the model is a hero piece.
- Cutout foliage (alpha textures) automatically gets a Painterly Horizon LOD impostor in the zone.
