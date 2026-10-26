"""Collect harness text evidence without opening or enumerating image assets."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / ".build/harness-validation"
results = json.loads((OUT / "results.json").read_text())
lines = ["# Harness validation report", "", "Worktree: `N:/rawmetal/.build/codex-wt`; base: `2d2b975`; branch: `codex/harness-validation`.", "", "## Changes", "", "1. One CommandLineToArgvW parser handles flags and single-argument values. Missing list files return 21 with a text error. Render flags apply before dispatch and override variant defaults. Variant renders restore the selected settings. Automation exceptions write stderr and RawMetal-error.txt without a dialog.", "2. Shared authored-map load validation checks doors, structures, compactors, fixture cores, timeline/cue limits and unreachable script events, with map/record/value diagnostics. Automation reports the 17 known malformed structures in maps 7/24/60 as explicit warnings; interactive loads still reject them. Other malformed records remain errors. The custom parser uses the same World validation and rejects terrain ranges outside [-11,18].", "3. State-name collision tool ports the exact 32-bit FNV hash, 23-bit mask and zero remap. The native integrity test checks event IDs across chunks and timer IDs within each map, including Campaign, Ashfall and both embedded custom campaigns.", "4. MapAudit resolves creature sentinel heights exactly, checks full hull overlap with structures/solid fixtures, reports structures/decks outside the chunk, and checks door/exit heights. Open seams match walkable surfaces across the actual border, including stacked floors and sloped banks. Lift doors are checked with their platform at the matching landing.", "5. Edge checker reads height-specific border records for each planned edge and tests all border cells for reachable pits.", "", "No campaign/map/AI/renderer content was edited. No image assets were opened for inspection. The requested ref-tools.txt brief is absent at this base commit; script definitions/runtime are in src/world/ScriptDefinition.h and src/game/Script.cpp.", "", "## Build and tool checks", "", "Initial strict content-integrity probe: exit 21, exposing the 17 structures below; final automation retains them as warnings. Initial background Build.cmd: exit 0. Updated and final audit Build.cmd runs: exit 0. Packaged-resource verification passed; mismatch fallback was not needed. Python harness tool tests: 4 passed, exit 0. git diff --check: exit 0.", "", "## Exit codes", "", "| Run | Exit | Expected | Seconds |", "|---|---:|---:|---:|"]
for r in results:
    lines.append(f"| {r['name']} | {r['exit']} | {r['expected']} | {r['seconds']} |")
boundary = hull = 0
trap_maps = []
audit_bad = []
for level in range(68):
    path = OUT / "audits" / ("map-audit-%d.txt" % level)
    if path.is_file():
        text = path.read_text(encoding="utf-8", errors="replace")
        boundary += sum(line.startswith("OUTSIDE CHUNK") for line in text.splitlines())
        hull += sum(line.startswith("HULL INTERSECTION") for line in text.splitlines())
        if any(line.startswith("TRAP ") for line in text.splitlines()):
            trap_maps.append(level)
audit_bad = [r["name"].rsplit("-", 1)[1] for r in results if r["name"].startswith("map-audit-") and r["exit"] != 0]
lines += ["", "## Finding totals", "", f"17 known malformed authored structures are warnings in automation (8 in map 7, 8 in map 24, 1 in map 60); interactive map loads still reject them. Placement audits report {boundary} out-of-bounds structures, {hull} creature hull intersections and traps in maps {trap_maps}. Failed audits: {', '.join(audit_bad)}. Every record and value is listed below.", "", "The full gameplay suite passed before the final MapAudit-only refinements. All 68 audits plus edge and hash checks were rerun after those refinements; gameplay, AI and renderer code were unchanged.", ""]
lines += ["", "## Native test details", ""]
for name in ("each-test.txt", "content-integrity-test.txt", "initial-strict-content-integrity.txt", "load-validation-test.txt", "command-arguments-test.txt", "map-shots-spaces.txt", "zone-timing.txt", "state-ids.log", "edge-check.log"):
    path = OUT / name
    if path.is_file():
        lines += ["### " + name, "", "```text", path.read_text(encoding="utf-8", errors="replace").rstrip(), "```", ""]
lines += ["## Findings by map", "", "Every finding below is retained for the map designer. HULL INTERSECTION, OUTSIDE CHUNK and DOOR STEP are new check classes. The remaining lines include existing checks and height-corrected reachability. Record indices refer to the loaded runtime world; generated decks are included in the structure indices.", ""]
for level in range(68):
    path = OUT / "audits" / ("map-audit-%d.txt" % level)
    if not path.is_file():
        lines += [f"### Map {level}", "", "Missing audit report.", ""]
        continue
    findings = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line or (len(line) == 24 and set(line) <= set("#.^oc ")) or line.startswith(("Reached floor", "Map ", "BORDER ", "PASS:", "FAIL:")):
            continue
        if line.startswith("WARNING existing content: map ") and not line.startswith(f"WARNING existing content: map {level} "):
            continue
        findings.append(line)
    if findings:
        lines += [f"### Map {level}", "", "```text", *findings, "```", ""]
lines += ["## Evidence", "", "Full per-map reports and reachable viewpoints are in `audits/`. Individual process stdout/stderr and JSON exit codes are alongside this report. Build output is the worktree's UTF-16 `build.log`, `build-final.log`, `build-audit-final.log` and `build-audit-verified.log`.", ""]
(OUT / "REPORT.md").write_text("\n".join(lines), encoding="utf-8")
print(OUT / "REPORT.md")
