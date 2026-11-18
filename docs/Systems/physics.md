# Vectra physics

Vectra is RawMetal's rigid-body engine. It moves the game's model props (Clutter with a model): crates,
barrels, bottles, chairs, tables, carts, containers and the rest. Junk and shards keep the older point physics.
The prop physics that came before Vectra, a yaw-mostly box with tipping by rule, is still there as **legacy**.
**Vectra is the default**; legacy stays selectable as a fallback (below).

- Core: `src/game/physics/Vectra.h`, `Vectra.cpp` (namespace `retro::vectra`). It has no game code in it.
- Props as bodies: `src/game/physics/PropBodies.h`, `.cpp`. The shape and material catalogue, the switch, the
  Clutter-to-body mapping and the save fields live here.
- Game step: `src/game/RigidClutter.cpp` (`Game::simulateRigidProps`). It covers world contacts, glass, water,
  sounds, body contacts, the held prop and the player's body.
- Tubes: `src/lab/tubes/RigidProps.cpp`. `rigid-ab` compares Vectra and legacy and prints the numbers.
  `rigid` and `rigid-legacy` film the same scenes on each. `vectra-emergence` checks behaviour that comes from
  the solver alone. `rigid-trace` and `rigid-probe` are for debugging.
  `tools/dev/rigid_sheet.py` lays the films side by side.
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
within reach of a sleeping body wakes it: a blow from a moving body is never stopped by a sleeper.
A sleeping prop out of reach of every awake prop and of the player is not built at all.

## Continuous collision

- Speculative contacts look as far ahead as the body moves in a step, for floors, walls and other props.
- A body moving more than half its smallest size in a step is also swept by its middle against the world.
  If the sweep crosses a surface, the body is set back against that surface with its velocity into it removed.
- Glass gets a look-ahead ray along the body's path: a pane it will meet this step is struck with the energy
  of the speed it will arrive at.

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
- **Waking.** A body joined to an awake one wakes with it, through the whole chain. Islands link through joints.
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

`Carcass::stepVectra` (RM_RAGDOLL=vectra, or the knob `carcass.vectra 1`) steps a corpse's sections as Vectra bodies (their
points' mass and inertia, their capsule as a box), its joints as Vectra balls with the rig's limits (an elliptical cone centred
on the bend's range, the side either way, and the twist), the floor and walls against the sections' skin points, and the props
round it as kinematic boxes. It is not the default: on the ragdoll-fall fixtures it still lets limits pass (by up to 55
degrees), lets sections overlap by 2-4 cm and jumps going to sleep, where the position-based step passes six of fourteen.

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
  - **The shield.** A carried prop that does not break is torn out of the hands once it has taken
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

## For phase 3

- **Gravity gloves.** `Space::drive` pulls a body to a pose with a capped spring. It sets velocity, so
  contacts still hold.
