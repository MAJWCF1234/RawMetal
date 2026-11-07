"""Package a release the way players download it.

  python tools/release/package.py TAG [--name GAME] [--exe PATH] [--out DIR] [--notes-tag EARLIER]

The engine builds RawMetal.exe; the files players download carry the game's
name (--name, DepthWorks by default). Writes into DIR (default .build/release/TAG):
  GAME-TAG.zip     GAME.exe, this version's notes (and the Windows guide if present),
                   and "custom maps/": the folder the game loads maps from, holding the
                   Starter Map, GUIDE.html and its pictures
  GAME-CampaignEditor-TAG.zip
                   the campaign editor as its own OPTIONAL download beside the
                   game's zip (self-contained single-file exe + asset blob);
                   players who only want the game never pay for its size.
                   Skip with --no-editor.
  GAME-TAG.sha256  SHA-256 of GAME.exe and of the zip (and the editor zip)
  body.md              the GitHub release text: download links first, then
                       only this version's section of RELEASE_NOTES.md

The whole changelog must never become the release text: GitHub lists the
downloadable files after the text, so a long changelog buries them.
"""
import argparse
import hashlib
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPO = 'MAJWCF1234/RawMetal'
# The starter kit shipped in "custom maps/" (other packs there are built into the exe).
CUSTOM_MAPS = ROOT / 'custom maps'
STARTER_KIT = ['Starter Map.rmc', 'GUIDE.html']


def starter_kit():
    """(source, name in the zip) for the custom maps starter kit."""
    files = [CUSTOM_MAPS / name for name in STARTER_KIT] + sorted((CUSTOM_MAPS / 'guide').glob('*.jpg'))
    missing = [str(f) for f in files[:len(STARTER_KIT)] if not f.is_file()]
    if missing:
        raise SystemExit('missing starter kit file(s): ' + ', '.join(missing) +
                         ' (run tools/campaign/make_starter_map.py and make_custom_map_guide.py --shots)')
    return [(f, 'custom maps/' + f.relative_to(CUSTOM_MAPS).as_posix()) for f in files]


def section(tag):
    """This version's section of RELEASE_NOTES.md (cp1252), without the '---' rule."""
    text = (ROOT / 'RELEASE_NOTES.md').read_bytes().decode('cp1252').replace('\r\n', '\n')
    match = re.search(r'^## ' + re.escape(tag) + r'\b.*?(?=^---\s*$|^## v|\Z)', text, re.M | re.S)
    if not match:
        raise SystemExit('RELEASE_NOTES.md has no "## %s" section' % tag)
    return match.group(0).strip() + '\n'


def package_editor(out, name, tag):
    """Publish the campaign editor and zip it beside the game's zip.

    Returns the zip path, or None (with a warning) when the editor cannot
    be built - it is an optional download and must never sink a game release.
    The exe finds editor-assets.rmblob, mappings.json and ModelIds.h beside
    itself, so all three ride in the zip root next to it.
    """
    project = ROOT / 'tools' / 'level-editor-native' / 'RawMetalCampaignEditor.csproj'
    blob = ROOT / 'tools' / 'level-editor' / 'editor-assets.rmblob'
    publish = ROOT / '.build' / 'editor-publish'
    for needed in (project, blob):
        if not needed.is_file():
            print('WARNING: editor skipped, missing %s' % needed)
            return None
    if publish.exists():
        shutil.rmtree(publish)
    run = subprocess.run(
        ['dotnet', 'publish', str(project), '-c', 'Release', '-r', 'win-x64',
         '--self-contained', 'true', '-p:PublishSingleFile=true', '-o', str(publish)],
        capture_output=True, text=True)
    if run.returncode != 0:
        print('WARNING: editor publish failed; shipping the game without it.\n' + run.stdout[-2000:] + run.stderr[-2000:])
        return None
    exe = publish / 'RawMetalCampaignEditor.exe'
    if not exe.is_file():
        print('WARNING: editor publish produced no exe in %s' % publish)
        return None
    shutil.copyfile(blob, publish / blob.name)
    model_ids = ROOT / 'src' / 'world' / 'ModelIds.h'
    if model_ids.is_file():
        shutil.copyfile(model_ids, publish / model_ids.name)
    (publish / 'README-EDITOR.txt').write_text(
        '%s Campaign Editor (%s)\n\n'
        'Unzip anywhere and run DepthWorksCampaignEditor.exe - nothing is installed.\n'
        'Open a campaign (.rmc) from the game\'s "custom maps" folder, build, then\n'
        'Export .rmc back into that folder to play it in the game.\n'
        'Keep editor-assets.rmblob, mappings.json and ModelIds.h beside the exe:\n'
        'they are the model/texture library the editor previews with.\n' % (name, tag),
        encoding='utf-8')
    editor_exe = publish / ('%sCampaignEditor.exe' % name)
    exe.replace(editor_exe)
    archive = out / ('%s-CampaignEditor-%s.zip' % (name, tag))
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for entry in sorted(publish.iterdir()):
            if entry.is_file() and entry.suffix not in ('.pdb',):
                z.write(entry, entry.name)
    return archive


def sha256(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('tag')
    parser.add_argument('--name', default='DepthWorks', help='the name players see on the exe, zip and checksum files')
    parser.add_argument('--exe', type=Path, default=ROOT / 'RawMetal.exe')
    parser.add_argument('--out', type=Path)
    parser.add_argument('--notes-tag', help="use this earlier version's notes (a patch with no section of its own)")
    parser.add_argument('--no-editor', action='store_true', help='skip the optional campaign editor zip')
    args = parser.parse_args()
    tag, out, name = args.tag, args.out or ROOT / '.build' / 'release' / args.tag, args.name
    if not args.exe.is_file():
        raise SystemExit('missing ' + str(args.exe))
    out.mkdir(parents=True, exist_ok=True)
    notes = section(args.notes_tag or tag)
    if args.notes_tag:
        notes = 'Patch release of %s; its notes follow.\n\n' % args.notes_tag + notes
    exe = out / (name + '.exe')
    if args.exe.resolve() != exe.resolve():
        shutil.copyfile(args.exe, exe)
    (out / ('RELEASE_NOTES_%s.md' % tag)).write_text(notes, encoding='utf-8')
    archive = out / ('%s-%s.zip' % (name, tag))
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.write(exe, exe.name)
        z.write(out / ('RELEASE_NOTES_%s.md' % tag), 'RELEASE_NOTES_%s.md' % tag)
        guide = ROOT / 'docs' / 'windows-compatibility.md'
        if guide.is_file():
            z.write(guide, 'windows-compatibility.md')
        for source, entry in starter_kit():
            z.write(source, entry)
    contents = ('the game, these notes, the Windows guide' if guide.is_file() else 'the game, these notes') + \
        ' and the custom maps folder (Starter Map and modding guide)'
    editor_archive = None if args.no_editor else package_editor(out, name, tag)
    checksums = out / ('%s-%s.sha256' % (name, tag))
    # LF endings: sha256sum -c on any platform reads them; Windows text mode would write CRLF.
    with open(checksums, 'w', encoding='ascii', newline='\n') as f:
        f.write('%s  %s\n%s  %s\n' % (sha256(exe), exe.name, sha256(archive), archive.name))
        if editor_archive is not None:
            f.write('%s  %s\n' % (sha256(editor_archive), editor_archive.name))
    base = 'https://github.com/%s/releases/download/%s/' % (REPO, tag)
    editor_line = ''
    if editor_archive is not None:
        editor_line = ('- [%s](%s%s): the campaign editor (optional, only needed to make your own maps)\n'
                       % (editor_archive.name, base, editor_archive.name))
    body = ('## Download\n\n'
            '- **[%s](%s%s)**: %s\n'
            '- [%s](%s%s): the game on its own (one file, all assets inside)\n'
            '%s'
            '- [%s](%s%s): SHA-256 checksums\n\n'
            'Windows 10/11 x64 with a Vulkan 1.0 GPU driver and the Microsoft Visual C++ x64 '
            'Redistributable. Unzip anywhere and run %s; nothing is installed. Custom maps go in the '
            '`custom maps` folder beside it: open `custom maps/GUIDE.html` to make your own.\n\n'
            '---\n\n' % (archive.name, base, archive.name, contents, exe.name, base, exe.name, editor_line, checksums.name, base, checksums.name, exe.name)) + notes
    (out / 'body.md').write_text(body, encoding='utf-8')
    print('PACKAGED', out)
    for path in (exe, archive, checksums) + ((editor_archive,) if editor_archive else ()):
        print('  %s %d bytes' % (path.name, path.stat().st_size))


if __name__ == '__main__':
    main()
