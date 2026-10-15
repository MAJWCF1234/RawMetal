# Cleanup motion sampling — 13 October 2026

**Historical visual acceptance failed.** The subsequent
[photo audit](cleanup-photo-audit-20261013.md) found stretched arm/torso geometry,
Medic case penetration and poor gun/shoulder seating. The numerical results
below do not establish that the rendered animation looks good.

The later [visual correction](cleanup-animation-visual-fix-20261013.md)
documents the repaired binding/attachment faults and fresh validation.

The current cleanup bake stores **32 samples per clip**, with runtime
interpolation. The initial repair stored 16. Sampling follows the donor cycle;
it does not blindly discard alternating frames. Death retains sampling based
on donor angular motion so its rapid collapse remains smooth.

Both densities were compared against exact retargeted donor poses at 16
between-key phases. Measurements use referenced geometry only. Across the
six walking roles, mean pose RMS error falls from **3.881 mm to 1.420 mm**.
Mean foot RMS error falls from **6.800 mm to 2.184 mm**. Idle and firing
are already close to the 1 mm position-quantization floor.

| Role | 16-sample foot RMS | 32-sample foot RMS |
|---|---:|---:|
| Assault | 6.75 mm | 2.34 mm |
| Engineer | 6.53 mm | 1.81 mm |
| Sharpshooter | 7.14 mm | 2.20 mm |
| Heavy | 7.01 mm | 2.44 mm |
| Medic | 7.10 mm | 2.26 mm |
| Rusher | 6.28 mm | 2.06 mm |

These measurements establish closer donor tracking, not a perceptual score.
The waist/spine split, semantic hand ownership, gameplay barrel alignment and
Medic/Engineer attachments remain the same runtime system.

Repaired models, RMC2 calibration and textures remain unchanged: all **26**
protected asset hashes match. Maximum baked hand-edge interpolation error is
**1.30 mm**. Loop endpoints and planar pelvis drift are zero; treatment anchors
match the stored wrists exactly.

The retained index banks contain unused slots in two models. Those slots now
duplicate immutable bind geometry and its weights instead of treating the
previous animated frame as bind. Two consecutive complete bakes produce
**byte-identical results for all seven animation/anchor files**.

Release build: **28,488,192 bytes**, below **33,554,432 bytes**. All **222**
embedded resources match their source losslessly. Raw donor FBX/ZIP files stay
local and ignored. The runtime bakes, baker and validator are tracked.

Fresh runtime verification passes **103** split checks and **180** native
pose/view checks, plus Friendly AI, guard combat, guard actions and Engineer
drone suites. Walking, signed aim, crouch and terminal views were inspected.
Final attachments preserve hand edges within **0.0019 mm** and keep the barrel
and reachable wrists aligned within floating-point precision.

Reproduce asset checks with `py -3 tools/VerifyCleanupAnimation.py`; native
layering and attachment checks use `RawMetal.exe --cleanup-split-test` and
`RawMetal.exe --cleanup-layer-inspection`. Local measurement logs are under
`docs/evidence/animation-quality-20261013/`.

Full smoke still exits 38 at the authored-save byte-exact check; the four
Medic detour/healing assertions also still fail. The earlier Vulkan
radiance-filtering failures documented in the
[initial report](cleanup-animation-repair-20261013.md) remain outside this
sampling change. Its 180 fresh captures use the native Vulkan renderer.
