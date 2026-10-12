# Guard grenade danger interruption — October 11, 2026

Guards now interrupt ordinary tasks for a locally perceived grenade warning,
escape through normal physical movement, and resume their work after danger
clears. This closes the stationary/enemy-grenade blind spot in the earlier
allied landing avoidance. It applies to every authored guard, including all
six Cleanup squad roles; maps need no new configuration.

## Behavior

- A slow physical grenade, or a grenade in its final fuse second, produces a
  local warning within 6 m when the guard has a clear blast ray to it. Faction
  does not exempt a projectile. Blast exposure within the conservative 4.5 m
  danger radius starts the interruption; solidly protected guards retain cover.
- Perception records the actual projectile position. Brief occlusion retains
  that last position for 600 ms. The response does not use hidden enemy movement
  or an enemy thrower's predicted landing. A nearby clear warning keeps an
  already safe guard from walking back into the grenade before detonation.
- Escape samples sixteen local destinations, checks physical support and hull
  clearance, and rejects intermediate paths that increase combined exposure to
  nearby perceived grenades. Each search consumes one opportunity from the
  shared four-search frame budget and retries after 350 ms. Named squad members
  reserve separate destinations. A trapped guard holds a valid position.
- Ordinary doors remain operated by normal movement; planning can consider an
  unlocked door's open state. Locked and transfer doors retain their rules.
  Escape uses a 1.35 movement-speed multiplier and turns toward movement.
- Danger releases firing leases and cancels unspent fire, bash, grenade throw,
  Medic treatment, Engineer assembly and repair. Damage, ammunition and stock
  already spent remain spent. A pending reload pauses with its consumed events
  intact, then completes once after danger clears.
- Fresh target perception still runs. Patrol progress and ordinary task clocks
  pause during the interruption. Living guards remain resident while escaping,
  including a home Patrol without enemy memory. Disabling a guard clears its
  escape state. Bounds-checked danger memory and destinations use existing save
  storage; the save format remains unchanged.

Valve's grunt prioritizes danger before ordinary combat and selects its
sound-cover schedule; changing a squad schedule releases the attack slot.
Its grenade also publishes near-rest and final-fuse warnings. These primary
references guided the behavior, with an independent RawMetal implementation:
[grunt danger priority](https://github.com/ValveSoftware/halflife/blob/master/dlls/hgrunt.cpp#L1877-L1900),
[sound-cover schedule](https://github.com/ValveSoftware/halflife/blob/master/dlls/hgrunt.cpp#L1468-L1486),
[squad slot release](https://github.com/ValveSoftware/halflife/blob/master/dlls/squadmonster.cpp#L95-L112),
[grenade warnings](https://github.com/ValveSoftware/halflife/blob/master/dlls/ggrenade.cpp#L232-L309).

## Validation

The Release build uses MSVC 19.44 in `.build/squad-release`. All 222 embedded
resources match their sources losslessly. `RawMetal.exe` is 27,082,752 bytes,
below the 28,000,000-byte ceiling. No character, texture or animation asset was
added for this slice.

The eleven focused native guard/squad/projectile/drone suites contain **610
passing checks and zero failures**, including 76 new danger checks. They cover
60/120 Hz physical escapes, stationary and allied/hostile warnings, two grenade
exposure, walls, trapped attackers, same-frame Medic/Engineer interruptions,
paused reloads, interrupted saves, malformed state, ordinary/locked doors,
residency, disabling, squad reservations and nine-guard search fairness.

All 23 distinct supported native invocations passed: the eleven focused suites,
full smoke, friendly AI, physics/AI, hearing, saves, streaming, world isolation,
campaign maps, mechanisms, Vulkan, the existing squad encounter inspection and
the new danger inspection. The merged exit-code record is
`diagnostics/guard-danger-20261011/validation.json`.

The older blast-attribution test now physically traps its neutral survivor
behind low hull barriers while preserving the blast ray. A free guard correctly
escapes the warning and receives no blast damage; the trapped fixture continues
to verify identification and retaliation against the real NPC thrower.

The Vulkan inspection loads the actual stitched Cleanup campaign and enters
its authored activation trigger. A real player-attributed grenade is placed
beside its stationary Engineer; normal updates handle the response. Three
captures show the warning, escape at 0.6 seconds, and recovery at 4 seconds.
Runtime assertions confirm movement, survival and task recovery without setting
health, task clocks, charges or awareness for the camera. Engineer health stays
90; displacement reaches 3.19727 m, danger clears, and ordinary movement resumes.
The camera is positioned only after each gameplay milestone.

Inspect `diagnostics/guard-danger-20261011/danger-observation.png` and the three
`*-runtime.txt` records. Captures were visually reviewed for the physical
walking pose, visible grenade and surviving squad. Cameras follow encounter
focus, so no pixel-difference quality score is inferred. Reproduce with
`RawMetal.exe --guard-danger-inspection --smoke-test`, then
`py -3 tools/CompareGuardDangerRenders.py diagnostics/guard-danger-20261011`.

## Scope

Full GoldSrc parity remains unproven. This is bounded local grenade danger
interruption, not a complete equivalent of Valve's sound registry or schedule
system. Escape searches remain within the guard's current chunk; existing
stitched pursuit and ordinary recovery still handle travel after danger.
Matched Half-Life encounters, broader scripted interactions and long-session
behavior remain outside this slice's evidence.
