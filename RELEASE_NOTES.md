# RawMetal v0.3.3 — Fresh Build & Self-Signature

## What changed

- Freshly compiled from a new, isolated Visual Studio/CMake build directory with no reused objects or linker cache. The executable is self-signed as `RawMetal Development Build` using SHA-256.
- Corrected the lift freight-bay crates: they are staggered clear of the fixed concrete partition rather than intersecting it. Added a route regression test that rejects any crate/fixed-structure overlap.
- Reactor valves now have cylindrical pipe risers, flanges, valve bodies, projecting stems and solid metal handwheels. Removed the electrical switch cabinets behind them.
- Fixed opaque concrete panels around the boarding cage. Open steel bars reveal the lift plant while retaining fall protection.
- Added a freight holding area with crates, stocked shelving and a drum; a traction bay with a cable winch, bearing blocks, generator and backed electrical cabinets; and ventilation ductwork. Equipment is concentrated near cab sightlines, with matching collision and a clear boarding route.
- Passing coolant bays now have cylindrical risers and flanges. Upper floors remain compact scenery rather than full maps.
- Includes the reactor art pass: removed computer/disk instruction boards and the persistent puzzle walkthrough, opened the containment sightline, replaced the thin-post grid, improved floor materials, and added PSX Tech instrument cabinets.
- Puzzle order, save slots, developer console and the interrupted ascent/cable-snap sequence are retained.

## Verification and limitations

Player-height Vulkan captures were reviewed from both valve stations, the boarding approach, freight area, winch bay and inside the cab. Lift traversal and fall protection, reactor puzzle, save/load, audio, console, hardware tests, and software/Vulkan smoke suites pass. Vulkan synchronization validation reports no errors.

Vulkan remains the default at full 640x360 internal resolution. In the 1,560-frame local Intel Graphics benchmark, lift/reactor scenes averaged 53–58 FPS and all six culling/reference comparisons matched exactly. However, three ride frames exceeded 50 ms, with a worst frame of 184 ms: the strict minimum-20-FPS test fails. Intermittent elevator hitches remain a known issue. Measurements exclude startup and OS window presentation; no universal minimum-FPS guarantee is claimed.

## Download

Download `RawMetal.zip`, extract it, and run `RawMetal.exe`. The separately offered EXE is identical to the one in the ZIP. Assets and shaders are embedded; a Vulkan-capable driver is sufficient and the Vulkan SDK is not needed to play. Windows x64 is supported; `--software` enables the fallback renderer.

The code-signing certificate is self-signed, so it is not trusted by Windows on other PCs and does not create Microsoft SmartScreen reputation. It verifies the packaged file came from this release build, but it cannot independently establish publisher identity like a certificate from a public certificate authority.

Press backtick and enter `map lift` for the full sequence, or `map reactor` for the reactor. Developer map commands start fresh. Esc provides Save Game and Load Game; saves live in `%LOCALAPPDATA%\RawMetal\saves`.

Source builds use Visual Studio 2022 C++ tools, CMake and the Vulkan SDK with `glslc`; run `Build.cmd`.
