# RawMetal v0.3.4 — Movement & Reactor Warden

## Changes

- Player contact resolution reaches the wall instead of losing a whole movement step; unobstructed wall-axis motion is retained. Descending steps stay grounded. Reduced airborne steering preserves launch momentum. Existing jump buffering, coyote time, crouch clearance and duck-jumps remain supported.
- Thrown clutter retains tangential velocity on wall impacts instead of reversing both horizontal axes. Ceiling contacts clamp to the available clearance.
- New reactor-only Warden using the supplied Criaturas1 W2 creature (738 triangles) and texture. It locks aim for a 1.1-second audiovisual charge, fires a narrow dodgeable strike, respects cover and repositions between attacks. Existing brute sounds are reused at a lower pitch.
- Bugs no longer separate from creatures on other floors or mistake reloading for a shot. Blocked movement falls back to cached routing without rebuilding a path every frame.
- R reloads the six-round shotgun tube; mouse wheel up selects shotgun and down selects fists. Restart moved to the Esc menu with confirmation. HUD distinguishes loaded shells from reserve.
- Save format 3 supports the Warden and reads version 1/2 saves. Old saves retain their original enemy populations; start a fresh reactor map to encounter the new creature.
- Fixed an unbounded audio/combat regression test after the reload change.

## Verification and limitations

Targeted movement/AI/clutter tests, save/load and controls tests, and full Vulkan and software smoke suites pass. Tests cover 30/60/120 Hz movement consistency, wall sliding, glancing clutter impacts, stacked-floor separation, stationary/dodging/covered Warden targets, bug pursuit and stairs. Vulkan images were inspected in a controlled scene and the reactor.

Local Intel Graphics benchmark, 640x360: reactor active AI averaged 17.06 ms/frame (58.6 FPS), with a 28.12 ms worst frame (35.6 FPS); update time peaked at 0.39 ms. The balcony averaged 16.69 ms. All six culling/reference comparisons matched. These measurements exclude startup and OS presentation. Elevator rendering hitches still fail the strict 50 ms minimum-frame-rate budget; no universal 20 FPS minimum is claimed.

This improves the movement foundation for future parkour levels; it does not add vaulting or wall-running. Physics remain a lightweight game controller and clutter simulation, not a full rigid-body engine.

## Download

Extract RawMetal.zip and run RawMetal.exe. Assets/shaders are embedded. Windows x64; Vulkan is default, with --software fallback. Use backtick and map reactor for a fresh reactor encounter, or map lift for the full ride. Saves are under %LOCALAPPDATA%/RawMetal/saves.

The executable is self-signed with the existing RawMetal Development Build certificate. This does not establish public publisher trust, create SmartScreen reputation, or guarantee antivirus clearance. The EXE in the ZIP is identical to the separately downloadable EXE.
