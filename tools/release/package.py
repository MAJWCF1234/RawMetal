"""Package a RawMetal release the way players download it.

  python tools/release/package.py TAG [--exe PATH] [--out DIR] [--notes-tag EARLIER]

Writes into DIR (default .build/release/TAG):
  RawMetal-TAG.zip     RawMetal.exe, this version's notes (and the Windows guide if present)
  RawMetal-TAG.sha256  SHA-256 of RawMetal.exe and of the zip
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


def section(tag):
    """This version's section of RELEASE_NOTES.md (cp1252), without the '---' rule."""
    text = (ROOT / 'RELEASE_NOTES.md').read_bytes().decode('cp1252').replace('\r\n', '\n')
    match = re.search(r'^## ' + re.escape(tag) + r'\b.*?(?=^---\s*$|^## v|\Z)', text, re.M | re.S)
    if not match:
        raise SystemExit('RELEASE_NOTES.md has no "## %s" section' % tag)
    return match.group(0).strip() + '\n'


def sha256(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('tag')
    parser.add_argument('--exe', type=Path, default=ROOT / 'RawMetal.exe')
    parser.add_argument('--out', type=Path)
    parser.add_argument('--notes-tag', help="use this earlier version's notes (a patch with no section of its own)")
    args = parser.parse_args()
    tag, out = args.tag, args.out or ROOT / '.build' / 'release' / args.tag
    if not args.exe.is_file():
        raise SystemExit('missing ' + str(args.exe))
    out.mkdir(parents=True, exist_ok=True)
    notes = section(args.notes_tag or tag)
    if args.notes_tag:
        notes = 'Patch release of %s; its notes follow.\n\n' % args.notes_tag + notes
    exe = out / 'RawMetal.exe'
    if args.exe.resolve() != exe.resolve():
        shutil.copyfile(args.exe, exe)
    (out / ('RELEASE_NOTES_%s.md' % tag)).write_text(notes, encoding='utf-8')
    archive = out / ('RawMetal-%s.zip' % tag)
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.write(exe, 'RawMetal.exe')
        z.write(out / ('RELEASE_NOTES_%s.md' % tag), 'RELEASE_NOTES_%s.md' % tag)
        guide = ROOT / 'docs' / 'windows-compatibility.md'
        if guide.is_file():
            z.write(guide, 'windows-compatibility.md')
    contents = 'the game, these notes and the Windows guide' if guide.is_file() else 'the game and these notes'
    checksums = out / ('RawMetal-%s.sha256' % tag)
    checksums.write_text('%s  RawMetal.exe\n%s  %s\n' % (sha256(exe), sha256(archive), archive.name), encoding='ascii')
    base = 'https://github.com/%s/releases/download/%s/' % (REPO, tag)
    body = ('## Download\n\n'
            '- **[RawMetal-%s.zip](%s%s)**: %s\n'
            '- [RawMetal.exe](%sRawMetal.exe): the game on its own (one file, all assets inside)\n'
            '- [RawMetal-%s.sha256](%s%s): SHA-256 checksums of both\n\n'
            'Windows 10/11 x64 with a Vulkan 1.0 GPU driver and the Microsoft Visual C++ x64 '
            'Redistributable. Unzip anywhere and run RawMetal.exe; nothing is installed.\n\n'
            '---\n\n' % (tag, base, archive.name, contents, base, tag, base, checksums.name)) + notes
    (out / 'body.md').write_text(body, encoding='utf-8')
    print('PACKAGED', out)
    for path in (exe, archive, checksums):
        print('  %s %d bytes' % (path.name, path.stat().st_size))


if __name__ == '__main__':
    main()
