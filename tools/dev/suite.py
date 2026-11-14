"""Run the whole --each-test suite fast: lanes in parallel, balanced by how long each test took last time.

  python tools/dev/suite.py EXE [--lanes N] [--only a,b,c] [--extras] [--out DIR]

Every test in main.cpp's --each-test table is dealt to N lanes (default: cores / 2, at most 8), the
slowest first, each to the lane with least work so far, using the times recorded in
.build/suite-times.json (a test never timed counts as 120 s). Each lane runs its own copy of EXE in
its own folder (tests write their reports into the working directory). --extras also runs the
stand-alone flags (--gore-test, --carcass-test, --campaign-map-test, --gameplay-rendering-test) as
one more lane. Lanes alternate between the machine's graphics cards (RM_GPU=0,1,... for each
lane; --gpus N, default 2). When every lane has finished, the times are written back, every FAIL is printed with
its lane, and the exit code is 1 if anything failed.
"""
import json, os, re, shutil, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TIMES = os.path.join(ROOT, '.build', 'suite-times.json')
EXTRAS = ['--gore-test', '--carcass-test', '--campaign-map-test', '--gameplay-rendering-test']


def suite_names():
    src = open(os.path.join(ROOT, 'src', 'main.cpp'), encoding='utf-8', errors='replace').read()
    i = src.find('const std::pair<const char*,bool(*)()> tests[]')
    return re.findall(r'\{"([a-z0-9-]+)",', src[i:src.find('};', i)])


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    exe = os.path.abspath(args[0])
    opt = lambda name, default=None: args[args.index(name) + 1] if name in args else default
    lanes = int(opt('--lanes', min(8, max(2, (os.cpu_count() or 4) // 2))))
    gpus = int(opt('--gpus', 2))
    env_for = lambda k: dict(os.environ, RM_GPU=str(k % gpus)) if gpus > 1 else None
    out = os.path.abspath(opt('--out', os.path.join(ROOT, '.build', 'suite')))
    names = opt('--only').split(',') if opt('--only') else suite_names()
    times = json.load(open(TIMES)) if os.path.exists(TIMES) else {}
    deal, load = [[] for _ in range(lanes)], [0.0] * lanes
    for n in sorted(names, key=lambda n: -times.get(n, 120)):
        k = load.index(min(load)); deal[k].append(n); load[k] += times.get(n, 120)
    print(f'suite: {len(names)} tests in {lanes} lanes, longest lane ~{int(max(load))} s by last times')
    procs = []
    for k, tests in enumerate(deal):
        if not tests:
            continue
        lane = os.path.join(out, f'lane{k + 1}'); os.makedirs(lane, exist_ok=True)
        shutil.copy2(exe, os.path.join(lane, 'rm.exe'))
        report = os.path.join(lane, 'each-test.txt')
        if os.path.exists(report):
            os.remove(report)
        procs.append((f'lane{k + 1}', lane, subprocess.Popen([os.path.join(lane, 'rm.exe'), '--each-test=' + ','.join(tests)], cwd=lane, env=env_for(k),
                                                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)))
    if '--extras' in args:
        lane = os.path.join(out, 'extras'); os.makedirs(lane, exist_ok=True); shutil.copy2(exe, os.path.join(lane, 'rm.exe'))
        script = ' && '.join(f'(rm.exe {f} > NUL 2>&1 && echo PASS {f} || echo FAIL {f}) >> extras.txt' for f in EXTRAS)
        if os.path.exists(os.path.join(lane, 'extras.txt')):
            os.remove(os.path.join(lane, 'extras.txt'))
        procs.append(('extras', lane, subprocess.Popen(['cmd', '/c', script.replace(' && ', ' & ')], cwd=lane, env=env_for(lanes))))
    start = time.time(); failed = []
    for name, lane, p in procs:
        p.wait()
        report = os.path.join(lane, 'each-test.txt' if name != 'extras' else 'extras.txt')
        text = open(report, errors='ignore').read() if os.path.exists(report) else ''
        for n, secs in re.findall(r'^PASS (\S+) ([\d.]+)s', text, re.M):
            times[n] = float(secs)
        for line in re.findall(r'^FAIL .*$', text, re.M):
            failed.append(f'{name}: {line}')
        print(f'  {name} done at {int(time.time() - start)} s, exit {p.returncode}')
    # A test that failed beside six others may have failed on load (a time budget, a GPU queue): run it
    # once more alone; only a second failure counts.
    if failed:
        lane = os.path.join(out, 'rerun'); os.makedirs(lane, exist_ok=True); shutil.copy2(exe, os.path.join(lane, 'rm.exe'))
        still = []
        for line in failed:
            m = re.search(r'FAIL (\S+)', line)
            if not m or m.group(1).startswith('--'):
                still.append(line); continue
            report = os.path.join(lane, 'each-test.txt')
            if os.path.exists(report):
                os.remove(report)
            subprocess.run([os.path.join(lane, 'rm.exe'), '--each-test=' + m.group(1)], cwd=lane, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            text = open(report, errors='ignore').read() if os.path.exists(report) else ''
            if re.search(rf'^PASS {re.escape(m.group(1))} ', text, re.M):
                print(f'  {m.group(1)} failed beside the other lanes and passed alone (load, not a fault)')
            else:
                still.append(line)
        failed = still
    os.makedirs(os.path.dirname(TIMES), exist_ok=True)
    json.dump(times, open(TIMES, 'w'), indent=1, sort_keys=True)
    print(f'suite: {int(time.time() - start)} s wall' + ('' if failed else ', ALL PASSED'))
    for f in failed:
        print('  ' + f)
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
