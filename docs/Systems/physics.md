# Vectra physics

Vectra is RawMetal's rigid-body engine. It moves the game's model props (Clutter with a model): crates,
barrels, bottles, chairs, tables, carts, containers and the rest. Junk and shards keep the older point physics.
The prop physics that came before Vectra, a yaw-mostly box with tipping by rule, is still there as **legacy**.
**Vectra is the default**; legacy stays selectable as a fallback (below).

- Core: `src/game/physics/Vectra.h`, `Vectra.cpp` (namespace `retro::vectra`). It has no game code in it.
  Its queries and continuous collision: `Queries.h`, `.cpp`; props through them: `PropQueries.h`, `.cpp`.
- Props as bodies: `src/game/physics/PropBodies.h`, `.cpp`. The shape and material catalogue, the switch, the
  Clutter-to-body mapping and the save fields live here.
- Game step: `src/game/RigidClutter.cpp` (`Game::simulateRigidProps`). It covers world contacts, glass, water,
  sounds, body contacts, the held prop and the player's body.
- Tubes: `src/lab/tubes/RigidProps.cpp`. `rigid-ab` compares Vectra and legacy and prints the numbers.
  `rigid` and `rigid-legacy` film the same scenes on each. `vectra-emergence` checks behaviour that comes from
  the solver alone. `rigid-trace` and `rigid-probe` are for debugging.
  `tools/dev/rigid_sheet.py` lays the films side by side. `src/lab/tubes/VectraQueries.cpp`: `vectra-queries` and
  `vectra-ccd` (below).
  `src/lab/tubes/Joints.cpp`: `joints` (every joint kind on the bare core at 60 and 144 steps a second).
  `src/lab/tubes/JointsRoom.cpp`: `joints-room` (the Starter Map's joints corner played).
  `src/lab/tubes/VectraShove.cpp`: `vectra-shove` (props against people and things by weight, legacy and Vectra),
  `mattress` (the chained mattress) and `vectra-feel` (the pictures, fixtures `feel-*`).
  `src/lab/tubes/VectraShapes.cpp`: `vectra-shapes` (capsules and hulls: mass, a pipe rolling, a rock and gib shards
  resting on their true faces, rays, cones and bottles in the game, the cost of a heap; RM_SHAPES_TRACE=1 prints a shard's
  contacts step by step).

## Bodies

A body has:
- its centre of mass;
- its orientation, a unit quaternion;
- linear and angular velocity (world axes);
- its mass and inertia tensor, worked out from its shape's parts by volume (parallel axes, turned parts);
- a material.

A **kinematic** body is moved by the caller and is never integrated. It pushes; it is not pushed. The prop in
the player's hand is kinematic: its velocity is the hand's.

On the Clutter side nothing changed for the rest of the game:
- `yaw`, `pitch` and `roll` still say how a prop is turned. Vectra writes them from its quaternion, picking
  the equivalent triple with the smaller tilt.
- `spin`, `pitchSpeed` and `rollSpeed` hold the angular velocity: world z, and the yaw-turned y and x axes.
  The mapping is exact both ways.
- When anything else writes yaw, pitch or roll (seeding, the hand, a save), the body is turned to match. It
  compares them with `orientEuler`, the angles Vectra last wrote.

## Shapes

A shape is up to eight convex parts in the prop's frame: boxes, cylinders, spheres, capsules and convex hulls. They come
from the catalogue (the prop's dims, and the mass and container volume in prop_physics.json), with overrides by model:

| Shape | Models |
| --- | --- |
| oriented box | the default |
| convex hull | a model whose mesh's hull fills less than 0.7 of its box (a gib piece: 0.8) and that has no other shape here: rocks, cones, crystals, lanterns, lamps, rubble, the Blender gibs' shards and chair legs (152 models, `src/world/PropHulls.h`) |
| cylinder, upright | barrels, drums, burn barrel, mug, specimen jar, cell core |
| capsule round a cylinder | an upright round prop 2.5 diameters long or more: bottle, extinguisher, gas cylinder, pill bottle, a bottle's neck (a capsule end to end, its poles 2 mm inside the ends, and inside it the cylinder it was at 0.95 of its radius: it rolls on the capsule as a true round, stands on the cylinder's flat base, and leant on a rim its ends catch as the cylinder's did) |
| cylinder along its own axis | tyre (y) |
| capsule along its own axis | straight pipe (x), when 2.5 diameters long or more |
| sphere | pumpkin |
| compound | office and boardroom chairs (a thin star base, column, seat, back); wooden, scrap and platform chairs (four legs, seat, back); tables and benches (top, four legs); wheelchair; desk lamp (an L); a supine body (torso, head, legs) |
| open container | anything with `container` in prop_physics.json: a floor slab up to the inner floor, and four walls up to the rim |
| chained | a soft prop (mattress, bedroll, pad) 0.8 m or longer and under 0.35 m thick: three segment boxes along its length (below) |

Chair backs sit on the side `tools/dev/model_fronts.json` puts behind the seat.

- **Capsules** (`PartKind::Capsule`, `Shape::addCapsule`): a segment along the part's z (`half.z` its half length) and a
  radius (`half.x`). Mass and inertia are its cylinder's and its two half balls' (by hand in the tube).
- **Hulls** (`PartKind::Hull`, `vectra::ConvexHull`, `Shape::addHull`; the Shape keeps the hulls its parts use): at most 32
  corners and 60 faces. `ConvexHull::build` simplifies any point set to the cap (the farthest corners along the axes and
  diagonals first, then each the farthest outside the hull so far), builds it point by point as triangles (each edge always
  between two), merges neighbours in one plane into one face (a box's side is one face of four corners), and works out its
  volume, centre and full inertia tensor by tetrahedra. Its corners are centred on its box (`half`).
- `tools/library/make_hulls.py` takes each model's mesh into the prop's frame as the renderer draws it, keeps the models
  whose hull is clearly not their box, and writes their hulls, simplified to 32 corners, normalised to the box, into
  `PropHulls.h`; PropBodies scales one by the prop's dims. Run it again after importing models.

## Collision

- **Prop against prop.** A box and a cylinder (a 12-sided prism with the cylinder's own cross-section area)
  are convex polyhedra. The separating axis test runs over the faces of each and the crosses of their edge
  directions, preferring faces. A face contact clips the incident face to the reference face's sides, which
  gives up to four points (the deepest, then the widest spread). An edge contact uses the closest points of
  the two edges. A sphere meets a box, a cylinder or another sphere exactly.
- **Round cylinders** (`Settings::roundCylinders`, Vectra.cpp `roundFix`). A cylinder against a box or a hull (a drum, a
  barrel, a mug, a tyre, a jar against a crate, a table, a rock) is a true round: its prism finds the contact, and a cap
  flat on a face keeps it (flat is flat); where its round side is in it, the contact is worked again from the cylinder
  itself. Against one of the other's faces, the cylinder's exact feature along that face's normal (its side's line, or a
  rim point) clipped to the face, as the world's floor meets it; against an edge or a corner, each point's exact distance
  from the round. Two cylinders, and a cylinder and a capsule, still meet as their prisms. (The world met round parts
  exactly already.) A drum rolled on its side at 2 m/s over a static slab: as the prism its vertical speed varied by
  12,423 e-6 m^2/s^2 and its speed by 9,909 e-6 (it bumped to 0.2 m/s); as the round by 0.0000 e-6 and 0.57 e-6, still
  rolling at 1.62 m/s. Stood leant 2 degrees on its end on the slab it settles flat and sleeps (`vectra-shapes`, drums).
- **Hulls** use the same separating axis test. Their edge pairs are tested only where the two edges' arcs cross on the
  Gauss map (Gregorius; each arc's plane taken from its edge, so a glass shard's knife edge, whose faces' normals are nearly
  opposite, is still right), with each pair rejected by two dot products first. A hull's incident face is chosen among the
  faces round its deepest corner (on a many-faceted curve the face most nearly against the normal can miss the corner that
  goes deepest). Its floor feature (`supportFeature`) is the four widest corners of the face most nearly facing down (within
  7 degrees), by no depth, so a shard resting on a many-cornered face is held by the same points each step and sleeps, and
  the hull's deepest corner with them when it is a millimetre or more under that face (a broken monitor's casing on its
  many-faceted back had sunk 5 cm). On a flat open floor the game asks only that feature, as for a round part, and the
  world's four contacts are kept from the first point within 0.3 mm of the deepest (`keepFour`), not the deepest by a hair.
  Neighbouring faces within a degree of each other are one face (a mesh's flat side simplified to a few corners came out
  as slivers a quarter of a degree apart, and a piece lying on them rocked from one to the other).
- **Capsules** meet exactly: a sphere by the nearest point of its segment; a capsule by the segments' nearest points, two
  points when they lie side by side; a polyhedron by the exact nearest points while the segment is clear of it (each end
  against it, the segment against each of its edges), by the shallowest way out over its faces and the crosses of its edges
  with the segment once the segment is in it, and lying along a face, by the segment clipped to the face's sides (two
  points). A pipe rolled at 2 m/s over a static slab: as the 12-sided prism its vertical speed varied by 9179 e-6 m^2/s^2
  and its speed by 5083 e-6 (it bumped to a stop at 0.21 m/s); as a capsule by 0.44 e-6 and 1.58 e-6, still rolling at
  1.33 m/s (tube `vectra-shapes`).
- **Prop against the world.** Vectra asks the World what it already knows: `supportBelow`, `fits`,
  `clearanceAbove` and `shotFraction`, the same queries the player and creatures collide with.
  - Each part offers points every 12 cm over its surface, plus its exact lowest points along the floor's
    normal and any ramp's. A curved part meets floors only at its exact lowest points (sampled rim points would
    make a barrel roll like a polygon).
  - A point near what it stands on is a floor contact. Its normal is the slope, from differences of the support.
  - A point inside a wall, fixture or door gets its face from a ray out of the body's middle.
  - A point that will be inside one by the end of the step gets a speculative contact at the face its own
    motion meets.
  - The corners of the fixtures and structures round a body are tested against its own shape too, so a
    counter's corner cannot go into a crate's face between two of its points. A point more than 6 cm under
    what the World says supports it is inside that thing from its side, and meets it as a wall. A point whose
    way ahead is blocked where no ray finds a face still gets a contact against its way.
  - Only a part's underside stands on what is under it. A point up its side that has gone into a fixture's top
    edge meets the fixture's face, as a wall (a tall crate shoved against a counter is not lifted onto it).
  - The contacts are grouped by facing into manifolds of four. The floor under each part of a compound is its
    own manifold, so a chair on its back rests on its back and its seat's edge at once.
  - A ramp (`ShapeRamp`) collides in the World as 10 cm step proxies. Vectra uses its true slope instead, and filters
    the proxies out as a mesh engine filters internal edges: a point's way ahead onto a ramp meets the slope's plane (a
    speculative contact), not a riser; the proxies' corners are not tested against bodies; the swept test for fast
    bodies passes a riser or a step's top over the slope and meets the slope's plane; and a floor's slope by differences
    across a crease (a floor meeting a ramp's foot, a crest meeting its landing) takes the flatter side's, the face the
    point stands on, not the average no face has. A crate slid onto a ramp's foot at 3.4 m/s had hopped 45 mm and a
    bottle had bounced back off the first riser; both now go up it on the slope (tube `vectra-gaps`, seams). The joins
    of a platform's boxes and of floor tiles were never seams: the World answers heights, not triangles.
  - The usual case is asked once per body: one flat floor under the whole footprint, and every column round
    and through it open. The answer is kept for a region 4 cm round the body (a resting or sliding prop asks
    again only when it leaves it, falls 2 cm, or a door moves), and on such a floor only the lowest points and
    a box's corners are asked about.

## Solver

TGS-soft, after Catto's Solver2D and Box2D v3. Each fixed **120 Hz** step (accumulated from frame time, at most
six a frame) runs one collision pass, then **four substeps**. Each substep:
1. integrates velocities (with an implicit gyroscopic step);
2. warm-starts the contacts from last step's impulses (matched by contact position on the body);
3. solves with a soft bias;
4. integrates positions;
5. relaxes without the bias.

Restitution runs once per manifold at the end.
- **Contacts** are springs of 120 Hz (the substeps' limit) with a damping ratio of 2, against the world and
  between bodies. Push-out is capped at 1 m/s, and up to 1 mm of overlap is allowed (0.5 mm against the world).
  A woken ten-crate tower settles in 0.27 s with its top 0.6 mm off where it stood.
- **New contacts** (nothing to warm start from: a stack just woken or set down whole) are iterated eight more
  times on the step's first substep, so the weight reaches the floor before anything moves. What a sleeping
  body's contacts held is kept while it sleeps, so a stack woken by a shot starts from its own impulses.
  Warm starting matches contacts by the props' own indices, not their places in the step's body list. A manifold's
  key (`Manifold::key`) gives each its own field: the two bodies' ids (20 bits each), A's part (3) and B's part or the
  world cluster the caller numbered (21: a cell or a point's number will do). They had been XORed over each other, so a
  cluster of 256 or more under part 1 came out as cluster 0 under part 0, and one of the two lost its warm start each
  step; the game's own clusters (RigidClutter's groups, under 48) never met it, a corpse's walls (numbered by point)
  could.
- Each impulse's turn on a body (its inverse inertia times r x axis) is worked out once a step, and each part's
  hull is built once a step however many pairs it is in.
- **Speculative contacts** close a gap within the step and no more.
- **Impacts.** A manifold first takes an impact at its middle, so a box landing flat does not spin. What it
  reports (glass, breaking) carries the body's mass at that middle, not one corner's share.
- **Friction** is Coulomb, inside a cone. A contact uses static friction if the surfaces were sliding slower
  than 8 cm/s when the step began, dynamic friction otherwise.
- **Rolling and twisting resistance** is a length times the load. A bottle stops sooner than a drum.
- **Mass scaling.** A body resting on a lighter one, by a contact within 45 degrees of vertical
  (`Settings::shockNormal`, knob `props.shock`), meets it as if the one under were as heavy as itself, in that
  contact only (its prepare, warm start, solve and restitution alike). A 100 kg drum on a 1 kg crate sinks 1 mm into
  it instead of 5 (22 mm dropped 10 cm on it), and the crate 0.5 mm into the floor. A body a joint holds keeps its
  true mass (a bridge's planks), and a ragdoll's sections are never scaled (`vectra-stack`). Only a body held up itself
  is scaled under another: one touching the world under it (within a centimetre), or a body that does not move (asleep,
  held), or one so held, up a stack from the bottom. Two falling together meet with their true masses (the crate in the
  air had been taken as 100 kg): a 100 kg drum meeting a 1 kg crate in the air at 4 m/s slows by 0.05 m/s and knocks the
  crate on by 5.2, as with no mass scaling at all (`vectra-stack`).
- **Driving.** `Space::drive` pulls a body to a pose with a critically damped spring, applied as velocity so contacts
  still hold. The change in velocity is held to `maxForce` and the change in spin to `maxTorque` (by default `maxForce`
  times the body's reach), through its inertia: a 2 m plank driven into a doorframe's jamb rests against it at
  0.003 rad/s, where the spin set outright kept it shaking at 0.11 (`vectra-drive`). Nothing in the game drives a
  body yet (the held prop is kinematic, Physgloves throw).
- **The slide and roll** of each manifold when the step began (`Manifold::slideSpeed`, `rollSpeed`, m/s) are what the
  scrape and roll sounds go by.

## Sleeping

Islands are bodies linked by contacts and joints. An island sleeps when every body in it has stayed under
6 cm/s and 0.12 rad/s for 0.2 s. A sleeping body takes part only as an immovable obstacle. Something moving that comes
within reach of a sleeping body wakes it: a blow from a moving body is never stopped by a sleeper. A body woken that
way wakes what it touches in the same step, and meets it, so it is neither driven into a neighbour unopposed nor stopped
by one still asleep.
A sleeping prop out of reach of every awake prop and of the player is not built at all. One that is built keeps its
bounds while it has not moved, and a part's hull is built only when a pair needs it: 400 resting props cost 0.065 ms a
step, and 20 fast shards through them 0.48 ms (`vectra-scale`; 0.14 and 0.62 before). Continuous collision meets a
fast body only with the bodies whose swept boxes reach its own (sorted along x once a step).

**Active-only stepping.** After the collision pass the step keeps the list of bodies it moves (`Space::active`: awake
and movable, by index). The substeps' integrations go through it alone, and only an awake body's inertia is turned to
the world each step (one a touch woke in the pass is turned then). The pair sweep starts from the active bodies only:
every body's interval along x is sorted once, and each active body looks back by the widest interval and on while
they start before it ends, so two sleepers are never even looked at as a pair. The warm-start cache sorts only this
step's manifolds and merges them with what the sleepers kept (already in order). 600 props asleep with 30 awake among
them (89 awake once they woke what they struck) went from 0.585-0.650 ms a step to 0.540-0.558 on the bare core, and in
the game (591 built, 50 awake) from 0.733 to 0.644-0.652; 600 sleepers nothing touches now add 0.06 ms to 30 awake
bodies' 0.10 (0.1 us a sleeper; tube `vectra-gaps`, active). Per-island solving on worker threads is not done: the
islands are only known after the step, the game's awake set is small (its solve is 0.15 ms), and a thread's start
would cost more than it saved.

## Continuous collision

- Speculative contacts look as far ahead as the body moves in a step, for floors, walls and other props.
  They stay the first line; the sweeps below catch what passes them.
- A body is **fast** when, in a step, its centre goes or its reach turns (angle times its bounding radius)
  more than half its smallest size (2 cm at least). A plank spinning in place is fast though its middle is
  still. `RM_CCD=off` turns the whole pass off (to tell what it changes in a check).
  `Space::continuous` runs `continuousCollision` (`Queries.cpp`) on each fast body, in index order:
  - **Jointed islands.** Bodies joined by joints that hold (a corpse's sections, a door on its hinge, a lamp and its
    chain, a bridge's planks) are swept as one island: the earliest time of impact over every fast body in it puts
    all of its bodies back to that moment together, each along its own way through the step, so what joins them is as
    it was then (one put back alone would open its joints by as far as it went). Against the world the one that struck
    takes the blow; against another body a jointed one takes none there (its partners share it, which only the step's
    solver knows: a 2 kg plank of a bridge struck alone took a 20 kg crate's landing as if it were loose), and the two,
    put back a hair apart, meet by their contacts on the next step. A jointed body that strikes nothing at its edges
    is not put back by its centre's sweep. A six-box ragdoll thrown at 15 m/s at a 3 cm wall, a 20 kg door slammed at
    15 rad/s into a 3 cm post and a lamp whipped on its chain at 20 m/s at a 3 cm pole stay on their side with their
    joints together (`vectra-ccd`, 60 and 120 steps a second); with `RM_CCD=off` the door and the lamp go through.
  - **Against the world.** Points on its edges are ray cast by `Space::sweep` through the step: its corners (a
    cylinder's and a capsule's rings of eight), each edge's middle, and along each edge every 2.5 cm where the point
    leads (it goes out of a face round it, not into the body) and goes further than the body is thick (a thin thing
    could pass it whole there; nearer, it would end inside the body, which the contacts push out). Points that lead
    nowhere or hardly move are left out. A body is held to `Settings::ccdProbes` (128) points, its leading edges' points
    spaced wider to keep to it, and a step to about `ccdBudget` (4096) sweeps, each fast body its share of what is left
    (its leading edges thinned first, its corners and middles always). Each point's way is swept in pieces of the turn,
    as few as keep its chord within 2 mm of its arc (a power of two, at most the 0.15 rad pieces the body's reach asks):
    a point near the middle of a turning body takes one. `ccdProbes 0` (knob `props.ccd_probes`, with
    `props.ccd_budget`) is the old way, every 2.5 cm along every edge through every piece. Twenty crates of 0.3 to 1 m
    launched by a blast in a pen of 3 cm walls ask 2,412 world sweeps over half a second (100 in the worst step) where
    the old way asked 25,023 (1,130), 2.5 ms against 4.5 on the bare core; in the game, against the World, 2,706 against
    28,572 and 3.9 ms against 18.8; none gets out either way. A 1 m crate thrown at 35 m/s turning at 10 rad/s asks 11
    in its first step (304), the plank spinning at 60 rad/s by its 3 cm post 11,659 in a second (20,526), still never
    through it (`vectra-ccd`). Trailing points are skipped as before. A point that strikes a surface counts when it
    went through what it struck (a ray back from where it ends finds a face between) or ends more than 2 cm
    into it; shallower is the contacts' business. The body goes back to that moment, 1 mm off the surface, and
    takes a blow at the middle of the points that struck that face together (so a plank's face hitting a post
    along its height is not flipped over by one corner). If no edge point strikes anything, the old sweep of
    its middle still runs.
  - **Against other bodies.** Each body whose swept sphere comes near it is tested for a time of impact:
    conservative advancement on the GJK distance, with the gap's closing speed bounded by their approach along
    the normal plus each one's turn times its reach. Left out, as the step itself leaves them: pairs the Space's
    filter or groups rule out, pairs a holding joint joins, pairs the caller meets itself (`Space::pairContacts`:
    a corpse's skin), pairs with a soft body (a padded prop, a prop going through a corpse), and pairs going
    together (a crate and the bottles in it: the other moved, in this one's frame, no further than the thinner of
    the two is thick). Pairs already touching when the step began are left to the contacts. At the time of impact the body (and the
    other, if it was fast too) goes back to that moment and the two take a blow at the point (restitution as
    the contacts mix it).
  - Each answer is reported as a `ContactEvent`, so sounds, breaking and glass hear it as a contact.
- Glass gets a look-ahead ray along the body's path: a pane it will meet this step is struck with the energy
  of the speed it will arrive at.
- `vectra-ccd` checks it: a plank spinning at 60 rad/s beside a 3 cm post (the world's and a fixed body), two
  thin boards and two crates thrown at each other at 25 m/s, a bottle at 30 m/s at a 3 cm wall, and the jointed
  islands above.

## Queries

`src/game/physics/Queries.h` asks bodies what a ray, a shape or a region meets, in their true pose. No state;
any thread. `PropQueries.h` puts a prop (or a chained pad's three segments) through the same queries.

- **Rays** meet each part exactly: a turned box by its slabs, a cylinder by its round and caps (the true
  cylinder, not the solver's prism), a sphere by its round, a capsule by its round side and its end balls, a hull by its
  faces' planes (the last way in before the first way out). A compound meets only its parts. 200 rays at a rock's hull from
  every side all land on its surface (0.0006 mm off it at worst); its box would have stopped 155 of them sooner.
- **Sphere and box casts** sweep a shape along a line by conservative advancement on a GJK distance (support
  functions), so a turned box meets a turned cylinder as it is.
- **Overlaps** of a sphere or a box with a body.
- **Many bodies**: lists are pruned by each body's bounding sphere first; `rayCast(Space&)` by the boxes the last
  step found (moved and grown by the motion since). `rayCastAll` gives every hit in order.
- **Distance and time of impact**: `partDistance`, `bodyDistance`, `timeOfImpact` (above).
- Each hit gives the distance, point, outward normal, body and part, and whether the cast began inside.

What uses them:
- **Gunfire** (`Game::traceShot`): each prop by `rayCastProp`. A round past a turned crate's corner passes; one
  through the gap under a chair's seat passes the chair. The blockMass rule is unchanged: a light prop is a
  passage the round goes on through, a heavy one stops it. Water passages, glass, bodies and ragdolls are as
  before. The VR fist's sweep goes through `traceShot` too.
- **Shielding** (`Game::shieldBlow`): the blow's way, a box 10 x 24 x 60 cm swept from the attacker's reach
  (at most 1.6 m out) to the front of the player's body over his upper body, must meet the held prop as it is
  held. A blow from a side it does not cover lands on him (the old rule took anything within 60 degrees).
  A mug is still not a shield.
- `vectra-queries` checks the core against hand-worked distances, gunfire past a turned crate and under a
  chair, and shielding from in front, the side and 45 degrees off.
- Left for later: the Physgloves' targeting and the carry's placement (Clutter.cpp, phase 3's) still use
  boxes and the prop's middle; `rayCastProp`/`sphereCastProp` are the drop-in. Character contact can use
  `boxCast`/`sphereCast` for the hull sweep and `overlapBox` for a man standing in a prop, and
  `timeOfImpact` for a fast prop against a moving body.

## Materials

| Material | Static friction | Dynamic friction | Restitution | Rolling (m) |
| --- | --- | --- | --- | --- |
| paper | .55 | .45 | .05 | .01 |
| cardboard | .6 | .5 | .12 | .008 |
| plastic | .45 | .35 | .4 | .002 |
| wood | .62 | .45 | .3 | .002 |
| metal | .45 | .38 | .32 | .001 |
| glass | .45 | .35 | .45 | .0015 |
| ceramic | .5 | .4 | .3 | .0015 |
| electronic | .5 | .4 | .2 | .002 |
| cloth | .85 | .7 | .02 | .02 |
| organic | .7 | .55 | .15 | .006 |
| rubber (lab bodies) | .95 | .85 | .82 | .002 |
| floors (the World) | .6 | .5 | 0 | .004 |

A floor gives its own material (`Surface::grip`), which `Space::prepare` mixes with the prop's for that manifold.
The game sets it from the map where the floor manifold's points are (RigidClutter.cpp `floorGrip`): concrete as
above; a metal deck (and the corrugated and pressure finishes) .45/.35, rolling .002; clinic tile and lab panel
.5/.4; mine timber .62/.5; rock and stone .7/.6; office carpet .8/.65, rolling .012; on painted terrain grass
.55/.45 and mud .4/.3 (both rolling .03), dry ground .65/.55, gravel .7/.6 (rolling .02). Water over any of them
(a WET patch by its amount, a puddle fully) draws it toward .22/.15: a crate shoved at 3 m/s slides 0.67 m on
concrete, 1.22 m in a puddle and 0.59 m on carpet (`vectra-grip`). Walls keep the plain floor material.

Two materials meeting take the geometric mean of their frictions, the larger restitution and the larger
rolling resistance. Chairs on casters and carts on wheels (office and boardroom chairs, wheelchair, crash cart)
use .3/.25.

Padded props, mattresses and bedrolls (`model::soft`), are 4 Hz springs, so a load sinks in. A padded fixture
gives where `softSinkAt` says it does.

## Joints

`vectra::Joint` (Vectra.h, `src/game/physics/Joints.cpp`) joins two bodies, or a body and the world (`b = -1`). Its rows are solved
in the same substeps as the contacts: before them, and once more after them, `Settings::jointIterations` (4) times each pass.
Locked rows are springs of `jointHertz` (60) with damping ratio `jointDamping` (2), warm started, and relaxed without the bias as
the contacts are. Limits are one-sided rows: a gap closes within the substep, a pass is pushed out softly. The anchors'
row works its effective mass out from the arms as they are at each pass (taken from the step's first arms, a ragdoll's limb
flung against a wall, turning half a radian in a step, overshot more each pass and blew the body apart).

Each body carries the joint's frame: an anchor (from its centre of mass, unturned) and a turn whose z is the joint's axis.
`attach` sets both from the bodies as they stand, so the pose they stand in then is the joint's rest.

| Kind | Holds | Free | Options |
| --- | --- | --- | --- |
| hinge | the anchors together, z of B on z of A | the turn about z | limits, spring, motor (speed or servo), friction, detents |
| ball | the anchors together | every turn | an elliptical cone (`coneX`, `coneY`), a twist limit, friction |
| slider | the turn, B's anchor on A's z line | travel along z | limits, spring, motor, friction, detents |
| weld | everything | nothing | |
| distance | the anchors `length` apart | | |
| rope | the anchors at most `length` apart (slack when nearer) | | a winch: its motor reels the length in at `motorSpeed` while the pull is under `motorMaxForce` |
| spring | pulled toward `length` (`springHertz`, `springDamping`) | | limits on the length |
| pulley | `lengthA + ratio * lengthB <= length` over `groundA` and `groundB` | | a motor turning the wheel |

- **Motors.** Speed holds a speed with at most `motorMaxForce` (a speed of 0 with a force is a brake). Servo drives to
  `motorTarget` at up to `motorSpeed` (a hinge's angle goes round). A motor at work keeps its bodies awake.
- **Friction.** `frictionTorque`: on a ball, the whole turn (N m); on a hinge, about its axis (N m, a door's pin); on a
  slider, along its rail (N, a drawer's runners). A row that takes the rate out with at most that much, warm started
  (`frictionImpulse`), after the motor and before the limits: a cabinet door nudged at 1.2 rad/s with 1.5 N m stops
  14 degrees on and stays; with none it glides to its stop.
- **Detents.** A hinge or slider catches at up to four places (`detents`, `detentCount`; radians or metres from its rest):
  within `detentRange` of the nearest a critically damped spring of `detentHertz` (6) pulls it there, holding with at most
  `detentForce` (0: the spring alone). Out of range it holds nothing (`detentImpulse` is taken back out). A drawer pushed
  shut that friction stops 1.5 cm short clicks shut; latched at 60 N, a 20 N pull leaves it, 150 N opens it (tube
  `vectra-gaps`). Doors latch where they were drawn shut, hatches stay shut, drawers catch (the Connect tool's defaults).
- **Breaking.** `force` and `torque` are what the joint carried over the last step (its impulses over the substep). Past
  `breakForce` or `breakTorque` it snaps once (`Space::jointEvents`) and holds nothing after. A board nailed across a
  doorway, a sign on a wall, a shelf's welded breakables are a `weld` (or a `ball`, one nail) with `breaks=` (the
  Connect tool's Nailed writes a weld at 600 N): a 2 kg box at 1 m/s pulls it with 11 N and it holds; a 30 kg crate
  at 6 m/s tears it off (tube `vectra-gaps`, breaking).
- **Position correction** (`Joint::positionCorrection`, after the step, `Settings::projectIterations` passes): what the
  substeps left open is put back by moving the bodies, each by its share of mass and inertia, velocities left to the rows.
  A ball: its anchors and its swing-twist limits (a corpse's). A hinge: its anchors, its axis and its angle's limits. A
  weld: its turn and anchors. A slider: its turn, B's anchor on A's line and within its travel. A distance: its length; a
  rope: no longer than it. Not a spring or a pulley, nor against a held (kinematic) partner (a prop dragged off its
  chain still gives). A hinge's to a rope's pass is held to `projectMaxLinear` (0.1 m) and `projectMaxAngular`
  (0.3 rad), so one wrenched far open closes over a few steps rather than being thrown through what is beside it. The
  game's prop joints all have it but springs and pulleys: a 40 kg door struck near its edge by a 30 kg crate at 12 m/s
  opens its pin 1.3 mm uncorrected, 0.01 mm corrected; a welded pair struck end on by 60 kg at 15 m/s opens 6.9 mm and
  turns 2.1 degrees uncorrected, 0.01 mm and 0.04 degrees corrected (tube `joints`, 60 steps a second).
- **Warm starting across rebuilt spaces.** A joint with a `key` stores its impulses in `Space::jointCache` (the game keeps
  the cache per level), so a joint built afresh each step starts from what it carried.
- **Waking.** A body joined to an awake one wakes with it, through the whole chain. Islands link through joints. A held
  (kinematic) body on the move wakes what is joined to it.
- **Mass ratios.** A 1 kg lamp on a 50 kg cabinet, and a 50 kg weight under two 1 kg links, hold within 2 cm at 60 and
  144 steps a second; five 20 kg crates dropped on a bridge of 2 kg planks open its pins up to 6 cm as they land and under
  1.5 cm once they lie on it (tube `joints`).
- **The mattress** (PropBodies.h `chained`) keeps its API: `vectra::Hinge` is now a Joint, its fields (`axisA`, `rest`,
  `hertz`, `damping`, `limit`) set the hinge's spring and limits. A chained prop is three segment bodies, each a third of its
  length, joined by two hinges about its width (`pad.hertz` 2.5, `pad.damping` .25, `pad.limit` 1.5 rad); its frame is the
  middle segment's, or while carried the end nearest the player (`Clutter::boneRoot`); `Clutter::bend` and `bendSpeed` are
  the joints' bends. The renderer skins its mesh by segment (`chainSkin`).

### Joints between props in play

`src/game/PropJoints.cpp`. A map's `JOINT` records (map-format.md) become Vectra joints in each step of the props
(`Game::simulateRigidProps`):
- A joint's frames come from where the map laid its props out (their home pose), whatever has happened to them since.
- Both props of a joint are built whenever either is: a sleeping lamp is not left behind by its chain.
- What snaps (past `breaks=`, pulled half a metre apart (`joints.tear`), its prop broken, or a rope or chain shot through
  by a round: `cutJointsByShot` from `resolveShot`) is marked in the world's `PropJointState`, sounded (`LiftSnap`, heard),
  and sets its `state=`. A snapped rope from the ceiling leaves a stub hanging.
- The player stands on jointed props of 8 kg and more (`jointedPropTop`, in Movement's ground): a lift's platform, a seesaw,
  a bridge's planks. His weight (75 kg) is on the prop where he stands, and his mass in its inertia. A jointed prop under his
  feet neither blocks him (`onJointedTop`) nor strikes him.
- A `LIGHT` riding a hung prop (its prop field) moves with it and burns while the prop is whole (`hungLampAt`, `lampPower`).
- Ropes and chains are drawn by Scene3D from `jointLine`: a hemp tube or beads of links, sagging by their slack, a pulley's
  two runs over its wheels.
- The Starter Map's SHOWCASE: PHYSICS has the joints corner (make_starter_map.py `joints_corner`): cage lamps on chains,
  a crate on a rope to shoot down, a seesaw, a sprung gate, a rope bridge, and a lift on a slider hung over a pulley against
  a counterweight. The tube `joints-room` plays each (fixtures `joints-*.lab`; `joints-shot-*.lab` take the 1600 x 900 pictures).

### Curtains, straps and cables (src/game/Cloth.h, Cloth.cpp; tubes `cloth`, `cloth-room`)

A map's `CLOTH` records (map-format.md) hang as beads in Vectra, each cloth in a `vectra::Space` of its own kept while the
level is played (as a corpse keeps its own), so warm starting, the joints' impulses and sleep carry from step to step.
- **Beads and joints.** A curtain is columns every 20 cm along its rail and rows every 20 cm down it (3-14 by 3-12); a strap
  or a cable one row every 8 cm along it (4-20, 5-32). Each bead is a small sphere (a curtain's 3 cm, a strap's half its
  width, a cable's its radius) sharing one group, so it never meets itself. Neighbours are joined by ropes (`Joint::Rope`:
  at most as far apart as they hang, slack nearer, so it folds and drapes), soft at `stiffness` (`cloth.hertz` 120 Hz: at
  30 a 1.2 m strap stretched 9 cm under its own weight, at 120 it stretches 6 mm); a strap's and a cable's every other bead
  by a weak spring (`cloth.bend` 1.5 Hz: it bends rather than kinks; a curtain hangs in folds without). Two joint passes a
  substep. A curtain's rail, a strap's top and a cable's ends are kinematic beads where the map put them. Air drag
  `cloth.drag` 1.2. People are slick to it (friction 0.12): it parts round them rather than being carried along.
- **What it meets.** The floor under each bead and the walls round it, as a corpse's points (a bead in a wall meets it toward
  where it was last clear; one a bead's width from a wall meets it at no gap). The props near it as kinematic bodies of their
  own Vectra shapes at their velocities; the player and the creatures as capsules (the player's hull height, her velocity as
  she moves). So it parts and sways round whoever walks through it and drapes over what is under it; it does not push back.
- **Shots.** A round through it (`Game::shootCloth`, from `resolveShot` beside the joints' ropes) knocks the beads it passes,
  most the nearest (`cloth.shot` 4 m/s), and wakes it.
- **Wind.** Out under the sky (a zone sky or terrain) the breeze the plants bend in (Foliage.cpp: turning and gusting,
  harder in a storm, `cloth.wind`) pushes each bead by its own flickering share; indoors there is none and it settles.
- **Sleep and cost.** It sleeps when it settles (0.4 s still); asleep with nothing moving near it and no wind it is not
  stepped at all. Fixed steps of 1/120 s, at most four a frame. A wall is looked for round a bead only once it has gone 5 cm.
  Four 1.4 x 1.8 m curtains (80 beads each) cost 4.4-4.9 ms a frame swaying (0.58 ms a curtain a step, nearly all the
  joints' solve) and 0.02 ms asleep (tube `cloth`).
- **Checks** (tube `cloth`): a 1.4 x 1.8 m curtain hangs plumb with its foot 2 mm off its drop and sleeps in 0.42 s; walked
  through at 1.5 m/s it parts and sways (beads up to 2.5 m/s) and is asleep again 6.9 s later within 2 cm of where it hung;
  a 2.6 m cable over 2 m sags to 0.41 m bare and drapes over a 0.6 m crate with its middle 1.4 cm over the top; a 1.2 m
  strap hangs to 6 mm of its length, swings 11 cm when shot and sleeps 3.9 s later; the record goes out and back in.
- **Drawn** (Scene3D, after the joints' ropes): a curtain as a sheet of the ward curtain fabric lit on both faces, a strap as a
  leather ribbon, a cable as a rubber tube with a steel clamp at each held end.
- Not saved: a loaded game hangs it afresh from where the map fixes it, and it settles in a second.

### Corpses on Vectra (opt-in)

`Carcass::stepVectra` (RM_RAGDOLL=vectra, or the knob `carcass.vectra 1`; RM_RAGDOLL=legacy forces the position-based step)
steps a corpse on Vectra:
- **One space a body keeps** (`Carcass::VectraState`): each section is a body for as long as the corpse lasts (its points'
  mass and inertia, 75 kg a man), so warm starting, sleep timers and the joints' impulses carry from step to step. A round,
  a blast or a cut pushes the points as before (`kickVectra`), and the bodies take their velocities from them.
- **Joints** are Vectra balls at each pivot, frames carried by each section's bind turn, so B's turn from A's is exactly the
  turn the rig measures (`Carcass::jointStates`). The limits are `Joint::swingLimited`: the swing-twist split of that turn,
  the bend between its two stops, the side lean and the twist either way, each a one-sided row along the split's own rate
  (worked out by a central difference each pass, so a knee stops at its own angle at any bend, not at a cone's rim).
  `Joint::frictionTorque` (`carcass.vfriction`, 1 = what the limb's own weight turns it by, held level) lets a limb land
  and stay rather than flop on. `Joint::positionCorrection` puts back what is past a limit by more than 1.5 degrees
  (`Settings::projectSlack`), turning both bodies about the pivot together, not while a prop goes through the body.
- **Sections against each other**: joined ones never meet; the others meet by `Space::pairContacts`, the skin points of one
  against the other's oval capsule (the measure the lab checks), one contact per point within reach.
- **The world**: each section's points, its hull points and its outermost skin (its vertices farthest out over 96 ways)
  meet the floor and the walls. A point more than 4 cm under what supports it (`carcass.vsunk`) is in it from its side and
  meets it as a wall, never lifted onto its top.
- **Props** near it are kinematic boxes going at their share of their speed; one moving through it is a 12 Hz spring
  (`carcass.vshoveHertz`), so it shoves the body without pressing one part into another.
- **Sleep** is Vectra's island sleep (5 cm/s and 0.15 rad/s for 0.4 s): it sleeps where it lies, nothing is moved on the way.

Not the default yet. ragdoll-fall, 14 fixtures (lab, 2026-11-18):

| Fixture | Position-based (default) | Vectra |
| --- | --- | --- |
| bed | fails: volume 4.9% | passes |
| brute | passes | passes |
| chimera | fails: jitter before sleep, a core 1.9 cm | passes |
| crate | passes | passes |
| drop | fails: volume 4.2% | passes |
| headless | fails: volume 7.3% | fails: volume 8.4% |
| shot-behind | passes | passes |
| shot-front | passes | passes |
| shot-side | fails: volume 4.7% | fails: volume 6.1% |
| shotgun | fails: 6 corners in the floor; the save check | passes |
| stairs | passes | fails: volume 8.5% |
| table | passes | passes |
| thrown | fails: volume 4.2% | fails: slides 0.51 m, overlap 0.5 cm, a limit 3.4 deg, volume 4.3% |
| wall | fails: volume 14% | fails: volume 19% |

- **Volume** (each section's open skin about its own middle): the men's meshes are coarse, and many triangles of one
  section have corners on another (the worker's pelvis is 46 triangles, 41 of whose corners ride other sections). A bend
  at the hips or the belly then changes those sections' measure whatever the physics does: joints hold to under a
  millimetre and every rigid core to 0.01 cm. A man sat against a wall (90 degrees at the hips) loses 14 to 19% on either
  system. It needs the skin or the measure looked at, not the solver.
- **Stairs**: he tumbles down the steps after he is first down on the landing (his middle drops 0.38 m while it goes
  0.38 m on). The slide check now starts again each time his middle comes down 10 cm lower (a step further down): going
  down is falling, not sliding; on one level nothing changes. On the stairs he then slides 0.12 m. A point of a section
  4 cm or more under a step's top meets the riser as a wall, each such point with its own normal, so nothing rests in a
  step. What is left is the volume (8.5%, the coarse skin again: a forearm folded to its stop).
- **Never propped**: near rest (nothing moving a centimetre a frame), a man on knees bent past 40 degrees with his hips
  30 cm or more over what is under them has no joint friction, so he slumps as dead weight does; ragdoll-fall checks that
  no body rests in a crouch (hips over 0.35 m on knees bent past 45 degrees).
- **Thrown**: the crate in the game does not feel the body (corpses are not in the props' solver), so it shoves him all
  the way through.
- No jump into sleep on Vectra (under 1 mm a frame; the position-based step's is 4-15 mm).

## How the rest of the game meets it

- **Hand.** The held prop is kinematic at the hand's velocity. Throws and the desktop flick still set velocity
  and spin. VR uses the tracked hand's velocity.
- **Player.** In the solver the player is a 20 kg body from his step height up. His contacts are 5 Hz springs,
  so he shoves light props, and a prop pinned against a wall stops him rather than going into it. What he
  pushes goes no faster than the old push's speed. A light thing at his feet is set off ahead of him at his
  pace. A heavy prop (8 kg and up) blocks him, then is pushed at a walking pace, put in low so it slides
  rather than tips; after half a second it yields as it always did.
- **Containers.** What lies in one rests on its inner floor, and is held by its walls, by contact. A
  container in the hand, or one the player is shoving, carries what is in it rigidly (neither the hand nor his
  shove is a body with mass, and a sprint's shove would jerk it out from under them). One thrown, let fall or
  knocked takes its contents by its floor and walls, with their weight on it when it lands. Tipped or broken, it
  pours out by itself.
- **Hand.** The held prop spins with the hand (its turn since the last frame), so a crate swung round carries
  what stands on it by friction and flings it off when jerked.
- **Water.** Each part is 27 cells, each under the surface by its own share. A round part's (a cylinder's, a capsule's)
  cells lie round its axis, one on it and eight on a ring (at 0.72 of its radius: each holds as much of its section), each
  as tall as its disc stands, so a drum turning in the water rides on fewer corners: afloat on its side and turned about
  its axis at 4 rad/s its vertical speed varies by 2,158 e-6 m^2/s^2, 6,864 e-6 with the square turned with it as before
  (`water.round_cells` 0; `vectra-shapes`, drums). The lift acts at the middle of
  what is under, drawn toward the body's middle (`water.lever`: a prop's mass is in its walls), and the water
  rights a floater onto its nearest face it can float on: one whose waterplane's lever over the draft (BM) beats
  the drop from the middle of what is under to its own (BG, taken at `water.hollow` of a solid block's). A crate
  rides on any face; a plank, a log or a pipe only lying flat; a drum or a bottle on its side. Linear drag is
  linear plus quadratic in the speed, as before; the turn is resisted by each wet cell's own speed through the
  water, by its square (half rho Cd A |u| u at the cell, `water.turn_drag` the Cd), so a long thing spun or
  tipping is checked by its reach. A sealed drum dropped in upright bobs, tips onto its side and settles; a plank
  dropped in on its end lies flat (before, the nearest face held them upright and on end: tube `vectra-gaps`,
  water). Swell and drift are as before. A body coming through the surface splashes.
- **Glass.** A pane takes the energy the contact's effective mass brings into it (the look-ahead above for
  a fast throw).
- **Sounds, AI hearing and breaking** come from each contact's approach speed. Breaking uses the speed of the
  prop's middle, which is what the break speeds were tuned on.
- **Scrapes and rolls.** Each prop eases `Clutter::scrapeLevel` and `rollLevel` (0 to 1, not saved) toward the
  fastest slide and roll of the contacts it bears on (from 6 cm/s, full at 1.26 m/s sliding and 1.56 m/s rolling),
  and drops them to 0 when it is held, asleep or broken. The AudioEngine loops the bank's dragged-metal sound for
  each while it is over 0.02, weighted and pitched by the prop's material (metal as recorded, wood lower, glass and
  ceramic higher and quieter, soft things low and faint), a roll an octave lower and softer (`vectra-scrape`).
- **Bodies.** The old body contacts still run: people and creatures struck, carried along, stood on.
  Heavy-impact tipping is real torque now.
  - **Bodies in the solver.** Each man and creature within reach of a moving prop is a body in the solver: a
    box of his hull from just over his feet, his weight, his own speed, no gravity, no turning, hard contacts. A
    prop meets him by contact and never passes into him; one leaning on him or pushed slowly into him moves him
    as far as it moved that body. (The prop in the hand is left to the carry's own sweep.)
  - **Knocked back by momentum.** A prop that comes at him gives him the speed its momentum shares between them
    (1.2 times its mass times its closing speed, over both masses). Under 0.15 m/s it is a nudge (a mug: a
    centimetre). Over it he stumbles back 1.6 m per m/s of it, at most 1.5 m, slowing at 4 m/s^2, through the hull
    sweeps he walks by, so nothing is pushed into a wall: a thrown 9 kg crate knocks a guard about 0.6 m back
    and a creature 1 m. Past 0.5 m/s a creature's blow is spoilt and it waits before the next; past 1.3 m/s (a
    35 kg drum at a run) a man is knocked down, out cold where he falls (the restraint knockout), and a creature
    waits longer. (Game::m_bodyKnocks, updateBodyKnocks.)
  - **Hurt by the blow's energy.** Damage is a fifth of the energy past 2 J (half m v squared at the closing
    speed, from 1.5 m/s): a thrown mug stings (2 to 3), a thrown crate hurts badly (about 20).
  - **Breaking by the blow.** A prop breaks past its material's break speed, or when the blow it takes (the
    contact's impulse over its own mass, over 1 + restitution, from 1 m/s) is past it: a crate dropped on a mug
    crushes it, the mug dropped as far alone stays whole. The impact sound's cooldown never spares a prop the
    blow that breaks it.
  - **Glass** takes the energy at the manifold's middle: a 20 kg box dropped 1.2 m goes through a glass floor,
    a 1 kg box does not.
  - **The shield.** A blow is the prop's only if its way meets the prop as held (Queries, above). A carried prop
    that does not break is torn out of the hands once it has taken
    `ShieldKnockAway` times its mass over 9 kg (half to three times): a 20 kg crate takes 9 blows, a 4 kg one 2.
- **Cables, saves and the renderer** read yaw, pitch and roll as before.

## Saves

The `physics` save section (save-format.md) keeps each prop's quaternion (x, y, z, w), its angular velocity
(world axes, rad/s) and its sleep island, through `vectra::captureProp` and `vectra::restoreProp` installed in
`physicsSaveHooks()`. A save without the section, or from before format 28, turns the body from yaw, pitch
and roll; the result matches the saved turn to 0.001 mrad (`rigid-ab`).

## The legacy switch

**Vectra is the default.** Legacy stays as a fallback: `RM_PROPS=legacy` in the environment, or the knob
`props.legacy 1` (`RM_TUNE` / `lab/tune.txt`), runs simulateClutter's own step and the old container riding.
`RM_PROPS=vectra` forces Vectra. `vectra::forceLegacyProps` sets either for the lab's
A/B tubes (`rigid-ab`, `rigid`, `rigid-legacy`, `vectra-emergence`). Vectra's own knobs:

| Knob | Default | What it sets |
| --- | --- | --- |
| `props.substeps` | 4 | substeps a step |
| `props.contact_hertz` | 120 | contact stiffness |
| `props.contact_damping` | 2 | damping ratio between bodies |
| `props.world_damping` | 2 | damping ratio against the world |
| `props.pushout` | 1 | the most a contact pushes out at (m/s) |
| `props.slop` | 0.001 | allowed overlap (m) |
| `props.cold` | 8 | extra iterations for contacts with nothing to warm start from |
| `props.shock` | 0.7 | mass scaling on resting contacts within this cosine of vertical (0 off) |
| `props.sleep_time`, `props.sleep_linear`, `props.sleep_angular` | 0.2, 0.06, 0.12 | sleeping |
| `pad.hertz`, `pad.damping`, `pad.limit` | 2.5, 0.25, 1.5 | the mattress hinges |
| `joints.tear` | 0.5 | how far a jointed prop is pulled from its joint before the joint gives (m) |
| `props.sample_spacing` | 0.12 | spacing of the world contact samples |
| `props.ccd_probes` | 128 | the most points a fast body is swept against the world by (0: every 2.5 cm along every edge) |
| `props.ccd_budget` | 4096 | about the most world sweeps continuous collision asks in a step (0: no bound) |
| `props.player_hertz` | 5 | the player body's contact stiffness |
| `props.player_mass` | 20 | the player body's mass (kg) |
| `props.pad_hertz` | 4 | padded props' contact stiffness |
| `water.right` | 80 | how strongly a floater rights itself |
| `water.lever` | 0.35 | how far the lift is drawn toward the body's middle |
| `water.hollow` | 0.5 | how much of a solid block's drop to the middle of what is under counts against a face (which faces a floater rides on) |
| `water.turn_drag` | 1 | the drag coefficient on a wet cell's speed from the turn (quadratic) |

## Phase 3

### Physgloves (src/game/Physgloves.cpp; tube `physgloves`)

- **Aim.** Desktop: the look. VR: each hand's pointing. The target is the first prop the aim's ray meets in its
  true shape and turn (`rayCastProp`, PropQueries.h); a pinned or held prop in front blocks it. Missing everything,
  a 0.2 m `sphereCastProp` along the same line finds the nearest loose prop (aim assist). Within 7 m, the world
  clear between the eye (and the VR hand) and where the cast met it. It is drawn a little brighter, pulsing (Scene3D `drawClutter`), and never from the resting cache.
- **Flick.** Desktop: `G`, or hold the middle mouse button empty-handed (the look stays on the target) and flick
  the mouse back. VR: close the hand while pointing (it locks on), then snap the wrist back toward the face or up
  (1.3 m/s).
- **The arc.** The velocity that takes the prop's middle to the catch point (before the eye at carry reach, or in
  the VR hand) in 0.42 to 0.95 s by distance, under the props' own 14 m/s^2. Up to 6 kg it flies all the way;
  heavier, the speed is cut by 6 / mass, so a 9 kg crate hops part way and a 35 kg drum lurches. Something lying
  in an open container is first popped straight up over its rim, then sent. A prop on a joint is tugged: the joint
  holds it, so the hung crate swings out toward the hand on its rope; it is never caught.
- **The catch.** Inside the flight time plus 0.3 s: desktop, its middle within 0.55 m of the hold point; VR,
  within 0.45 m of a closed hand. Otherwise it goes on as it was going (turned away, walked off, hand open).
  It does not strike the one who pulled it.

### Two-hand VR carry and hands as bodies (TwoHand.cpp, VrHands.cpp; tube `vr-hands`)

- A large held prop (0.45 m across, or 4 kg and up) is taken by the free hand closing within 12 cm of its shape.
  Each hand keeps its grip point in the prop's frame; the prop turns by the turn that carries the line between the
  hands at the grab onto the line now (one hand up: it tilts), then about that line by the roll of the hands' mean
  pointing (both wrists forward: it rolls), and its grips' midpoint stays at the hands' midpoint. A pose that does
  not fit the world is not taken (it stops, it is not dropped). Letting go with one hand leaves it in the other;
  with both, it is thrown at the mean of the two hands' velocities.
- A tracked hand holding nothing is a kinematic 4.5 cm sphere in the solver, its velocity toward the tracked hand
  capped at 2.5 m/s, so a swat moves a light thing no faster than that. Props of 12 kg and up, and pinned ones, are
  left out of its contacts. After each step the drawn hand (the proxy, `Game::vrHandOffset`) goes on toward the
  tracked one 2 cm at a time and is set out of every prop along its nearest face: it stops at a drum's surface and
  never passes into or through a prop. The old overlap knock (`knockClutter` from a hand inside a box) runs under
  legacy only.

### Authored gibs (tools/library/make_gibs.py, src/world/GibSets.h; tube `gibs`)

- Pieces are made in Blender, headless, from the prop's own mesh by a recipe per prop: the pack mesh's own loose
  parts grouped by where they lie (a chair's legs, seat and back), the mesh cut with planes (bmesh bisect, the cut
  capped with an inner material: a crate's lid, floor and walls), and random plane fractures for brittle things (a
  bottle's body, a screen). Each piece is a library model (OBJ, a json sidecar with its shape, mass share, offset
  and size as fractions of the prop) registered as any library model; `make_gibs.py --table` writes GibSets.h and
  the pieces' masses (by bounding volume) and materials into prop_physics.json.
- `Game::breakClutter` puts each piece where it sat in the prop, turned as the prop was, at the prop's velocity at
  that place (its linear velocity plus its spin crossed with the piece's offset: the blow that broke it is already
  in both, the solver's or the round's knock), spinning as the prop spun, plus a throw out from its middle and
  1.2 m/s along the blow (the direction a caller passes, held to a unit: a prop broken by its speed passes its
  velocity). A 50 N s blow on a 9 kg crate's top edge sends its planks on at 6.9 m/s along it, the top ones 8.6 and
  the bottom 5.4; a crate spun at 8 rad/s comes apart with its sides going opposite ways (tube `vectra-gaps`,
  breaking). Each is an ordinary model prop: a Vectra body (its mesh's hull for a piece clearly not a box, so a shard lands on its
  broken face; a capsule on a disc foot for a bottle neck; else its box), saved with the
  props, carried and pulled like any. Glass pieces shatter further by the usual break speed. A prop with no set is
  sliced into eighths as before, and the slices too go on at the velocity their places had (they popped out in
  place, the blow's way their only motion).
- Sets: wooden_chair (four legs, seat, back), scrap_chair (frame, seat, back), wooden_crate (lid, floor, four
  side planks, two ends), packing_crate (lid, floor, two sides, two ends), glass_bottle (neck, four shards),
  crt_monitor (stand, casing, a screen cracked in three).

### Liquid in bottles (Liquids.h, Liquids.cpp; tube `liquids`)

- A vessel (glass bottle, specimen jar, pill bottle) holds a level of water, oil or a green chemical (glowing).
  Authored with a CLUTTER record's `p.liquid=water|oil|chemical` and `p.fill=0..1`.
- The surface is a plane in the bottle's frame. Its normal is a damped spring (water 2.4 Hz, damping 0.07; oil
  1.5 Hz, 0.35) toward the effective gravity (gravity less the bottle's acceleration, from its middle's own path);
  the bottle's turn carries the old surface with it by the liquid's thickness. Its offset is found from the volume
  (the inner cylinder, 0.85 of the radius, mouth at the top). It is drawn as the inner cylinder cut by that plane
  inside the bottle's glass, which is drawn clear while it holds liquid.
- Past the mouth it pours at sqrt(2 g h) by the head over the mouth, times the kind's run (oil 0.32). A round
  holes a bottle that holds liquid (its first round; the next breaks it) and it leaks from the hole down to it.
  Broken, it spills all it held. Drops fly; on standing water they splash into it (`raiseWaterSplash`), on a floor
  they pool (`Game::puddles`, merged by kind, drawn as films). The baked wetness mask is not touched.
- Its mass is in `Clutter::mass` (a full glass bottle weighs 1.6 kg, empty 0.5), and a bottle holding liquid
  displaces its outer shape in water: an empty one floats, a full one sinks.
- Saves keep each bottle's kind, level, surface and hole (the `liquids` section, save-format.md). Drops and
  puddles are not saved.

### The Starter Map

SHOWCASE: PHYSICS has a phase-3 corner along its south wall (make_starter_map.py `phase3_corner`): a high shelf of
things to pull, a crate hung on a rope, mugs in an open crate, a table of breakables with authored gibs, bottles of
water, oil and chemical on a bench, and a crate for two-hand carrying. Fixture `physgloves-starter`.
