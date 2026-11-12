#!/usr/bin/env python3
"""Sync the Campaign Builder's engine copy with the game.

The builder (tools/Level Editor) carries its own copy of the engine in
`tools/Level Editor/src`. Most files are the game's own; some carry editor code
(adoptEditorCampaign, EditorView, observer mode, the unified build). This tool
brings the copy up to a game commit without losing that code.

The game commit the copy was last synced to is kept in
`tools/Level Editor/ENGINE_SYNC`. For every engine file:

  editor == game at last sync   -> copy the game's file at the target
  editor == game at the target  -> already current
  otherwise                     -> three-way merge (git merge-file):
                                   base = game at last sync, ours = editor,
                                   theirs = game at the target.
                                   A clean merge is written. A conflicting one
                                   is reported and left alone; --force writes
                                   it with conflict markers.

New game sources are copied. Files only the editor has are never touched.
Files the game deleted are reported, not deleted. The game's tracked files in
`custom maps/` (GUIDE.html, which the builder reads its record fields from,
its pictures, the Starter Map, the Ashfall zone) go the same way; the
builder's own maps there are never touched.

Assets: every file the editor's assets.rc embeds (the voice bank among them)
and everything under src/assets/library (gitignored purchased models) is
copied when the game's copy is new, or newer and the editor's was not changed
on its own.

  python tools/editor/sync_engine.py --dry           what it would do
  python tools/editor/sync_engine.py                 sync to game HEAD
  python tools/editor/sync_engine.py --to REV        sync to another commit
  python tools/editor/sync_engine.py --worktree      take the game's uncommitted
                                                     working files (does not
                                                     advance ENGINE_SYNC)
  python tools/editor/sync_engine.py --from REV      override the recorded base

Exit code: 0 clean, 1 conflicts left (or written with --force), 2 usage error.
Comparisons ignore CRLF/LF differences; merged files keep the editor file's
line endings.
"""
from __future__ import annotations

import argparse
import datetime
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EDITOR_DIR = ROOT / "tools" / "Level Editor"
STATE = EDITOR_DIR / "ENGINE_SYNC"
# Paths are repository-relative keys ("src/game/Game.cpp"); the builder keeps
# the same layout under tools/Level Editor. "custom maps/" carries the map
# guide (GUIDE.html is where the builder reads every record's fields), the
# Starter Map and the Ashfall zone; only the files the game tracks there are
# synced, never the player's own maps.
GAME = ROOT
EDITOR = EDITOR_DIR
TREES = ("src/", "custom maps/")
SKIP = ("custom maps/Autosave/",)

# Files that exist only in the editor copy. They are never compared or merged.
EDITOR_ONLY_HINT = (
    "src/core/EditorView.h",
    "src/platform/EditorChannel.h",
    "src/platform/EditorShots.cpp",
    "src/platform/EditorShots.h",
)


def git(*args: str, data: bytes | None = None, check: bool = True) -> bytes:
    run = subprocess.run(["git", "-C", str(ROOT), *args], input=data,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check and run.returncode != 0:
        raise SystemExit("git %s failed: %s" % (" ".join(args), run.stderr.decode(errors="replace").strip()))
    return run.stdout


class Blobs:
    """Reads many blobs through one `git cat-file --batch`."""

    def __init__(self) -> None:
        self.proc = subprocess.Popen(["git", "-C", str(ROOT), "cat-file", "--batch"],
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.cache: dict[str, bytes | None] = {}

    def get(self, rev: str, path: str) -> bytes | None:
        key = rev + ":" + path
        if key in self.cache:
            return self.cache[key]
        assert self.proc.stdin and self.proc.stdout
        self.proc.stdin.write((key + "\n").encode("utf-8"))
        self.proc.stdin.flush()
        header = self.proc.stdout.readline().decode("utf-8", "replace").split()
        if len(header) < 3 or header[1] != "blob":
            self.cache[key] = None
            return None
        size = int(header[2])
        body = self.proc.stdout.read(size)
        self.proc.stdout.read(1)
        self.cache[key] = body
        return body

    def close(self) -> None:
        if self.proc.stdin:
            self.proc.stdin.close()
        self.proc.wait()


def norm(data: bytes | None) -> bytes | None:
    return None if data is None else data.replace(b"\r\n", b"\n")


def is_binary(*items: bytes | None) -> bool:
    return any(item is not None and b"\0" in item[:65536] for item in items)


def uses_crlf(data: bytes) -> bool:
    lines = data.count(b"\n")
    return lines > 0 and data.count(b"\r\n") * 2 >= lines


def to_eol(data: bytes, crlf: bool) -> bytes:
    data = data.replace(b"\r\n", b"\n")
    return data.replace(b"\n", b"\r\n") if crlf else data


def read(path: Path) -> bytes | None:
    try:
        return path.read_bytes()
    except FileNotFoundError:
        return None


def tracked_files(rev: str) -> set[str]:
    out = git("-c", "core.quotepath=off", "ls-tree", "-r", "--name-only", "-z", rev, "--", *TREES).decode("utf-8")
    return {p for p in out.split("\0") if p.startswith(TREES) and not p.startswith(SKIP)}


def tree_files(root: Path, tree: str) -> set[str]:
    found = set()
    for path in (root / tree).rglob("*"):
        if path.is_file():
            found.add(path.relative_to(root).as_posix())
    return found


def editor_files() -> set[str]:
    # The builder's own maps in custom maps/ are not engine files: only src/ is listed.
    return tree_files(EDITOR, "src/")


def is_asset(rel: str) -> bool:
    return rel.startswith("src/assets/")


def merge_file(ours: bytes, base: bytes, theirs: bytes) -> tuple[int, bytes]:
    with tempfile.TemporaryDirectory(prefix="rm-sync-") as tmp:
        paths = []
        for name, data in (("ours", ours), ("base", base), ("theirs", theirs)):
            path = os.path.join(tmp, name)
            with open(path, "wb") as handle:
                handle.write(data)
            paths.append(path)
        run = subprocess.run(["git", "merge-file", "-p",
                              "-L", "editor", "-L", "game at last sync", "-L", "game now",
                              *paths], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if run.returncode < 0 or run.returncode > 127:
            raise SystemExit("git merge-file failed: " + run.stderr.decode(errors="replace"))
        return run.returncode, run.stdout


def editor_lines_kept(base: bytes, ours: bytes, merged: bytes) -> list[bytes]:
    """Lines the editor added (not in the game at the last sync) that the merge lost."""
    base_lines = set(base.split(b"\n"))
    merged_lines = set(merged.split(b"\n"))
    lost = []
    for line in ours.split(b"\n"):
        if line.strip() and line not in base_lines and line not in merged_lines:
            lost.append(line)
    return lost


def rc_assets(rc_text: bytes) -> list[str]:
    names = re.findall(rb'^\s*\d+\s+\w+\s+"([^"]+)"', rc_text, re.M)
    return [n.decode("utf-8", "replace").replace("\\\\", "/").replace("\\", "/") for n in names]


def cmake_sources(cmake_text: bytes) -> list[str]:
    """Source paths (relative to src/) the CMakeLists names: game/Foo.cpp, main.cpp, the shaders."""
    found = set()
    for line in cmake_text.decode("utf-8", "replace").splitlines():
        if line.lstrip().startswith("#"):
            continue
        shaders = re.match(r"\s*foreach\(SHADER\s+([^)]*)\)", line)
        if shaders:
            found |= {"renderer/shaders/" + name for name in shaders.group(1).split()}
            continue
        for name in re.findall(r'(?<![\w/.${}-])((?:[\w.-]+/)*[\w.-]+\.(?:cpp|c|h|hpp|inc|rc|manifest))\b', line):
            if "/" in name or name in ("main.cpp", "app.rc", "assets.rc", "app.manifest"):
                found.add(name)
    return sorted(found)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--dry", action="store_true", help="list what would happen, change nothing")
    parser.add_argument("--force", action="store_true", help="write conflicting merges with conflict markers")
    parser.add_argument("--to", default="HEAD", help="game commit to sync to (default HEAD)")
    parser.add_argument("--from", dest="base", help="game commit last synced (default: ENGINE_SYNC)")
    parser.add_argument("--worktree", action="store_true",
                        help="take the game's working files, uncommitted edits included")
    parser.add_argument("--no-assets", action="store_true", help="skip assets")
    parser.add_argument("-v", "--verbose", action="store_true", help="also list unchanged files")
    parser.add_argument("--editor", help="a copy of tools/Level Editor to sync instead (for testing the tool)")
    args = parser.parse_args()
    global EDITOR, STATE
    if args.editor:
        EDITOR = Path(args.editor).resolve()
        STATE = EDITOR / "ENGINE_SYNC"

    base_rev = args.base
    if not base_rev:
        if not STATE.exists():
            print("No %s. Pass --from <game commit the editor was last synced to>." % STATE)
            return 2
        for line in STATE.read_text(encoding="utf-8").splitlines():
            if line.startswith("commit "):
                base_rev = line.split()[1]
        if not base_rev:
            print("%s has no 'commit <sha>' line." % STATE)
            return 2
    base_sha = git("rev-parse", "--verify", base_rev + "^{commit}").decode().strip()
    target_sha = git("rev-parse", "--verify", args.to + "^{commit}").decode().strip()
    tag = "DRY " if args.dry else ""
    print("%sGame %s -> %s%s" % (tag, base_sha[:10], target_sha[:10],
                                 " + working files" if args.worktree else ""))

    blobs = Blobs()
    base_files = tracked_files(base_sha)
    target_files = tracked_files(target_sha)
    mine = editor_files()
    game_live = {p for p in tree_files(GAME, "src/") if not is_asset(p)} if args.worktree else set()

    candidates = sorted({p for p in (mine | target_files | base_files | game_live) if not is_asset(p)})
    actions: list[tuple[str, str, str]] = []  # (kind, rel, note)
    conflicts = 0
    writes: list[tuple[Path, bytes]] = []
    planned: dict[str, bytes] = {}  # rel -> planned editor content (for rc/cmake follow-up)

    def theirs_of(rel: str) -> tuple[bytes | None, bytes | None]:
        """(normalized game content at target, bytes to write for a copy)."""
        live = read(GAME / rel)
        if args.worktree and live is not None:
            return norm(live), live
        blob = blobs.get(target_sha, rel)
        if blob is None:
            return None, None
        if live is not None and norm(live) == norm(blob):
            return norm(blob), live  # the game's own checked-out bytes
        smudged = git("cat-file", "--filters", "%s:%s" % (target_sha, rel), check=False)
        return norm(blob), (smudged or blob)

    for rel in candidates:
        ours_raw = read(EDITOR / rel)
        ours = norm(ours_raw)
        base = norm(blobs.get(base_sha, rel))
        theirs, theirs_bytes = theirs_of(rel)
        if ours_raw is not None:
            planned[rel] = ours_raw

        if theirs is None:
            if ours is None:
                continue
            if base is None:
                if (GAME / rel).exists() and not args.verbose:
                    continue  # untracked in the game too (build output, pdbs)
                if args.verbose or rel not in EDITOR_ONLY_HINT:
                    actions.append(("editor-only", rel, "kept"))
                continue
            actions.append(("game-deleted", rel, "game removed it; editor copy kept (remove by hand if unused)"))
            continue
        if ours is None:
            if base is not None:
                actions.append(("editor-deleted", rel, "editor removed it; not restored"))
                continue
            actions.append(("new", rel, "new game file"))
            writes.append((EDITOR / rel, theirs_bytes))
            planned[rel] = theirs_bytes
            continue
        if ours == theirs:
            if args.verbose:
                actions.append(("current", rel, ""))
            continue
        if base is not None and ours == base:
            actions.append(("copy", rel, "game changed it; editor had no changes"))
            writes.append((EDITOR / rel, theirs_bytes))
            planned[rel] = theirs_bytes
            continue
        if base is not None and theirs == base:
            if args.verbose:
                actions.append(("editor-change", rel, "game unchanged since last sync; editor code kept"))
            continue
        if is_binary(ours, base, theirs):
            conflicts += 1
            actions.append(("CONFLICT", rel, "binary file changed on both sides; left alone"))
            continue
        merge_base = base if base is not None else b""
        count, merged = merge_file(ours, merge_base, theirs)
        lost = editor_lines_kept(merge_base, ours, merged) if count == 0 else []
        out = to_eol(merged, uses_crlf(ours_raw))
        if count == 0 and not lost:
            note = "merged cleanly" + ("" if base is not None else " (added on both sides)")
            actions.append(("merge", rel, note))
            writes.append((EDITOR / rel, out))
            planned[rel] = out
            continue
        conflicts += 1
        if lost:
            note = "merge would drop %d editor line(s), e.g. %r" % (len(lost), lost[0].strip()[:60].decode("utf-8", "replace"))
        else:
            note = "%d conflict(s)" % count
        if args.force:
            actions.append(("CONFLICT", rel, note + "; written with markers (--force)"))
            writes.append((EDITOR / rel, out))
            planned[rel] = out
        else:
            actions.append(("CONFLICT", rel, note + "; left alone (use --force for markers)"))

    # The editor build: sources its CMakeLists names that the copy lacks.
    cmake = planned.get("src/CMakeLists.txt")
    if cmake:
        for src in cmake_sources(cmake):
            rel = os.path.normpath(src).replace("\\", "/")
            if rel.startswith("..") or rel.startswith("${"):
                continue
            rel = "src/" + rel
            if rel in planned or (EDITOR / rel).exists():
                continue
            if (GAME / rel).exists():
                data = (GAME / rel).read_bytes()
                actions.append(("new", rel, "named by the editor's CMakeLists"))
                writes.append((EDITOR / rel, data))
                planned[rel] = data
            elif "/" in rel:
                actions.append(("missing", rel, "CMakeLists names it; neither copy has it"))

    # Assets: what assets.rc embeds, and the library folder.
    asset_actions: list[tuple[str, str, str]] = []
    asset_copies: list[tuple[Path, Path]] = []
    if not args.no_assets:
        rc = planned.get("src/assets.rc") or read(EDITOR / "src/assets.rc") or b""
        wanted = {"src/" + name for name in rc_assets(rc)}
        wanted |= tree_files(GAME, "src/assets/library/")
        for rel in sorted(wanted):
            g = GAME / rel
            e = EDITOR / rel
            if not g.exists():
                if not e.exists():
                    asset_actions.append(("missing", rel, "assets.rc names it; neither copy has it"))
                continue
            if not e.exists():
                asset_actions.append(("new", rel, "%d bytes" % g.stat().st_size))
                asset_copies.append((g, e))
                continue
            gs, es = g.stat(), e.stat()
            if gs.st_size == es.st_size and int(gs.st_mtime) == int(es.st_mtime):
                continue
            if gs.st_size == es.st_size and g.read_bytes() == e.read_bytes():
                continue
            if rel.startswith("src/assets/library/") or blobs.get(target_sha, rel) is None:
                if gs.st_mtime > es.st_mtime:
                    asset_actions.append(("copy", rel, "game copy newer"))
                    asset_copies.append((g, e))
                else:
                    asset_actions.append(("editor-change", rel, "editor copy newer; kept"))
                continue
            base_blob = blobs.get(base_sha, rel)
            edata = e.read_bytes()
            if base_blob is not None and (edata == base_blob or norm(edata) == norm(base_blob)):
                asset_actions.append(("copy", rel, "game changed it"))
                asset_copies.append((g, e))
            elif gs.st_mtime > es.st_mtime:
                asset_actions.append(("copy", rel, "game copy newer"))
                asset_copies.append((g, e))
            else:
                asset_actions.append(("editor-change", rel, "editor copy differs and is newer; kept"))

    blobs.close()

    order = {"CONFLICT": 0, "missing": 1, "merge": 2, "copy": 3, "new": 4, "game-deleted": 5,
             "editor-deleted": 6, "editor-only": 7, "editor-change": 8, "current": 9}
    for kind, rel, note in sorted(actions, key=lambda a: (order.get(a[0], 9), a[1])):
        print("%s%-14s %s%s" % (tag, kind, rel, ("  - " + note) if note else ""))
    for kind, rel, note in asset_actions:
        print("%s%-14s %s%s" % (tag, "asset " + kind, rel, ("  - " + note) if note else ""))

    counts: dict[str, int] = {}
    for kind, _, _ in actions + [("asset " + a[0], a[1], a[2]) for a in asset_actions]:
        counts[kind] = counts.get(kind, 0) + 1
    print("%sSummary: %s" % (tag, ", ".join("%s %d" % kv for kv in sorted(counts.items())) or "nothing to do"))

    if args.dry:
        return 1 if conflicts else 0

    for path, data in writes:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    for src, dst in asset_copies:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
    if writes or asset_copies:
        print("Wrote %d file(s) and %d asset(s). Rebuild with tools/Level Editor/Build.cmd%s." % (
            len(writes), len(asset_copies),
            " (assets.rc changed: a full repack)" if any(p.name == "assets.rc" for p, _ in writes) else ""))

    if conflicts and not args.force:
        print("%d conflict(s) left; ENGINE_SYNC stays at %s. Resolve them, then run again." % (conflicts, base_sha[:10]))
        return 1
    if args.worktree:
        print("Working files taken; ENGINE_SYNC stays at %s (uncommitted game edits are not a commit)." % base_sha[:10])
        return 1 if conflicts else 0
    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    subject = git("log", "-1", "--format=%s", target_sha).decode("utf-8", "replace").strip()
    STATE.write_text(
        "# Game commit the builder's engine copy (src/) was last synced to.\n"
        "# Written by tools/editor/sync_engine.py; it is the merge base of the next sync.\n"
        "commit %s\n"
        "# %s, %s\n" % (target_sha, stamp, subject),
        encoding="utf-8")
    print("ENGINE_SYNC -> %s%s" % (target_sha[:10], " (conflict markers written)" if conflicts else ""))
    return 1 if conflicts else 0


if __name__ == "__main__":
    sys.exit(main())
