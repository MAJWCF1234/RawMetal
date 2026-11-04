"""Run registered engine checks in isolated processes with bounded runtimes.

Examples: python tools/hunt_issues.py --tests movement,gameplay-fixes --repeat 2
          python tools/hunt_issues.py --no-core --deep --timeout 300
Reports and individual test artifacts are saved under .build/issue-hunt/.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / '.build' / 'issue-hunt'

# These focused executable checks are not part of --each-test. Their report
# names are explicit, so an unknown/ignored command cannot look successful.
EXTRA = {
    'deep-guard-combat': ('--guard-combat-test', 'guard-combat-test.txt'),
    'deep-guard-tactics': ('--guard-tactics-test', 'guard-tactics-test.txt'),
    'deep-guard-policy': ('--guard-policy-test', 'guard-policy-test.txt'),
    'deep-guard-defense': ('--guard-defense-test', 'guard-defense-test.txt'),
    'deep-guard-pursuit': ('--guard-pursuit-test', 'guard-pursuit-test.txt'),
    'deep-guard-actions': ('--guard-actions-test', 'guard-action-test.txt'),
    'deep-guard-roles': ('--guard-roles-test', 'guard-role-test.txt'),
    'deep-guard-table': ('--guard-table-test', 'guard-table-test.txt'),
    'deep-guard-cover': ('--guard-cover-test', 'guard-cover-test.txt'),
    'deep-guard-patient': ('--guard-patient-test', 'guard-patient-test.txt'),
    'deep-guard-schedule': ('--guard-schedule-test', 'guard-schedule-test.txt'),
    'deep-guard-danger': ('--guard-danger-test', 'guard-danger-test.txt'),
    'deep-guard-ai': ('--guard-ai-test', 'guard-ai-test.txt'),
    'deep-creature-schedule': ('--creature-schedule-test', 'creature-schedule-test.txt'),
    'deep-civilian-schedule': ('--civilian-schedule-test', 'civilian-schedule-test.txt'),
    'deep-engineer-drone': ('--engineer-drone-test', 'engineer-drone-test.txt'),
    'deep-tactical-grenade': ('--tactical-grenade-test', 'tactical-grenade-test.txt'),
    'deep-ai-query': ('--ai-query-test', 'ai-query-test.txt'),
    'deep-ai-decision': ('--ai-decision-test', 'ai-decision-test.txt'),
    'deep-terrain-query': ('--terrain-query-test', 'terrain-query-test.txt'),
    'deep-nav-graph': ('--nav-graph-test', 'nav-graph-test.txt'),
    'deep-prologue': ('--prologue-test', 'prologue-test.txt'),
    'deep-exclusion': ('--exclusion-test', 'exclusion-test.txt'),
    'deep-exclusion-authoring': ('--exclusion-authoring-test', 'exclusion-authoring-test.txt'),
    'deep-world-isolation': ('--world-isolation-test', 'world-isolation-test.txt'),
    'deep-vr-math': ('--vr-test', 'vr-test.txt'),
}
GPU = {
    'gpu-lighting-stability': ('--gameplay-rendering-test', 'gameplay-rendering-test.txt'),
    'gpu-vulkan': ('--vulkan-test', 'vulkan-test.txt'),
    'gpu-actors': ('--gpu-actor-test', 'gpu-actor-test.txt'),
    'gpu-visibility': ('--visibility-test', 'visibility-test.txt'),
}
STATIC = {'static-integrity': ('static_issue_checks.py', 'static-check.txt')}


def registered_tests(source):
    marker = 'const std::pair<const char*,bool(*)()> tests[]={'
    if marker not in source:
        raise ValueError('Could not locate the registered test list in src/main.cpp')
    block = source.split(marker, 1)[1].split('};', 1)[0]
    names = re.findall(r'\{"([a-z0-9-]+)",', block)
    if not names or len(names) != len(set(names)):
        raise ValueError('Could not discover a unique test list from src/main.cpp')
    return names


def verify_report(name, code, timed_out, report):
    matches = re.findall(r'^(PASS|FAIL) ([a-z0-9-]+) ', report, re.MULTILINE)
    return (not timed_out and code == 0 and matches == [('PASS', name)]
            and re.search(r'^ALL PASSED 0\s*$', report, re.MULTILINE) is not None), matches


def verify_detail(code, timed_out, report):
    # Accept the native tests' two report conventions, while rejecting empty,
    # incomplete, or contradictory reports even if the process returned zero.
    passes = re.findall(r'(?m)^(?:PASS(?:\b|:)|.*:\s*PASS\s*$|.*result=PASS\s*$)', report)
    failures = re.findall(r'(?m)^(?:FAIL(?:\b|:)|.*:\s*FAIL\s*$|.*result=FAIL\s*$)', report)
    return not timed_out and code == 0 and bool(passes) and not failures, len(passes), len(failures)


def verify_audit(level, code, timed_out, report):
    passed, checks_passed, checks_failed = verify_detail(code, timed_out, report)
    match = re.search(r'^Map (\d+) \([^\n]*\): (\d+) reachable cells \((\d+) standing\) of (\d+)', report, re.MULTILINE)
    grid = [line for line in report.splitlines() if len(line) == 24 and set(line) <= set('#.^oc ')]
    # These counts include distinct height states, not unique horizontal cells;
    # separate standing and crouched flood fills can merge heights differently.
    complete = bool(match and int(match[1]) == level and int(match[2]) > 0 and
                    int(match[3]) > 0 and int(match[4]) == 9216 and len(grid) == 24 and
                    any(mark in line for line in grid for mark in '.^c'))
    return passed and complete, checks_passed, checks_failed, complete


def extract_findings(kind, report):
    if kind == 'audit':
        labels = ('TRAP ', 'UNREACHABLE ', 'STUCK ', 'HULL INTERSECTION ', 'BLOCKED ',
                  'DOOR STEP ', 'EXIT STEP ', 'OUTSIDE', 'FLOATING ', 'BURIED ',
                  'HANGING ', 'OVERLAP ', 'FRAMELESS ', 'DISTORTED ', 'TOO HEAVY ',
                  'WATER ABOVE ', 'STAIRS TOUCH ', 'PIPE THROUGH ', 'SIGNAGE ',
                  'AREA TITLE ', 'CLUTTER ', 'ERROR ')
        return [line for line in report.splitlines() if line.startswith(labels)]
    return [line for line in report.splitlines() if line.startswith('DETAIL ') or re.search(r'^(?:FAIL(?:\b|:)|.*:\s*FAIL\s*$|.*result=FAIL\s*$)', line)]


def file_sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def select_checks(core, requested, deep, gpu, no_core, levels):
    selected = requested.split(',') if requested is not None else ([] if no_core else list(core))
    for group in (EXTRA if deep else (), GPU if gpu else (), ('map-' + str(level) for level in levels)):
        selected += [name for name in group if name not in selected]
    return selected


def iteration_seed(seed, iteration):
    return (seed + iteration - 1) & 0xffffffff


def campaign_names(source):
    marker = 'CampaignMapNames{{'
    if marker not in source:
        return []
    block = source.split(marker, 1)[1].split('}};', 1)[0]
    block = re.sub(r'//[^\n]*', '', block)
    return re.findall(r'"([^"\n]*)"', block)


def newer_sources(exe):
    # A timestamp warning is a hint, not proof that this executable is stale.
    built = exe.stat().st_mtime_ns
    newer = []
    for path in (ROOT / 'src').rglob('*'):
        if path.suffix.lower() not in ('.h', '.hpp', '.c', '.cpp') or 'ThirdParty' in path.parts:
            continue
        try:
            if path.stat().st_mtime_ns > built + 1_000_000_000:
                newer.append(str(path.relative_to(ROOT)))
        except OSError:
            continue
    return sorted(newer)


def parse_maps(value):
    levels = []
    for part in value.split(','):
        if not re.fullmatch(r'\d+(?:-\d+)?', part):
            raise ValueError('map selection must be numbers or inclusive ranges, e.g. 0-3,64')
        bounds = list(map(int, part.split('-')))
        first, last = bounds[0], bounds[-1]
        if first > last or last > 179:
            raise ValueError('map numbers must be in ascending ranges from 0 through 179')
        levels.extend(range(first, last + 1))
    if len(levels) != len(set(levels)):
        raise ValueError('map selection contains duplicates')
    return levels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tests', help='comma-separated names; default: all registered tests')
    parser.add_argument('--list', action='store_true', help='list available core and focused checks')
    parser.add_argument('--deep', action='store_true', help='also run focused AI, schedule, navigation, VR math and world checks')
    parser.add_argument('--gpu', action='store_true', help='also run hardware rendering and Vulkan checks')
    parser.add_argument('--static', action='store_true', help='run fast source/content integrity checks')
    parser.add_argument('--no-core', action='store_true', help='run only explicitly enabled deep, GPU or map checks')
    parser.add_argument('--maps', help='audit campaign maps, e.g. 0-73,179 (74-178 are unfinished placeholders)')
    parser.add_argument('--repeat', type=int, default=1)
    parser.add_argument('--seed', type=lambda value: int(value, 0), default=0x4257a21,
                        help='initial collision-test seed (decimal or 0x hex); each repeat advances it')
    parser.add_argument('--timeout', type=float, default=300)
    parser.add_argument('--exe', type=Path, default=ROOT / 'RawMetal.exe')
    args = parser.parse_args()
    if args.repeat < 1 or args.timeout <= 0 or not 0 <= args.seed <= 0xffffffff:
        parser.error('--repeat and --timeout must be positive; --seed must fit an unsigned 32-bit integer')
    names = registered_tests((ROOT / 'src/main.cpp').read_text(encoding='utf-8'))
    specs = {name: ('--each-test=' + name, 'each-test.txt', 'each') for name in names}
    for name, (flag, report) in {**EXTRA, **GPU}.items():
        specs[name] = (flag, report, 'detail')
    for name, (script, report) in STATIC.items():
        specs[name] = (script, report, 'static')
    if args.list:
        for label, group in [('Core', names), ('Deep', EXTRA), ('GPU', GPU), ('Static', STATIC)]:
            print(label + ': ' + ', '.join(group))
        print('Map audits: --maps 0-73,179 (add --no-core to run only selected profiles)')
        return 0
    try:
        levels = parse_maps(args.maps) if args.maps else []
    except ValueError as exc:
        parser.error(str(exc))
    for level in levels:
        specs['map-' + str(level)] = ('--map-audit=' + str(level), 'map-audit-' + str(level) + '.txt', 'audit')
    selected = select_checks(names, args.tests, args.deep, args.gpu, args.no_core, levels)
    if args.static and 'static-integrity' not in selected:
        selected.append('static-integrity')
    if not selected or len(selected) != len(set(selected)) or any(n not in specs for n in selected):
        parser.error('unknown, empty, or repeated test name; available: ' + ', '.join(specs))
    exe = args.exe.resolve()
    if not exe.is_file():
        parser.error('executable not found: ' + str(exe))
    initial_digest = file_sha256(exe)
    newer = newer_sources(exe)
    if newer:
        print('WARN: %d source files are newer than the executable; the build may be stale' % len(newer), flush=True)
    labels = campaign_names((ROOT / 'src/world/WorldDefinition.h').read_text(encoding='utf-8')) if levels else []
    OUT.mkdir(parents=True, exist_ok=True)
    # A unique directory avoids confusing stale files with this run's output.
    run_dir = Path(tempfile.mkdtemp(prefix='run-', dir=OUT))
    results = []
    for name in selected:
        for iteration in range(1, args.repeat + 1):
            folder = run_dir / name / str(iteration)
            folder.mkdir(parents=True)
            temp = folder / 'tmp'
            temp.mkdir()
            seed = iteration_seed(args.seed, iteration)
            env = dict(os.environ, TEMP=str(temp), TMP=str(temp), RAWMETAL_ISSUE_SEED=str(seed))
            start = time.monotonic()
            flag, report_name, kind = specs[name]
            try:
                if name in ('deep-nav-graph', 'deep-exclusion', 'deep-exclusion-authoring', 'gpu-actors', 'gpu-visibility'):
                    source = ROOT / 'custom maps' / 'RawMetal-Ashfall-Exclusion-Zone.txt'
                    if source.exists() or not name.startswith('gpu-'):
                        (folder / 'custom maps').mkdir()
                        shutil.copy2(source, folder / 'custom maps' / source.name)
                command = [sys.executable, str(ROOT / 'tools' / flag)] if kind == 'static' else [str(exe), flag]
                proc = subprocess.run(command, cwd=folder,
                                      env=env, capture_output=True, timeout=args.timeout)
                code, stdout, stderr = proc.returncode, proc.stdout, proc.stderr
                timed_out = False
            except subprocess.TimeoutExpired as exc:
                code, stdout, stderr, timed_out = None, exc.stdout or b'', exc.stderr or b'', True
            except OSError as exc:
                code, stdout, stderr, timed_out = None, b'', str(exc).encode('utf-8', errors='replace'), False
            (folder / 'stdout.log').write_bytes(stdout)
            (folder / 'stderr.log').write_bytes(stderr)
            report_path = folder / report_name
            report = report_path.read_text(encoding='utf-8', errors='replace') if report_path.exists() else ''
            # The executable returns success for an unrecognized test selector.
            # Require exactly the requested PASS and the suite completion marker.
            audit_complete = None
            if kind == 'static':
                report_path.write_bytes(stdout)
                report = report_path.read_text(encoding='utf-8', errors='replace')
                passed, checks_passed, checks_failed = verify_detail(code, timed_out, report)
                matches = []
                audit_complete = None
            elif kind == 'each':
                passed, matches = verify_report(name, code, timed_out, report)
                checks_passed, checks_failed = len(matches), sum(item[0] == 'FAIL' for item in matches)
            elif kind == 'audit':
                passed, checks_passed, checks_failed, audit_complete = verify_audit(int(name[4:]), code, timed_out, report)
                matches = []
            else:
                passed, checks_passed, checks_failed = verify_detail(code, timed_out, report)
                matches = []
            findings = extract_findings(kind, report)
            level = int(name[4:]) if kind == 'audit' else None
            map_name = labels[level] if level is not None and level < len(labels) else None
            result = dict(name=name, map_name=map_name, iteration=iteration, passed=passed, timeout=timed_out,
                          exit_code=code, seconds=round(time.monotonic()-start, 3),
                          report_present=report_path.exists(), observed=matches, checks_passed=checks_passed,
                          checks_failed=checks_failed, audit_complete=audit_complete, flag=flag, seed=seed, findings=findings,
                          artifacts=str(folder.relative_to(ROOT)))
            results.append(result)
            print(('PASS' if passed else 'FAIL'), name, ('(' + map_name + ')') if map_name else '', iteration, result['seconds'], flush=True)
            if not passed:
                for finding in findings[:3]:
                    print('  ', finding, flush=True)
            (run_dir / 'results.json').write_text(json.dumps({'executable': str(exe), 'sha256_at_start': initial_digest,
                                                             'complete': False, 'results': results}, indent=2), encoding='utf-8')
    try:
        final_digest = file_sha256(exe)
    except OSError:
        final_digest = None
    unchanged = initial_digest == final_digest
    summary = dict(executable=str(exe), sha256=initial_digest, sha256_at_end=final_digest,
                   executable_unchanged=unchanged, complete=True, timeout_seconds=args.timeout,
                   newer_source_files=newer, results=results, passed=unchanged and all(r['passed'] for r in results))
    (run_dir / 'results.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print('Evidence:', run_dir, flush=True)
    if not unchanged:
        print('FAIL: executable changed or disappeared during the run', flush=True)
    return 0 if summary['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
