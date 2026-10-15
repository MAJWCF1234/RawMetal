# Exclusion encounter authoring slice

The existing three chunks and six Cleanup roles remain the encounter. This pass
adds cover screens and industrial set dressing, makes every role patrol, and authors two local
distractions using existing terminals, saved timelines, spatial cues, and guard
investigation. Tracks 0–5 retain their original role IDs, 90 health, damage/death
state names, squad, firearms, and `exclusion_clock` identity. Track 6 remains the
70-health witness. No actor models, animation banks, or AI policies are added.

## Playable route

1. Start at (4.1, 6.2), facing the arrival cache at (4.1, 5.1). The first E press
   reads the brief and grants an owned flashlight from this physical cache.
   The arrival objective changes to the relay objective and writes a checkpoint.
   F can now toggle the acquired light. The screen at x5.7 shields the player
   and waiting witness; loose objects also permit manual distractions.
2. Operate the north scrap-rig console at (4.3, 7.8). Stand about (4.3, 7.0) to
   interact without standing inside its housing. `exclusion_north_lure` emits
   JunkMetal at (9.6, 10.2) after 1600 ms, then a second clatter at 3200 ms.
   Gain .60 gives an effective impact hearing radius of approximately 4.65 m.
   The Assault's patrol has a clear path to the source; the Sharpshooter starts
   beyond hearing range and has its own intervening screen. The facility shell
   isolates the support roles. Sound-only Search keeps a narrow yaw sweep so a
   rear approach stays viable after the distraction.
3. Crouch around the west screen's south end, through (5.35, 11.45), wait until
   the Assault is busy at the lure facing away, then approach from behind.
   A failed approach can become a normal fight. The authored lane permits
   either outcome; no actor is teleported or killed by a timeline. The north
   sequence grants `exclusion_north_lure_done` at 5 s.
4. Enter the facility through the full two-metre south doorway at x11–13. Use
   the field relay at (11.0, 13.5), requiring the first test, to set
   `exclusion_relay_secured`. This completes the relay objective, activates the
   east-winch objective, checkpoints progress, and authorizes the east console
   and witness's existing Escort behavior.
5. Operate the east winch at (17.3, 19.7), from approximately (17.3, 20.5).
   `exclusion_east_lure` emits JunkMetal at (21.0, 17.0) after 1600 ms, then a second
   ratchet at 3200 ms. Gain .64 gives an effective impact hearing radius of 4.8 m. The Rusher's rectangular
   patrol can investigate the source through the clear east bay. The player
   follows the staging screen's west end via x16.5; there is 1.1 m between the
   screen and the facility's east wall. This approaches the east-facing
   investigator from the west. `exclusion_east_lure_done` is set at 5 s,
   completing the winch objective and activating departure. Returning to the
   protected staging area writes a checkpoint for the extraction leg.
6. Take the existing south connection into South Wash and use its departure
   relay at (12.3, 4.7), which requires east-winch clearance. Registering departure
   completes the extraction objective and campaign, then writes a final won-state
   checkpoint. Escorting the witness is optional. Scrap Basin stays
   a connected salvage/combat option with cover and a route log. The four
   original creatures remain distributed across Basin and Wash, avoiding an
   automatic creature/cleanup fight during the first stealth beat.

The duplicate grid facility shell was removed: its narrower, offset opening
conflicted with the authored thin shell. The STRUCT shell and its original
south doorway now define that room. Cover hints were moved off solid walls.
The Heavy's spawn and the interior fixture were moved clear of one another.

## Preview and validation points

- Arrival: (4.1, 6.2), looking at (4.1, 5.1). The first control must be usable
  immediately; the west screen should protect the cache interaction.
- North beat: (5.35, 11.45), looking at (9.6, 10.2), after the north terminal cue.
  The patrol must turn toward the rig rather than tracking the hidden player.
- Facility: (12.0, 19.5), looking at (12.0, 14.0), through the south doorway.
- East beat: (16.5, 17.8), looking at (21.0, 17.0), after the east terminal cue.
  Check the complete standing/crouched west flank around the staging screen.

Parser/record checks alone do not prove either beat. Native validation must
check standing spawn/patrol clearance, patrol progress, hearing isolation,
actual Investigate/Search transitions, rear interaction, and save continuation.

## Existing jobs for the later combat consequence slice

| Role / track | Existing encounter job |
| --- | --- |
| Assault / 1 | First isolated north patrol; if alerted, closes on the player from the yard. |
| Sharpshooter / 2 | Holds the separate north/east firing lane behind its divider; pressures an exposed facility approach. |
| Rusher / 5 | Second isolated east patrol; if alerted, drives the southwest flank rather than holding the shooting lane. |
| Heavy / 3 | Interior anchor near the south entrance; uses existing suppression during a loud relay breach. |
| Engineer / 0 | Interior support route; uses existing grenades, drone deployment, and repair while other members hold the fight. |
| Medic / 4 | Separate interior patrol; existing casualty treatment makes an injured anchor a priority instead of another shooter. |

## Bounded custom authoring records

```
OBJECTIVE_TEXT|map|name|display label
SCRIPT_EVENT|map|name|x1|y1|x2|y2|bottom|top|gate_state|gate_value|clear_enemies
SCRIPT_ACTION|map|event_name|STATE|state_name|value
SCRIPT_ACTION|map|event_name|OBJECTIVE|objective_name|status
SCRIPT_ACTION|map|event_name|ITEM|item_name|count
SCRIPT_ACTION|map|event_name|CHECKPOINT
SCRIPT_ACTION|map|event_name|COMPLETE
```

Events are always once-only and must be declared before their actions. Names
are 1–96 ASCII letters/digits or `_`, `-`, `.`, `/`; event and objective IDs are
unique across the campaign. Objective labels contain 1–80 printable ASCII
characters and every OBJECTIVE action must have a label. Objective status is
0 hidden, 1 active, 2 complete, or 3 failed. Item grants accept counts 1–99;
state/gate values accept -100000 through 100000. A blank gate requires value 1.
Event XY regions stay inside their 24 m chunk and every axis has positive
extent. Each map accepts at most 128 events/labels; each event accepts at most
16 actions. Unknown actions, missing references, malformed counts, empty
events, duplicate IDs, and nonfinite or inverted regions are rejected.

These records map directly onto the shared ScriptEvent/ScriptAction executor.
Authored objective text supplies the HUD; existing named-state gates control
the terminals. COMPLETE deliberately precedes the final CHECKPOINT, so its
once-only event cannot save an unfinished campaign. Distraction terminals can
be operated again to restart their saved pulse. No NPC-kill or witness-presence
condition is added; the six-role combat route and witness escort remain choices.

`--exclusion-authoring-test` checks malformed parser records, real E-operated
cache and terminal gates, saved objective/item progression, protected staging
checkpoints, and final completion after checkpoint reload. It uses temporary
saves and the same three-chunk exported map, without adding a gameplay test area.


## Preview .15 readability follow-up

The same cover footprints now distinguish low steel from high concrete screens;
yellow hazard caps identify the silhouettes. Numbered signs and local task lamps
mark the cache, rig, field relay, winch, and exit. Existing cargo, scrap and a
small winch housing supply industrial context without another animation bank.
Signs use local terrain elevation once, like actors and navigation hints.

The east Rusher now patrols the winch bay. A north-side screen breaks the
Sharpshooter sightline across the approach; the west end stays open. The winch
emits metal impacts at (21,17) at 1.6 and 3.2 seconds, matching the north beat's
real hearing stimulus. The walking test starts at the concealed console, uses E,
walks around the west end while crouched, and requires Q to incapacitate the
investigating Rusher with player health still 100. It never moves the NPC or
sets its awareness, target or schedule.

## Existing campaign route / Receiving 02

Main campaign map 14 now has an inspection hold occupied by the same six Cleanup
roles. The Medic and Engineer carry pistols; the Engineer's existing drone
support and the Medic's existing treatment work remain available during combat.
Six short patrols cover the personnel aisle, truck lane and support position.
The old lone Huntsman is removed from this hold to prevent an unseen faction
fight before the player arrives. Campaign chunk count and progression gates stay
unchanged.

A field-cache terminal at (7,21.2) grants the owned flashlight once. A chain-test
terminal at (7,19.5) emits a real impact near the Assault at (8.5,14.5). The
inspection screen offers concealment and a west flank; the through aisle remains
open. Reach it through normal campaign travel or use console `MAP 14`.
