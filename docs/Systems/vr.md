# SteamVR preview

Double-click RawMetal.exe and select Play in VR, or use `RawMetal.exe --vr`. Use `--pc` to bypass the chooser for desktop play. VR requires Windows 10/11 x64, the Vulkan loader, SteamVR installed in the main Steam library, and a connected headset. The existing C++ runtime requirements still apply.

The VR bridge implements the RawIron tracking/locomotion concepts using SteamVR's OpenVR Vulkan interfaces. RawMetal retains its own Vulkan materials, linear HDR, MSAA and post processing. It does not copy RawIron's OpenXR backend. The SteamVR DLL is loaded from the installed runtime, not bundled beside the game.

You play as Claire, all of her, in the VRChat style (docs/Systems/avatar.md):

- The headset is her head.
- Her arms reach your controllers through IK, and her fingers follow SteamVR's finger tracking.
- Her legs walk with the stick.
- Ducking for real kneels her.

Controls:

- Left stick: smooth movement. Right stick: smooth turn, at the TURN SPEED set in VR COMFORT. There is no snap turn and no teleport.
- Right trigger: fire. Right A: jump. Right B: reload. Left X: crouch toggle. Left Y: pause. Right stick click: inventory.
- **Holsters.** The shotgun rides on her back and the handgun at the gun hand's hip. Grip the gun hand over a shoulder to draw the shotgun, or at the hip to draw the handgun. A drawn gun stays in her hand when you let go. Let go at its own holster, after moving the hand away from it, to put it back.
- **Shotgun support.** Squeeze the other grip on the fore-end to hold the shotgun in both hands. The hand wraps the wood, and the gun is aimed through it. After a shot, pull the fore-end back and forward to chamber the next shell; the wood slides with the hand.
- **Grabbing.** Reach near a physics object and squeeze a grip to hold it; let go to drop it or throw it with your hand's speed. A large prop can be held in both hands: tilt it and roll it with them. Let go with both to throw it with both hands' speed. One hand letting go hands it to the other after a moment.
- **Solid hands.** Empty hands push light props (at most 2.5 m/s) and stop at the surface of heavy ones.
- **Reach.** Her hands are where her arms reach. A controller held beyond her arm's length leaves her hand at the arm's end, and grabbing, pushing and firing happen from there.
- **Physgloves.** Point an open hand at a loose prop up to 7 m away (it glows), close the hand, and flick back toward you; keep the hand closed to catch it. A hand holding a drawn gun, or on the fore-end, does not lock on.
- **Doors and terminals.** Grip beside one to operate it.
- **Punching.** Close an empty fist and swing it into an enemy.
- **Left-handed play** (VR COMFORT) swaps the gun hand, the sticks and the hip holster.

Menus, the inventory and terminals open on a panel 2 m ahead; point either controller at it and pull its trigger. Nothing is attached to her hands or wrists. Area titles, subtitles and a short status line (health and the load, shown for a few seconds after a change, during a reload, or while health is low) hang ahead of you and lazily follow your look.

**VR COMFORT** (Settings, the row that is INVERT MOUSE Y on the desktop):

- turn speed;
- comfort vignette (the edges darken while the sticks move or turn you);
- seated mode;
- recalibrate height (stand straight and choose it);
- left-handed;
- render scale (AUTO, or 100/90/80/70 %);
- a frame-rate readout.

Your standing height is taken from the moment play starts, and your tracking is scaled to Claire's height, so your reach is hers.

**Performance.**

- The second eye replays the first eye's draw lists and shadow maps.
- VR uses 2x MSAA and no post stage.
- With RENDER SCALE on AUTO, each eye is drawn into a share of its image (60-100 % each way) chosen from the compositor's GPU timing, and SteamVR stretches that share.
- The two far sun cascades are redrawn on alternate frames in VR, each looked up with the matrix it was drawn with.
- `--vr-test` checks the controller, and the lab's `engine.vr-bench` times one Quest eye headless at each scale.

The desktop spectator composites the right eye through the existing Vulkan renderer. It does not render the scene a third time or read every frame back to the CPU. Desktop image acquisition is nonblocking to avoid waiting for a monitor refresh before submitting headset images. Capture the Depthworks desktop window for streaming; streaming software was not exercised in the automated tests.

Finger articulation uses SteamVR skeletal summaries when available, with grip/trigger curls as fallback. Optical finger tracking availability and controller calibration depend on the connected hardware. This release is a VR preview; full headset/controller coverage still requires playtesting.

Validation commands: `--vr-test` (no headset; writes vr-test.txt: the avatar's solve, arms and mirror chain, the fore-end snap, comfort maths and menu, hand hardening, finger deformation, physical punches), `--vr-smoke-test` (12 live stereo and desktop frames), `--vr-inspection-test` (240 frames and a diagnostic eye capture), `--physics-ai-test`, and `--smoke-test --vulkan`. Eye readback exists only in the explicit inspection diagnostic.

SteamVR releases its imported Vulkan resources before RawMetal destroys the device, preventing the shutdown crash found during live testing.

Preview 2 neutralizes movement, grip, firing and controller actions when tracking is lost. Recovered headset tracking resets the room-scale movement reference while retaining the original eye-height calibration, so an unobserved displacement is not applied to the player. Controller action state is ignored until that controller has a valid pose. This recovery patch has offline regression coverage; reconnect behavior still needs a headset playtest.

The in-headset checklist is docs/Systems/vr-playtest.md.
