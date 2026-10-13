# Mixamo donor motion (local)

Dropped Mixamo rifle/pistol packs live here as **source donors only**. FBX and zip
files are gitignored.

Cleanup/Worker runtime clips are still baked from Quaternius UAL via
`tools/BakeCleanupSkin.py` into RMA2 topology. Mixamo does **not** drop straight
onto those meshes — a retarget bake (same path as UAL) is required before any
clip can ship in the cart.
