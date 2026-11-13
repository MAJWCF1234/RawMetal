"""Install a custom map campaign (.rmc) where the game and the Campaign Builder will find it, and check it loads.

  python tools/release/install_map.py "path/to/My Map.rmc"                 # into this checkout's game and Builder
  python tools/release/install_map.py "My Map.rmc" --to "D:/Games/DepthWorks"  # also into an unzipped release folder
  python tools/release/install_map.py --list                                # what is installed

It copies the .rmc (and its .rmc.project.json if there is one, which keeps the Builder's editing state) into:
  custom maps/                          (the game in this checkout: RawMetal.exe)
  tools/Level Editor/custom maps/       (the Campaign Builder and its play button)
  <--to folder>/custom maps/            (each release folder named; the game reads "custom maps" beside DepthWorks.exe)
then runs the game's own loader over the installed campaign (RawMetal.exe --custom-map-smoke=FILE, from this
checkout: every map loaded, stepped and checked) and prints what it reports, so a map that will not load is caught before anyone plays it.
In the game, custom campaigns are under the title menu's CUSTOM MAPS.
"""
import os, shutil, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TARGETS = [os.path.join(ROOT, 'custom maps'), os.path.join(ROOT, 'tools', 'Level Editor', 'custom maps')]


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    if '--list' in args:
        for t in TARGETS:
            print(t)
            for name in sorted(os.listdir(t)) if os.path.isdir(t) else []:
                if name.lower().endswith('.rmc'):
                    print('   ', name)
        return
    source = os.path.abspath(args[0])
    if not source.lower().endswith('.rmc') or not os.path.isfile(source):
        sys.exit(f'install_map: not a .rmc file: {source}')
    targets = list(TARGETS)
    if '--to' in args:
        release = args[args.index('--to') + 1]
        targets.append(os.path.join(release, 'custom maps'))
    files = [source] + ([source + '.project.json'] if os.path.isfile(source + '.project.json') else [])
    for t in targets:
        os.makedirs(t, exist_ok=True)
        for f in files:
            if os.path.abspath(os.path.dirname(f)) != os.path.abspath(t):
                shutil.copy2(f, t)
        print(f'installed {os.path.basename(source)} -> {t}')
    exe = os.path.join(ROOT, 'RawMetal.exe')
    if not os.path.isfile(exe):
        print('install_map: RawMetal.exe not built here, so the map was not load-checked')
        return
    installed = os.path.join(TARGETS[0], os.path.basename(source))
    result = subprocess.run([exe, '--custom-map-smoke=' + installed], cwd=ROOT)
    report = os.path.join(ROOT, 'RawMetal-custom-map-smoke.txt')
    if os.path.isfile(report):
        text = open(report, encoding='utf-8', errors='replace').read()
        mine = [l for l in text.splitlines() if os.path.splitext(os.path.basename(source))[0].lower() in l.lower() or 'FAIL' in l]
        print('\n'.join(mine[-30:]) or text[-1500:])
    print('install_map: the game loads it' if result.returncode == 0 else f'install_map: the load check FAILED (exit {result.returncode}); see {report}')
    sys.exit(result.returncode)


if __name__ == '__main__':
    main()
