## Summary

- Ashfall Guard Lab (`RawMetal // Ashfall Guard Lab`) is embedded in the EXE so playtesters get it from a one-file download under **Custom Maps**.
- Squad/escort AI no longer freezes under the shared four-search nav budget: flank/cover/portal backoff, deferred Move stall charging, Deploy/Repair timeouts, and committed reload resume after Danger.
- Direct Mixamo squad bake and glue-gun/tank aim from the previous bake pass remain in this build.

## Playtester

1. Download `RawMetal.exe` from this release.
2. Run it, choose **Custom Maps → RawMetal // Ashfall Guard Lab**.
3. No sidecar `custom maps/` folder is required.

## Validation

- Guard Lab loads from packed resource 346 when no disk copy is present
- AI smokes: friendly, danger, schedule, tactics, roles, patient, policy
- Release build under OoT 32 MiB cart ceiling
