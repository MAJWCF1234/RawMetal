# Building, testing and shipping without an assistant

Everything below runs from `N:/rawmetal` in a normal terminal. Each step writes its log in `.build/ship/`.

## Make a release

1. Write the release notes first: a new section at the top of `RELEASE_NOTES.md`,
   `## v1.4.6 - Title (2026-11-18)`, in the game's voice. Its title becomes the release title.
2. Commit or stage your work.
3. Ship:

```
python tools/release/ship.py 1.4.6
```

That one command:
- sets the version (both `app.rc` files and the menu tag);
- commits;
- syncs the Campaign Builder's engine copy;
- builds the game and the Builder in parallel;
- runs every test: the suite in parallel lanes on both GPUs, gore, carcass, campaign-map, repeated-frame rendering, the audio test, the Builder's self-test, editing test and map round-trips;
- packages the release and checks that the zipped exe is the tested one;
- pushes, publishes the GitHub release as Latest with its four files, and downloads it back to verify the checksums.

It stops at the first failure and prints the log to read.

Options:
- `--offline` does everything except push and publish. The finished release sits in `.build/release/v1.4.6/`; run the command again without `--offline` later to publish.
- `--check` builds and tests the current tree without changing the version or packaging. Use it before deciding to ship.

From the launcher: `RawMetalMaster.cmd ship 1.4.6` or `RawMetalMaster.cmd ship 1.4.6 offline`.

**When the Builder sync stops with a conflict:**
- It resolves its own menu tag ("CAMPAIGN BUILDER") itself.
- For any other conflict, open the named file in `tools/Level Editor/src`:
  - keep the Builder's lines where they are Builder features (editor views, preview hooks);
  - take the game's lines where the Builder's are just an older copy.
- Then run `ship.py` again.

**Publishing:** it needs `gh` signed in (`gh auth status`). Nothing else needs the internet: building, testing, `--offline` and map installation all work without it.

## Install a map

```
python tools/release/install_map.py "path/to/My Map.rmc"
python tools/release/install_map.py "My Map.rmc" --to "D:/Games/DepthWorks"
python tools/release/install_map.py --list
```

It copies the `.rmc`, plus its `.rmc.project.json` editing state if there is one, into:
- `custom maps/` here, for the game;
- the Campaign Builder's `custom maps/`;
- any unzipped release folder you name with `--to`.

It then runs the game's loader over the map, so a map that won't load is caught straight away. In the game, custom maps are under the title menu's CUSTOM MAPS.

## Work on a feature fast

The feature lab rebuilds in seconds instead of about 10 minutes (see `lab/README.md`):

```
python tools/dev/lab.py build
python tools/dev/lab.py run --feature=NAME [--with-friends]
python tools/dev/suite.py RawMetal.exe --extras     # the whole suite, in parallel lanes, about 15 minutes
```

- `RM_GPU=0` or `RM_GPU=1` picks a graphics card.
- `RM_LOOK=name=value,...` changes look settings live.
