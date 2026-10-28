"""Prove two --perf-profile runs made the same AI decisions, frame by frame.

    python -I tools/perf/same.py base.csv change.csv [--columns=a,b,c]

Compares the decision columns (by default the ones a cache or a faster query
must leave untouched) of every scene and frame both files hold, and sums the
verify modes' mismatch counters in the second file. Exit 0 only when every
compared cell is equal, no scene or frame is missing, and every mismatch
counter reads zero. Timing and probe-count columns are never compared.
"""
import csv
import sys

# Plans made and failed, door plans, who hunts, and which actors were updated at all.
DECISIONS = ["stacked_calls", "stacked_no_route", "flood_fills", "flood_no_route", "door_plans", "creatures_hunting",
             "guard_intents", "creature_updates", "friendly_updates"]
MISMATCHES = ["route_cache_mismatches", "streaming_mismatches"]


def load(path):
    rows = {}
    with open(path, newline="") as handle:
        for row in csv.DictReader(handle):
            rows[(row["scene"], int(row["frame"]))] = row
    return rows


def main(argv):
    paths = [a for a in argv if not a.startswith("--")]
    if len(paths) != 2:
        print(__doc__)
        return 2
    columns = DECISIONS
    for a in argv:
        if a.startswith("--columns="):
            columns = a.split("=", 1)[1].split(",")
    base, change = load(paths[0]), load(paths[1])
    failed = False
    missing = sorted(set(base) ^ set(change))
    if missing:
        failed = True
        print(f"frames in only one file: {len(missing)} (first {missing[0]})")
    shared = [k for k in base if k in change]
    absent = [c for c in columns if c not in next(iter(base.values()), {}) or c not in next(iter(change.values()), {})]
    if absent:
        print("columns missing from a file (skipped): " + ", ".join(absent))
    scenes = {}
    for key in shared:
        for column in columns:
            if column in absent:
                continue
            if float(base[key][column]) != float(change[key][column]):
                first = scenes.setdefault(key[0], {})
                first.setdefault(column, (key[1], base[key][column], change[key][column], 0))
                f, b, c, n = first[column]
                first[column] = (f, b, c, n + 1)
    for scene, differing in scenes.items():
        failed = True
        for column, (frame, b, c, n) in differing.items():
            print(f"DIFF {scene} {column}: {n} frames, first at frame {frame} ({b} -> {c})")
    for column in MISMATCHES:
        if column in next(iter(change.values()), {}):
            total = sum(float(r[column]) for r in change.values())
            print(f"{column} total {total:.0f}")
            failed |= total != 0
    print(f"{len(shared)} frames compared on {len(columns) - len(absent)} columns: {'FAIL' if failed else 'SAME'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
