---
tags: [design, narrative, prologue]
date: 2026-10-23
---

# Security Office prologue (test map)

Custom map: `custom maps/RawMetal-Security-Office-Prologue.txt`. It tests a
playable opening scene that leads into the current game start (campaign Map 0).
It is in-engine and stays playable throughout: the player keeps control,
Half-Life style, rather than watching a locked camera.

## Lore check

Against [[campaign-system-overview-20261010]] section 1.1:

- **Fits.** The inciting incident is an elevator failure that drops the guard
  into sub-levels beyond their clearance. The prologue plays out exactly that.
- **Fits.** The friend "was stationed at her terminal sending messages" when it
  happened. She talks to the player over the workstation chat. Her last line is
  cut off and the crash ends on "NO SIGNAL FROM LAB 4". That sets up the
  breadcrumb: retrace her steps to her workstation terminal.
- **Fits.** The Cleanup Force lockdown is to eliminate witnesses. The chat hints
  that sub-level doors are being sealed and nobody says why, before the alarm.
- **Tension in the doc itself.** "The Mission: Returning to the facility
  immediately in the wake of the disaster" reads as if the guard was away. The
  prologue puts the guard on shift instead, which matches the elevator-failure
  line. Pick one.
- **Duplicate fall.** The campaign already has a lift failure on Map 3: surface
  dispatch, jam, cable snap, brake catch, crash into the reactor level. A
  prologue fall before Map 0 makes two. Options:
  1. Keep both, and reword Map 3 as a second escape attempt that fails the
     same way, a deliberate echo.
  2. The prologue cab's brake catches (the existing Caught phase), so the
     prologue drop is shorter and the full crash stays on Map 3.
  3. Drop the Map 3 fall and make the prologue the only one.
- **Open:** the friend is unnamed in the docs. The prologue uses her terminal
  handle `LAB 4` as a placeholder. Where her workstation is in the 136-map
  manifest is also undecided. Chapter 2 labs (Maps 45-76) would be a long
  delayed payoff.

## Beat sheet

1. Night shift at Security Desk 2. The objective is to answer the message on
   your workstation. The service lift is called up, unseen, behind the lobby.
2. **E** opens the chat (about 20 s):
   - "you still on shift?"
   - "readings just went off the scale"
   - "they're sealing the sub-level doors"
   - "something's wrong. really wrong. hold on"
3. Facility alarm: a repeating high beep plus "CONTAINMENT LOCKDOWN". The
   friend says "RUN. the service lift past the lobby. GO". The objective
   changes to "Get to the service lift".
4. Run: office, doorway, corridor east, corridor south, lobby, cab. Signs and
   lamps lead the way.
5. Boarding seals the doors, then "LOAD UNSTABLE", then "CABLE FAILURE". The
   cab drops 9 m at 7 m/s.
6. Impact: screen shake and flash, the crash sound, then "EMERGENCY POWER /
   SUB-LEVEL / NO SIGNAL FROM LAB 4".
7. The game hands off to the main campaign's first map, exactly like New Game.

## Systems used

Everything is an existing custom-map record. Chat lines and the alarm are
`TIMED_SEQUENCE`s chained by finish states, with `SEQUENCE_CUE` captions. The
lift is a `CARGO_LIFT` with a flush `STAIR` threshold (the Freight pattern).
Objectives and events use the existing script records.

Two script actions were exposed to custom maps:

- `SHAKE|amount`: the engine's existing Shake action.
- `ENTER_CAMPAIGN`: starts the main campaign at Map 0 through the same path as
  the title screen's New Game. It applies at the start of the next update,
  because actions run while the event list is being iterated.

## Verification

- `--prologue-test`, 11 checks, using real input from desk to hand-off:
  - workstation usable from the spawn;
  - chat before the alarm;
  - alarm orders the run;
  - real movement through the corridor onto the cab;
  - 9 m drop with the passenger;
  - impact;
  - Map 0 at full health.
- `--prologue-inspection` renders the desk, chat, corridor during the alarm,
  lobby, cab after impact, and four views around the workstation.

## Known gaps for a production version

- No alarm klaxon sound. The test uses a repeated high-pitched status beep.
  ROT Horror Audio and Echoes have alarms that could be imported through the
  existing audio pipeline.
- No flashing alarm light state. Lights are static records.
- Raised floors always render as steel walkway, so the office floor is grating
  rather than carpet. `FloorMaterial` only applies to the base layer.
- Captions use the small subtitle strip. A dedicated chat window on the CRT
  would read better for longer exchanges.
