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
- **Two falls (decided 2026-10-23: keep both).** The prologue's service lift
  is the first failure. Map 3's surface lift is a second escape attempt that
  fails the same way: jam, cable snap, brake catch, crash into the reactor
  level. Its mechanics are unchanged. Its terminals now carry the echo:
  - `LIFT SYSTEM / OVERRIDE`: "LAST CAB TO THE TOP. BOARD AND DISPATCH."
  - `CAB CONTROL / DISPATCH`: "WARNING: SAME BRAKE MODEL AS SERVICE LIFT."

  The Map 1 night-shift log, "SURFACE LIFT IS ON EMERGENCY POWER", stays as
  the setup.
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

## Set dressing (rebuilt 2026-10-23)

The first pass used generic stand-ins and the user rejected it. The office is
now dressed from the purchased OFFICE pack and PSX Textures (see
`src/assets/office/OFFICE-SOURCE.md`):

- A CCTV bank of four CRT sets on wide filing cabinets.
- A wooden security desk with a CRT, keyboard, phone, mug and binder, a PC
  tower underneath, and an L-shaped corner desk with a second CRT.
- An office chair, tall file cabinets, a water cooler, a coffee machine, and a
  bookcase with binders.
- Painted block walls, a drop ceiling and green office carpet. The corridor is
  office tile up to the industrial service lobby.

The workstation terminal sits on the desk's front edge and draws no console of
its own: the desk CRT is the computer.

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

- Done since the first pass:
  - real alarm audio (ROT `warning_signal_smx_1` klaxon over the Echoes
    `alarm_distant_1` bed);
  - the red beacon wash on each blast;
  - a lit green terminal on the desk CRT and CCTV corridor feeds.
- Raised floors render as walkway unless dressed. The office lays carpet and
  tile `STRUCT` slabs over the deck.
- Captions use the small subtitle strip. A dedicated chat window on the CRT
  would read better for longer exchanges.
- The friend's name and the location of her workstation in the 136-map
  manifest are still open.
