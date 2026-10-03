# v0.7.0-preview.3 — Controller interaction and depot rail equipment

VR doors and terminals now use the gripping controller's reach and direction instead of requiring the player's head to face them. The existing depot locomotives and suspended inspection chassis have been developed further; no map chunks were added.

- Either hand can touch nearby controls or point at more distant controls and grip. Queries check controller validity, hand reach, head reach and wall visibility.
- A new grip on the second hand can operate a control while the first grip remains held. Nearby physics objects retain pickup priority.
- Wrist prompts describe grip and release actions.
- Locomotives have round steel wheels, axles, hubs, suspension blocks, engine louvers and a painted stripe.
- The suspended inspection chassis uses open longitudinal beams and crossmembers, replacing its solid slab. The under-pit view reveals the actual frame.
- Authored cylindrical equipment can select panel-steel material through the shared renderer. Adds repeatable track-height inspection views.
- Fixes collinear UV coordinates on cylinder caps throughout the shared renderer. Caps now have continuous planar textures and valid tangent frames for normal/gloss shading.
- Reuses a precomputed cylinder ring instead of evaluating trigonometry for every cylinder, preserving the same ten-sided geometry.
- Adds suspended inspection lighting beside the locomotive and open chassis.

Offline validation covers controller terminal/door activation while looking away, second-hand grip edges, invalid tracking, pointing rejection, elevation and wall occlusion, along with existing VR math/hands/melee, freight traversal, physics and PC Vulkan checks. Renders were inspected and compared at matching cameras. The headset is disconnected; live controller feel and comfort remain playtest items.

This is another focused development pass, not completion of every item in the freight-area brief. The executable remains below the 25,000,000-byte limit; embedded assets are verified lossless during the build.

Development-PC measurements: RTX 5060 Ti, native 1920x1080 Vulkan, linear HDR, 4x MSAA, 30 warmup and 120 measured frames per scene. The final depot trackside/pit/gantry scenes measured 116.9 / 127.2 / 131.8 FPS, with zero static-map rebuilds during motion. Repeated preview-2 baseline runs varied substantially (gantries 124.0–145.2 FPS), so these samples do not establish an overall speed gain or headset performance. Matching-camera pit images changed 47.85% (under-chassis) and 41.14% (workshop); these are image-difference measurements, not quality scores. Executable: 22,082,560 bytes.
