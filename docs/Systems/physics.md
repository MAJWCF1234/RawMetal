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

A shape is up to eight convex parts in the prop's frame: boxes, cylinders and spheres. They come from the
catalogue (the prop's dims, and the mass and container volume in prop_physics.json), with overrides by model:

| Shape | Models |
| --- | --- |
| oriented box | the default |
| cylinder, upright | barrels, drums, burn barrel, gas cylinder, bottle, mug, pill bottle, specimen jar, extinguisher, cell core |
| cylinder along its own axis | tyre (y), straight pipe (x) |
| sphere | pumpkin |
| compound | office and boardroom chairs (a thin star base, column, seat, back); wooden, scrap and platform chairs (four legs, seat, back); tables and benches (top, four legs); wheelchair; desk lamp (an L); a supine body (torso, head, legs) |
| open container | anything with `container` in prop_physics.json: a floor slab up to the inner floor, and four walls up to the rim |
| chained | a soft prop (mattress, bedroll, pad) 0.8 m or longer and under 0.35 m thick: three segment boxes along its length (below) |

Chair backs sit on the side `tools/dev/model_fronts.json` puts behind the seat.

## Collision

- **Prop against prop.** A box and a cylinder (a 12-sided prism with the cylinder's own cross-section area)
  are convex polyhedra. The separating axis test runs over the faces of each and the crosses of their edge
  directions, preferring faces. A face contact clips the incident face to the reference face's sides, which
  gives up to four points (the deepest, then the widest spread). An edge contact uses the closest points of
  the two edges. A sphere meets a box, a cylinder or another sphere exactly.
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
  - A ramp (`ShapeRamp`) collides in the World as 10 cm step proxies. Vectra uses its true slope instead.
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
  Warm starting matches contacts by the props' own indices, not their places in the step's body list.
- Each impulse's turn on a body (its inverse inertia times r x axis) is worked out once a step, and each part's
  hull is built once a step however many pairs it is in.
- **Speculative contacts** close a gap within the step and no more.
- **Impacts.** A manifold first takes an impact at its middle, so a box landing flat does not spin. What it
  reports (glass, breaking) carries the body's mass at that middle, not one corner's share.
- **Friction** is Coulomb, inside a cone. A contact uses static friction if the surfaces were sliding slower
  than 8 cm/s when the step began, dynamic friction otherwise.
- **Rolling and twisting resistance** is a length times the load. A bottle stops sooner than a drum.

## Sleeping

Islands are bodies linked by contacts and joints. An island sleeps when every body in it has stayed under
6 cm/s and 0.12 rad/s for 0.2 s. A sleeping body takes part only as an immovable obstacle. Something moving that comes
within reach of a sleeping body wakes it: a blow from a moving body is never stopped by a sleeper. A body woken that
way wakes what it touches in the same step, and meets it, so it is neither driven into a neighbour unopposed nor stopped
by one still asleep.
A sleeping prop out of reach of every awake prop and of the player is not built at all.

## Continuous collision

- Speculative contacts look as far ahead as the body moves in a step, for floors, walls and other props.
  They stay the first line; the sweeps below catch what passes them.
- A body is **fast** when, in a step, its centre goes or its reach turns (angle times its bounding radius)
  more than half its smallest size (2 cm at least). A plank spinning in place is fast though its middle is
  still. A body a joint holds (a bridge's plank, a corpse's section, a lamp on its chain) is never fast: put
  back alone to where it met something it would open its joints by as far as it went, so it is left to its
  joints and contacts. `RM_CCD=off` turns the whole pass off (to tell what it changes in a check).
  `Space::continuous` runs `continuousCollision` (`Queries.cpp`) on each fast body, in index order:
  - **Against the world.** Points along its edges (corners, every 2.5 cm along each edge, a cylinder's rims
    and side lines) are ray cast by `Space::sweep` through the step, in pieces of at most 0.15 rad of its turn.
    Trailing points and points that hardly move are skipped. A point that strikes a surface counts when it
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
  thin boards and two crates thrown at each other at 25 m/s, a bottle at 30 m/s at a 3 cm wall.

## Queries

`src/game/physics/Queries.h` asks bodies what a ray, a shape or a region meets, in their true pose. No state;
any thread. `PropQueries.h` puts a prop (or a chained pad's three segments) through the same queries.

- **Rays** meet each part exactly: a turned box by its slabs, a cylinder by its round and caps (the true
  cylinder, not the solver's prism), a sphere by its round. A compound meets only its parts.
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

Two materials meeting take the geometric mean of their frictions, the larger restitution and the larger
rolling resistance. Chairs on casters and carts on wheels (office and boardroom chairs, wheelchair, crash cart)
use .3/.25.

Padded props, mattresses and bedrolls (`model::soft`), are 4 Hz springs, so a load sinks in. A padded fixture
gives where `softSinkAt` says it does.

## Joints

`vectra::Joint` (Vectra.h, `src/game/physics/Joints.cpp`) joins two bodies, or a body and the world (`b = -1`). Its rows are solved
in the same substeps as the contacts: before them, and once more after them, `Settings::jointIterations` (4) times each pass.
Locked rows are springs of `jointHertz` (60) with damping ratio `jointDamping` (2), warm started, and relaxed without the bias as
the contacts are. Limits are one-sided rows: a gap closes within the substep, a pass is pushed out softly.

Each body carries the joint's frame: an anchor (from its centre of mass, unturned) and a turn whose z is the joint's axis.
`attach` sets both from the bodies as they stand, so the pose they stand in then is the joint's rest.

| Kind | Holds | Free | Options |
| --- | --- | --- | --- |
| hinge | the anchors together, z of B on z of A | the turn about z | limits, spring, motor (speed or servo) |
| ball | the anchors together | every turn | an elliptical cone (`coneX`, `coneY`), a twist limit |
| slider | the turn, B's anchor on A's z line | travel along z | limits, spring, motor |
| weld | everything | nothing | |
| distance | the anchors `length` apart | | |
| rope | the anchors at most `length` apart (slack when nearer) | | a winch: its motor reels the length in at `motorSpeed` while the pull is under `motorMaxForce` |
| spring | pulled toward `length` (`springHertz`, `springDamping`) | | limits on the length |
| pulley | `lengthA + ratio * lengthB <= length` over `groundA` and `groundB` | | a motor turning the wheel |

- **Motors.** Speed holds a speed with at most `motorMaxForce` (a speed of 0 with a force is a brake). Servo drives to
  `motorTarget` at up to `motorSpeed` (a hinge's angle goes round). A motor at work keeps its bodies awake.
- **Breaking.** `force` and `torque` are what the joint carried over the last step (its impulses over the substep). Past
  `breakForce` or `breakTorque` it snaps once (`Space::jointEvents`) and holds nothing after.
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
- **Water.** Each part is 27 cells, each under the surface by its own share. The lift acts at the middle of
  what is under, drawn toward the body's middle (a prop's mass is in its walls), so a floater rights itself
  onto a face. Drag, swell and drift are as before. A body coming through the surface splashes.
- **Glass.** A pane takes the energy the contact's effective mass brings into it (the look-ahead above for
  a fast throw).
- **Sounds, AI hearing and breaking** come from each contact's approach speed. Breaking uses the speed of the
  prop's middle, which is what the break speeds were tuned on.
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
| `props.sleep_time`, `props.sleep_linear`, `props.sleep_angular` | 0.2, 0.06, 0.12 | sleeping |
| `pad.hertz`, `pad.damping`, `pad.limit` | 2.5, 0.25, 1.5 | the mattress hinges |
| `joints.tear` | 0.5 | how far a jointed prop is pulled from its joint before the joint gives (m) |
| `props.sample_spacing` | 0.12 | spacing of the world contact samples |
| `props.player_hertz` | 5 | the player body's contact stiffness |
| `props.player_mass` | 20 | the player body's mass (kg) |
| `props.pad_hertz` | 4 | padded props' contact stiffness |
| `water.right` | 80 | how strongly a floater rights itself |
| `water.lever` | 0.35 | how far the lift is drawn toward the body's middle |

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
- `Game::breakClutter` puts each piece where it sat in the prop, turned as the prop was, at the prop's speed plus
  a throw out from its middle and along the blow. Each is an ordinary model prop: a Vectra body (a box, or a
  cylinder for a bottle neck), saved with the props, carried and pulled like any. Glass pieces shatter further by
  the usual break speed. A prop with no set is sliced into eighths as before.
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
