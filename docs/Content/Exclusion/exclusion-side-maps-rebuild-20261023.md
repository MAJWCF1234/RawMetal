# Scrap Basin and South Wash rebuild

Scope: Exclusion maps 1 and 2 only. Map 0 (Forward Facility) records are
unchanged, so its tuned stealth encounter and its 47 original checks stand.
Only existing record types are used: STRUCT cover with hazard caps, fixtures,
wall lamps, signs, terminals, creatures, pickups and the existing script events.

## Problems found

- Both maps used the same copy-pasted template grid (`####` boxes, `GG`/`CC`
  runs) with a few walls and two creatures each.
- South Wash's departure relay sat 1.2 m from the map entrance, so the
  extraction map was never actually crossed.
- Scrap Basin opened its south edge into a chunk that does not exist, against
  the "no open passages into maps that don't exist" rule in
  [[utility-campaign]].

## Scrap Basin (optional)

- Landmark: Convoy 3, a stalled evacuation van with spilled cargo, an abandoned
  forklift and drag marks. It is visible from the west entry.
- Salvage gantry on the north edge, deck at 1.25 m. Six settled steps climb it
  from the west and the east end is an open drop. A Huntsman nests in the
  1.1 m crouch space under the deck.
- The convoy manifest terminal on the deck carries the story: 14 witnesses were
  loaded and the Cleanup escort halted the convoy here.
- South scrap lanes mix high and low stacks. A Brute guards an ammo cache.
- The east and south edges are closed.

## South Wash (extraction)

- Gauge hut on the arrival apron: flow log terminal, health, a floppy.
- A dry drainage channel crosses the map. There are two ways over it:
  - an exposed footbridge at 1.25 m, with wasps;
  - a crouch route along the channel under the bridge, with a Huntsman.
- The departure gate sits at the far south: a full-width wall with a lit 2.4 m
  opening. Its lamps and sign are visible from the arrival point.
- The relay terminal, objective text and `exclusion_departed` event keep their
  existing names and gates. The terminal and event region now sit inside the
  gate.

## Steps instead of STAIR

`STAIR` records are not settled onto the Exclusion heightfield, but `STRUCT`
records are (`settleExclusionFacilityOnTerrain`). Raised routes are therefore
stacked STRUCT steps, each rising 0.208 m, under the 0.215 m step-up allowance.

## Verification

- `--exclusion-test` 65/65. New real-movement route proofs:
  - gantry climb and manifest use;
  - footbridge crossing;
  - crouched channel passage;
  - gate to relay;
  - spawn and pickup hull clearance.
- `--exclusion-authoring-test` 38/38. The departure relay position was updated
  to (12.0, 20.1). Cross-map warps now sample the ground from above.
- `--exclusion-inspection` adds views 13-16 for the two rebuilt maps.
