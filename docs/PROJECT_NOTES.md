# Project notes

## October 10, 2026 — campaign direction and asset availability

The user supplied the [Campaign & System Overview](campaign-system-overview-20261010.md).
That document is preserved verbatim as a design reference. It describes intended
narrative, gameplay, pacing and future areas; statements about functional systems
in the supplied document are not a fresh implementation audit.

The protagonist is a Depthworks security guard searching for surviving coworkers
and a close friend, starting with her workstation terminal. The cleanup force
recognizes the protagonist as a colleague who is absent from its assigned roster.
Progression uses local media, access consoles and mechanical overrides. Industrial
exploration and quieter intervals should precede the research, mining and Dark
Below reveals; show human exploitation and containment before explaining the anomaly.

The new reference labels its master manifest as 136 maps and lists areas 14–136.
Keep that supplied numbering as planning information. Reconcile it with earlier
scope notes and the actual stitched runtime chunks before authoring or renumbering
content. Chapter 4 names are tonal placeholders. Medical Screening is the primary
medical hub, with preliminary aid outposts in Maintenance District.

Near-term design priorities are a flashlight viewmodel, clear floppy/terminal
interaction prompts, and guard distraction from crouched object throwing.
Unconscious takedowns, grappling/human shields and neck snaps are design targets.
Implement reusable behavior through engine systems and authored content bindings;
the design reference does not validate the previously quarantined human-system
experiment. See [current shared AI behavior](ai-perception.md) and
[the AI/GitHub recovery report](ai-streaming-pass-20261009.md).

**External asset library temporarily unavailable:** the user reported taking the
external drive away on October 10, 2026. Treat `O:\retro official` and its source
archives, including `forklift.zip`, as offline until the user reports their return.
Continue work with assets already present in the project and embedded runtime
resources. New imports requiring that drive must wait for its return. Retain the
existing source provenance in [ASSET_SOURCES.md](ASSET_SOURCES.md).

## October 10, 2026 — evolving custom Ash playground

The user clarified that the Ash testing area in **Custom Maps** is temporary
experimental content that should change with engine updates. Use the existing
`custom maps/RawMetal-Horror-OpenWorld.json` and matching TXT as the evolving
testbed. This is separate from the built-in Ashfall world and campaign maps.

The current [guard lab](ashfall-playground.md) tests engine-owned patrol,
recognition and distraction using the existing Worker visual rig. Its authored
runtime records survive editor save/load/export. These are shared authoring
records; there is no Ash-specific guard behavior or new custom human subsystem.

On October 11 the same playground gained opt-in armed combat: shared explicit-origin
ballistics, blocked shots, aim/recovery/reload timing, persistent ammunition and
two-hand weapon rendering. The external asset drive remains unnecessary for this
prototype. See [the combat verification report](guard-combat-pass-20261011.md).

The subsequent [cover-repositioning pass](guard-tactics-pass-20261011.md) repairs
armed guards stopping forever behind low cover. Shared shot geometry selects
bounded local detours; movement keeps target-facing and uses a lowered weapon.
The same custom Ash playground now includes low cover for runtime observation.
