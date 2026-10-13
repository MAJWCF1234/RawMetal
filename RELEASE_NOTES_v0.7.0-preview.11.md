## Summary
- Cleanup NPC idle / walk / hit / crouch retargeted from Mixamo (rifle aiming idle, walking, hit reaction, pistol kneeling idle)
- Punch and death remain Quaternius UAL1; Medic treatment clip stays UAL2
- New bake: `tools/BakeCleanupMixamo.py` (anatomical limb retarget through Mixamo world-scale matrices)

## Validation
- Blender bake evidence: `diagnostics/cleanup-mixamo-bake.json`
- `RawMetal.exe --friendly-ai-test` + kill-squad stage 0
- Release build: 24,044,032 / 33,554,432 bytes (OoT 32 MiB)
