# v0.7.0-preview.1 — SteamVR controls and wrist computer

Adds PC/VR launch selection and native SteamVR Vulkan stereo rendering while retaining the game's HDR/material pipeline. This is a VR preview, not a claim of finished headset coverage.

- Replaces VR screen-space HUD with a black/green terminal in a 13.9 cm steel wrist computer; right-controller pointing, clicks and slider dragging navigate the screen.
- Corrects the backward shotgun orientation in VR.
- Adds either-hand grip pickup, grip-release drop/throw, shoulder shotgun stow/equip, and gripped physical fist strikes with swept collision and wall checks.
- Adds a desktop right-eye spectator for streaming, including the physical wrist screen.
- Fixes SteamVR/Vulkan shutdown ordering.
- Raises the executable limit to 25,000,000 bytes. All 166 packaged assets remain lossless.

See [VR controls and requirements](docs/vr.md). PC presentation and controls retain their existing behavior.
Validation on the development PC: VR control/melee regressions, physics/AI, PC Vulkan smoke, and live SteamVR stereo/desktop presentation passed. The Windows 10/11 binary/manifest audit passed; this is not a Windows 10 runtime certification. Live rendering used the RTX 5060 Ti, linear HDR and 4x MSAA at 1596x1708 per eye. The inspection run presented 240 stereo and desktop frames. Hardware finger-summary availability varied between runs, and no universal controller compatibility is claimed.

Executable: 22,072,320 bytes; limit: 25,000,000 bytes. All 166 packaged resources were verified lossless. Streamer software itself was not tested.
