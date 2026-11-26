"""Run the native map checks in isolated processes and write a short health report."""

import argparse
import collections
import concurrent.futures
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time


def run_native(exe, work, flag, campaign, timeout):
    args = [str(exe), flag]
    if campaign:
        args.append(f"--map-health-campaign={campaign}")
    return subprocess.run(args, cwd=work, capture_output=True, text=True,
                          errors="replace", timeout=timeout, check=True)


def check_map(exe, work, index, campaign, timeout):
    work = work / str(index)
    work.mkdir()
    try:
        run_native(exe, work, f"--map-health={index}", campaign, timeout)
        with (work / "map-health.tsv").open(encoding="utf-8", errors="replace") as stream:
            rows = [line.rstrip("\n").split("\t", 5) for line in stream]
        if len(rows) < 2 or rows[0][:2] != ["MAP", str(index)] or rows[-1] != ["END"]:
            raise ValueError("incomplete native result")
        if any(len(row) != 6 or row[0] != "FAIL" for row in rows[1:-1]):
            raise ValueError("invalid native result")
        return f"{index} {rows[0][2]}", [tuple(row[1:]) for row in rows[1:-1]]
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        return str(index), [("validator", "map worker", "tools/dev/validate_maps.py",
                             "0", str(error).replace("\n", " "))]


def reason_key(reason):
    # Audit messages start with a check name followed by object indices,
    # dimensions and coordinates. Keep the first complete message in the report.
    return re.sub(r"[-+]?\d+(?:\.\d+)?", "#", reason)


def write_report(out, results, seconds):
    groups = collections.defaultdict(list)
    failed = 0
    for label, issues in results:
        failed += bool(issues)
        for system, obj, file, line, reason in issues:
            groups[system, file, line, reason_key(reason)].append((label, obj, reason))
    summary = f"MAPS {len(results) - failed} PASS {failed} FAIL | {sum(map(len, groups.values()))} findings | {seconds:.1f}s"
    lines = [summary]
    # Leave the final line for an aggregate if the distinct groups exceed budget.
    for (system, file, line, _), hits in list(groups.items())[:97 if len(groups) > 98 else 98]:
        label, obj, reason = hits[0]
        extra = f" [x{len(hits)}, {len({hit[0] for hit in hits})} maps]" if len(hits) > 1 else ""
        lines.append(f"FAIL {system} | map {label} | {obj} | {file}:{line} | {reason}{extra}")
    if len(groups) > 98:
        omitted = list(groups.items())[97:]
        counts = collections.Counter()
        for key, hits in omitted:
            counts[key[0]] += len(hits)
        lines.append(f"FAIL {len(omitted)} additional groups | " +
                     ", ".join(f"{name} x{count}" for name, count in sorted(counts.items())))
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(summary)
    return int(failed > 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("--campaign", type=Path, help="validate one custom campaign instead of the built-in maps")
    parser.add_argument("--out", type=Path, default=Path("health.txt"))
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--timeout", type=float, default=90, help="seconds per native process")
    args = parser.parse_args()
    if args.jobs < 1 or args.timeout <= 0:
        parser.error("jobs and timeout must be positive")
    exe = args.exe.resolve()
    campaign = args.campaign.resolve() if args.campaign else None
    start = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="rawmetal-health-") as folder:
        work = Path(folder)
        try:
            run_native(exe, work, "--map-health-count", campaign, args.timeout)
            count = int((work / "map-health-count.txt").read_text())
            if count < 1:
                raise ValueError("no maps found")
        except (OSError, ValueError, subprocess.SubprocessError) as error:
            return write_report(args.out, [("catalog", [("validator", "catalog", "src/main.cpp", "0",
                                str(error).replace("\n", " "))])], time.monotonic() - start)
        with concurrent.futures.ThreadPoolExecutor(max_workers=min(count, args.jobs)) as pool:
            results = list(pool.map(lambda i: check_map(exe, work, i, campaign, args.timeout), range(count)))
    return write_report(args.out, results, time.monotonic() - start)


if __name__ == "__main__":
    raise SystemExit(main())
