# VR playtest checklist

Use this list in the headset (Quest through SteamVR) to check what the automated tests cannot see. Everything under `--vr-test` was checked without a headset (see the last section). Nothing below has been tried on a headset yet.

**Before you start:**

- Start the game in VR (`RawMetal.exe --vr`).
- Stand up straight when the game starts: your standing height is set from that moment.
- Turn on **Settings → VR COMFORT... → FRAME RATE READOUT**. It shows frames per second, the display rate, the game's GPU time and the render scale under the subtitles.

## 1. Your body

- [ ] Look down. You see Claire's chest, belt, legs and boots, and no floating hands. Her arms run from her shoulders to where your hands are.
- [ ] Move your hands around: out in front, out to the sides, above your head, behind your back. Her elbows point out and down and never pass through her chest. Her wrists don't twist in a way your real wrists can't.
- [ ] Reach as far as you can. Her hand stops at the end of her arm instead of floating away from it.
- [ ] Open and close each finger (Index controllers), or squeeze the grip and trigger (Touch). Her fingers follow.
- [ ] Turn your head about 45 degrees left and right. Her shoulders follow partway; past that, her hips turn too.
- [ ] Bend forward without crouching. Her torso leans.
- [ ] Duck for real. She kneels. Stand up again and she stands.
- [ ] Walk with the stick forward, back and to the sides. Her legs walk, back-pedal and side-step in time with the footsteps.
- [ ] Look at her shadow under a ceiling lamp. It is her whole body and moves with you.

## 2. Guns on your body

- [ ] Reach over either shoulder with your gun hand and squeeze the grip. The shotgun comes off her back into the hand.
- [ ] Reach to the gun-side hip and squeeze. The handgun comes out (once you own it). While it isn't drawn, you can see it on her hip.
- [ ] Let go of the grip anywhere else. The gun stays in her hand.
- [ ] Move the gun away, bring it back to its holster and let go there. It goes back.
- [ ] Shotgun: put your other hand on the fore-end and squeeze. That hand wraps the wood and stays on it as you aim.
  - [ ] Roll that wrist round the barrel. The drawn hand rolls with it and never flips.
  - [ ] Fire, then pull the fore-end back about 7 cm and push it forward. The wood slides with your hand and a shell comes out.
- [ ] Press reload (right B). The muzzle tips up and the support hand goes to her hip and up to the gun (three times for the shotgun).

## 3. Hands in the world

- [ ] Grab a crate in one hand, then close the other hand on its far side. Tilt it and roll it with both hands.
- [ ] Let go with both hands in one motion. It flies at your hands' speed and doesn't jump to one hand first.
- [ ] Let go with one hand only. After a moment the other hand holds it alone.
- [ ] Do both grabs crouched, and with your back against a wall.
- [ ] Hold a crate in one hand and turn your wrist slowly: palm up, palm down, tip it forward and back, swing it left and right. The crate turns exactly as your hand does, the same way round, with no flip, mirror or snap. It does not jump to point where your hand points when you grab it.
- [ ] With both hands on it, roll both wrists the same way. The crate rolls with them.
- [ ] Squeeze the grip on a prop, then open your hand. It drops at once. Try it with a light, half squeeze too (Index: a soft squeeze).
- [ ] Holding a prop, pull the trigger of the hand that holds it. It is punted along that hand's pointing: a bottle flies, a heavy crate is shoved. With no gun drawn, the gun hand's trigger does the same. With a gun drawn, its trigger fires and the prop stays in your other hand.
- [ ] Break a full bottle, walk into the next area and look around. The spill is not there; go back and it is where it fell.
- [ ] Push a light box with an open hand. It moves; her hand doesn't pass into it. Press into a heavy drum: her hand stops at its surface.
- [ ] Try to grab something bolted down. It doesn't come loose.
- [ ] Physgloves:
  - [ ] Point an open hand at a loose prop up to 7 m away (it glows), close the hand and flick back. It flies to your hand and you catch it.
  - [ ] Try it with the hand holding a drawn gun, and with the hand on the fore-end while pumping. Neither should pull anything.

## 4. Comfort settings (Settings → VR COMFORT...)

- [ ] **TURN SPEED** changes how fast the right stick turns you. Turning is smooth only.
- [ ] **COMFORT VIGNETTE**: the edges of the view darken while you move or turn, more as the value rises. OFF removes it.
- [ ] **SEATED MODE**: sit down, turn it on, then choose RECALIBRATE HEIGHT. Your view is at Claire's standing height and leaning forward doesn't crouch her (the crouch button still does).
- [ ] **RECALIBRATE HEIGHT**: stand straight and choose it. Your eye is set back to her standing eye.
- [ ] **LEFT-HANDED**: the gun goes in your left hand, the left stick turns, the right stick moves, the hip holster moves to her left, and the right hand takes the fore-end.
- [ ] **RENDER SCALE**: AUTO lowers the resolution when the GPU falls behind and raises it again; 100/90/80/70 % fix it. Watch the percentage in the readout.

## 5. Subtitles and status

- [ ] Subtitles and area titles sit ahead of you. Glance aside and they stay put; turn further and they drift round after you; turn right round and they jump to where you now look.
- [ ] Health and the gun's load show below the subtitles for a few seconds after either changes, during a reload, and whenever health is low. Nothing is on her hands or wrists.

## 6. Frame rate

Report the readout in these places:

- standing in the Security Office;
- in a large lit room;
- outdoors in the Ashfall zone;
- during a fight.

For each, give FPS / HZ, GPU MS and the scale percentage, with RENDER SCALE on AUTO and on 100 %.

## Reporting a problem

- Hold the pose that shows it, then quit, and start the game with `RawMetal.exe --vr-inspection-test`.
  - It writes `vr-smoke-test.txt`: per-stage timings, the head and controller poses, and the avatar scale, calibrated eye, hips' yaw, handedness and support state.
  - It writes `vr-eye-1.ppm` and the following eye images.
  - Send all of them.
- `RawMetal.exe --vr-hand-dump` writes the hand and weapon meshes as OBJ files.
- `RawMetal.exe --vr-test` needs no headset. It writes `vr-test.txt`, every automated VR check with PASS or FAIL, and `vr-avatar-arms.obj`, Claire's arms posed on four scripted poses.
- Buttons doing the wrong thing: send `%LOCALAPPDATA%/RawMetal/vr/bindings-active.txt`, which lists what SteamVR bound to each action.
- `vr-compositor-errors.txt` beside the game collects SteamVR's compositor errors.
- `--dev` (or the console's `dev`) gives god mode, no creatures and every door open, for testing without a fight.

## Checked without a headset

`--vr-test` (and the lab's `engine.vr`) checks the following:

- **Avatar solve:** frames, reach clamp, purity, legs, lean and twist, holster zones, finger curls.
- **Mirror chain:** a tracking-space frame mapped to game space and then into the arm rig comes out as a proper rotation.
- **Arm IK on scripted poses:** wrist within 2 cm of its target, finger and palm direction, elbows outward, palm curl sign, tracked fingers closing.
- **Fore-end snap:** a full roll and a ±50 degree swing with no flip, the gun aimed through the support fist, and pump travel.
- **Lazy-follow cards.**
- **Dynamic-resolution controller.**
- **Avatar scale.**
- **VR COMFORT menu and its saved settings.**
- **Two-hand release grace, the pinned-prop grab, the Physgloves lock hands, and the hand jump.**

Everything that depends on SteamVR has only been reasoned about, not run:

- real poses, and the skeletal finger data;
- the compositor's timing;
- the texture bounds used by the render scale;
- how the body looks and feels in the headset.
