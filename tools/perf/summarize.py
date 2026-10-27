"""Condense a --perf-profile CSV into one table row per scene.

    python tools/perf/summarize.py perf-profile.csv [more.csv ...] [--columns a,b,c]

Each cell is mean/p95/max ms per frame (sections) or mean/max calls per frame
(counters). Scenes keep the order they ran in. Pass several CSVs (a section-timed
run and a --perf-bare run, say) to compare them scene by scene.
"""
import csv
import sys
from collections import OrderedDict

SECTIONS = ["frame_ms", "update_ms", "render_ms", "enemies", "enemy_stacked", "enemy_repath", "enemy_door_plan",
            "enemy_sense", "enemy_stalk", "enemy_move", "stacked_waypoint", "friendly", "guard_mind", "guard_threat",
            "guard_schedule", "walk", "streaming", "chunk_needed", "voices", "combat", "door_requests"]
COUNTERS = ["creature_updates", "creatures_hunting", "stacked_calls", "stacked_no_route", "stacked_nodes", "flood_fills",
            "flood_no_route", "flood_edges",
            "navigable", "solid", "fits_calls", "support_below", "door_blocks", "ray_clear_steps", "state_lookups"]


def stats(values):
    ordered = sorted(values)
    p95 = ordered[min(len(ordered) - 1, int(0.95 * len(ordered)))]
    return sum(values) / len(values), p95, ordered[-1]


def load(path):
    scenes = OrderedDict()
    with open(path, newline="") as handle:
        for row in csv.DictReader(handle):
            scenes.setdefault(row["scene"], []).append(row)
    return scenes


def main(argv):
    paths = [a for a in argv if not a.startswith("--")]
    sections, counters = SECTIONS, COUNTERS
    for a in argv:
        if a.startswith("--columns="):
            chosen = a.split("=", 1)[1].split(",")
            sections = [c for c in chosen if c.endswith("_ms") or c not in COUNTERS]
            counters = [c for c in chosen if c in COUNTERS]
    for path in paths:
        scenes = load(path)
        print(f"# {path}")
        header = ["scene"] + [f"{c} mean/p95/max" for c in sections] + [f"{c} mean/max" for c in counters]
        print("\t".join(header))
        for name, rows in scenes.items():
            cells = [name]
            for column in sections:
                if column not in rows[0]:
                    cells.append("-")
                    continue
                mean, p95, top = stats([float(r[column]) for r in rows])
                cells.append(f"{mean:.2f}/{p95:.2f}/{top:.2f}")
            for column in counters:
                if column not in rows[0]:
                    cells.append("-")
                    continue
                mean, _, top = stats([float(r[column]) for r in rows])
                cells.append(f"{mean:.0f}/{top:.0f}")
            print("\t".join(cells))
        print()


if __name__ == "__main__":
    main(sys.argv[1:])
