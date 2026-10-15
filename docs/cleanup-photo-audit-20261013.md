# Cleanup photo audit — 13 October 2026

This is the pre-correction audit of `5d28f55`. The subsequent
[visual correction](cleanup-animation-visual-fix-20261013.md) includes fresh
evidence and verification of the corrective build.

**Visual acceptance: FAIL.** The current photos do not justify describing these
animations as visually good. The earlier acceptance was too generous and
confused numerical consistency with convincing deformation and attachment.

Reviewed all six role sheets and closer crops of the current 32-sample native
captures, timestamped 13 October around 12:33. Source captures are under
the repair captures and `docs/images/`. Crops preserve
aspect ratio and use original pixels without exposure correction or generated
detail. These are staged pose inspections, not proof of live AI behavior.

| Severity | Photo evidence | Visual finding |
|---|---|---|
| P0 | `rusher-aim-up-side` | Skin/clothing below the raised arm stretches into large triangular sheets. The shoulder/arm transition is visibly broken. |
| P0 | `medic-support-front`, `medic-support-side` | The case crosses the abdomen; white case/lid planes protrude behind the torso. Attaching one mathematical corner to the palm did not establish clearance for the rest of the prop. |
| P1 | `assault-aim-down-side`; similar rifle poses | The stock sits above/behind the shoulder as the torso bends forward. The hand and barrel coordinates can match while the gun is poorly seated on the body. |
| P1 | `engineer-aim-down-side`, rifle down-aim views | Downward aim produces a pronounced whole-upper-body hinge and hunched head/neck silhouette. The intended slight torso articulation does not read as natural weapon aiming. |
| QA gap | Engineer terminal views | The display is readable, but the photos do not establish convincing finger contact or an authored operating gesture. The staged support fixture also uses a walking leg pose. |
| QA gap | All sheets | Small, dark views conceal fingers and shoulder seams. Static photos cannot establish foot planting, stride synchronization or smooth state transitions. |

The Rusher deformation and Medic prop intersection are directly visible. The
shoulder/armpit pinching in other roles needs a bind-pose comparison before
attributing every dark gap to a new deformation bug. No claim of foot floating
or broken authored fingers is established by these stills.

## Why the passing checks missed this

The checks establish preserved selected hand edges, finite geometry, bounded
waist deformation, loop closure, reachable wrist targets and matching barrel
coordinates. They do not reject stretched arm/torso triangles outside the
protected hand mask, test stock-to-shoulder seating, test whole-prop/body
penetration, or evaluate believable motion.

The implementation locations worth investigating are the final weighted arm
attachment in `Scene3D.cpp`, the baked/live ownership of shoulder and sleeve
vertices, the torso aim multiplier in `Mesh.cpp`, and the whole-case placement
around the carrying palm. These are hypotheses from source inspection; the
photos alone do not isolate which stage first creates each deformation.

## Repair priority

1. Compare bind, baked, composed and finally attached geometry to isolate the
   Rusher's stretched triangles; repair arm ownership/weight transitions.
2. Fit the entire gun to both the shoulder and hands while retaining a shared
   physical barrel. Establish case clearance and a visible carrying grasp.
3. Reduce the excessive torso contribution and coordinate shoulders, arms and
   head for believable aiming.
4. Review neutral-lit close-ups plus continuous in-place and moving sequences,
   including gait wrap, aim changes, reload and support transitions.

Increasing sample count improves donor tracking but cannot repair these
binding, prop-clearance and pose-design failures. The original repaired OBJ
and finger geometry must remain protected throughout the next repair.
