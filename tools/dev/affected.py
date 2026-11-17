"""Map git changes since the last release to the suite tests they affect.

  python tools/dev/affected.py              # print the selection for the current tree
  python tools/dev/affected.py --json

Ship and suite.py import select() from here. Unknown files under src/ force a full suite
(fail open to thorough). Docs, notes and pure tooling with no src hit select nothing.
"""
from __future__ import annotations

import json, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Stand-alone flags suite.py --extras can run. Names match the flag without leading dashes... 
# kept as the full flag string used by suite.EXTRAS.
EXTRA_GORE = '--gore-test'
EXTRA_CARCASS = '--carcass-test'
EXTRA_CAMPAIGN = '--campaign-map-test'
EXTRA_RENDER = '--gameplay-rendering-test'

# (path regex against repo-relative forward slashes, tests, extras, audio, builder_open)
# First matching rule wins per file. Order: specific before broad.
RULES = [
    (r'^src/audio/',
     ['lift-mix', 'ambience', 'reverb', 'alert-music', 'audio-events', 'npc-voices'],
     [], True, False),
    (r'^src/lab/',
     [], [], False, False),  # feature lab; ship suite comes from src/game|audio|renderer
    (r'^src/game/(Door|Locker)',
     ['doors', 'door-leaf', 'door-axis', 'lockers', 'locker-guards', 'locker-capture', 'movement'],
     [], False, False),
    (r'^src/game/Movement\.cpp$',
     ['movement', 'doors', 'door-leaf', 'stance', 'unarmed'], [], False, False),
    (r'^src/game/(Clutter|Crates)\.cpp$',
     ['clutter', 'props', 'physics-stages', 'glass', 'crates'], [], False, False),
    (r'^src/game/(Handgun|Hitscan|Combat|Inventory|Stealth)\.',
     ['handgun', 'combat', 'weapon-motion', 'inventory', 'unarmed', 'pickups', 'flashlight'],
     [], False, False),
    (r'^src/game/ai/',
     ['ai', 'creature-behaviour', 'chimera-hunt', 'caution', 'machine', 'zone-people', 'locker-guards'],
     [], False, False),
    (r'^src/game/Saves\.cpp$',
     ['saves'], [], False, False),
    (r'^src/game/(Lift|Reactor|Gantry|Mechanisms)\.',
     ['lift', 'reactor', 'gantry', 'mechanisms', 'lift-mix'], [], False, False),
    (r'^src/game/(Console|Script|Settings|Radar|Storm|Electricity)\.',
     ['console', 'scripting', 'systems', 'settings', 'radar', 'storm', 'alarm', 'darkness'],
     [], False, False),
    (r'^src/game/(Ragdoll|Streaming|MapAudit|EngineRegression|GameplayRegression|CampaignMapTest|ServiceMapTest|PrologueTest|Exclusion)',
     ['engine-regression', 'gameplay-fixes', 'campaign-maps', 'service-maps', 'streaming', 'progression', 'chapter-route', 'chapter-sequence'],
     [EXTRA_CAMPAIGN], False, True),
    (r'^src/game/Game\.(h|cpp)$',
     ['systems', 'gameplay-fixes', 'saves', 'props', 'clutter'], [], False, False),  # shared header: the save version and the prop struct live here
    (r'^src/game/',
     ['systems', 'gameplay-fixes', 'engine-regression'], [], False, False),
    (r'^src/renderer/SoftwareRenderer\.(cpp|h)$',
     [], [], False, False),  # every release retags the menu; other renderer files still map
    (r'^src/renderer/(Carcass|Particles|Humanoid|ClaireOutside)',
     ['human-ragdoll', 'particles', 'creature-animation', 'hazmat'],
     [EXTRA_GORE, EXTRA_CARCASS, EXTRA_RENDER], False, False),
    (r'^src/renderer/',
     ['projection', 'creature-animation', 'cleanup-tank-aim', 'human-ragdoll', 'third-person',
      'light-probes', 'particles', 'weapon-motion', 'hazmat'],
     [EXTRA_RENDER], False, False),
    (r'^src/world/Campaign',
     ['campaign-maps', 'chapter-route', 'chapter-sequence', 'service-maps', 'streaming', 'mechanisms'],
     [EXTRA_CAMPAIGN], False, True),
    # World.cpp/h build every map's runtime state (loose chairs, fixtures, nav): what reads it reaches far.
    (r'^src/world/World\.(h|cpp)$',
     ['primitives', 'streaming', 'service-maps', 'glass', 'props', 'clutter', 'saves', 'campaign-maps',
      'creature-behaviour', 'chimera-hunt', 'movement'], [EXTRA_CAMPAIGN], False, True),
    (r'^src/world/',
     ['primitives', 'streaming', 'service-maps', 'glass'], [EXTRA_CAMPAIGN], False, True),
    (r'^src/platform/',
     ['command-arguments', 'settings'], [], False, False),
    (r'^src/vr/',
     ['third-person', 'projection'], [EXTRA_RENDER], False, False),
    (r'^src/core/',
     ['content-integrity', 'load-validation', 'primitives'], [], False, False),
    # Dispatch / flag wiring: the feature's own sources should also be in the diff.
    (r'^src/main\.cpp$',
     ['command-arguments', 'content-integrity', 'load-validation'], [], False, False),
    (r'^src/app\.rc$',
     [], [], False, False),
    (r'^src/',
     None, None, None, None),  # any other src file -> full
    # Builder engine sync copies src/; do not re-expand the game suite from the copy.
    (r'^tools/Level Editor/ENGINE_SYNC$',
     [], [], False, False),
    (r'^tools/Level Editor/src/',
     [], [], False, False),
    (r'^tools/Level Editor/',
     [], [], False, True),  # builder UI / project files -> builder open checks
    (r'^tools/release/',
     [], [], False, False),
    (r'^tools/dev/',
     [], [], False, False),
    (r'^tools/editor/sync_engine\.py$',
     [], [], False, False),
    (r'^tools/editor/',
     [], [], False, True),
    (r'^docs/',
     [], [], False, False),
    (r'^RELEASE_NOTES\.md$',
     [], [], False, False),
]


def _run(cmd):
    return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')


def published_release_commit():
    """The newest release actually published (a v* tag on origin) below HEAD.

    A ship that stopped after its release commit (a failed build or test) leaves a 'release:' commit
    that was never published; diffing from it would leave everything before it untested.
    """
    out = _run(['git', 'ls-remote', '--tags', 'origin', 'v*'])
    if out.returncode != 0 or not out.stdout.strip():
        return None
    head = _run(['git', 'rev-parse', 'HEAD']).stdout.strip()
    tags = {}
    for line in out.stdout.splitlines():
        sha, _, ref = line.partition('\t')
        m = re.match(r'refs/tags/v(\d+)\.(\d+)\.(\d+)(\^\{\})?$', ref.strip())
        if m:
            key = tuple(int(g) for g in m.groups()[:3])
            if m.group(4) or key not in tags:   # a peeled annotated tag names the commit itself
                tags[key] = sha.strip()
    for key in sorted(tags, reverse=True):
        sha = tags[key]
        if sha == head:
            continue
        if _run(['git', 'merge-base', '--is-ancestor', sha, 'HEAD']).returncode == 0:
            return sha
    return None


def previous_release_commit():
    """The last published release (origin's v* tags); offline, the last 'release:' commit not on HEAD."""
    published = published_release_commit()
    if published:
        return published
    log = _run(['git', 'log', '--grep', '^release: v', '-n', '8', '--format=%H%x09%s'])
    if log.returncode != 0 or not log.stdout.strip():
        return None
    rows = [line.split('\t', 1) for line in log.stdout.strip().splitlines() if line.strip()]
    head_subj = _run(['git', 'log', '-1', '--format=%s']).stdout.strip()
    current = None
    if m := re.match(r'release: (v\d+\.\d+\.\d+)', head_subj):
        current = m.group(1)
    elif m := re.search(r'Campaign Builder for (v\d+\.\d+\.\d+)', head_subj):
        current = m.group(1)
    for h, subj in rows:
        vm = re.match(r'release: (v\d+\.\d+\.\d+)', subj)
        if not vm:
            continue
        if current and vm.group(1) == current:
            continue
        return h
    return None


def changed_files(base=None):
    base = base or previous_release_commit()
    files = set()
    if base:
        for args in ([f'{base}..HEAD'], ['HEAD'], ['--cached']):
            out = _run(['git', 'diff', '--name-only', *args])
            if out.returncode == 0:
                files.update(p.replace('\\', '/') for p in out.stdout.splitlines() if p.strip())
    else:
        out = _run(['git', 'diff', '--name-only', 'HEAD'])
        files.update(p.replace('\\', '/') for p in out.stdout.splitlines() if p.strip())
        out = _run(['git', 'diff', '--name-only', '--cached'])
        files.update(p.replace('\\', '/') for p in out.stdout.splitlines() if p.strip())
    return sorted(files), base


def select(base=None, suite_names=None):
    """Return a dict describing what to run for the current diff.

    keys: mode ('full'|'subset'|'none'), tests (list), extras (list of flags),
    audio (bool), builder_open (bool), files, base, reasons (file -> rule summary)
    """
    files, base = changed_files(base)
    if suite_names is None:
        src = open(os.path.join(ROOT, 'src', 'main.cpp'), encoding='utf-8', errors='replace').read()
        i = src.find('const std::pair<const char*,bool(*)()> tests[]')
        suite_names = re.findall(r'\{"([a-z0-9-]+)",', src[i:src.find('};', i)])

    if not files:
        return dict(mode='none', tests=[], extras=[], audio=False, builder_open=False,
                    files=[], base=base, reasons={})

    tests, extras, reasons = set(), set(), {}
    audio = builder_open = False
    for path in files:
        matched = False
        for pattern, rule_tests, rule_extras, rule_audio, rule_builder in RULES:
            if not re.search(pattern, path):
                continue
            matched = True
            if rule_tests is None:
                return dict(mode='full', tests=list(suite_names),
                            extras=[EXTRA_GORE, EXTRA_CARCASS, EXTRA_CAMPAIGN, EXTRA_RENDER],
                            audio=True, builder_open=True, files=files, base=base,
                            reasons={path: f'unscoped ({pattern}) -> full suite'})
            tests.update(rule_tests)
            extras.update(rule_extras or [])
            audio = audio or bool(rule_audio)
            builder_open = builder_open or bool(rule_builder)
            reasons[path] = pattern
            break
        if not matched and (path.startswith('src/') or path.startswith('tools/Level Editor/src/')):
            return dict(mode='full', tests=list(suite_names),
                        extras=[EXTRA_GORE, EXTRA_CARCASS, EXTRA_CAMPAIGN, EXTRA_RENDER],
                        audio=True, builder_open=True, files=files, base=base,
                        reasons={path: 'unmatched src -> full suite'})
        if not matched:
            reasons[path] = 'ignored'

    # Cheap always-on when anything game-side is selected.
    if tests:
        tests.update(['content-integrity', 'load-validation', 'command-arguments'])

    known = set(suite_names)
    tests = sorted(t for t in tests if t in known)
    return dict(mode='subset' if tests or extras or audio or builder_open else 'none',
                tests=tests, extras=sorted(extras), audio=audio, builder_open=builder_open,
                files=files, base=base, reasons=reasons)


def main():
    args = sys.argv[1:]
    base = args[args.index('--base') + 1] if '--base' in args else None
    sel = select(base)
    if '--json' in args:
        print(json.dumps(sel, indent=2))
        return
    print(f"affected: mode={sel['mode']} base={sel['base'] or '(none)'}")
    print(f"  files ({len(sel['files'])}):")
    for f in sel['files']:
        print(f"    {f}  [{sel['reasons'].get(f, '?')}]")
    if sel['mode'] == 'full':
        print('  suite: FULL')
    elif sel['mode'] == 'none':
        print('  suite: (skip)')
    else:
        print(f"  tests ({len(sel['tests'])}): {','.join(sel['tests']) or '(none)'}")
        print(f"  extras: {', '.join(sel['extras']) or '(none)'}")
        print(f"  audio: {'yes' if sel['audio'] else 'no'}")
        print(f"  builder open maps: {'yes' if sel['builder_open'] else 'no'}")


if __name__ == '__main__':
    main()
