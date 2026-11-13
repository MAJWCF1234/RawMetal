"""The feature lab: build RawMetalLab fast, run feature tubes, write fixtures.
lab/README.md is the workflow; this is the glue.

  python tools/dev/lab.py build [--hot FILE[,FILE]] [--opt "/O2 /Ob1"] [--src DIR] [--dir DIR]
      Configure (once) and build RawMetalLab with Ninja into DIR (default
      .build/lab of this checkout or worktree). No global lock: every checkout
      and worktree has its own lab build. --hot compiles the files you are
      editing at /Od (a big renderer file rebuilds in seconds instead of a
      minute); --hot "" clears it. Prints the time and the files compiled.
  python tools/dev/lab.py run [RawMetalLab args...]
      Run the lab from the checkout root, e.g.
        lab.py run --list
        lab.py run --feature=ragdoll
        lab.py run --feature=ragdoll --with-friends --jobs=4
        lab.py run --feature=ragdoll+blood+chimera --frames=300
        lab.py run --feature=blood --live            (window; edit lab/tune.txt or a shader and watch)
      Output: .build/lab-out/report.html, metrics.json, <feature>-sheet.png.
  python tools/dev/lab.py fixture NAME --map N --at X Y [--z Z] [--yaw DEG] [--pitch P] [--world campaign|ashfall|room|door-room]
      Write lab/fixtures/NAME.lab for a place in a map.
  python tools/dev/lab.py fixture NAME --from-save PATH [--map N] [--world W]
      A fixture from a game save (the save is copied beside it). In a running
      game (--dev), the console command `labsnap NAME` writes both files itself
      under lab/fixtures (RM_LAB_FIXTURES overrides the folder).
  python tools/dev/lab.py tune NAME VALUE
      Set a knob in lab/tune.txt (RM_TUNE overrides); a running lab (or a game
      started with RM_TUNE) picks it up within a quarter second.
  python tools/dev/lab.py worker NAME [--base REF]
      A worker's own git worktree (.claude/worktrees/lab-NAME, branch lab/NAME),
      its src/assets/library junctioned to this checkout's, and its own lab
      build: no shared exe, no shared build dir, no lock. Prints the paths.
"""
import os, re, shutil, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

def opt(args, name, default=None):
    if name in args:
        i = args.index(name); v = args[i + 1]; del args[i:i + 2]; return v
    return default

def vcvars():
    vswhere = os.path.join(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'), 'Microsoft Visual Studio', 'Installer', 'vswhere.exe')
    path = subprocess.run([vswhere, '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'],
                          capture_output=True, text=True).stdout.strip().splitlines()
    if not path: raise SystemExit('Visual Studio C++ tools not found (vswhere)')
    return os.path.join(path[0], 'VC', 'Auxiliary', 'Build', 'vcvars64.bat')

def cmd_build(args, root=ROOT):
    args = list(args)
    src = os.path.abspath(opt(args, '--src', os.path.join(root, 'src')))
    d = os.path.abspath(opt(args, '--dir', os.path.join(root, '.build', 'lab')))
    opt_flags = opt(args, '--opt'); hot = opt(args, '--hot')
    os.makedirs(d, exist_ok=True)
    defs = []
    if opt_flags is not None: defs.append('"-DRAWMETAL_LAB_OPT=%s"' % opt_flags.replace(' ', ';'))
    if hot is not None: defs.append('"-DRAWMETAL_LAB_HOT=%s"' % hot.replace(',', ';'))
    steps = []
    if not os.path.exists(os.path.join(d, 'build.ninja')) or defs:
        steps.append('cmake -S "%s" -B "%s" -G Ninja -DCMAKE_BUILD_TYPE=Release %s' % (src, d, ' '.join(defs)))
    steps.append('cmake --build "%s" --target RawMetalLab' % d)
    script = os.path.join(d, 'lab-build.cmd')
    with open(script, 'w') as f:
        f.write('@echo off\ncall "%s" >nul 2>nul || exit /b 1\n' % vcvars())
        for s in steps: f.write(s + ' || exit /b 1\n')
    t0 = time.time()
    p = subprocess.run(['cmd', '/c', script], cwd=root, capture_output=True)
    out = (p.stdout + p.stderr).decode('mbcs', 'replace'); dt = time.time() - t0
    open(os.path.join(d, 'build.log'), 'w', encoding='utf-8').write(out)
    if p.returncode:
        errs = [l.strip() for l in out.splitlines() if re.search(r'\berror\b|: fatal ', l)]
        print('LAB BUILD FAILED (%.1f s)' % dt)
        for e in (errs or out.splitlines()[-40:])[:40]: print(' ', e)
        return 1
    compiled = len(re.findall(r'Building (?:CXX|C|RC) object', out))
    print('LAB BUILD OK in %.1f s (%d files compiled) -- %s' % (dt, compiled, os.path.join(d, 'RawMetalLab.exe')))
    return 0

def cmd_run(args):
    args = list(args); d = os.path.abspath(opt(args, '--dir', os.path.join(ROOT, '.build', 'lab')))
    exe = os.path.join(d, 'RawMetalLab.exe')
    if not os.path.exists(exe): raise SystemExit('no lab exe yet: python tools/dev/lab.py build')
    return subprocess.run([exe] + args, cwd=ROOT).returncode

def cmd_fixture(args):
    args = list(args); name = args.pop(0)
    folder = os.environ.get('RM_LAB_FIXTURES') or os.path.join(ROOT, 'lab', 'fixtures'); os.makedirs(folder, exist_ok=True)
    world = opt(args, '--world', 'campaign'); level = opt(args, '--map', '0'); save = opt(args, '--from-save')
    lines = ['# written by tools/dev/lab.py fixture', 'world %s' % world, 'map %s' % level]
    if '--at' in args:
        i = args.index('--at'); x, y = args[i + 1], args[i + 2]; del args[i:i + 3]
        lines.append('camera %s %s %s %s %s' % (x, y, opt(args, '--z', '-999'), opt(args, '--yaw', '0'), opt(args, '--pitch', '0')))
    if save:
        shutil.copy2(save, os.path.join(folder, name + '.sav')); lines.append('save %s.sav' % name)
    lines += ['size 480 270', 'frames 60']
    path = os.path.join(folder, name + '.lab'); open(path, 'w').write('\n'.join(lines) + '\n'); print(path); return 0

def cmd_tune(args):
    name, value = args[0], ' '.join(args[1:])
    path = os.environ.get('RM_TUNE') or os.path.join(ROOT, 'lab', 'tune.txt')
    lines = open(path, encoding='utf-8').read().splitlines() if os.path.exists(path) else []
    out, done = [], False
    for l in lines:
        w = l.split('#')[0].split()
        if w and w[0] == name: out.append('%s %s' % (name, value)); done = True
        else: out.append(l)
    if not done: out.append('%s %s' % (name, value))
    open(path, 'w', encoding='utf-8').write('\n'.join(out) + '\n'); print('%s = %s in %s' % (name, value, path)); return 0

def cmd_worker(args):
    args = list(args); name = args.pop(0); base = opt(args, '--base', 'HEAD')
    wt = os.path.join(ROOT, '.claude', 'worktrees', 'lab-' + name)
    if not os.path.exists(wt):
        r = subprocess.run(['git', '-C', ROOT, 'worktree', 'add', '-b', 'lab/' + name, wt, base])
        if r.returncode: return r.returncode
    lib_src = os.path.join(ROOT, 'src', 'assets', 'library'); lib_dst = os.path.join(wt, 'src', 'assets', 'library')
    if os.path.isdir(lib_src) and not os.path.exists(lib_dst):
        subprocess.run(['cmd', '/c', 'mklink', '/J', lib_dst, lib_src], capture_output=True)
    print('worker %s: worktree %s (branch lab/%s), lab build %s' % (name, wt, name, os.path.join(wt, '.build', 'lab')))
    return cmd_build([], root=wt)

if __name__ == '__main__':
    a = sys.argv[1:]
    if not a: print(__doc__); sys.exit(2)
    fn = {'build': cmd_build, 'run': cmd_run, 'fixture': cmd_fixture, 'tune': cmd_tune, 'worker': cmd_worker}.get(a[0])
    if not fn: print(__doc__); sys.exit(2)
    sys.exit(fn(a[1:]))
