# v0.7.0-preview.2 — Tracking recovery and freight detail

Fixes VR tracking recovery and develops the existing warehouse and depot maps. No campaign chunks were added.

- Lost headset tracking clears movement, grips, firing, jump/reload and wrist pointer state. Recovery starts room-scale movement from the newly tracked position and preserves standing-height calibration.
- Disconnected or untracked controllers cannot feed stale SteamVR actions into movement, interaction or menus.
- Warehouse shelves now have varied pallet-supported loads, empty stock slots and rear cross-bracing, exposing their depth and construction.
- The inspection-pit workshop has track-height workbenches, parts, storage cabinets and a tool-bay sign beside the pit. Existing jacks, stairs and routes retain their clearance.
- Adds a repeatable tool-bay inspection view.

Validation: freight chunk traversal/control/enemy/lamp checks, campaign extension, VR coordinate/wrist/hand/melee and tracking-loss tests, physics/AI, and PC Vulkan smoke. Same-camera before/after renders were inspected and compared. This pass was developed with the headset disconnected; live recovery, wrist interaction and comfort remain headset playtest items. This is an incremental freight art pass, not completion of every item in the eight-area brief.

The executable remains below the 25,000,000-byte limit. Embedded assets are checked lossless during the build.
