# The player's body: Claire

The protagonist has a body. Looking down shows Claire's belt, skirt, legs and
boots stepping in time with the footsteps, and her silhouette throws the floor
shadow under the nearest lamp. Forced third person shows all of her, with the
equipped gun in her hands.

## Model and textures

`O:/retro official/Characters/Claire/Claire.fbx` and its ten PNGs, the user's own
character (rigged, with full fingers). They are copied unchanged into the
gitignored library at `src/assets/library/player/`:

| Resource | File |
| --- | --- |
| 2300 | `Claire.fbx` |
| 2301 | `claire-animation.bin` (16 clips, RMA3) |
| 2302-2311 | `SOCKS.png` (her skin), `BELTS`, `PANTS`, `BOOTS`, `VEST`, `SKIRT`, `JACKET`, `MASK`, `EYES`, `HAIR` |
| 2312 | `claire-animation.bin.anchors` (right and left hand per sample) |
| 111 | `claire-arms.fbx`: her own arms on the first-person rig (below) |

Faces find their texture by FBX material name. The bake maps every finger
joint too (`Index1_R` for Mixamo's `RightHandIndex1`), and carries each wrist
and finger bend in its forearm's frame, so her hands close on the gun. Her rig names its bones its own
way (`Hips`, `Spine1-3`, `LegUp_L`, `Arm_R`...). `RawMetalCreatureBake --takes`
maps Mixamo's names onto them.

## Clips

Rebuild after changing the model or the takes. `P` and `Q` are
`source-assets/mixamo-motion/investigator-direct/pro-rifle-pack` and `.../special`,
`A` is `.../assault-direct/special` and `C` is `.../claire-direct` (her own
Mixamo downloads, see its README). Rebuild the tool first if it is stale:
`cmake --build .build/vs2022 --config Release --target RawMetalCreatureBake`.

```
RawMetalCreatureBake --takes src/assets/library/player/Claire.fbx src/assets/library/player/claire-animation.bin \
 "P/idle.fbx:1:64" "P/run forward.fbx:1:16" "P/run backward.fbx:1:16:loop" "P/run left.fbx:1:16:loop" \
 "P/run right.fbx:1:16:loop" "P/sprint forward.fbx:1:16:loop" "P/idle crouching.fbx:1:64:loop" \
 "P/walk crouching forward.fbx:1:31:loop" "P/walk crouching backward.fbx:1:31:loop" \
 "P/walk crouching left.fbx:1:31:loop" "P/walk crouching right.fbx:1:31:loop" "P/jump loop.fbx:1:30:loop" \
 "P/death from the front.fbx:1:104" "P/walk forward.fbx:1:31:loop" \
 "Q/prone-idle-lying-down-prone-with-rifle.fbx:1:107:loop" "Q/prone-forward-moving-forward-while-in-prone-position.fbx:1:57:loop" \
 "P/walk backward.fbx:1:30:loop" "P/walk left.fbx:1:31:loop" "P/walk right.fbx:1:31:loop" \
 "A/crawl-backwards-in-prone.fbx:1:24:loop" "A/standing-to-crouched.fbx:1:16" "C/crouch to stand with rifle.fbx:1:89" \
 "Q/crouch-to-prone-crouching-to-laying-prone-transition.fbx:1:44" "Q/prone-to-crouch-transition-transition-from-prone-to-crouch.fbx:1:44"
```

Clip order:

| Clip | Use |
| --- | --- |
| 0 | idle |
| 1-4 | run forward, back, left, right (the player's 3.6 m/s) |
| 5 | sprint |
| 6 | crouched idle (a kneel) |
| 7-10 | crouch-walk forward, back, left, right |
| 11 | falling |
| 12 | death |
| 13 | walk forward (under 2 m/s) |
| 14, 15 | prone idle and crawl |
| 16-18 | walk back, left, right (under 2 m/s) |
| 19 | crawl backward |
| 20-23 | transitions: stand to crouch, crouch to stand (first 55 %), crouch to prone, prone to crouch |

A change of stance while standing still plays its transition once (0.45 s
down to a kneel, 0.9 s up, 0.95 s to and from prone). One gait cycle is two
footsteps (`Game::bodyGait`), so her feet land on the step sounds. A change of
clip blends over 0.2 s.

## Stances

Three, as Metal Gear has them (`Player`, `Game::updateMovement`):

| Stance | Hull | Eye | Speed |
| --- | --- | --- | --- |
| Stand | 1.60 m | 1.42 m | 3.6 m/s, 5.4 sprinting |
| Crouch (hold C) | 1.05 m | 0.92 m | 1.65 m/s |
| Prone | 0.58 m | 0.40 m | 0.9 m/s |

- A crouch where a kneel cannot fit is prone, and a kneel walking into such a
  passage drops onto her belly, so every 0.58 m crawlspace works as before.
- Z toggles prone anywhere; a jump or a sprint gets her up.
- Crouching in flight tucks to the prone hull, which keeps the crouch-jump over
  the Intake's 1.1 m dock bumper.
- `crouched` is true for both low stances, so stealth treats them alike; low
  cover hides only what is below it, so behind a desk she needs to be prone.
- The prone flag is not saved: on load a crouch re-derives it from the room.

## Views

- **First person.** Her whole body is under the camera, as the hybrid rigs of
  Battlefield, Call of Duty and Cyberpunk have it: gameplay drives the camera
  and her body follows. Her animated head sits 10 cm behind the eye wherever
  the clip carries it, so her legs fall under the view, and she is scaled so
  her eyes are the camera's. Her head, hair, mask and bare arms are left out
  (the viewmodel's arms are her own, below), as is anything within 25 cm of
  the eye and 18 cm below it, and anything up at the eye when crouched or
  prone. Her shadow is cast by her whole body, head included, from the faces
  turned to the lamp, so it darkens the floor once.
- **Third person.** A console command only, off at every start: `THIRDPERSON`
  (`ON`, `OFF`, or no argument to toggle). The camera sits 2.5 m back
  and 0.5 m over her right shoulder, following the look up and down. It pulls
  in short of anything between her head and the lens, and keeps a 0.4 m box
  clear round itself.
  - The gun sits in her right hand, pointing at her left.
  - Shots still leave her eye, so the crosshair moves to where her aim meets
    something solid.
  - Her death plays out.
- **Never drawn:** in VR (the tracked hands have nothing joining them to a
  body), inside lockers, and in inspection or test cameras
  (`Game::inspectionCamera`, set by `mapInspection`).

## First-person arms

The viewmodel (resource 111) is Claire's own arms, sliced from `Claire.fbx` and
fitted onto the original arm rig (`src/assets/models/arms_rig.fbx`), so every
viewmodel action, the grip IK, the finger grips and the VR hands drive them
unchanged. Rebuild after changing her model:

```
blender -b --python tools/player/build_claire_arms.py -- src/assets/models/arms_rig.fbx  src/assets/library/player/Claire.fbx src/assets/library/player/claire-arms.fbx
```

- Faces kept are those whose every corner is mostly weighted to an arm bone
  (upper arm down to the fingertips); her vest and jacket stay with the torso.
- Each of her bones maps onto its rig bone: along the bone by their length
  ratio, so her elbow, wrist and knuckles land on the rig's joints; across it by
  one arm girth and one palm width, so she keeps her own proportions. The
  index-to-pinky line keeps the twist.
- Only the two shoulder cuts stay open: the renderer continues open rings into
  sleeves, and her armbands and cuffs are open tubes, so the script closes them.
- The rig's skeleton and its 18 actions are re-exported unchanged (the script's
  round trip moves no bone), with the original default pose, the first frame
  of `relax`.
- Her wrist was hard-weighted (one ring of triangles took every turn of the hand)
  and a third slimmer than the rig's. The script blends the hand's influence over
  the forearm's last rings and restores some wrist girth, and the renderer skins
  the arms with dual quaternions (`Mesh(111)`), so the grip's large hand-to-forearm
  twist turns the wrist instead of pinching it to a twisted neck.
- The shotgun has its own hold (`kShotgunHold`, `ShotgunView` in Scene3D.cpp),
  chosen from a sweep of layouts: the gun turned 0.08 rad in toward the
  crosshair and low on the right so the stock clears the frame, the arm rig
  placed so the trigger arm bends at about 57% of its reach with its elbow
  out to the side (`ArmHold::elbow`) and the support arm at 89%, with room for
  the smoke test's sway and recoil. `RAWMETAL_SHOTGUN_VIEW` ("gx gy gz ax ay az
  toe tx ty tz sx sy sz") overrides the layout for tuning without a rebuild.
- Each forearm rolls about its own axis with 90% of its hand's turn
  (`Mesh::grip`), as the radius turns, so no wrist is wrung: the support
  hand's palm-up grip is a half turn from the forearm's natural roll.
- Faces keep her ten materials; her arm skin and gloves are the `BOOTS`
  texture. The viewmodel and VR hands pick her textures by material name, and
  frame the arms about the original rig's centre (`kArmsRigCenter`), not the
  mesh's bounds.

## Inspection

`RawMetal.exe --body-inspection` films her on the most open floor of the
Foundry, shotgun drawn. It renders eight poses (idle, run, back, strafe,
sprint, crouch, crouch-walk, jump) from five views: looking down, behind,
beside, three-quarter, and the live third-person camera. Then her hands on
the gun close up from four sides, and first person looking ahead: idle,
walking, firing, reloading, fists and a jab, then crouched (ahead, low,
looking down, walking), standing and walking looking down, and prone (ahead,
looking down, crawling). Frames go to
`.build/body/`, with a line per frame in `body-inspection.txt`.

`RawMetal.exe --arms-inspection` renders the arms close up: the shotgun idle and
just fired, fists and the pistol, each as played and then the rig from eight
sides at arm's length, into `.build/arms/`.

It also writes `arms-inspection.txt`: for each hand, the arm's reach as a
share of its length, the wrist's twist from the bind pose, the elbow's
direction, and the middle finger's curl toward the computed palm normal
(positive when that hand's palm cross product is the right way round). It
fails if a palm curls away, a wrist twists past 30 degrees or an arm is
past 97% of its reach.
