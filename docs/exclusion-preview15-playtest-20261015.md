# Preview .15 playtest and readability pass

Scope: deterministic desktop input replays and native Vulkan screenshots on the
existing Exclusion chunks and Receiving 02. These are developer walkthroughs,
not observations from recruited players. No new map, actor role, animation bank,
codec or size ceiling is part of this pass.

## Confusion found and changes made

| Moment | Observed problem | Change |
| --- | --- | --- |
| Locked terminal | INTERLOCK did not identify what the player should do next. | The hint says E / READ WHY LOCKED. The CRT says the required step is shown above. North names the cache, relay names the north test, east names relay authorization, and departure names east clearance. |
| Acquiring light | Ownership and F could be missed while approaching the low cache. | A raised FIELD CACHE / START placard repeats E pickup and F toggle; the brief and campaign cache repeat the same keys. The owned light retains its procedural housing. |
| Rear approach | Direction alone did not communicate waiting for the back and the final action. | Numbered cover signs and terminal text say CROUCH / WAIT FOR HIS BACK / Q. In range, the hint leads with Q / REAR KNOCKOUT, followed by E grab and the lethal option. |
| East winch | The original patrol could see the player before the distraction committed it. | Move its pauses to the actual winch bay, break the northern sightline, and emit two metal impacts. Keep the west flank physically open. |
| Moving on from a computer | E closes an open CRT before it can operate another control. | Walking replays now close the CRT through real E input before leaving. The existing E TO CLOSE TERMINAL line remains visible. |
| Cover reading | Repeated gray slabs concealed the role of each screen. | Low steel / high concrete materials, hazard caps, task lights, numbered signs and reused industrial props define the route. |

Native review also caught an oversized cache placard, a lamp overlapping its
copy, and labels longer than the sign texture could display. The final placard
is narrower, mounted behind the cache, and uses shorter copy. The lamp sits to
the side, and the waiting witness is clear of the cache sightline.

## Verification boundaries

The north replay walks around the west screen; the east replay uses the west
flank from its actual console. Both require a real rear Q incapacitation and
100 player health. The east replay also verifies that the guard's perception
memory names the winch source while investigating/searching. It does not set an
NPC pose, target, recognition, health, restraint or schedule. Relay clearance is
set only to isolate that already-tested stage. Native view 9 repeats this same
walking function and photographs its resulting body.

Receiving checks all six roles, their spawn and patrol hulls, support pistols,
physical cache interaction, owned flashlight persistence, chain-control E input,
and the existing campaign seam/progression tests. Its inspection hold replaces
the old lone creature so the encounter is not consumed by an unseen faction
fight. The personnel aisle stays available.

The Script exit-42 fixture now retains canonical fresh-world state, shuffles the
whole saved state payload, reloads it, and compares every ID/value with the full
expected store. No smoke test is skipped. The console test checks the tick-1
initialization produced by restart and requires progression only after closing
the console. This fixes test expectations while preserving save strictness.

Final command results and selected native frames are stored with this report's
[evidence folder](evidence/exclusion-preview15-20261015/). Human playtesting is still needed to measure whether newcomers notice
the cues without assistance; no participant results are implied here.


## Final build results

The release executable is **25,629,184 bytes (24.44 MiB)**, leaving
**7,925,248 bytes (7.56 MiB)** under the unchanged 32 MiB ceiling. The build
verified all 229 embedded resources against their sources. Loose and embedded
Exclusion copies are byte-identical.

Full `--smoke-test --vulkan` exits **0**. Running `--exclusion-test` from a folder
without a loose map also exits **0**, proving the shipped embedded encounter.
All 47 encounter checks pass, including three north approach timings and the
physical east KO. Authoring 38, stealth 36, campaign 431, squad roles 44, Medic
patient 60, and Engineer drones 134 pass: **790 focused checks** in total.

[Machine-readable results and executable hash](evidence/exclusion-preview15-20261015/validation.json).

Selected native captures: [cache](evidence/exclusion-preview15-20261015/entry.png),
[north investigation](evidence/exclusion-preview15-20261015/north-investigation.png),
[east staging](evidence/exclusion-preview15-20261015/east-winch.png),
[Receiving 02](evidence/exclusion-preview15-20261015/receiving-cleanup.png), and
[body after the physical east approach](evidence/exclusion-preview15-20261015/east-physical-knockout.png).
The final image retains the replay's player position and 100 health; it is not a
teleported rear-action fixture.
