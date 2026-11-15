# Chapter 2 inspection, 2026-11-14 (the user's inspectors)

Each item is from the inspectors' notes and screenshots. A map number is the campaign chunk; the zone title is what the HUD showed.

## Engine
- E1. Saves keep failing to load (several inspectors).
- E2. Add a `noclip` console command for quick bug checking.
- E3. Map 95 Containment Intake: an NPC that can be shot and flinches but never dies, never moves or shoots.
- E4. Map 109 Extraction Interface / The Head: the big rusted ring structure has no collision. With collision it would block the main path, so the path must be rerouted or the ring moved.

## Medical / Analysis / Energy (68-93)
- M1. Map 68, the cabinet left of the spawn (HUD "...CONCOURSE"): a wooden shelf cabinet with a door panel and an arched frame sticking out of it, and a beige box clipping through its shelves.
- M2. Map 73 Medical / Imaging, and its x-ray 2 room: thin metal bars floating above the beds.
- M3. Map 77 Medical / Surgery / Theatre Corridor: a floating med kit.
- M4. Map 80 Sample Storage / Freezers: a wall trim strip runs across the door opening.
- M5. Map 81: a cabinet is bugged (clipping or misplaced).
- M6. Map 84 Field Measurement / Calibration pan: lamps floating next to the railing.
- M7. Map 86 High-Energy Test Bay / Gallery: a floating fire extinguisher (off the floor, no bracket), and a big box obstructing the doorway path.
- M8. Map 88 Observation Ring / N Gallery: cabinets bugged.

## Containment / Deep Research (94-109)
- C1. Map 94: a door blockage, and a line of corpses laid head to foot along the wall, one floating above a bed (HUD "QUARANTINE WARD / HOLDING" in the screenshot). They read as placeholders, not a scene.
- C2. Map 99 Specimen Handling / Gallery: a railing across the walking path.
- C3. Map 102 Breach Sector: wall lamps stacked in one place.
- C4. Map 103 Emergency Containment: a PC and desk sitting on top of another desk or console (stacked).
- C5. Map 107 Energy Core Research: a terminal facing too close to the wall.
- C6. Map 108 Deep Access Control / Booth: a terminal (a beige box on a table) facing too close to the wall.
- C7. Map 108 Deep Access Control: a bench blocking a door, and the door is blocked from inside the room too.
- C8. Map 108 Deep Access Control: the ID card scanners and keypads are giant (about 0.5 m cubes) and float. The inspectors asked whether they should be that size; they should not. They should be real reader size, mounted on the wall.
- C9. Map 108 Ready Room: a notice board on the wall overlaps a door and blocks it.

## Done when
- Every item is fixed and shown in a before/after shot from the inspector's angle.
- The map audit gains rules that catch each class automatically, run over all 180 maps, with any other instances fixed too:
  - floating fixtures;
  - fixtures stacked or inside other fixtures;
  - anything in a door's opening or swing;
  - trim crossing doorways;
  - lamps stacked;
  - terminals facing a wall;
  - props not at a sensible scale.
- Saves load, and noclip works.
