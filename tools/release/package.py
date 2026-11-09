"""Package a release the way players download it.

  python tools/release/package.py TAG [--name GAME] [--exe PATH] [--out DIR] [--notes-tag EARLIER]

The engine builds RawMetal.exe; the files players download carry the game's
name (--name, DepthWorks by default). Writes into DIR (default .build/release/TAG):
  GAME-TAG.zip     GAME.exe, this version's notes (and the Windows guide if present),
                   and "custom maps/": the folder the game loads maps from, holding the
                   Starter Map, GUIDE.html and its pictures
  GAME-CampaignEditor-TAG.zip
                   RawMetal.exe (play and edit, menu first) and custom maps.
                   Skip with --no-editor.
  GAME-TAG.sha256  SHA-256 of GAME.exe, the game zip, and the editor zip
  body.md              the GitHub release text: download links first, then
                       only this version's section of RELEASE_NOTES.md

The whole changelog must never become the release text: GitHub lists the
downloadable files after the text, so a long changelog buries them.
"""
import argparse
import hashlib
import re
import shutil
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


EDITOR = ROOT / 'tools' / 'Level Editor'
# Beside the exe in every public zip: the game and the Campaign Builder then list and export
# only released chapters (world/CampaignExport.h, Chapter2Released). Builds without it,
# the owner's own and any he hands on, open every level.
PUBLIC_MARKER = ('public-release.txt',
                 'This is a public release. Levels of chapters not yet released stay sealed.\r\n')


def package_editor(out, name, tag):
    """Zip the standalone level editor beside the game.

    Players unzip it anywhere. It does not need the game folder. RawMetal.exe
    opens on a menu: new campaign, a previous project, or play.
    """
    window = EDITOR / 'RawMetal.exe'
    engine = EDITOR / '.build' / 'engine' / 'RawMetal.exe'
    maps = EDITOR / 'custom maps'
    program = window if window.is_file() else engine
    if not program.is_file():
        print('WARNING: editor skipped, missing %s' % program)
        return None
    archive = out / ('%s-CampaignEditor-%s.zip' % (name, tag))
    readme = (
        '%s Campaign Editor (%s)\n\n'
        'Unzip anywhere. The game campaign is a separate download.\n'
        'Run RawMetal.exe. New campaign starts a blank map. A project opens\n'
        'that map in the editor. Play runs the game from the same program.\n'
        % (name, tag))
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.writestr('README-EDITOR.txt', readme)
        z.writestr(*PUBLIC_MARKER)
        z.write(program, 'RawMetal.exe')
        if maps.is_dir():
            for entry in sorted(maps.rglob('*')):
                # The editor's unsaved-work copies stay on the author's machine.
                if entry.is_file() and 'Autosave' not in entry.relative_to(maps).parts:
                    z.write(entry, 'custom maps/' + entry.relative_to(maps).as_posix())
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
    # The menu, the editor and Play are one program. That exe is the Level Editor
    # build. The repo-root RawMetal.exe is the game alone and has no editor in it.
    unified = EDITOR / 'RawMetal.exe'
    parser.add_argument('--exe', type=Path, default=unified if unified.is_file() else ROOT / 'RawMetal.exe')
    parser.add_argument('--out', type=Path)
    parser.add_argument('--notes-tag', help="use this earlier version's notes (a patch with no section of its own)")
    parser.add_argument('--no-editor', action='store_true', help='skip the standalone campaign editor zip')
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
        z.writestr(*PUBLIC_MARKER)
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
        editor_line = ('- [%s](%s%s): the campaign editor and custom-map player (optional, unzip anywhere)\n'
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
