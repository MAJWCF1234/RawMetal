"""Shared build and inspection runner for parallel map authors.

Several authors work in this one checkout at once, each on their own chapter
file. Builds are serialized by a system-wide mutex; each author then runs a
private copy of the exe in .build/agents/<name>/, so frames, audits and logs
never collide.

  python tools/dev/agent.py build  NAME [--own PATTERN ...]
      Build under the lock and copy RawMetal.exe to .build/agents/NAME/.
      If the build fails only in files that are not yours (--own patterns,
      e.g. CampaignChapterPower), it waits and retries: someone else is
      mid-edit. Prints the compiler errors otherwise.
  python tools/dev/agent.py shots  NAME LISTFILE [--scale 0.5]
      Render every line of LISTFILE ("name level x y feet yawDeg pitch" with
      x,y chunk-local, feet the absolute floor z, yaw 0=+x 90=+y, pitch in
      1/140 rad) with the private exe; writes PNGs to .build/agents/NAME/shot/.
  python tools/dev/agent.py audit  NAME LEVEL [LEVEL ...]
      Run --map-audit for each level; prints the verdict and any FAIL lines.
      The ASCII reach map is in .build/agents/NAME/map-audit-LEVEL.txt.
  python tools/dev/agent.py sweep  NAME LEVEL [--scale 0.5]
      Audit then render the four-direction sweep of every reachable 3 m block
      into .build/agents/NAME/sweep/ as PNGs (contact sheet: sweep-LEVEL.png).
  python tools/dev/agent.py check  NAME LEVEL [LEVEL ...] [--shots]
      Dump each level and run tools/dev/placement_check.py: seats turned from
      their tables, cabinets, screens, clocks and sinks facing the wall, and
      the like. --shots also renders every fault from the room side into
      .build/agents/NAME/check/sheet-NN.png for confirming by eye.
  python tools/dev/agent.py test   NAME FLAG [FLAG ...]
      Run test flags (e.g. --campaign-map-test) with the private exe.
  python tools/dev/agent.py selftest
      Check the source fingerprint that keeps a stale exe from being reused.

Every build records a fingerprint of src/** (path, size, mtime) next to the
shared exe and next to each private copy (rm-NAME.exe.src). The shared exe is
reused only while the whole tree still matches it, and every shots, audit,
sweep, check and test run prints STALE EXE when the private exe was built
from an older tree.
"""
import ctypes, glob, hashlib, json, os, re, shutil, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
kernel32 = ctypes.windll.kernel32

def agent_dir(name):
    d = os.path.join(ROOT, '.build', 'agents', name)
    os.makedirs(d, exist_ok=True)
    return d

class BuildLock:
    def __enter__(self):
        self.h = kernel32.CreateMutexW(None, False, 'Global\\RawMetalAgentBuild')
        t0 = time.time()
        while True:
            r = kernel32.WaitForSingleObject(self.h, 10000)
            if r in (0, 0x80):  # WAIT_OBJECT_0 / WAIT_ABANDONED
                break
            if time.time() - t0 > 3600:
                raise SystemExit('gave up waiting for the build lock')
            print('  waiting for the build lock (%ds)...' % int(time.time() - t0), flush=True)
        return self
    def __exit__(self, *a):
        kernel32.ReleaseMutex(self.h); kernel32.CloseHandle(self.h)

def run_build():
    # Only the shared build output is locked by a running game; private copies
    # (rm-<agent>.exe) and other worktrees' exes are left alone.
    target = os.path.join(ROOT, 'RawMetal.exe').replace("'", "''")
    subprocess.run(['powershell', '-NoProfile', '-Command',
                    "Get-Process RawMetal -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq '%s' } | Stop-Process -Force" % target],
                   capture_output=True)
    p = subprocess.run(['cmd', '/c', os.path.join(ROOT, 'Build.cmd')], cwd=ROOT, capture_output=True)
    out = (p.stdout + p.stderr).decode('mbcs', 'replace')
    return p.returncode, out

def errors_in(out):
    errs = []
    for line in out.splitlines():
        if re.search(r'\berror\b|: fatal ', line) and 'warning' not in line.lower():
            line = re.sub(r'\s*\[[^\]]*vcxproj\]\s*$', '', line.strip())
            if line not in errs: errs.append(line)
    return errs

STAMP = os.path.join(ROOT, '.build', 'agents', 'last-build.txt')

SHARED_SRC = os.path.join(ROOT, '.build', 'agents', 'last-build.src')  # fingerprint of the tree the shared exe was built from

def src_fingerprint(src=None):
    # Every file under src/** (build dirs skipped) as (path, size, mtime): an edit, addition or
    # removal anywhere -- shared headers, assets.rc, the model library -- changes the hash.
    src = src or os.path.join(ROOT, 'src'); h = hashlib.sha1(); files = {}
    for dp, dn, fn in os.walk(src):
        dn[:] = sorted(x for x in dn if x not in ('.build', '__pycache__'))
        for f in sorted(fn):
            p = os.path.join(dp, f); st = os.stat(p); r = os.path.relpath(p, src).replace(os.sep, '/')
            files[r] = st.st_mtime; h.update(('%s|%d|%d\n' % (r, st.st_size, st.st_mtime_ns)).encode('utf-8'))
    return h.hexdigest(), files

def exe_id(path):
    st = os.stat(path); return {'size': st.st_size, 'mtime_ns': st.st_mtime_ns}

def exe_summary(path):
    data = open(path, 'rb').read(); return '%s, %.1f MB, sha1 %s' % (os.path.basename(path), len(data) / 2**20, hashlib.sha1(data).hexdigest()[:12])

def read_json(path):
    try: return json.load(open(path, encoding='utf-8'))
    except (OSError, ValueError): return None

def write_json(path, rec):
    with open(path, 'w', encoding='utf-8') as f: json.dump(rec, f)

def ago(s):
    return '%d min' % (s // 60) if s >= 60 else '%d s' % s

def stale_reason(rec, src=None):
    # None when the exe described by rec was built from the tree as it is now; otherwise why not.
    if not rec or 'hash' not in rec: return 'no source fingerprint for this exe (built before fingerprints or by hand): run build'
    h, files = src_fingerprint(src)
    if h == rec['hash']: return None
    newer = sorted(((m, r) for r, m in files.items() if m > rec['started']), reverse=True)
    if newer: return 'src/%s changed %s after this build%s' % (newer[0][1], ago(newer[0][0] - rec['started']), ' (+%d more files)' % (len(newer) - 1) if len(newer) > 1 else '')
    gone = sorted(set(rec.get('files', ())) - set(files))
    return 'src tree differs from this build (%s)' % ('removed: ' + ', '.join(gone[:3]) if gone else 'a file was restored or replaced with an older timestamp')

def cmd_build(name, own):
    d = agent_dir(name); shared = os.path.join(ROOT, 'RawMetal.exe')
    for attempt in range(8):
        with BuildLock():
            # Reuse the shared exe only when it is the one the last agent build produced and the
            # whole src tree still matches the fingerprint taken when that build began.
            rec = read_json(SHARED_SRC)
            if rec and os.path.exists(shared) and rec.get('exe') == exe_id(shared) and stale_reason(rec) is None:
                shutil.copy2(shared, private_exe(name)); write_json(private_exe(name) + '.src', rec)
                print('BUILD OK (shared: src unchanged since the last build) -- ' + exe_summary(private_exe(name)))
                return 0
            started = time.time(); h, files = src_fingerprint()
            code, out = run_build()
            open(os.path.join(d, 'build.log'), 'w', encoding='utf-8').write(out)
            if code == 0:
                shutil.copy2(shared, private_exe(name))
                rec = {'hash': h, 'started': started, 'files': sorted(files), 'exe': exe_id(shared)}
                write_json(SHARED_SRC, rec); write_json(private_exe(name) + '.src', rec)
                open(STAMP, 'w').write(repr(started))
        if code == 0:
            print('BUILD OK -- ' + exe_summary(private_exe(name)))
            return 0
        errs = errors_in(out)
        mine = [e for e in errs if not own or any(o.lower() in e.lower() for o in own)]
        if errs and not mine and own:
            print('  build broken by someone else (%d errors, e.g. %s); retrying in 45 s' % (len(errs), errs[0][:160]), flush=True)
            time.sleep(45); continue
        print('BUILD FAILED')
        for e in (mine or errs)[:40]: print(' ', e)
        if not errs: print(out[-3000:])
        return 1
    print('BUILD FAILED: still broken by other files after retries'); return 1

def private_exe(name):
    return os.path.join(agent_dir(name), 'rm-%s.exe' % name)

def exe(name):
    e = private_exe(name)
    if not os.path.exists(e):
        old = os.path.join(agent_dir(name), 'RawMetal.exe')  # copies made before the rename
        if os.path.exists(old): shutil.copy2(old, e)
        else: raise SystemExit('no private exe yet: run build first')
    return e

def to_png(src, dst, scale):
    from PIL import Image
    im = Image.open(src)
    if scale != 1: im = im.resize((int(im.width * scale), int(im.height * scale)), Image.BILINEAR)
    im.save(dst)

_warned = set()
def warn_if_stale(name):
    # Once per command: say so loudly when the private exe predates the tree it is about to judge.
    if name in _warned: return
    _warned.add(name); why = stale_reason(read_json(exe(name) + '.src'))
    if why: print('STALE EXE: %s' % why, flush=True)

def run_exe(name, args, timeout=900):
    d = agent_dir(name); warn_if_stale(name)
    try: os.remove(os.path.join(d, 'RawMetal-error.txt'))
    except OSError: pass
    p = subprocess.run([exe(name)] + args, cwd=d, capture_output=True, timeout=timeout)
    err = os.path.join(d, 'RawMetal-error.txt')
    if os.path.exists(err): print('RawMetal-error.txt:', open(err, encoding='utf-8', errors='replace').read()[:2000])
    return p.returncode

def cmd_shots(name, listfile, scale):
    d = agent_dir(name); listfile = os.path.abspath(listfile)
    local = os.path.join(d, 'shots.txt'); shutil.copy2(listfile, local)
    names = [l.split()[0] for l in open(local) if l.strip() and not l.lstrip().startswith('#') and len(l.split()) >= 7]
    sd = os.path.join(d, '.build', 'shot'); os.makedirs(sd, exist_ok=True)
    for n in names:
        for ext in ('.ppm', '.png'):
            try: os.remove(os.path.join(sd, n + ext))
            except OSError: pass
    code = run_exe(name, ['--map-shots=shots.txt'])
    out = os.path.join(d, 'shot'); os.makedirs(out, exist_ok=True)
    made = []
    for n in names:
        src = os.path.join(sd, n + '.ppm')
        if os.path.exists(src):
            dst = os.path.join(out, n + '.png'); to_png(src, dst, scale); made.append(dst)
        else: print('  missing frame', n)
    print('exit %d, %d frames:' % (code, len(made)))
    for m in made: print(' ', m)

def cmd_audit(name, levels):
    d = agent_dir(name); bad = 0
    for lv in levels:
        code = run_exe(name, ['--map-audit=%s' % lv], timeout=300)
        f = os.path.join(d, 'map-audit-%s.txt' % lv)
        lines = open(f, encoding='utf-8', errors='replace').read().splitlines() if os.path.exists(f) else []
        verdict = lines[-1] if lines else 'NO AUDIT OUTPUT (exit %d)' % code
        print('map %s: %s' % (lv, verdict))
        for l in lines:
            if re.match(r'\s*(UNREACHABLE|STUCK|FLOATING|BURIED|OVERLAP|TRAP|BLOCKED|WATER|STAIRS|PIPE|GHOST)', l): print('   ', l.strip())
        bad |= code != 0
    return 1 if bad else 0

def cmd_sweep(name, level, scale):
    d = agent_dir(name)
    cmd_audit(name, [level])
    sw = os.path.join(d, '.build', 'sweep')
    for f in glob.glob(os.path.join(sw, '%s-*' % level)): os.remove(f)
    run_exe(name, ['--map-sweep=%s' % level], timeout=900)
    frames = sorted(glob.glob(os.path.join(sw, '%s-*-*.ppm' % level)), key=lambda p: [int(x) for x in re.findall(r'\d+', os.path.basename(p))])
    from PIL import Image
    if not frames: print('no sweep frames'); return
    tw, th = int(640 * scale), int(360 * scale)
    rows = (len(frames) + 3) // 4
    sheet = Image.new('RGB', (tw * 4, th * rows))
    for i, fpath in enumerate(frames):
        im = Image.open(fpath).resize((tw, th), Image.BILINEAR); sheet.paste(im, ((i % 4) * tw, (i // 4) * th))
    out = os.path.join(d, 'sweep-%s.png' % level); sheet.save(out)
    print('%d frames (rows = points, columns = +x, +y, -x, -y): %s' % (len(frames), out))

def cmd_check(name, levels, shots):
    d = agent_dir(name)
    for lv in levels: run_exe(name, ['--map-dump=%s' % lv], timeout=300)
    out = os.path.join(d, 'check.json')
    code = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'dev', 'placement_check.py'), d] + list(levels) + ['--json', out]).returncode
    if shots and code:
        subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'dev', 'placement_shots.py'), out, d, exe(name), os.path.join(d, 'check')])
    return code

def cmd_test(name, flags):
    rc = 0
    for fl in flags:
        code = run_exe(name, [fl], timeout=1800); print('%s: exit %d' % (fl, code)); rc |= code != 0
    return rc

def selftest():
    # A throwaway tree stands in for src/: the fingerprint must match itself and catch an edit to a
    # file no --own pattern covers (a shared header), a removed file and an old-timestamp restore.
    import tempfile
    t = tempfile.mkdtemp(); fails = []
    def put(r, text, age=0):
        p = os.path.join(t, *r.split('/')); os.makedirs(os.path.dirname(p), exist_ok=True); open(p, 'w').write(text)
        if age: os.utime(p, (time.time() - age, time.time() - age))
    def expect(label, ok):
        print('  %s %s' % ('PASS' if ok else 'FAIL', label)); ok or fails.append(label)
    def record(started):
        h, files = src_fingerprint(t); return {'hash': h, 'started': started, 'files': sorted(files)}
    try:
        for r in ('world/CampaignKit.h', 'world/CampaignChapterPower.cpp', 'assets.rc', '.build/x.obj'): put(r, r, 3600)
        rec = record(time.time() - 1800)
        expect('unchanged tree is fresh', stale_reason(rec, t) is None)
        put('.build/x.obj', 'rebuilt'); expect('build dirs ignored', stale_reason(rec, t) is None)
        put('world/CampaignKit.h', 'edited'); why = stale_reason(rec, t) or ''
        expect('shared header edit flagged: ' + why, why.startswith('src/world/CampaignKit.h changed 30 min after this build'))
        rec = record(time.time() + 1); expect('rebuilt tree is fresh again', stale_reason(rec, t) is None)
        os.remove(os.path.join(t, 'assets.rc')); why = stale_reason(rec, t) or ''
        expect('removed file flagged: ' + why, 'removed: assets.rc' in why)
        put('assets.rc', 'assets.rc', 7200); why = stale_reason(rec, t) or ''
        expect('old-timestamp restore flagged: ' + why, 'restored' in why)
        expect('exe without a fingerprint flagged', 'no source fingerprint' in (stale_reason(None, t) or ''))
    finally: shutil.rmtree(t, ignore_errors=True)
    print('agent selftest: ' + ('PASS' if not fails else 'FAIL (%d)' % len(fails))); return 1 if fails else 0

if __name__ == '__main__':
    a = sys.argv[1:]
    if a[:1] == ['selftest']: sys.exit(selftest())
    if len(a) < 2: print(__doc__); sys.exit(2)
    cmd, name, rest = a[0], a[1], a[2:]
    scale = 1.0
    if '--scale' in rest: i = rest.index('--scale'); scale = float(rest[i + 1]); rest = rest[:i] + rest[i + 2:]
    if cmd == 'build':
        own = []
        while '--own' in rest: i = rest.index('--own'); own.append(rest[i + 1]); rest = rest[:i] + rest[i + 2:]
        sys.exit(cmd_build(name, own))
    if cmd == 'shots': sys.exit(cmd_shots(name, rest[0], scale) or 0)
    if cmd == 'audit': sys.exit(cmd_audit(name, rest))
    if cmd == 'sweep': sys.exit(cmd_sweep(name, rest[0], scale or .5) or 0)
    if cmd == 'check':
        shots = '--shots' in rest; rest = [r for r in rest if r != '--shots']
        sys.exit(cmd_check(name, rest, shots))
    if cmd == 'test': sys.exit(cmd_test(name, rest))
    if cmd == 'locked':  # run a shell command while holding the build lock (lead only)
        with BuildLock(): sys.exit(subprocess.run(' '.join(rest), shell=True, cwd=ROOT).returncode)
    print(__doc__); sys.exit(2)
