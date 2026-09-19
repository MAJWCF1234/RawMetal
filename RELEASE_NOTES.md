# RawMetal v0.2.0 — Vulkan, Surface Lift & Reactor

## Hardware rendering and performance

- Vulkan 1.0 hardware rendering is the default at full 640x360 resolution.
  Normal maps, texture mip chains, emission, alpha cutouts and additive effects
  remain enabled. There is no shortened view distance or floor-count reduction.
- Materials upload at startup; opaque geometry is batched by material.
- Conservative deck occlusion preserves views through the shaft and stairwells.
- Static lighting caches no longer reset as the lift moves. Spatially indexed
  lights, bounded shadow work and per-cell collision queries reduce CPU cost.
- A persistent animation worker runs the arm rig alongside world preparation.
  Mesh topology and open shoulder boundaries are cached instead of rebuilt.
- Software rendering remains available with `--software`, and is used if
  Vulkan initialization fails. `RawMetal-renderer.txt` records the adapter or
  fallback reason. The game currently supports Windows x64; the renderer's
  portable API is not a complete Linux/macOS port.

## Surface Lift and reactor

- Turbine Gantry now leads into seven stacked map floors around a continuous
  lift shaft. The enclosed room rises three floors, jams, then falls six with
  the player inside. The two bottom floors form the reactor complex.
- Redesigned freight cab with sliding split gates, inspection windows,
  recessed panels, handrails, hoist details and side-mounted dispatch control.
- The main OST fades into motor sounds, followed by strained creaking, a cable
  snap and a crash. A darker reactor OST fades in after impact.
- Reactor stairs, encounters and a locked lower exit complete the authored
  route. The cab's state persists through map geometry unloading.
- Vertical sight rays respect solid decks; hearing distance and remembered
  target height account for stacked floors.

## Developer console

Press **backtick (`)**. Commands include:

- `maps`, `help`
- `map foundry`, `map pressureworks`, `map gantry`, `map lift`, `map reactor`
- `map 0` through `map 3`
- `reload`, `where`, `fps`, `clear`
- `r_scale 50`, `r_scale 75`, `r_scale 100` (default)

Map loading starts fresh. `map reactor` skips directly to the crashed lift.
Up/Down recalls command history. Backtick or Esc closes the console.

## Verification on the release build

On this Intel Core 3 100U / Intel Graphics machine at full 640x360, the seven
benchmark scenarios averaged **68–92 FPS**. The slowest measured frame was
**23.21 ms (43 FPS)**; none of the 1,260 measured gameplay frames exceeded the
50 ms / 20 FPS target. Timing includes update, audio control, rendering and GPU
readback, but excludes startup and OS window presentation.

Software and Vulkan smoke suites, hardware material/depth tests, parallel/serial
animation comparisons and all six exact culling/reference image comparisons
passed. Lift, audio, console, movement, combat and streaming regressions passed.

## Download and build

Download `RawMetal.zip`, extract, and run `RawMetal.exe`. The ZIP contains the
same standalone executable offered separately. Assets and SPIR-V shaders are
embedded. Hardware rendering needs a Vulkan-capable graphics driver, not the SDK.

Source builds require Visual Studio 2022 C++ tools, CMake and the Vulkan SDK
with `glslc`. Use `Build.cmd`.

The renderer still uses an offscreen Vulkan image with readback to the existing
Win32 presentation/HUD path. See `src/RENDERING.md` for architecture and test scope.
Benchmark results depend on hardware and background load; a universal minimum
FPS is not guaranteed.
