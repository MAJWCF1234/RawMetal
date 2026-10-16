# Squad support and declarative guard execution — October 12, 2026

Moving wounded squad members can now withdraw to their reserved Medic,
hold within treatment reach and resume their original combat role after
healing. Guards also execute explicit task lists through a reusable bounded
kernel, with failure destinations and sensory interests declared in data.

## Runtime behavior

- `ai/Scheduler.h` maintains variable task lists through New, Running,
  Complete and Failed states. It reports failure IDs and bounds immediate
  transitions; it does not own an NPC's resources, senses or physical clock.
- Eleven guard rows in `guard/ScheduleTables.h` declare task lists, arguments,
  failure IDs, permitted work, interrupt conditions and sound masks.
  Ordinary rows prepare, move and perform selected work; Danger owns Escape;
  Recovery owns a 700 ms Wait and declares Patrol as its own fallback.
- `guard/Plans.cpp` returns a typed choice through mode and role dispatch.
  `Schedule.cpp` binds that choice to the task-list kernel. Mechanical combat,
  grenades, Medic work and Engineer hardware consume only their running task.
- Conditions include real audible categories, remembered-enemy occlusion,
  personally witnessed death, physical ranged/melee readiness and fresh
  light/heavy damage. Hidden death or unloaded targets do not reveal a corpse.
  Patrol/investigation hear ordinary sources; Medic, Engineer and Patient jobs
  retain only danger sounds as interrupt interests. Exposed grenades still
  preempt support independently of sound filtering.
- Accepted interrupts cancel uncommitted treatment, assembly, repair, throw,
  aim and pre-impact Bash progress, even when the same job is chosen again.
  Consumed attack events, reload phases/masks and finite spent stock survive.
- `guard/Casualties.cpp` reserves a specific live Medic through reciprocal
  subject ownership. A moving Assault/Rusher may choose a fixed direct rear
  rendezvous only when hull/floor, reservation and remembered-threat checks
  pass. Stationary/protected recipients hold their existing position. The
  provider holds its rear position while an active recipient withdraws, then
  performs the existing visible kit work. No role or authored flank identity
  changes; a recipient's temporary flank participation resumes after healing.
- Fresh incoming recipient damage cancels treatment before its finite event,
  including when the Medic is authored first. Healing cannot erase an unsensed
  damage cue by reducing the wound before the recipient's own sensory pass.
- `guard/Squads.cpp` recognizes local reloads, defensive moves, paired treatment,
  patient withdrawal and deployment/repair work as covering opportunities.
  Heavy and authored suppression anchors keep their existing personal memory
  limits and physical barrel/body checks. Sharpshooter coverage requires actual
  visibility. Covering gets at most one free fire lease and steals no live lease.
- Saves persist cursor, cursor marker, failure ID, compatible phase, subject,
  goal and resource baseline. Old saves without table metadata adopt their
  exact existing work phase. Partial/contradictory cursors, wrong listed
  operations and invalid terminal phases are rejected before action execution.

## Verification

Final build, suite counts and native capture evidence are recorded here after
the current centralized verification completes.

Reproduce the new suites with:

```powershell
.\RawMetal.exe --guard-patient-test --smoke-test
.\RawMetal.exe --guard-cover-test --smoke-test
.\RawMetal.exe --guard-table-test --smoke-test
.\RawMetal.exe --squad-support-inspection --smoke-test
```

The support inspection clones the actual Ash cargo map and preserves its
six authored members, starting poses, geometry and entry trigger. A durable
scripted hostile keeps the encounter active. The Rusher begins real confirmed
forward movement, receives an actual half-health hit, waits through fresh
defense, withdraws, holds for kit work and resumes after one finite heal.
Camera placement happens only after each gameplay milestone.

## Parity boundary

This closes guard task-table and broader-sensory slices. Overall GoldSrc
equality remains unproven: retained node/hint navigation and graph cover,
creature/civilian/escort participation in the common executor, scripted
handoff and matched Half-Life encounters remain. The current capability
scorecard is `docs/goldsrc-ai-gap-analysis.md`.
