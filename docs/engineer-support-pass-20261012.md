# Engineer terminal and five-drone support — October 12, 2026

The Engineer now uses a visible field terminal and pistol, can deploy a fleet
of five physical drones, and falls back to support. This pass preserves the
AI consolidation in `530ffa2`: hardware and supervision stay in
`src/game/ai/engineer/`, shared movement stays in `friendly/`, role coordination
stays in `guard/`, and encounter inspection stays in `debug/`.

## Resulting behavior

- Five finite deployments per Engineer, with at most five active owned drones
  and twenty active bodies across the world. Destroyed units do not replenish
  stock. Multiple drones belonging to one owner now survive save/load.
- The first deployment requires personally confirmed sight of a threat. A
  living fleet can justify subsequent assembly through confirmed threat memory;
  it does not supply concealed enemy coordinates. Assembly takes 1.2 seconds,
  with 2.2 seconds between deployments.
- After deploying, the Engineer chooses a physical rear position, preferring
  cover. Stand-off ranges from 6 to 8 m; the authored Cleanup Engineer's pistol
  configuration selects 7.2 m. The role does not pursue a hidden enemy to
  supervise drones. Drones keep clear firing positions or return in a separated
  formation and still use first-hit checks before spending cannon ammunition.
- A close threat interrupts terminal work and requests retreat. Work cannot
  restart in the same threatened frame. Shared defensive cover and protected
  reloads take priority over ordinary Engineer supervision; grenade danger
  remains the higher-priority interruption.
- The Engineer physically approaches a safely placed wounded unit and repairs
  one selected recipient at a time. One health point costs one charge from the
  shared eighty-charge reservoir. Fleet size cannot multiply the repair clock
  or throughput. Death, disabling and drone destruction clear obsolete work.
- Engineer defaults to the existing pistol mesh and twelve-round NPC magazine.
  During assembly and maintenance the pistol is holstered and both calibrated
  hands manipulate a rugged terminal. Its five channel indicators reflect
  actual owned units. During pistol use the terminal remains stowed on the hip.
  No new texture, model, animation download or lossy asset conversion is used.

The actual campaign JSON, installer payload and refresh generator agree on
the Engineer's pistol configuration. The other Cleanup roles retain their
existing authoring.

## Review fixes

Reviewing the draft against the consolidated AI exposed concrete defects:
the retreat stall threshold was beyond the movement timer's reset point;
high drone slots could produce route timers rejected by saves; and Engineer
supervision could erase a protected reload. These are fixed. Cached rear paths
are revalidated, NPC movement now respects drone bodies, and support candidates
reject paths through deployed drones. A regression exercises the twentieth
slot's actual stitched route and save, not just its nominal capacity.

## Runtime evidence

The native Vulkan inspection retains the real cargo map, six supplied squad
models and ordinary activation trigger. It adds a durable authored Facility
opponent so fleet growth can run without resetting health. Normal game updates
reach assembly, five deployed units, maintenance and pistol fire. Maintenance
begins from an actual eighteen-point drone damage event.

At frame 931 the Engineer has five living units, has spent five deployments,
and has moved 2.31948 m from its original position. At frame 960 the selected
damaged drone has regained one health point and repair stock has fallen from
80 to 79. At frame 993 the Engineer produces a real pistol shot. Engineer
health stays 90. The remote observer receives ordinary damage and survives;
neither observer nor target health is reset.

Camera placement happens after simulation milestones. The fleet view uses a
raised observer and requires unobstructed body rays and projected frame bounds
for all five units. Other views show the handheld terminal, repair interaction
and stowed terminal during firing. Captures are in
`diagnostics/engineer-support-20261012/`; `engineer-observation.png` assembles
the four views. Reproduce with `RawMetal.exe --engineer-support-inspection
--smoke-test`, then use `tools/CompareGuardDangerRenders.py` with the four
capture names and `--columns 2 --output engineer-observation.png`.

All 24 supported native regression invocations passed. The affected eight
invocations were rerun after adding close-threat preemption; the final camera
check was then rerun independently. The eleven focused suites contain 648
passing checks, including 114 Engineer checks. Editor runtime-record tests and
map-payload export also pass. Build packaging verifies all 222 original embedded
resources losslessly; the executable is 27,134,976 bytes, below 28,000,000 bytes.
`diagnostics/engineer-support-20261012/validation.json` retains the full,
affected-suite and final-visual exit codes with the executable hash.

## GoldSrc boundary

This does not establish GoldSrc equivalence. Valve's base AI has explicit task
start/run/completion, schedule interruption masks and failure schedules;
RawMetal still combines awareness states with distributed behavior clocks and
role overrides. The priority fixes above address real conflicts, but a shared
task executor with consistent failure recovery remains a major architectural
gap. Compare [Valve's scheduler](https://github.com/ValveSoftware/halflife/blob/master/dlls/schedule.cpp)
and [grunt schedules](https://github.com/ValveSoftware/halflife/blob/master/dlls/hgrunt.cpp).
Matched Half-Life encounters and long-session play are also still missing from
the evidence. Passing fixtures and staged runtime captures cannot prove that
the squad plays as well as GoldSrc NPCs.
