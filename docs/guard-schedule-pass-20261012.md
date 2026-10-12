# Shared guard task execution — October 12, 2026

The Cleanup squad now runs through one shared schedule executor. Perception
still updates every frame, but movement, treatment, terminal work and attacks
must own the appropriate task before advancing an event or spending resources.
This preserves the consolidated `src/game/ai/` layout from `530ffa2` and the
Engineer hardware integration from `9811500`.

## Runtime behavior

- `guard/Schedule.h` defines schedule identities, interruption masks and
  allowed work. `guard/Schedule.cpp` executes prepare, move and work phases,
  with at most four immediate phase transitions per actor per frame.
- Each guard has one running work owner: Fire, Bash, Reload, Throw, Treat,
  Deploy, Repair or Hold. Movement and failed tasks cannot execute work.
  Shooting leases are awarded only to eligible running Fire tasks, so
  support or recovering actors cannot consume their teammates' firing slots.
- Local grenade danger owns Escape immediately, before ordinary movement or
  work. Fresh threats and defensive conditions interrupt incompatible support
  work. A Medic cannot override a retreat or heal while its reload is pending.
  Engineer deployment and repair are selected before the hardware tick and
  repair remains bound to one specific owned drone.
- At contact range an owned Bash holds position and pauses pending reload
  time and event masks before refill. A consumed attack can finish its animation
  and recovery after ownership changes; it cannot repeat its damage event.
- Physical movement reports progress toward its actual route waypoint before
  chunk migration. A denied navigation search charges no stalled time. Failed
  approaches recover after 2.5 seconds without progress; patrol retains its
  established five-second deadline. Failure releases jobs and reservations,
  holds for 700 ms and retries through existing navigation. Failed support
  movement cannot skip an unrelated authored patrol point.
- Investigation and search deadlines continue during recovery. A blocked
  retry cannot refresh unseen coordinates or preserve hostile knowledge forever.
- An unreleased Throw fails after 750 ms, including a completed windup blocked
  by the global projectile pool. Grenade retries cannot replace an already
  raised or aiming Fire task. The guard can complete a safe actual shot before
  reconsidering another throw, rather than repeatedly resetting its aim.
- Saves retain schedule, phase, status, clocks, global goal, threat reference,
  role subject, failure reason and resource baseline. Missing scheduler keys
  adopt older mechanical phases. Validation counts the complete authored NPC
  collection, including subjects appearing after their owner. Death or
  disabling releases pending jobs while preserving finite spent stock.

## Verification

Release executable: **27,215,872 bytes**, below **28,000,000**. Build packaging
verified all **222 resources** losslessly: exact texture pixels/dimensions and
byte-identical models, audio and animation.

**716 focused checks pass**, including 68 new scheduler checks. These cover
real encounters at 60/120 Hz, conflicting work ownership, danger and incoming
damage, blocked patient routes, all nine budget-limited blocked guards, saved
and legacy treatment, consumed Fire/Bash events, paused reloads, lifecycle
cleanup and malformed state. Actual unsafe grenade landing fixtures with
800/1500 ms aim reach their first firearm shot at frames 191/233; the full
eight-slot pool fixture reaches a shot at frame 233 while all slots remain
occupied, with no grenade stock consumed.

The first rendered ownership assertion caught an Engineer busy flag surviving
completed deployment. Completion now releases that flag and the maintenance
reservation. One initial smoke run returned audio-test exit 13 without a
failed audio diagnostic; the dedicated audio test and subsequent full smoke
run passed. The cause of that isolated audio result was not established, and
this pass changes no audio code.

The guard, combat, tactics, policy, defense, pursuit, actions, roles, danger,
schedule, tactical-grenade and Engineer-drone suites pass. Broader validation
covers friendly AI, physics/creature AI, hearing, saves, streaming, world
isolation, campaign maps, mechanisms, Vulkan and smoke execution. Actual Vulkan
captures cover all fourteen Ash Cleanup milestones, the four Engineer support
milestones and three danger milestones.

Reproduce the new suite with:

```powershell
.\RawMetal.exe --guard-schedule-test --smoke-test
```

Evidence is local under `diagnostics/guard-schedule-20261012/`, plus native
inspection captures and logs under `diagnostics/kill-squad-20261011/`,
`diagnostics/engineer-support-20261012/` and
`diagnostics/guard-danger-20261011/`. Ash task ownership CSV rows include the
schedule, phase, task, status, subject, reason, failures and bounded transitions.
The rendered observation sheet shows actual firing, pursuit, kit opening and
treatment; screenshots are visual evidence, not a pixel-score parity claim.

## GoldSrc comparison boundary

Valve's [schedule executor](https://github.com/ValveSoftware/halflife/blob/master/dlls/schedule.cpp)
uses task status, schedule interruption conditions and failure schedules.
The [human grunt schedules](https://github.com/ValveSoftware/halflife/blob/master/dlls/hgrunt.cpp)
combine those with combat and squad behavior. This independently implemented
RawMetal slice addresses the corresponding shared execution and recovery gap.

Full GoldSrc parity remains unproven. This pass does not establish matched
Half-Life encounter outcomes, the complete scripted/friendly NPC behavior
catalogue, or an equivalent baked animation/event library. It validates the
shared execution machinery and the existing Depthworks squad roles through
actual finite gameplay events, saved continuation and rendered encounters.
