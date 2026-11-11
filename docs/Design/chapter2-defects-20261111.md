# Chapter 2 defect hunt, 2026-11-11

Judged by the lead from six spot views per map (`.build/judge/spots-A-B.png`, rendered after the second
Containment and Deep Research pass) and from the owner's own promo frames, which showed the faults below.
The bar: a Valve level designer would not ship it. Nothing here is a crash or a route break; every map
passes its audit, placement check and the campaign map test. These are faults a player sees.

## Systemic causes (fix these first, they explain most of what looks wrong)

1. **Black faces.** Chapter 2's light is nearly all overhead, its ambient is low (0.08-0.30, average 0.21
   against Chapter 1's 0.26) and the engine has no bounce light. So every face turned away from a lamp
   falls to black: stair sides (a STAIR is a stack of solid columns whose side faces take only ambient),
   cabinet and switchgear fronts, pillars, pedestals, the backs of counters and whole walls. Seen in 74,
   78, 83, 84, 85, 86, 92, 93, 95, 97, 103. Fix: an ambient floor of 0.22 in rooms people worked in;
   every room needs a source at wall height (wall lamp, lit screen, desk lamp, work light, emergency
   lamp, glow through a doorway) that lights the vertical faces the player sees; stairs as open steel
   stairs (openUnderside, with treads and stringers) or lit from the side; nothing large in a dark finish
   where the light cannot reach it.
2. **One wall finish for a whole district.** The grey moulded-panel wall covers every room of 68, 69, 70,
   80, 83 and the Containment office blocks (96); the Ring and Energy districts (86-91) are one sodium
   brown from floor to roof. Rooms do not read as different rooms. Fix: give each room type its finish
   (offices: painted plaster with a wainscot or skirting and a lit ceiling; labs: tile or two-tone paint
   with a coloured band; plant: concrete with painted lower walls and hazard edging; corridors: a
   different floor), and contrast warm against cold light inside each map instead of one colour.
3. **Hand-built resonance growth and crystal.** Growth is box and dome primitives under one tiled
   magenta texture, stretched until its pixels show (98) and pasted on walls as flat arch-shaped
   "fingers" (102); the crystals are tall faceted slabs under a tiled glitter texture. They read as
   cardboard, not as ore. Fix: crystals as clusters of narrow hexagonal prisms of varied height, half
   buried in rock and rubble, glass-like with a cold inner glow and a few bright tips, the cave pack's
   spike bed and the gem piles (O:/retro official/Props/Gems, to import) at their feet; growth as
   rock first, with the magenta only in seams, cracks and wet patches at a texture scale that never
   shows its pixels; no cut-out shapes flat on a wall.
4. **Suits hung by the head.** The hazmat suit rail (69, 86, 91) hangs each suit from a rod into its
   hood, feet nearly on the floor: it reads as a row of hanged men. Fix: suits hang from the shoulders on
   a hanger, high, with the feet clear, or lie folded on benches and in open lockers.
5. **Rusted lockers in clean rooms.** psx_locker is rust-streaked; it stands in the clean front offices
   (69) and in the Shielding Corridor post (91). Use it only where rust belongs.
6. **Empty rooms.** Many rooms hold one or two props: 68 (#1, #5), 69 (#1), 70 (#2), 72 (#1), 73 (#3),
   75, 76 (#1, #3), 77 (#2, #3, #5), 80 (#3), 81 (#1, #2), 85 (#1, #2), 87 (#3), 88 and 89 (most rooms),
   91, 97, 103 (#3, #4). Each needs what its last shift left, set where they used it.
7. **Texture scale.** A wall panel in 90 (#3) and the growth domes in 98 are stretched until their
   pixels show. Keep every face at the finish's own repeat.

## Per map (spot view numbers from the sheets)

- **68 Research Intake.** Salmon-pink texture on the door columns at the screening line (#0); black
  door-sized slab beside it; grey panel everywhere; empty rooms (#1, #5); ceiling black.
- **69 Personnel Processing.** Suits hung by the head (#2); black cage block beside them; rusted locker
  bank in a clean locker room (#3); big empty grey walls (#0, #1).
- **70 Research Administration.** Grey panel everywhere; offices thin.
- **71-73 Medical.** The best-reading district (two-tone walls, ceiling grid). Thin rooms (72 #1 has an
  air-conditioner on a floating plinth and nothing else); 73 #1 a stained concrete box inside the clinic.
- **74 Pharmacy.** Black shelving and stock (#1); a grey office-panel block inside the pharmacy (#3).
- **75 Ward C North.** One view fully black (#1); otherwise fair.
- **76 Ward C South.** Black shadow smears on the walls (#3); thin rooms.
- **77 Surgery.** Thin rooms (#2, #3, #5).
- **78 Morgue.** A large black slab fills a view (#2).
- **79 Materials Intake.** Good: reads as a goods-in warehouse. Keep.
- **80 Sample Storage.** Grey panel; empty rooms.
- **81 Analytical Chemistry.** Benches are boxes under a grime texture; brick side rooms empty.
- **82 Instrumentation.** Good. Keep.
- **83 Lab 4.** Grey panel; black shadow streaks under fittings; rust columns.
- **84 Field Measurement.** Lit violet so dark nothing reads (#1, #4, #5); the pan's pedestal is a black box.
- **85 Resonance Laboratory.** Big empty rooms; black wall faces (#0).
- **86 High-Energy Test Bay.** Has scale and a hero object (the stack). Stairs render as black stepped
  blocks; the black stand-in boxes at the stair head; walls one flat sepia; the tower columns black.
- **87-89 Observation Ring.** One muddy brown: brown concrete, sodium light, brown grating floor; large
  empty floors; nothing to look at between the windows.
- **90 Ring SE.** Brown again; stretched pixel texture (#3); black cabinet slabs (#4).
- **91 Shielding Corridor.** Near black throughout; empty; suits by the head; rusted locker.
- **92 Experimental Power.** Good scale and the plant bus reads. Stair sides black; the switchgear
  fronts black; the arcs are white sprite cut-outs on the floor.
- **93 Conversion Lab.** Dark violet; black stair sides; cages read.
- **94 Quarantine Ward.** Fair.
- **95 Containment Intake.** Black stair stringers; a black wall slab fills a view (#1).
- **96 Containment A.** Grey panel office block inside containment; growth columns crude.
- **97 Containment Service.** Brown, dark, empty corridors.
- **98 Containment B.** Stretched pixel textures on the growth domes; magenta still dominant; crude
  shapes.
- **99 Specimen Handling.** Reads (warm, cages, crane). Keep.
- **100-101.** See the redress report; not re-judged in depth here.
- **102 Breach Sector.** Crystals and wall growth as in cause 3: the chapter's showpiece is its worst
  art. Rebuild the crystals and the wall growth; keep the layout, seams and beat.
- **103 Emergency Containment.** Flat dead walls (#3, #4); dark.
- **104-109.** See the Deep Research redress report; 108 and 109 improved; re-judge after the fix pass.

## Done means

Every view in a fresh spot-view sheet of the map shows no black face larger than a hand, no stretched
texture, no empty room on the route, and a room you could name from its contents. The lead re-renders
and re-judges each district before it is called done; the promo set is redone from the fixed maps.
