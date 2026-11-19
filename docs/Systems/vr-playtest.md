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
- [ ] Own the MAC-11 too: the hip carries the sidearm you are not holding (with neither out, the one drawn last). Draw the MAC-11 from the hip, then put the shotgun in your hand: the MAC-11 is the one on her hip.
- [ ] Let go of the grip anywhere else. The gun stays in her hand.
- [ ] Move the gun away, bring it back to its holster and let go there. It goes back.
- [ ] Each draw and each put-away gives a short buzz in the gun hand.

## 2a. Gunplay (2026-11-19)

### Push-to-talk and bindings version 4

The controller bindings are now version 4 (`*-v4.json` in `%LOCALAPPDATA%/RawMetal/vr`): they add push-to-talk for voice chat. SteamVR picks the new defaults up by their new name. If you had changed the bindings yourself, check them once in the SteamVR binding screen.

| Button | Action |
|---|---|
| Right A | Jump |
| Right B | Magazine out / reload |
| Left X | Crouch toggle |
| Left Y | Menu |
| Right stick click | Inventory |
| **Left stick click, held** | **Push-to-talk** |

- [ ] In a match, press the left stick in and hold it while you speak. The other player hears you while it is held and not after you let go. Walking with the stick does not open the microphone by itself.
- [ ] `bindings-active.txt` now has a `talk:` line naming the left stick (Vive wands: the left trackpad's west click).

Version 3 of the bindings added the two vibration outputs.

### Aim

The gun now points the way a real pistol sits in your hand. The barrel runs 14 degrees above your knuckles (the grip's rake), so pointing the controller like a pistol puts the barrel where you point. The shot now leaves from the muzzle, not from your fist, and it meets the sight line 15 m out.

- [ ] Hold the handgun out as you would a real one. The barrel points where you point and the grip lies along your fist; the bottom of the grip does not poke out of the heel of your hand.
- [ ] Raise it to your eye and line up the rear and front sights on something 5-10 m away. Fire. The hole lands where the sights were.
- [ ] Do the same with the shotgun: the bead on the end of the barrel, the top of the receiver behind it.
- [ ] Do it with **LEFT-HANDED** on. The gun is upright in your left hand. Before this fix it was upside down.
- [ ] If the barrel still points too high or too low, say by how much. The angle is one number (`VrRuntime::GunPitch`).
- [ ] The MAC-11's grip is square to its barrel, so it sits 10 degrees lower in the same hand than the pistol (`VrRuntime::MacPitch`, 4 degrees above the knuckles). Hold it out: the barrel points where you point and the long magazine runs straight down out of the bottom of your fist. Raise it to the eye: the rear sight's notch at the back of the receiver and the front post line up.

### Reloading by hand

Reloading is by hand only. The button no longer starts a timed reload. Ammo comes from the same pools as on the desktop.

- [ ] **Handgun:**
  - [ ] Press B on the gun hand. The magazine drops out and falls to the floor, and you feel a light buzz. If a round was chambered, it stays in and the gun can still fire once.
  - [ ] Put your other hand on her belt, on the side away from the holster, a little ahead of the hip, and squeeze. That hand now holds a magazine.
  - [ ] Bring it under the grip. Once it is lined up with the magazine well, it slides up with your hand. Push it home. It clicks in, both hands buzz, and the counter fills to 12 or what is left.
  - [ ] Fire until it is empty. The slide locks back.
  - [ ] Put in a fresh magazine, then press B again (the slide release), or take the back of the slide with your other hand, pull it back 2 cm and let go. Either one chambers a round.
- [ ] **MAC-11:**
  - [ ] Hold the trigger. It fires at its full-auto rate (18 a second) while held, with a quick, light buzz for each round and a .380 case thrown out of the port on the right of the receiver for each round. The gun climbs a few degrees in your hand during a burst and settles when you let go; the view never moves.
  - [ ] Press B. The long magazine drops out. Take a fresh one from the belt pouch and push it straight up into the grip until it clicks; the counter fills to 32 or what is left of the .380.
  - [ ] Fire it empty. The cocking knob on top of the receiver stays back. After a fresh magazine, take the knob with your other hand, pull it back 2 cm and let go: it chambers. B does not release it (the MAC-11 has no bolt release; B drops the magazine again).
  - [ ] Close your other hand round the gun hand: it wraps the grip as on the pistol, and the kick is about half.
- [ ] **Shotgun:**
  - [ ] With the shotgun drawn, squeeze your other hand at the same belt pouch. It takes one shell.
  - [ ] Push it into the loading port under the gun, just ahead of the trigger guard. It lines up as it gets close and goes in with a click and a buzz. The counter goes up by one.
  - [ ] Let go and squeeze again at the pouch for the next shell. A full tube (6) gives no more.
  - [ ] Load an empty shotgun. It does not fire until you pump it once.
- [ ] The hip and back holsters still draw and put away as before.

### Two hands and the pump

- [ ] Shotgun: put your other hand along the barrel and squeeze. That hand closes round the wood with its palm under it, the fingers up the far side and the thumb along the near side. Its wrist sits under the wood, on the side your elbow is on, not on top of it.
  - [ ] Hold the controller any way you like there: upright, tipped, rolled. The drawn hand stays on the wood. It rolls with yours by up to about 35 degrees and never flips or twists back over the gun.
  - [ ] Aim with both hands. The gun follows the line between them.
  - [ ] Fire. It will not fire again until you pump. Pull your hand back about 7 cm: the wood slides with it, a spent shell flies out of the port on the right, and the support hand buzzes. Push forward: it chambers, with another buzz.
  - [ ] Fire one-handed (support hand off). It cycles itself and throws the shell.
- [ ] Handgun: close your other hand around the gun hand and squeeze. It wraps the grip from the side, and the pistol still aims from the gun hand. The kick is about half.

### Feel

- [ ] Every shot kicks the gun back and lifts the muzzle (handgun about 7 degrees and 1 cm back, shotgun about 10 degrees and 2 cm back, the MAC-11 about 2 degrees a round, half the rise with two hands). It settles back into your hand within a third of a second. The view never moves.
- [ ] Every shot gives a sharp buzz in the gun hand, and in the support hand too if it is on the gun. Softer pulses come on: the magazine out, the pouch, the magazine seating, the slide, the pump back and forward, a shell going in, a dry trigger, the draw and the put-away.
- [ ] With your eyes closed, the shot, the magazine, the slide, the pump and the shell sounds come from the gun or from the hand doing the work, not from your head.
- [ ] If there is no buzz at all, send `bindings-active.txt`. Its `haptic left/right` lines say whether SteamVR bound the vibration outputs. If they were not bound, the game falls back to the old single pulse.

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
- **Fore-end snap:** a Touch controller held upright under the wood snaps palm-under with the wrist below the wood, knuckles away from the elbow. A full roll of the controller is followed within 35 degrees and never jumps more than 4 degrees a step. A ±50 degree swing does not flip. The gun is aimed through the support fist, and the pump travel is checked. The two-handed pistol grip is checked for both hands.
- **Aim (scripted poses through `VrRuntime::script`):** handgun and MAC-11 (both hands) and shotgun. Raised to the eye, the eye looks through both sights onto a target 10 m away. The bore runs 14 degrees above the knuckles (the MAC-11: 4). The handgun's and the MAC-11's magazines lie within 5 degrees of the fist's channel. The gun is upright, left-handed too. The sight line, bore and shot agree within a degree. The fired trace starts at the drawn muzzle and passes within 2.5 cm of the point the sights were on. The recoil lifts the gun but not the view.
- **Gun geometry:** the muzzle, sights, port, magazine well, slide (the MAC-11's cocking knob) and loading port the game uses match the drawn meshes within 4 mm, for all three guns; `vr-gun-mac11-side.ppm` shows the MAC-11's points.
- **Bindings:** the manifest declares `talk` and points at the v4 files; on Touch and Index the left stick's click is push-to-talk and nothing else moved; on a Vive wand it is the left trackpad's west click.
- **Hand reloads (`Game::testVrGuns`):**
  - the magazine drop, pouch, mid-seat and seat, with 12 loaded from the pool;
  - the locked slide, the slide release and the rack by hand;
  - one shell per insert, up to the tube's six, and an empty gun needs a pump;
  - the pump's back stroke throws the case through `Game::ejectCasing` from the ejection port, with the hand's speed, and the forward stroke chambers;
  - the one-handed self-cycle;
  - recoil that settles within 350 ms and leaves the view alone, half with two hands;
  - the haptic pulses on each event (a recorded list);
  - the MAC-11: one .380 case per round through `ejectCasing` (the desktop shot skips it in VR), at the port with the gun hand's speed; 17-19 rounds and as many light pulses in a held second; a burst that rides the gun up but not the view; the magazine drop, pouch, mid-seat and seat with 32 from the .380 pool; the knob locked back on empty, no bolt release on B, and the knob pulled by hand to chamber.
- **Holster draw:** a grip at the hip draws the handgun with a pulse; with the MAC-11 drawn last the hip wears it and the grip draws it, and with it in hand the handgun takes the hip.
- **Pictures:** the lab's `vr-guns` tube draws the VR view from scripted poses: the shotgun two-handed, pumped, a pistol magazine part way in, a shell part way into the port, the MAC-11 in hand and mid-burst, its magazine part way up the grip and its knob pulled, and the MAC-11 on her hip.
- **Lazy-follow cards.**
- **Dynamic-resolution controller.**
- **Avatar scale.**
- **VR COMFORT menu and its saved settings.**
- **Two-hand release grace, the pinned-prop grab, the Physgloves lock hands, and the hand jump.**

Everything that depends on SteamVR has only been reasoned about, not run:

- real poses, and the skeletal finger data;
- whether 14 degrees is the right grip angle for your hand on a Touch controller (and 4 for the MAC-11);
- push-to-talk on the real left stick, and SteamVR taking up the v4 defaults;
- the vibration outputs on the real controllers;
- the compositor's timing;
- the texture bounds used by the render scale;
- how the body looks and feels in the headset.
