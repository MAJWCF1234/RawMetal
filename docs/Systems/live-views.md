# Live views: cameras, monitors, mirrors, lenses, portals

One system for every surface that shows a picture of somewhere else: a security-camera monitor, a mirror, a scope lens, a portal.
Status: design (2026-11-24). Nothing here is built yet; phase 0 below decides what is cheap and what is not.

## The idea in one paragraph
A **view** is a camera that renders the world into a small texture. A **view surface** is a prop face that shows a view through a
shader. Everything else is a preset: a monitor is a surface with a CRT shader on a fixed camera; a mirror is a surface whose camera is
the player's, mirrored across the surface's plane; a portal is the same with the camera carried through a paired surface; a scope lens
is a surface fed by a small crop of the frame already drawn (no second render). The game never knows which preset it is: it knows
views, surfaces, and a budget.

## Rules (what keeps it stable)
1. **One path.** Monitors, mirrors, portals and lenses all go through ViewManager (budget, state) -> ViewCapture (GPU) -> ViewMaterial
   (shader). No special case per kind outside the shader and the camera maths.
2. **Named records, not new machinery.** Cameras and surfaces are map-format-2 named records (docs/Systems/map-format.md): scripts reach
   them as `@name.key`, the Builder reads the field table, the audit validates them, saves keep their state in the existing variable store.
3. **A view never shows a live view.** A capture does not draw other live surfaces; a surface seen inside a capture shows its frozen
   last image or its "no signal" frame. No recursion, ever. Mirror-in-mirror is one bounce by construction.
4. **A capture draws the world, not the player's extras.** No HUD, no view model, no flashlight cone of the player, no post-pass
   exposure from the player's eye. Creatures, props, lights, particles and the sky are in.
5. **The budget is a rule, not a hope.** Each frame ViewManager picks which views render: by screen coverage and distance, up to
   `maxCapturesPerFrame`, round robin so nothing starves. A view that did not render keeps its last image (it never goes black).
6. **Everything degrades to a still image.** No GPU, headless test, quality off, budget exhausted, camera destroyed: the surface shows a
   "no signal" or last-frame texture. Never a missing texture, never a crash, never a stall.
7. **Deterministic for tests.** A fixed seed, a fixed refresh and the lab's fixed clock give the same capture every run.
8. **Old maps and saves load unchanged.** New records are optional; absent means no surfaces. Save state goes in a new section with a
   version (docs/Systems/save-format.md "Adding a section").

## Records (map format 2, named fields)
```
VIEWCAM|name=lobby|map=0|x=12.5|y=4|z=2.6|yaw=90|pitch=-15|fov=70|res=160x120|rate=12|on=1|power=@gen.on|range=40
VIEWSURF|name=monitor1|map=0|x=14|y=4|z=1.1|yaw=270|w=0.5|h=0.4|kind=tv|source=lobby|style=crt|noise=0.1|dirt=0.2
VIEWSURF|name=mirror1|map=0|x=3|y=2|z=1.4|yaw=0|w=1.2|h=2.0|kind=mirror|style=dirty|dirt=0.5|toggle=1|on=0
VIEWSURF|name=portalA|map=0|...|kind=portal|pair=portalB|style=ripple
```
- `source` on a TV is a camera name, or a list `a,b,c` the surface cycles through. `@monitor1.source` is a script property, so a
  **computer** (an office-pack terminal, a console prop, a button) switches a monitor with `SCRIPT_ACTION|...|STATE|@monitor1.source=lobby`
  or `ADD|@monitor1.channel=1`. Nothing new is needed on the script side beyond declaring these keys.
- `on` / `power`: a camera or surface needs `on` and its `power` expression true; off is a dark screen (a TV) or a plain dark panel
  (a mirror). A creature near a camera can raise `noise` through a script (the "snow means it is close" signal).
- `res`, `rate`, `range`: the authored cost. The engine clamps them to the quality tier; an author cannot overspend the frame.
- Mirrors and portals need `kind` plus a flat face; the surface's own transform gives the plane. Portals are paired by name.

## Engine layers
| layer | where | job |
|---|---|---|
| ViewManager | Game side (new `src/game/Views.cpp`) | owns cameras, surfaces and their state; picks this frame's captures; saves state; answers scripts |
| ViewCapture | GpuRenderer + SoftwareRenderer | render the scene from a camera into a pooled small target; a software fallback copies the last frame |
| ViewMaterial | shaders | map a capture onto a surface: pinhole (TV), projective screen-space (mirror, portal), magnified crop (lens); each with its post effect |
| Authoring | Builder | a "view surface" prop and a camera prop, source picker, preview, audit checks |

Capture kinds: **cube** (six faces from a point; the probes' existing path; cheap for fixed cameras and shared by both VR eyes) and
**planar** (one render, any projection, with an oblique near plane for mirrors and portals). The lens uses neither: it samples the
previous frame around the aim point.

## Shaders (presets, all one material family)
- `crt`: scanlines, curvature, phosphor bloom, interlace roll, tracking noise, signal snow (`noise`).
- `mirror`: Fresnel, dirt and fog masks, cracks, a frosted rim; always dirty by default, clean is an authored choice.
- `lens`: coating tint, vignette, rim darkening, colour fringe, gloss; magnified crop of the previous frame (lag of one frame, clamped).
- `portal`: edge glow and ripple over the projective sample.
- extras later: night vision, heat shimmer, fisheye, tape dropout. Each is a shader, never a code path.

## Budgets (initial, tune in phase 0)
- Targets pooled by tier: 128x96, 160x120, 256x192, 320x240 (scope capture, if ever, 512). Nothing larger without a map flag.
- Fixed cameras: 10-15 Hz, interlaced half-rate when far. Mirrors: the frame rate, only while on screen and within 12 m.
- Desktop: up to 2 captures a frame. VR: 1 view-dependent capture (a mirror or portal, rendered per eye) plus view-independent feeds.
- Frame-time governor: over budget for 30 frames steps the resolution tier down; recovered for 120 steps it back up.

## Edge cases (each is a test)
| case | rule |
|---|---|
| surface on screen but its camera is in an unloaded chunk | frozen last image, then no signal; never force a chunk resident |
| camera inside a wall, or its view fully blocked | the capture still renders (black); the audit warns on the authored camera |
| two surfaces use one camera | one capture, two materials |
| a camera's own monitor in its view | frozen frame (rule 3) |
| mirror facing a mirror | each shows the other's last frame; no recursion |
| portal pair on different maps / floors | allowed; the destination chunk must be resident or the portal shows no signal |
| player or ragdoll inside a mirror's plane | oblique clip hides it; no popping |
| save and load | `on`, `source`, `channel`, `noise` restore; captures re-render, last images are not saved |
| multiplayer | the world is already replicated, so each client renders its own captures; `source`/`on` are script state and replicate as variables do. Nothing is streamed as video. |
| VR | feeds and lenses shared across eyes; mirrors and portals per eye, capped by the VR budget |
| headless / lab / no GPU | a still image per surface; tests read the still |
| quality setting "off" | every surface shows its still image |
| flashlight, particles, post effects | a capture uses the room's lights; no player flashlight; particles included; the post pass is a light version |
| creature seen on a monitor | visible, subject to the camera's range and the room's light; a darkness-sensitive creature stays dark |
| surface destroyed (glass breaks) | it becomes plain glass; the view stops |
| 3D prop scaling or squash | the surface stays rectangular in its own space; audit checks aspect |

## Phases
0. **Spike (read first, build little).** Can the scene pass take a different camera into a small target without touching the player's
   view state? What per-frame state (exposure, TAA jitter, culling, LOD, impostors, shadows, probes) is tied to the player's eye? What
   does the VR path assume? Output: one page of answers and a measured cost of one extra small capture. Decides everything below.
1. **Core, monitors, cameras, switching.** ViewManager, ViewCapture (cube or planar by the spike), CRT material, the records, the audit,
   the Builder props (camera, monitor, computer that cycles channels), the save section, the lab tube, tests. Starter Map: a security
   room with three cameras, two monitors, one computer that switches, one camera that snows when a creature is near.
2. **Mirrors.** Planar capture with the oblique clip, the dirty/cracked/fogged presets, the toggle, the VR cap.
3. **Scope lens.** The previous-frame crop and the lens shader; the VR eye path; desktop keeps the real FOV zoom for the full scope.
4. **Portals.** Pairs, cross-map destinations, ripple and edge shader; VR cap.
5. **Polish.** Night vision, heat shimmer, tape dropout, quality tiers, Builder preview, docs, a showcase in the Starter Map.

## Tests (every phase adds to these)
- records: parse, export round-trip, refusal of bad kinds and unknown sources, FORMAT|2 required, old maps unchanged.
- state: scripts switch `source`/`on`; saves restore; cycling wraps; power loss darkens.
- budget: more surfaces than the cap renders only the cap, round robin, nothing starves, resolution steps down and recovers.
- degrade: no GPU gives a still image; an unloaded chunk gives no signal; a destroyed surface stops its view.
- render (lab tubes, photos judged by eye): a monitor showing a lit room, a monitor with snow, a dirty mirror with a creature in it, a
  lens at the hip, a portal pair. Each tube has a repeat-hash check.
- multiplayer: two peers with one switching a monitor; the other sees the new source within a tick.

## Open questions (answered by phase 0)
- Does the scene pass take another camera, and what does it cost per capture?
- Is previous-frame history available per eye in VR?
- Do probes' cube captures need a higher face resolution for TVs, and can they be recaptured live under the existing scheduler?
