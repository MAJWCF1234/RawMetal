# Reactor Service Gallery and Coolant Return

Campaign indices 4 and 5 remain in `src/world/World.cpp`. Both use their
existing 24 × 24 ground arrays at elevation -9 m, with structural room walls
and imported fixtures authored above those arrays. The shared chunk opening
is unchanged.

## Reactor Service Gallery

The reactor entrance opens into a receiving lobby. The west room is a repair
workshop with machinery and parts shelves. The east room is a dry electrical
room with a row of switchgear and space in front for maintenance. Further
south are spare-parts stores and a generator bay. A central service route and
cross aisles connect the rooms; secondary openings prevent the workshop and
stores from becoming mandatory dead ends. The workshop has a swinging door.

## Coolant Return

The entry landing contains paired vessels, followed by an open circulation
hall. Machinery banks flank two return channels. A dry cross bridge divides
each channel, while the centre and outer aisles provide parallel routes.
Suspended headers connect the vessels and machinery. Their geometry is map
data, not an extra scene selected by the renderer's campaign index.

The existing rear store retains its functioning swinging door. The liquid
surface, sloping bed and buoyancy use the same four water volumes; there are
no fountain emitters. The longer southern channels remain walkable at their
sloped ends.

## Assets and checks

The new machinery, switchgear and paired vessels are original PSX Mega Pack II
FBX/PNG assets. Their source proportions are preserved. See
`src/assets/facility/service/README.md` for provenance.

`--service-map-test` checks solid equipment against walls, opened doors, room
access using the player hull, overhead services, water entry/exit and debris
buoyancy. `--megamap-inspection` renders player-height views around both maps
and runs those route checks plus streaming checks. These are fictional game
spaces inspired by industrial functions, not engineering plans.
