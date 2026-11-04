# Expanded issue hunt — 2026-11-02

The issue hunter now runs core checks, 26 additional focused checks, optional
GPU checks, and selected campaign-map audits in separate processes. It checks
that reports are present and complete, captures the reported failure details,
records the executable hash, warns about newer source files, and checks that
the executable did not change during a run. Repeated collision tests advance
a recorded seed so failures can be reproduced.

The native gameplay regression compares full-scan and indexed structure hull
and support results for 12,000 generated cases per seed. Ten seeds, starting
at `0x1234`, passed 120,000 comparisons. The map auditor now ignores temporary
creature bodies during static route analysis, uses the actual scaled creature
hull, ignores non-solid trim, and considers the full footprint when finding
support on narrow rails. The Exclusion Zone spawn check likewise ignores the
creature's own body and uses its actual hull and initial settling height.

The zone quest test was corrected to use the elder's current conversation,
including E and the actual dialogue replies. Its formerly failing quest
chain now passes. Tests that require the installed Exclusion Zone map receive
an isolated copy, rather than accidentally testing a different embedded map.

## Results and leads

- All 51 core groups passed. Of 26 additional focused checks, 25 passed;
  `deep-exclusion` reported nine creature spawns without full standing
  clearance. The output records creature kind, authored and checked height,
  radius, hull height, blocked sample count and structure overlap.
- The spawn positions are map 2 `(12.5, 3.2)`; map 4 `(8, 13)`, `(14, 18)`,
  `(13, 12.5)` and `(17, 9.5)`; map 5 `(18, 0)`; map 10 `(23, 19)`;
  map 18 `(18.5, 15.5)`; and map 19 `(10, 12.5)`. Coordinates are local to
  the installed custom campaign chunks. These are collision-query findings;
  inspect their actual AI settling and movement before deciding how to fix them.
- The audit passed 71 of 75 authored campaign/prologue maps and flagged four:
  **28, Depot / Inspection Pits** (possible trapped region); **37, Switch
  Hall** (possible trapped region and two pipes through a walkway); **42,
  Machine Hall / Exciter Bay** (two pipes through a walkway); and **64,
  Research Receiving** (10 possibly trapped cells near `8.38, 13.62`).
  The audit uses a discrete approximate step/jump model and assumes open
  doors. These are leads, not confirmed softlocks or visual defects. In
  particular, the separate real-movement Chimera gate test still passes.
- All 11 runner unit checks passed, including stale-build and report validation.
- Four GPU checks passed: stable lighting, Vulkan materials and shadows, actor
  rendering and visibility culling. The stable-lighting check also passed on
  the final build.

## Evidence and usage

Use `python tools/hunt_issues.py --list` to see all checks. For example:

```
python tools/hunt_issues.py --tests gameplay-fixes --repeat 10 --seed 0x1234
python tools/hunt_issues.py --no-core --deep
python tools/hunt_issues.py --no-core --maps 0-73,179
python tools/hunt_issues.py --no-core --gpu --timeout 600
```

The core, initial deep, GPU and ten-seed runs used executable SHA-256
`46a95ac6cb0441400d59cad0398013cc451f347aef9875749d9e4af7ef8a3ff1`:

- `.build/issue-hunt/run-xsyjxkeb/results.json`: 51 core groups.
- `.build/issue-hunt/run-8ihpsqj1/results.json`: the 24 passing deep checks.
- `.build/issue-hunt/run-bt82iodv/results.json`: the zone spawn failures and
  complete test report.
- `.build/issue-hunt/run-3xs5rj22/results.json`: ten collision seeds.
- `.build/issue-hunt/run-tx6t1cdx/results.json`: the four GPU checks.

The final build adds the standalone terrain-query report and scaled-creature
map-audit corrections. Its SHA-256 is
`2cce9f0ac9aac1aae36f0c00f0a316354603fe53f6b9751c780ca21441d821de`.
Validation on that build:

- `.build/issue-hunt/run-6e8b85kf/results.json`: 75 campaign map audits.
- `.build/issue-hunt/run-50pa2kre/results.json`: CPU-only indexed-versus-full
  collision query checks.
- `.build/issue-hunt/run-ae9gzo7r/results.json`: three collision seeds.
- `.build/issue-hunt/run-nqq_ive4/results.json`: GPU lighting stability.

The Python runner, documentation and reports are outside the executable and
are not included by the release packager. The game already contains native
test code, and the new native collision comparison is compiled into it too;
a separate diagnostics build is needed to exclude all native tests from a
shipped executable.

The automated results do not constitute a manual full-campaign playthrough or
headset VR session.
