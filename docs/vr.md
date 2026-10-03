# SteamVR preview

Double-click RawMetal.exe and select Play in VR, or use `RawMetal.exe --vr`. Use `--pc` to bypass the chooser for desktop play. VR requires Windows 10/11 x64, the Vulkan loader, SteamVR installed in the main Steam library, and a connected headset. The existing C++ runtime requirements still apply.

The VR bridge implements the RawIron tracking/locomotion concepts using SteamVR's OpenVR Vulkan interfaces. RawMetal retains its own Vulkan materials, linear HDR, MSAA and post processing. It does not copy RawIron's OpenXR backend. The SteamVR DLL is loaded from the installed runtime, not bundled beside the game.

Controls:

- Left stick: move. Right stick: smooth turn.
- Right trigger: fire the shotgun. Right A: reload. Left A (Touch: X): jump.
- Left B (Touch: Y): pause. Right B: inventory. Vive application-menu buttons provide pause/reload; keyboard inventory remains available.
- Reach near a physics object and squeeze either grip. Keep squeezing to hold it in that hand; release to drop or throw with tracked hand velocity. One physics object can be held at a time.
- Grip beside a door or terminal to operate it. Within 65 cm, controls accept direct reach; farther away, point the gripping controller at the control (maximum 1.8 m hand reach). Wall visibility and head reach are checked. A nearby physics object takes priority. Either hand can operate a control, including a new grip with the other hand already closed. Wrist prompts show grip controls.
- Squeeze near either shoulder, behind your head, to stow/equip the shotgun. A held object takes priority. This currently switches shotgun/empty hands, not a collection of firearms.
- Close an empty fist with grip and physically swing into an enemy. Swept contact, speed threshold, wall visibility and per-hand cooldown determine hits. The shotgun hand and an occupied hand do not punch.
- Raise the left wrist computer. Point the right controller at its screen and pull the trigger to click. Inventory and Menu / Save buttons open their respective pages. Click rows and drag sliders with the trigger held. The left stick can also select rows and adjust settings.

VR gameplay has no screen-space HUD. Health, ammunition, contacts, map, logs, inventory and menus use a black screen with green graphics in a steel wrist casing. The casing is 13.9 cm wide, with a 12.1 cm screen. It is actual scene geometry, so it shares depth occlusion and appears in the desktop spectator image. The startup menu, and menu recovery when left-hand tracking is lost, use a room-anchored panel two metres away.

The desktop spectator composites the right eye through the existing Vulkan renderer. It does not render the scene a third time or read every frame back to the CPU. Desktop image acquisition is nonblocking to avoid waiting for a monitor refresh before submitting headset images. Capture the Depthworks desktop window for streaming; streaming software was not exercised in the automated tests.

Finger articulation uses SteamVR skeletal summaries when available, with grip/trigger curls as fallback. Optical finger tracking availability and controller calibration depend on the connected hardware. This release is a VR preview; full headset/controller coverage and motion comfort still require playtesting.

Validation commands: `--vr-test` (coordinates, wrist hit mapping, shoulder slot, wrist-cut meshes, finger deformation, physical punches), `--vr-smoke-test` (12 live stereo and desktop frames), `--vr-inspection-test` (240 frames and a diagnostic eye capture), `--physics-ai-test`, and `--smoke-test --vulkan`. Eye readback exists only in the explicit inspection diagnostic.

SteamVR releases its imported Vulkan resources before RawMetal destroys the device, preventing the shutdown crash found during live testing.

Preview 2 neutralizes movement, grip, firing and controller actions when tracking is lost. Recovered headset tracking resets the room-scale movement reference while retaining the original eye-height calibration, so an unobserved displacement is not applied to the player. Controller action state is ignored until that controller has a valid pose. This recovery patch has offline regression coverage; reconnect behavior still needs a headset playtest.
