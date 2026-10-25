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
  python tools/dev/agent.py test   NAME FLAG [FLAG ...]
      Run test flags (e.g. --campaign-map-test) with the private exe.
"""
import ctypes, glob, os, re, shutil, subprocess, sys, time

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
    subprocess.run('taskkill /F /IM RawMetal.exe', shell=True, capture_output=True)
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

def own_mtime(own):
    # Newest edit among the caller's files (src/**/*PATTERN*); 0 when unknown.
    newest = 0
    for o in own:
        for f in glob.glob(os.path.join(ROOT, 'src', '**', '*%s*' % o), recursive=True):
            newest = max(newest, os.path.getmtime(f))
    return newest

def cmd_build(name, own):
    d = agent_dir(name)
    for attempt in range(8):
        with BuildLock():
            # A build that started after your last edit already contains it:
            # take its exe instead of building again.
            last = float(open(STAMP).read()) if os.path.exists(STAMP) else 0
            mine = own_mtime(own)
            if own and mine and last > mine and os.path.exists(os.path.join(ROOT, 'RawMetal.exe')):
                shutil.copy2(os.path.join(ROOT, 'RawMetal.exe'), os.path.join(d, 'RawMetal.exe'))
                print('BUILD OK (shared: a build that started after your last edit)')
                return 0
            started = time.time()
            code, out = run_build()
            open(os.path.join(d, 'build.log'), 'w', encoding='utf-8').write(out)
            if code == 0:
                shutil.copy2(os.path.join(ROOT, 'RawMetal.exe'), os.path.join(d, 'RawMetal.exe'))
                open(STAMP, 'w').write(repr(started))
        size = [l for l in out.splitlines() if 'RawMetal executable' in l]
        if code == 0:
            print('BUILD OK' + (' -- ' + size[0].strip(' -') if size else ''))
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

def exe(name):
    e = os.path.join(agent_dir(name), 'RawMetal.exe')
    if not os.path.exists(e): raise SystemExit('no private exe yet: run build first')
    return e

def to_png(src, dst, scale):
    from PIL import Image
    im = Image.open(src)
    if scale != 1: im = im.resize((int(im.width * scale), int(im.height * scale)), Image.BILINEAR)
    im.save(dst)

def run_exe(name, args, timeout=900):
    d = agent_dir(name)
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
            if re.match(r'\s*(UNREACHABLE|STUCK|FLOATING|BURIED|OVERLAP|TRAP|BLOCKED|WATER|STAIRS|PIPE)', l): print('   ', l.strip())
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

def cmd_test(name, flags):
    rc = 0
    for fl in flags:
        code = run_exe(name, [fl], timeout=1800); print('%s: exit %d' % (fl, code)); rc |= code != 0
    return rc

if __name__ == '__main__':
    a = sys.argv[1:]
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
    if cmd == 'test': sys.exit(cmd_test(name, rest))
    if cmd == 'locked':  # run a shell command while holding the build lock (lead only)
        with BuildLock(): sys.exit(subprocess.run(' '.join(rest), shell=True, cwd=ROOT).returncode)
    print(__doc__); sys.exit(2)
