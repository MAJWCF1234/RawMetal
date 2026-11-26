"""Run RawMetal's --each-test suite in parallel and print a summary plus any failures.

Tests are assigned to the least-loaded lane, starting with the slowest based on
.build/suite-times.json (120 s for new tests). Each lane gets a copy of the EXE
and a working directory for its reports. --extras runs standalone test flags
sequentially in a separate lane. --affected selects tests since the last release
or a supplied --base. Lanes alternate GPUs when --gpus is greater than one.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TIMES = ROOT / '.build' / 'suite-times.json'
EXTRAS = ['--gore-test', '--carcass-test', '--campaign-map-test', '--gameplay-rendering-test']
RESULT = re.compile(r'(PASS|FAIL) (\S+) (\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)s')
sys.path.insert(0, str(Path(__file__).resolve().parent))
import affected as affected_mod  # noqa: E402


def suite_names():
    src = (ROOT / 'src' / 'main.cpp').read_text(encoding='utf-8', errors='replace')
    # Inline tests contain their own closing braces. Stop at the report
    # declaration immediately after the table instead of the first '};'.
    table = re.search(
        r'const std::pair<const char\*,bool\(\*\)\(\)> tests\[\]\s*=\s*\{(.*?)'
        r'\};\s*std::ofstream report\("each-test\.txt"\)',
        src, re.S,
    )
    names = re.findall(r'\{"([a-z0-9-]+)",', table[1]) if table else []
    if not names:
        raise ValueError('could not find tests in src/main.cpp')
    return names


def lane_exe(exe, lane, report_name):
    lane.mkdir(parents=True, exist_ok=True)
    (lane / report_name).unlink(missing_ok=True)
    return shutil.copy2(exe, lane / 'rm.exe')


def read_lane(report, tests, exit_code, times):
    """Return failed tests and the explicit failures eligible for a serial retry."""
    try:
        lines = report.read_text(encoding='utf-8', errors='replace').splitlines()
    except FileNotFoundError:
        lines = []
    except OSError as error:
        return dict.fromkeys(tests, f'could not read {report.name}: {error}'), []

    results = {}
    duplicate = set()
    for line in lines:
        match = RESULT.fullmatch(line)
        if match:
            status, test, seconds = match.groups()
            if test in results:
                duplicate.add(test)
            results[test] = (status, float(seconds), line)

    missing = set(tests) - results.keys()
    unexpected = results.keys() - set(tests)
    unknown = next((line for line in lines if line.startswith("FAIL unknown test ")), '')
    failed_count = sum(status == 'FAIL' for status, _, _ in results.values())
    footer = f'FAILED {failed_count}' if failed_count else 'ALL PASSED 0'
    problem = None
    if not lines:
        problem = f'no {report.name} (exit {exit_code})'
    elif unknown:
        problem = f'{unknown} (exit {exit_code})'
    elif unexpected or duplicate:
        invalid = ', '.join(sorted(unexpected | duplicate))
        problem = f'unexpected or duplicate results for {invalid} (exit {exit_code})'
    elif not missing and (lines[-1] != footer or exit_code != int(bool(failed_count))):
        problem = f'lane did not complete cleanly (exit {exit_code})'
    if problem:
        return dict.fromkeys(tests, problem), []

    failures = {}
    retry = []
    for test in tests:
        if test in missing:
            failures[test] = f'no result in {report.name} (exit {exit_code})'
            continue
        status, seconds, line = results[test]
        if status == 'PASS':
            times[test] = seconds
        else:
            failures[test] = line
            if not missing:
                retry.append(test)
    return failures, retry


def summarize(total, failures, start):
    print(f'suite: {total - len(failures)} passed, {len(failures)} failed '
          f'in {time.monotonic() - start:.1f} s')
    for failure in failures:
        print(f'  {failure}')
    return int(bool(failures))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('exe', type=Path)
    parser.add_argument('--lanes', type=int, default=min(8, max(2, (os.cpu_count() or 4) // 2)))
    parser.add_argument('--gpus', type=int, default=2)
    parser.add_argument('--out', type=Path, default=ROOT / '.build' / 'suite')
    parser.add_argument('--only')
    parser.add_argument('--affected', action='store_true')
    parser.add_argument('--base')
    parser.add_argument('--extras', action='store_true')
    args = parser.parse_args()
    if args.lanes < 1:
        parser.error('--lanes must be at least 1')

    start = time.monotonic()
    try:
        known = suite_names()
    except (OSError, ValueError) as error:
        return summarize(1, [f'FAIL suite discovery: {error}'], start)
    names = known
    extras = EXTRAS if args.extras else []
    if args.affected:
        selected = affected_mod.select(args.base, known)
        if selected['mode'] == 'none':
            return summarize(0, [], start)
        if selected['mode'] == 'subset':
            names = selected['tests']
            extras = selected['extras'] if args.extras else []
    elif args.only:
        names = list(dict.fromkeys(args.only.split(',')))
    unknown = [test for test in names if test not in known]
    failures = {('selection', test): f'FAIL unknown test {test!r}' for test in unknown}
    names = [test for test in names if test in known]
    total = len(names) + len(unknown) + len(extras)
    if not names and not extras:
        return summarize(total, failures.values(), start)

    times_path = Path(TIMES)
    times = json.loads(times_path.read_text(encoding='utf-8')) if times_path.exists() else {}
    deal, load = [[] for _ in range(args.lanes)], [0.0] * args.lanes
    for test in sorted(names, key=lambda test: -times.get(test, 120)):
        k = load.index(min(load))
        deal[k].append(test)
        load[k] += times.get(test, 120)

    def env_for(k):
        return dict(os.environ, RM_GPU=str(k % args.gpus)) if args.gpus > 1 else None

    exe = args.exe.resolve()
    out = args.out.resolve()
    quiet = dict(stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    procs = []
    for k, tests in enumerate(deal):
        if not tests:
            continue
        lane = out / f'lane{k + 1}'
        try:
            runner = lane_exe(exe, lane, 'each-test.txt')
            proc = subprocess.Popen([runner, '--each-test=' + ','.join(tests)],
                                    cwd=lane, env=env_for(k), **quiet)
        except OSError as error:
            for test in tests:
                failures[lane.name, test] = f'{lane.name}: FAIL {test}: could not start: {error}'
            continue
        procs.append((lane, tests, proc))

    # Normal lanes are already running. Run extras in their own directory and
    # as a single lane so their shared report files cannot collide.
    if extras:
        lane = out / 'extras'
        try:
            runner = lane_exe(exe, lane, 'extras.txt')
            with (lane / 'extras.txt').open('w', encoding='utf-8') as report:
                for flag in extras:
                    try:
                        code = subprocess.run([runner, flag], cwd=lane,
                                              env=env_for(args.lanes), **quiet).returncode
                        result = f'FAIL {flag} (exit {code})' if code else f'PASS {flag}'
                    except OSError as error:
                        result = f'FAIL {flag}: could not start: {error}'
                    report.write(result + '\n')
                    report.flush()
                    if result.startswith('FAIL '):
                        failures[lane.name, flag] = f'extras: {result}'
        except OSError as error:
            for flag in extras:
                failures[lane.name, flag] = f'extras: FAIL {flag}: {error}'

    retry = []
    for lane, tests, proc in procs:
        report = lane / 'each-test.txt'
        failed, retry_tests = read_lane(report, tests, proc.wait(), times)
        for test, reason in failed.items():
            if test in retry_tests:
                retry.append((lane.name, test, reason))
            else:
                failures[lane.name, test] = f'{lane.name}: FAIL {test}: {reason}'

    # Retain original lane reports, and retry ordinary test failures in isolation.
    lane = out / 'rerun'
    for original_lane, test, reason in retry:
        try:
            runner = lane_exe(exe, lane, 'each-test.txt')
            proc = subprocess.run([runner, '--each-test=' + test], cwd=lane, **quiet)
            report = lane / 'each-test.txt'
            if report.exists():
                shutil.copyfile(report, lane / f'each-test-{test}.txt')
            failed, _ = read_lane(report, [test], proc.returncode, times)
        except OSError as error:
            failed = {test: f'could not retry: {error}'}
        if test in failed:
            failures[original_lane, test] = f'{original_lane}: {reason} (serial retry: {failed[test]})'

    times_path.parent.mkdir(parents=True, exist_ok=True)
    times_path.write_text(json.dumps(times, indent=1, sort_keys=True), encoding='utf-8')
    return summarize(total, failures.values(), start)


if __name__ == '__main__':
    raise SystemExit(main())
