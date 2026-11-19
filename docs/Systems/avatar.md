# The VR avatar

In VR the player is Claire, all of her, as in VRChat. There are no floating hands.

- **Upper body.** The headset places her head. Her arms reach the controllers through two-bone IK: shoulder, elbow and wrist. Her torso turns part of the way toward the head and leans forward when the real head is lower than standing without a crouch. Her hips trail the head's facing.
- **Fingers.** Each finger closes as far as SteamVR's skeletal summary says the player's finger is closed.
- **Legs.** The legs are automatic. They play the same locomotion clips as the desktop body (walk, run, strafe, back-pedal, kneel, crouch-walk, crawl, falling), chosen from the stick movement against the hips' facing. Ducking for real crouches her.
- **Holsters.** The shotgun rides across her back and the handgun at the gun hand's hip. Grip the gun hand over a shoulder to draw the shotgun, or at the hip to draw the handgun. A drawn gun stays in the hand when you let go. Let go at its own holster, after moving the hand away from it, to put it back. A grip used on a holster does not also grab or use anything.
- **Held guns.** A held gun sits in her hand, with the fingers closed on the grip and the index finger as far onto the trigger as the player's is. Its barrel runs `VrRuntime::GunPitch` (14 degrees) above the knuckles, which is the grip's rake (`VrRuntime::gunFrame`).
- **Support hand.** On the shotgun, the support hand lies on the fore-end (`VrRuntime::supportPose`) and its arm reaches it. The palm is under the wood and the knuckles point away from the elbow, so the wrist is under the wood. The real hand's roll turns it by at most 35 degrees. On the pistol, the support hand wraps the gun hand (`VrRuntime::pistolSupportPose`).
- **Belt pouch.** The pouch is on the support side of the belt (`AvatarSolve::pouchSlot`, `avatarPouchAt`). The VR hand reloads take magazines and shells from it (`src/game/VrGuns.cpp`).

## Code

| Piece | File |
| --- | --- |
| `AvatarPose`, `solveAvatar`, `avatarSlotAt`, `testAvatar` | `src/game/Avatar.h`, `Avatar.cpp` |
| The pose made from tracking and the game; the holsters, hips' lag and reach | `VrRuntime::avatar`, `VrRuntime::update` (`src/vr/VrRuntime.cpp`) |
| The body (legs and torso) | the first-person body pass in `Scene3D.cpp` (`vrBody`), `ClaireOutside::compose` for the lean and turn |
| The arms, fingers, gun in hand, holstered guns, cards, vignette | `SoftwareRenderer::poseVrArms`, `drawTrackedHands` (`Scene3D.cpp`), `Mesh::poseTracked` |

## The pose state, for a later network layer

Multiplayer through Epic Online Services is planned (custom deathmatch maps and co-op), and other players will see each other's avatars. The avatar is built so that this needs no rework.

- **One small struct.** Everything the body is posed from is in `AvatarPose`. That is the head (position, yaw and pitch), the hips' yaw, the feet, the eye height, the stance flags, the locomotion velocity and gait phase, and per hand the wrist, two direction vectors and five finger curls. It also holds the gun hand's side, what is drawn and what rides on the back and hip (each `WeaponId + 1`), the support grip, the pump's travel and the reload's progress. All of it is plain floats and bytes, a few hundred bytes, so it can be quantised and sent as it is.
- **A pure solve.** `solveAvatar(pose, time)` reads nothing but its arguments: no input, no clock, no globals and no per-frame state. The renderer's IK (`poseVrArms`, `Mesh::poseTracked`) and leg clip are deterministic functions of the solve. A remote copy that receives the same poses therefore poses the same body.
- **Smoothing happens where the pose is made, not in the solve.** The hips lagging the head is `VrRuntime`'s state, and it arrives in the pose as `bodyYaw`. A remote player's lag is already in the poses it sends.
- **Positions are level-local.** They are chunk-relative game space, as the game keeps the player. A network layer would send the level with them.
- **The local player's hands.** The game uses the avatar's hands, not the controllers: a wrist past her reach is pulled in to it. What a hand grabs, pushes or shoots from is therefore where her hand is drawn.

## The mirror trap

SteamVR's tracking space is right-handed. Game space is its mirror image (`rotateTracking` has determinant -1), and the arm rig (`claire-arms.fbx`: x her left, y up, z ahead) is right-handed again. Hand frames are built in tracking space and mapped into game space for the pose. `AvatarSolve::rig` maps them into the rig, which is a second reflection, so the rig receives proper rotations. `testAvatar` checks that chain numerically, and `testSupport` checks the fore-end snap's handedness through it.

## Checks without a headset

`RawMetal.exe --vr-test` (or the lab's `engine.vr`) writes `vr-test.txt`. It covers:

- the solve's frames, reach clamp, purity, legs, lean and twist, holsters and finger curls;
- the mirror chain;
- the arms posed on four scripted poses: wrist distance, finger and palm direction, elbows out and down, palm curl sign, and tracked fingers closing;
- the fore-end snap through a full roll and a ±50 degree swing;
- the guns in scripted hands (`VrRuntime::script`, `testGuns`): sights, bore and trace, the holster pulse, two hands on the pistol;
- the hand reloads, pump, recoil and haptic pulses (`Game::testVrGuns`).

It also writes `vr-avatar-arms.obj`, the four posed arm sets, for a look in any OBJ viewer.

What still needs the headset is in `vr-playtest.md`.
