# Mixamo donor motion (local)

Dropped Mixamo rifle/pistol packs live here as **source donors**. FBX and zip
files are gitignored.

Bake into cleanup RMA2 clips with:

```
blender -b --python tools/BakeCleanupMixamo.py
```

That retargets Mixamo idle / walk / hit / crouch onto each cleanup role, keeps
UAL1 punch + death, then re-appends Medic treatment from UAL2. Evidence lands in
`diagnostics/cleanup-mixamo-bake.json`.
