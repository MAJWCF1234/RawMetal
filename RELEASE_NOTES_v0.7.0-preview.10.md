## Summary
- Enable-gated squad staging now publishes complete authored XYZ (and restores it) before `saved=1`, so activation no longer teleports from origin
- Kill-squad / danger / engineer inspections resolve cargo roles by authored role instead of hard-coded actor indices (Guard Lab map grew map-0 actors)
- Medic inspection leaves the bay after wounding so treatment can complete; pursuit observer uses hull-safe south-chunk refuges
- Cleanup mesh validation requires head+body material parts; mesh material-part ID ranges cover all six cleanup role resources
- Mixamo packs parked under `source-assets/mixamo-motion/` as local donors (not baked yet)

## Validation
- `RawMetal.exe --friendly-ai-test` — all PASS (including elevated staged XYZ roundtrip)
- `RawMetal.exe --kill-squad-inspection --kill-squad-stage=0..13` — all PASS
- Release build: 24,045,056 / 33,554,432 bytes (OoT 32 MiB)
