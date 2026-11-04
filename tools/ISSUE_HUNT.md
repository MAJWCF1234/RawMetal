# Isolated issue hunt

Run `python tools/hunt_issues.py` from any directory to execute every test
registered in `--each-test` in a separate process. Each process has its own
working and temporary directories under `.build/issue-hunt/`, and a default
300-second timeout. The runner continues after failures and exits nonzero if
any check fails, crashes, times out, produces no report, or does not explicitly
report the selected test as passing. It records the executable SHA-256 and keeps
the complete per-test artifacts and a machine-readable `results.json`.

Use `--tests movement,gameplay-fixes --repeat 3` to look for intermittent
failures, `--timeout 600` for slower machines, or `--exe PATH` to test another
build. A fresh run directory prevents stale reports from being read as results.
The executable is hashed before and after the run; replacing it mid-run makes
the overall result fail. A partially written result is marked `complete: false`.
The runner also warns when C/C++ files are newer than the executable; file
timestamps alone cannot prove which sources a build contains. `--list` prints
all available core, deep and GPU checks.

Additional profiles:

- `--deep` adds 26 focused checks for guard decisions and tactics, creature and
  civilian schedules, drones, navigation, indexed-versus-reference collision,
  cached-versus-live AI queries, the
  prologue, exclusion zone, world isolation and VR math. The zone map needed by
  the navigation and exclusion checks is copied into their isolated directories.
- `--gpu` adds four hardware checks for stable lighting, Vulkan rendering,
  actors and visibility. These need working hardware rendering; the actor and
  visibility checks use the installed zone map when available.
- `--maps 0-73,179` audits the authored campaign maps and prologue. Maps
  74–178 are unfinished placeholders; select them explicitly if desired.
  Findings include reachability, suspected traps, geometry and placements.
  The audit assumes open doors and uses a simplified step/jump reachability
  model, so a finding is a lead to investigate, not proof of a gameplay bug.
- Add `--no-core` to run only selected deep, GPU and map profiles. Individual
  names such as `--tests deep-ai-query,deep-nav-graph` also work. For a single
  map use `--no-core --maps 64`.

Each map report is checked for the expected map number, a complete reach grid
and a valid summary. The map label is read from the current source tree, and
actionable audit findings are included in `results.json`.
Some checks may depend on files relative to the repository root; if an isolated
check fails for a missing input, inspect its artifacts and reproduce from the
normal game directory before treating that as a game failure.

The gameplay regression group additionally compares spatial-index hull and
support queries against an independent full scan for 12,000 deterministic
cases, including thin structures, non-solid trim and cell-boundary contacts.
Use `--tests gameplay-fixes --repeat 10 --seed 0x1234` to explore 120,000
reproducible cases: each repeat advances the seed, records it in `results.json`
and prints the seed and sample coordinates if the comparison fails.

Run `python -m unittest discover -s tools -p test_issue_hunt.py` to check the
runner's handling of skipped tests, incomplete reports, crashes, and timeouts.
These checks complement visual review, interactive play, and hardware tests;
they do not replace them. Build the game before running them if source changed.

The Python runner and its `.build/issue-hunt/` reports are outside the game
executable, and `tools/release/package.py` does not include them in release
archives. The existing native test harness and the new randomized collision
test are compiled into `RawMetal.exe`, however. Keeping all test code out of a
shipped binary would require a separate diagnostics build and release target.

## Z-fighting sweep

Run `python tools/detect_zfighting.py --exe RawMetal.exe --views PATH\\views.txt --out .build\\zfight-run`.
The view list uses the same `name map x y z yaw pitch` format as `--map-shots`.
The detector renders two tiny yaw offsets per view, reports changed-pixel
ratios, saves `*-diff.png` evidence, and marks high-instability views as
`SUSPECT`. This is a lead generator, not proof: animation, TAA, particles, and
moving lights can also change pixels, so suspected pairs need visual review.
