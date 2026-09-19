# RawMetal v0.3.6 — Stalker Arms & Hazmat Casualty

## Changes

- Corrected Stalker shoulder/elbow/wrist retargeting: arms follow the animation's actual limb directions instead of incompatible bone-roll axes. Idle, movement and melee remain articulated; there are no ranged attacks.
- Added the supplied PSX Character_28_HM worker: olive protective suit, sealed gas mask, black gloves and boots (988 triangles). Replaced the rejected bulky yellow radiation worker.
- Authored an asymmetric face-down collapse with an outstretched arm and unevenly folded legs. Blood follows the suit surface; a pool and narrow smears mark the floor.
- Added a 15-joint, fixed-120-Hz ragdoll with floor/wall contact, impact response and sleeping. Oriented suit contact prevents both floor penetration and the hovering caused by oversized spherical ground proxies.
- Save format 4 retains the ragdoll's pose and velocity and continues to read versions 1–3.
- Added reversible texture prediction to lossless asset packing. Original texture resolution and decoded pixels are preserved; packing verifies round trips.

## Verification

The hazmat test checks skin-floor clearance, joint lengths, impact response, settling, 60/120-Hz agreement, save/load and ray contact. Three in-engine views were inspected after settling. The Stalker baker checks arm-direction alignment in every sampled clip; front and side captures cover all five clips.

Vulkan and software smoke tests passed, as did targeted physics/AI, save/load, Stalker and Vulkan checks. All six culling/reference comparisons matched. The executable remains below the 19,800,000-byte limit.

## Performance limitation

The final 1,680-frame Intel Graphics test at 640x360 recorded an awake-ragdoll average of 19.95 ms (50.1 FPS), with a 24.02 ms worst frame (41.6 FPS). However, two reactor-balcony frames exceeded 50 ms, peaking at 81.89 ms. The strict whole-game minimum-20-FPS test therefore failed; this release does not claim a universal minimum. Startup and OS presentation are excluded from these timings.

## Download

Extract RawMetal.zip and run RawMetal.exe. Use the backtick console and map lift for the casualty, or map reactor for the Stalker. Existing version-4 saves retain their saved corpse pose; load the map fresh to see the new authored collapse.

The EXE is signed with the existing self-signed RawMetal Development Build certificate. Self-signing does not establish public publisher trust, SmartScreen reputation, or antivirus clearance. The EXE inside the ZIP matches the separately uploaded executable.
