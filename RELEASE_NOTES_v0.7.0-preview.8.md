## Summary
- Guard Lab / enable-gated squads keep cycling UAL idle instead of freezing on bind (sample 0)
- Armed draw softens bindPull so posed idle and legs stay readable (cleanup ×0.18, Worker idle ×0.55, walk/crouch ×0.4)
- Ashfall Guard Lab primary guard uses Cleanup Assault mesh for the stronger idle cycle
- Idle phase rate raised to 0.45 so staged and armed idles read as alive

## Validation
- `RawMetal.exe --friendly-ai-test` — all checks PASS, including disabled-squad idle advance and armed phase cycling
- Release build: 24,023,552 / 33,554,432 bytes (OoT 32 MiB)
