# RawMetal v0.3.0 — Reactor Puzzle, Save Slots & Staged Lift

## Playable changes

- Expanded reactor service bays with pumps, compressor, generator, shelving,
  switchgear and a computer with a floppy drive.
- Find the upper maintenance authorization disk, insert it at the lower
  computer, prime FEED, open upper RETURN, then confirm bulkhead authorization.
- Esc now offers Save Game and Load Game submenus with three slots, confirmation
  prompts and corruption checks. Saves retain all map progress, inventory,
  enemies, doors, the lift and the reactor puzzle.
- Saves live in `%LOCALAPPDATA%\RawMetal\saves`. Loading preserves local settings.

## Lift staging and visual repairs

- Approximately 47-second lift sequence: interrupted ascent, blackout, creaking,
  cable failure, a temporary brake catch and a second fall into the reactor.
- Four passing shaft storeys are compact scenic machinery bays instead of full
  maps: 28 deck tiles each rather than 448. Boarding and reactor rooms remain
  playable, with proper ceilings.
- Close guide rails, height markers, sparks and a loose cable provide motion cues.
  Free look is retained; effects pause and restore with the saved lift state.
- Fixed duplicate wall/rail surfaces causing z-fighting, stair materials,
  texture stretching, lamp mounting and the formerly floating dispatch sign.
  The sign is now mounted on the header, leaving the window clear.

## Rendering and verification

Vulkan remains the default at full 640x360 internal resolution, with software
fallback available via `--software`. Windows x64 remains the supported platform.

Two consecutive local 1,560-frame benchmark runs passed the 50 ms budget.
In the repeat run, lift/reactor scenes averaged approximately 92–106 FPS;
their worst measured frame was 14.45 ms (69 FPS). All six culling/reference
comparisons matched exactly. Measurements include simulation, audio control,
rendering and GPU readback, but exclude startup and OS window presentation.
These are local Intel Graphics results, not a universal minimum-FPS guarantee.
Earlier revisions exhibited intermittent hitches; those reports are retained.

Full software and Vulkan smoke suites, lift/route, reactor puzzle, save/load,
audio, console and hardware-rendering tests passed. Vulkan synchronization
validation logged no errors. Save tests include mid-fall restoration, invalid
files, cancelled overwrites and preservation of the old slot after write failure.

## Download

Download `RawMetal.zip`, extract it, and run `RawMetal.exe`. The ZIP contains
the same standalone executable offered separately; assets and shaders are
embedded. A Vulkan-capable graphics driver is sufficient; the SDK is not needed.

Press backtick and enter `map lift` to try the full sequence, or `map reactor`
to jump to the puzzle. Developer map commands start fresh. Use Esc to save/load.

Source builds use Visual Studio 2022 C++ tools, CMake and the Vulkan SDK with
`glslc`; run `Build.cmd`.
