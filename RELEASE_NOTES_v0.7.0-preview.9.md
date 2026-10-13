## Summary
- Reuse creature pose scratch buffer in `poseCreature` (no per-frame allocation for every humanoid)
- Skip armed bind-pull / sleeve IK work when influence is negligible
- Enable-gated guards only clear schedule/danger state while that state is still dirty
- Idle patrol guards, escorts, civilians, and pre-enable squads track visible threats via `friendlyAwareness`
- Awareness considers enemies on the NPC's resident streamed chunk, not only the player's level

## Validation
- `RawMetal.exe --friendly-ai-test` — all checks PASS
- `RawMetal.exe --smoke-test` — PASS
- `RawMetal.exe --performance-test` — PASS
- Release build: 24,025,600 / 33,554,432 bytes (OoT 32 MiB)
