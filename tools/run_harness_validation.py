"""Run the RawMetal harness in this worktree; never inspect rendered images."""
import json
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / ".build/harness-validation"
OUT.mkdir(parents=True, exist_ok=True)
AUDITS = OUT / "audits"
AUDITS.mkdir(exist_ok=True)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--audits-only", action="store_true", help="rerun all audits and edge/hash checks while retaining other results from the previous full run")
args = parser.parse_args()
results = json.loads((OUT / "results.json").read_text()) if args.audits_only else []

def run(name, argv, expected=0, timeout=1200):
    print("Running", name, flush=True)
    start = time.monotonic()
    try:
        p = subprocess.run(argv, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
        code, output = p.returncode, p.stdout.decode("utf-8", errors="replace")
    except subprocess.TimeoutExpired as error:
        code, output = "TIMEOUT", (error.stdout or b"").decode("utf-8", errors="replace")
    (OUT / (name + ".log")).write_text(output, encoding="utf-8")
    warnings = ROOT / "map-validation-warnings.txt"
    if str(argv[0]).lower().endswith(".exe") and warnings.is_file():
        shutil.copy2(warnings, OUT / (name + "-warnings.txt"))
    results[:] = [r for r in results if r["name"] != name]
    results.append(dict(name=name, argv=list(map(str, argv)), exit=code, expected=expected, seconds=round(time.monotonic()-start, 3)))
    (OUT / "results.json").write_text(json.dumps(results, indent=2))
    print(name, "exit", code, "expected", expected, flush=True)
    return code

exe = str(ROOT / "RawMetal.exe")
if not args.audits_only:
    for flag in ("each-test", "smoke-test", "chapter-route-test", "world-isolation-test", "exclusion-test", "exclusion-authoring-test", "prologue-test", "content-integrity-test", "load-validation-test", "command-arguments-test"):
        run(flag, [exe, "--" + flag])
        report = ROOT / (flag + ".txt")
        if report.is_file():
            shutil.copy2(report, OUT / report.name)
for level in range(68):
    # Preserve exceptions as per-map evidence, even when construction fails.
    run("map-audit-%d" % level, [exe, "--map-audit=%d" % level])
    for stem in ("map-audit", "map-views"):
        source = ROOT / ("%s-%d.txt" % (stem, level))
        if source.is_file():
            shutil.copy2(source, AUDITS / source.name)
run("edge-check", [sys.executable, "tools/campaign/edge_check.py", str(AUDITS), "34", "67"])
run("state-ids", [sys.executable, "tools/check_state_ids.py", "--cpp-values", "content-integrity-test.txt"])
if not args.audits_only:
    for mode in ("map-shots", "zone-shots"):
        run(mode + "-missing", [exe, "--nopost", "--%s=.build/missing list.txt" % mode, "--nopixel"], expected=21)
        report = ROOT / (mode + ".txt")
        if report.is_file():
            shutil.copy2(report, OUT / (mode + "-missing.txt"))
    run("map-sweep-missing", [exe, "--map-sweep=987654"], expected=21)
    lists = OUT / "shot lists"
    lists.mkdir(exist_ok=True)
    map_list = lists / "campaign views.txt"
    map_list.write_text("validation 0 3.5 4.5 0 0 -6\n")
    run("map-shots-spaces", [exe, "--nopost", "--map-shots=" + str(map_list), "--nopixel", "--post-ao"])
    shutil.copy2(ROOT / "map-shots.txt", OUT / "map-shots-spaces.txt")
    zone_list = lists / "zone views.txt"
    zone_list.write_text("validation-zone 0 7 7 0 -6\n")
    run("zone-shots-spaces", [exe, "--nopixel", "--zone-shots=" + str(zone_list), "--perf-variants", "--nopost", "--post-ao"])
    shutil.copy2(ROOT / "zone-shots.txt", OUT / "zone-shots-spaces.txt")
    timing = ROOT / ".build/shot/timing.txt"
    if timing.is_file():
        shutil.copy2(timing, OUT / "zone-timing.txt")
print("Results:", OUT / "results.json", flush=True)
raise SystemExit(int(any(r["exit"] != r["expected"] for r in results)))
