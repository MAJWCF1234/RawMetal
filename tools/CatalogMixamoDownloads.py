"""Preserve browser-downloaded character clips with verified hashes.

Usage: py -3 tools/CatalogMixamoDownloads.py --library <folder> --character <name>
The folder must contain downloads.json, a list of {name, kind, path} records.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', type=Path, required=True)
    parser.add_argument('--character', required=True)
    args = parser.parse_args()
    root = args.library.resolve()
    downloads = json.loads((root / 'downloads.json').read_text())
    groups = []
    for item in downloads:
        source = Path(item['path'])
        slug = re.sub('[^a-z0-9]+', '-', item['name'].lower()).strip('-')
        if not slug:
            raise ValueError('Empty download name')
        destination = root / (slug if item['kind'] == 'pack' else 'special')
        destination.mkdir(parents=True, exist_ok=True)
        if source.suffix.lower() == '.zip':
            archive = root / 'archives' / (slug + '.zip')
            archive.parent.mkdir(exist_ok=True)
            shutil.copy2(source, archive)
            if digest(source) != digest(archive):
                raise ValueError(f'Archive copy differs: {source}')
            with zipfile.ZipFile(archive) as zipped:
                if zipped.testzip() is not None:
                    raise ValueError(f'Corrupt archive: {source}')
                for member in zipped.infolist():
                    if not (destination / member.filename).resolve().is_relative_to(destination):
                        raise ValueError(f'Unsafe archive entry: {member.filename}')
                zipped.extractall(destination)
            files = sorted(f for f in destination.rglob('*') if f.suffix.lower() == '.fbx')
        else:
            if source.suffix.lower() != '.fbx':
                raise ValueError(f'Unsupported download: {source}')
            saved = destination / (slug + '.fbx')
            shutil.copy2(source, saved)
            if digest(source) != digest(saved):
                raise ValueError(f'Clip copy differs: {source}')
            files = [saved]
        if not files:
            raise ValueError(f'No FBX files: {source}')
        groups.append(dict(name=item['name'], kind=item['kind'],
                           download_sha256=digest(source),
                           files=[dict(file=f.relative_to(root).as_posix(),
                                       bytes=f.stat().st_size, sha256=digest(f)) for f in files]))
    manifest = dict(character=args.character, format='FBX Binary', fps=24,
                    keyframeReduction='uniform', packs=groups,
                    fbx_files=sum(len(g['files']) for g in groups))
    (root / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f"Verified {manifest['fbx_files']} source FBX files")


if __name__ == '__main__':
    main()
