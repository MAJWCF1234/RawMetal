"""Build, test and ship DepthWorks in one command, the way releases are made (no assistant needed).

  python tools/release/ship.py 1.4.6            # everything, then publish to GitHub as the latest release
  python tools/release/ship.py 1.4.6 --offline  # everything except git push and the GitHub release
  python tools/release/ship.py 1.4.6 --no-tests # skip the suite (only for a rebuild of a tested tree)
  python tools/release/ship.py --check          # build + suite + Builder checks on the current version, nothing else

Steps, stopping at the first failure (each step's log is in .build/ship/):
  1. Version: src/app.rc in the game and the Campaign Builder, and the menu tag in
     src/renderer/SoftwareRenderer.cpp, set to the version given.
  2. Release notes: RELEASE_NOTES.md must already have a "## vX.Y.Z - Title (date)" section at the top
     (write it first, in the game's voice); its title becomes the release title.
  3. Commit what is staged plus the version files ("release: vX.Y.Z, title"), never with an AI trailer.
  4. Campaign Builder engine sync (tools/editor/sync_engine.py). The only conflict it resolves itself is
     the Builder's own "CAMPAIGN BUILDER" menu tag; any other conflict stops the ship for a person to merge.
  5. Builds: Build.cmd (the game) and the Builder's Build.cmd + BuildEditor.cmd, in parallel.
  6. Tests: tools/dev/suite.py --extras (every --each-test check in lanes on both GPUs, plus gore, carcass,
     campaign-map and repeated-frame rendering) and --audio-test; the Builder's --selftest, --editing-test
     and --campaign-open-test on maps 0, 75 and 102.
  7. Package: tools/release/package.py; the zip's DepthWorks.exe must be byte-identical to the tested exe,
     and it re-runs two quick tests from the unzipped folder.
  8. Publish (not with --offline): commit the Builder sync, push main, gh release create --latest with the
     four assets, then download the release back and check every checksum.
"""
import concurrent.futures, hashlib, os, re, shutil, subprocess, sys, tempfile, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EDITOR = os.path.join(ROOT, 'tools', 'Level Editor')
LOGS = os.path.join(ROOT, '.build', 'ship')
REPO = 'MAJWCF1234/RawMetal'


def say(text):
    print(f'ship: {text}', flush=True)


def run(cmd, log, cwd=ROOT, check=True, env=None):
    os.makedirs(LOGS, exist_ok=True)
    path = os.path.join(LOGS, log)
    with open(path, 'w', encoding='utf-8', errors='replace') as out:
        result = subprocess.run(cmd, cwd=cwd, stdout=out, stderr=subprocess.STDOUT, shell=isinstance(cmd, str), env=env)
    if check and result.returncode != 0:
        tail = open(path, encoding='utf-8', errors='replace').read()[-1500:]
        sys.exit(f'ship: FAILED at {log} (exit {result.returncode}). Last lines:\n{tail}\nFull log: {path}')
    return result.returncode


def cmd_build(script):
    return ['powershell', '-NoProfile', '-Command', f'cmd /c "`"{script}`""']


def set_version(version):
    major, minor, patch = version.split('.')
    for rc in (os.path.join(ROOT, 'src', 'app.rc'), os.path.join(EDITOR, 'src', 'app.rc')):
        text = open(rc, 'rb').read().decode('utf-8')
        old = re.search(r'FILEVERSION (\d+),(\d+),(\d+),\d+', text)
        if not old:
            sys.exit(f'ship: no FILEVERSION in {rc}')
        old = '.'.join(old.groups())
        text = re.sub(r'(FILEVERSION|PRODUCTVERSION) \d+,\d+,\d+,\d+', lambda m: f'{m.group(1)} {major},{minor},{patch},0', text)
        text = text.replace('"' + old + '\\0"', '"' + version + '\\0"')   # the string values end in a literal \0
        if text.count(f'{major},{minor},{patch},0') != 2 or text.count(f'"{version}\\0"') != 2:
            sys.exit(f'ship: could not set the version in {rc}')
        open(rc, 'wb').write(text.encode('utf-8'))
    menu = os.path.join(ROOT, 'src', 'renderer', 'SoftwareRenderer.cpp')
    text = open(menu, 'rb').read().decode('utf-8')
    text, n = re.subn(r'V\d+\.\d+\.\d+  CHAPTER', f'V{version}  CHAPTER', text)
    if n != 1:
        sys.exit('ship: could not find the menu version tag in SoftwareRenderer.cpp')
    open(menu, 'wb').write(text.encode('utf-8'))


def notes_title(version):
    head = open(os.path.join(ROOT, 'RELEASE_NOTES.md'), encoding='utf-8').read()[:4000]
    m = re.search(rf'^## v{re.escape(version)} - (.+?) \(', head, re.M)
    if not m:
        sys.exit(f'ship: RELEASE_NOTES.md has no "## v{version} - Title (date)" section near the top. Write it first.')
    return m.group(1)


def sync_builder():
    run([sys.executable, 'tools/editor/sync_engine.py', '--force'], 'sync.txt', check=False)
    pattern = re.compile(r'<<<<<<< editor\r?\n(.*?)=======\r?\n(.*?)>>>>>>> game now\r?\n', re.S)
    for folder, _, files in os.walk(os.path.join(EDITOR, 'src')):
        if 'ThirdParty' in folder:
            continue
        for name in files:
            if not name.endswith(('.cpp', '.h', '.inc', '.frag', '.vert', '.txt', '.rc')):
                continue
            path = os.path.join(folder, name)
            text = open(path, 'rb').read().decode('utf-8', errors='replace')
            if '<<<<<<< editor' not in text:
                continue
            blocks = pattern.findall(text)
            if blocks and all('CAMPAIGN BUILDER' in mine for mine, _ in blocks):
                open(path, 'wb').write(pattern.sub(lambda m: m.group(1), text).encode('utf-8'))
                say(f'kept the Builder menu tag in {os.path.relpath(path, ROOT)}')
            else:
                sys.exit(f'ship: the Builder sync has a real conflict in {path}; merge it by hand, then run again.')
    run([sys.executable, 'tools/editor/sync_engine.py'], 'sync2.txt')


def builder_checks():
    exe = os.path.join(EDITOR, 'RawMetalEditor.exe')
    run([exe, '--selftest'], 'builder-selftest.txt', cwd=EDITOR)
    run([exe, '--editing-test'], 'builder-editing.txt', cwd=EDITOR)
    out = tempfile.mkdtemp(prefix='ship-open-')
    for level in (0, 75, 102):
        run([exe, '--campaign-open-test', str(level), os.path.join(out, f'{level}.rmc')], f'builder-open-{level}.txt', cwd=EDITOR)


def sha1(path):
    h = hashlib.sha1()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    args = sys.argv[1:]
    check_only = '--check' in args
    offline = '--offline' in args or check_only
    version = next((a for a in args if re.fullmatch(r'\d+\.\d+\.\d+', a)), None)
    if not check_only and not version:
        sys.exit(__doc__)
    if version:
        title = notes_title(version)
        say(f'v{version} "{title}"')
        set_version(version)
        run(['git', 'add', 'src/app.rc', 'src/renderer/SoftwareRenderer.cpp', 'RELEASE_NOTES.md'], 'git-add.txt')
        if subprocess.run(['git', 'diff', '--cached', '--quiet'], cwd=ROOT).returncode != 0:
            run(['git', 'commit', '-q', '-m', f'release: v{version}, {title.lower()}'], 'git-commit.txt')
    say('syncing the Campaign Builder engine')
    sync_builder()
    say('building the game and the Campaign Builder (in parallel)')
    with concurrent.futures.ThreadPoolExecutor(2) as pool:
        game = pool.submit(run, cmd_build(os.path.join(ROOT, 'Build.cmd')), 'build-game.txt')
        builder = pool.submit(lambda: (run(cmd_build(os.path.join(EDITOR, 'Build.cmd')), 'build-builder.txt'),
                                       run(cmd_build(os.path.join(EDITOR, 'BuildEditor.cmd')), 'build-editor-ui.txt')))
        game.result(); builder.result()
    exe = os.path.join(ROOT, 'RawMetal.exe')
    if '--no-tests' not in args:
        say('testing: the suite on both GPUs, the audio test and the Builder checks (about 15-25 minutes)')
        audio_dir = os.path.join(ROOT, '.build', 'ship-audio'); os.makedirs(audio_dir, exist_ok=True)
        shutil.copy2(exe, os.path.join(audio_dir, 'rm.exe'))
        with concurrent.futures.ThreadPoolExecutor(3) as pool:
            suite = pool.submit(run, [sys.executable, 'tools/dev/suite.py', exe, '--lanes', '7', '--extras', '--out', '.build/ship-suite'], 'suite.txt')
            audio = pool.submit(run, [os.path.join(audio_dir, 'rm.exe'), '--audio-test'], 'audio-test.txt', audio_dir, True, dict(os.environ, RM_GPU='1'))
            builder = pool.submit(builder_checks)
            suite.result(); audio.result(); builder.result()
        say('all tests passed')
    if check_only:
        say('check done (nothing packaged or published)')
        return
    tag = f'v{version}'
    out = os.path.join(ROOT, '.build', 'release', tag)
    say(f'packaging {tag}')
    run([sys.executable, 'tools/release/package.py', tag, '--exe', 'RawMetal.exe', '--out', out], 'package.txt')
    unzip = os.path.join(out, 'pkg'); shutil.rmtree(unzip, ignore_errors=True)
    with zipfile.ZipFile(os.path.join(out, f'DepthWorks-{tag}.zip')) as z:
        z.extractall(unzip)
    if sha1(os.path.join(unzip, 'DepthWorks.exe')) != sha1(exe):
        sys.exit('ship: the packaged DepthWorks.exe is not the exe that was tested')
    run([os.path.join(unzip, 'DepthWorks.exe'), '--each-test=doors,dialogue'], 'packaged-test.txt', cwd=unzip)
    say('package verified')
    if offline:
        say(f'offline: done. The release is in {out} (not pushed, not published).')
        return
    run(['git', 'add', '-u', 'tools/Level Editor'], 'git-add-builder.txt')
    if subprocess.run(['git', 'diff', '--cached', '--quiet'], cwd=ROOT).returncode != 0:
        run(['git', 'commit', '-q', '-m', f'chore: sync the Campaign Builder for {tag}'], 'git-commit-builder.txt')
    run(['git', 'push', 'origin', 'main'], 'git-push.txt')
    head = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    assets = [f'DepthWorks-{tag}.zip', 'DepthWorks.exe', f'DepthWorks-CampaignEditor-{tag}.zip', f'DepthWorks-{tag}.sha256']
    run(['gh', 'release', 'create', tag, '--target', head, '--title', f'DepthWorks {tag} - {notes_title(version)}',
         '--notes-file', 'body.md', '--latest', *assets], 'gh-release.txt', cwd=out)
    check = tempfile.mkdtemp(prefix='ship-download-')
    run(['gh', 'release', 'download', tag, '-R', REPO], 'gh-download.txt', cwd=check)
    run(['sha256sum', '-c', f'DepthWorks-{tag}.sha256'] if shutil.which('sha256sum') else
        ['certutil', '-hashfile', f'DepthWorks-{tag}.zip', 'SHA256'], 'download-verify.txt', cwd=check)
    say(f'published https://github.com/{REPO}/releases/tag/{tag} and verified the download')


if __name__ == '__main__':
    main()
