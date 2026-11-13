# Feature lab

`RawMetalLab` is a fast build of the engine for working on one feature at a time.
A feature plugs into a **tube** (a small saved scene). The runner builds the scene,
steps and renders it for N frames, measures it, and reports pass or fail. It does
this alone or with the features it talks to (its **friends**).

- It uses the same engine sources as `RawMetal.exe`. Lab code is in `src/lab/`
  (`Lab.cmake` is hooked at the end of `src/CMakeLists.txt`). It is never part of
  the release build.
- Assets load from the loose files that `src/assets.rc` names. Nothing is packed.
  A worktree falls back to the main checkout's `src/` for `assets/library`.
  `RM_ASSET_ROOT=dir;dir` overrides the roots.
- The build is Ninja into `.build/lab` of each checkout or worktree. There is no
  global lock.

## Build and run

```
python tools/dev/lab.py build                       # first ~3.5 min, then seconds
python tools/dev/lab.py build --hot renderer/Scene3D.cpp   # files you edit at /Od: seconds, not a minute
python tools/dev/lab.py run --list
python tools/dev/lab.py run --feature=ragdoll
python tools/dev/lab.py run --feature=ragdoll --with-friends      # alone, + each friend, + all
python tools/dev/lab.py run --feature=ragdoll+blood+chimera       # one group in one tube
python tools/dev/lab.py run --feature=brute,chimera-shot,chimera+chimera-shot   # the showcase bugs
python tools/dev/lab.py run --feature=engine.doors                # an existing --each-test, in the lab build
```

Useful flags: `--fixtures=GLOB`, `--frames=N`, `--step=S`, `--out=DIR`
(default `.build/lab-out`), `--software`, `--jobs=K` (a child process per tube),
`--no-repeat`, `--reporter=none|NAME`, `--live`.

The output in `DIR` is:

- `report.html`: every tube's checks, metrics, last frame and reporter view.
- `metrics.json`: the same data for machines.
- `<feature>-sheet.png`: a contact sheet per feature.
- `jobs/<tube>/`: the tube's frames.

The exit code is 1 if anything failed.

Every tube is run twice from scratch. Its last frame must repeat exactly, and its
metrics must agree to 1%. When they don't, the report adds `repeat last frame`
and `repeat diff` images.

With `--with-friends` the report includes a matrix of the feature against each
friend. A cell fails on any of these:

- a broken check;
- a broken repeat;
- a cost: the group's frame time over the bare fixture is more than 1.3 × the sum
  of the parts + 1.5 ms;
- new flicker: the share of pixels that change when the camera moves 1 mm grows
  by more than 2 % over the worst member alone.

## Write a tube

Add `src/lab/tubes/Thing.cpp`:

```cpp
static const bool registered=lab::add({.name="thing",.summary="...",.friends={"blood"},.fixtures={"thing-room"},
 .author=[](Scene& s,AuthoredMapData& m){/* add to an authored room before the Game is built */},
 .setup=[](Scene& s){/* stage on the Game */},
 .step=[](Scene& s,float dt){/* each frame; s.input is what the player does */},
 .capture=[](Scene& s,const Frame& f){/* after each render; s.shoot("label") keeps a picture */},
 .metrics=[](Scene& s,Report& r){r.metric("x",1);r.check("it works",true,"detail");},
 .reporter=[](Scene& s,Overlay& o){/* its invisible state, drawn over a copy of the last frame */},
 .with={{"blood",[](Scene& s,Report& r){/* checks only when blood shares the tube */}}}});
```

Engine privates go through `LabAccess` (`src/lab/LabAccess.h`). `Game` and
`SoftwareRenderer` each have one `friend struct LabAccess;` line.

## Fixtures

A fixture is a file `lab/fixtures/NAME.lab`. `src/lab/LabFixture.h` lists every
directive; the main ones are:

- `world campaign|ashfall|room|door-room|custom "path"|text`
- `map N`
- `camera X Y Z YAWDEG PITCH`
- `spawn creature KIND X Y`
- `spawn guard X Y`
- `light X Y Z I R`
- `state NAME V`
- `save F.sav`
- `frames N` and `step S`
- `hud on`
- a whole campaign inline between `begin-campaign` and `end-campaign`

Any other `key value` line is free for the features to read.

To snapshot a scene from the game, run it with `--dev` and type `labsnap NAME` in
the console. That writes `lab/fixtures/NAME.lab` and `NAME.sav`; the save holds
every state, creature, door and body. Or run
`python tools/dev/lab.py fixture NAME --map 70 --at X Y --yaw 90`.

## Live knobs

`retro::tune("name", default)` and `tuneColour()` come from `src/core/Tune.h`.
They read `lab/tune.txt` in the lab, or the file `RM_TUNE` names in the game. The
file is re-read within a quarter second of an edit. With no file the default is
returned and nothing is read.

To change a knob, run `python tools/dev/lab.py tune ragdoll.edge 0.3`, or edit the
file. `--live` opens a window on one tube and loops it. It restarts the tube when
the knobs change. Shader hot reload is not in this cut: a shader edit needs a lab rebuild (the .inc step, seconds).

## The loop

1. Build the feature in the lab, with its friends in the tube from the start.
2. Tune it live.
3. Iterate until the feature, its matrix and its repeat are green.
4. Hand it in. Integrating it is then a merge of code that is already proven.

Worked example: `--feature=ragdoll --with-friends` runs the ragdoll alone, with
blood, with the Chimera, and with both. In the feed room the Chimera's bite makes
the body, the body gets the pool, and every check of each feature must still hold.

## Tubes that activate after merge

`particles`, `probes` and the ClaireOutside checks in `claire` compile only when
`renderer/Particles.h`, `LightProbes.h` and `ClaireOutside.h` exist, through
`__has_include` in `LabAccess.h`. Those files are still uncommitted in the main
tree. Until they are merged:

- `particles` falls back to plain emitters;
- `probes` is not registered;
- `claire` checks only that she is drawn.
