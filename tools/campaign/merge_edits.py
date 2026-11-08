"""Merge campaign level edits sent back from the Campaign Builder into campaign-overrides/.

    python tools/campaign/merge_edits.py "Someone-campaign-edits-2026-11-08.zip" [--dry-run]

The zip is what the builder's "Export my changes" writes: manifest.txt (the author, and
per level its chunk, the game version and record hash it started from, and its own hash),
each edited level as "NNN NAME.rmc", and the level it started from as "base/NNN NAME.rmc"
when the sender's game still had it.

For each level the tool exports the level as this repository's game plays it now
(RawMetal.exe --export-rmc, which reads campaign-overrides/) and compares record hashes:
  clean            yours is still what the edit started from: theirs is installed;
  already merged   yours already is their edit: nothing changes;
  both changed     yours moved on since: theirs is installed as the override, yours is kept
                   in campaign-overrides/review/ beside their base, and the report says review.
Nothing of yours is overwritten without a copy. Each level gets a short record diff
(added, removed, moved) against the base. The report is printed and written to
.build/merge-edits-<date>.txt. CMake embeds campaign-overrides/*.rmc at the next build;
review/ is not embedded.
"""
import argparse
import collections
import datetime
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
START = '--- CUSTOM_CAMPAIGN_DATA_START ---'
END = '--- CUSTOM_CAMPAIGN_DATA_END ---'


def records(text):
    """The data lines of an .rmc: no comments, no blank lines (as the engine's rmcDataHash)."""
    start, end = text.find(START), text.find(END)
    if start < 0 or end < start:
        return []
    out = []
    for line in text[start:end].split('\n')[1:]:
        line = line.rstrip('\r')
        if line and not line.startswith('#'):
            out.append(line)
    return out


def data_hash(text):
    return hashlib.sha256(''.join(line + '\n' for line in records(text)).encode('utf-8')).hexdigest()


def chunk_of(name):
    match = re.match(r'^0*(\d+)(?:\D.*)?\.rmc$', name)
    return int(match.group(1)) if match else -1


def diff(base, other):
    """Per record kind: added, removed and moved (same record, different place)."""
    if base is None:
        return 'no base to compare with'
    a, b = collections.Counter(records(base)), collections.Counter(records(other))
    removed, added = list((a - b).elements()), list((b - a).elements())
    moved = 0
    kinds = collections.Counter()

    def shape(line):
        f = line.split('|')
        # Positions are fields 2-4 of most records (x, y, z); the rest says what it is.
        return (f[0], len(f), tuple(v for i, v in enumerate(f) if i not in (2, 3, 4)))
    pool = collections.defaultdict(list)
    for line in added:
        pool[shape(line)].append(line)
    still_removed = []
    for line in removed:
        match = pool.get(shape(line))
        if match:
            match.pop()
            moved += 1
            kinds[line.split('|')[0] + ' moved'] += 1
        else:
            still_removed.append(line)
    for lines in pool.values():
        for line in lines:
            kinds[line.split('|')[0] + ' added'] += 1
    for line in still_removed:
        kinds[line.split('|')[0] + ' removed'] += 1
    if not kinds:
        return 'no record changes'
    total = collections.Counter()
    for key, count in kinds.items():
        total[key.rsplit(' ', 1)[1]] += count
    summary = '%d added, %d removed, %d moved' % (total['added'], total['removed'], total['moved'])
    detail = ', '.join('%s %d' % (key, count) for key, count in sorted(kinds.items()))
    return summary + ' (' + detail + ')'


def export_current(game, chunk, folder):
    out = os.path.join(folder, 'current-%d.rmc' % chunk)
    flags = 0x08000000 if os.name == 'nt' else 0  # CREATE_NO_WINDOW
    code = subprocess.call([game, '--export-rmc=%d' % chunk, '--out=' + out], cwd=folder, creationflags=flags)
    if code != 0 or not os.path.isfile(out):
        report = os.path.join(folder, 'export-rmc.txt')
        detail = open(report, encoding='utf-8', errors='replace').read().strip() if os.path.isfile(report) else ''
        raise SystemExit('cannot export level %d with %s (exit %d) %s' % (chunk, game, code, detail))
    return open(out, encoding='utf-8', newline='').read()


def canonical(game, text, folder, name):
    """The file written again by the exporter: what it holds, whatever editor saved it."""
    source, out = os.path.join(folder, name + '.in.rmc'), os.path.join(folder, name + '.out.rmc')
    with open(source, 'w', encoding='utf-8', newline='') as f:
        f.write(text)
    flags = 0x08000000 if os.name == 'nt' else 0
    code = subprocess.call([game, '--canonical-rmc=' + source, '--out=' + out], cwd=folder, creationflags=flags)
    if code != 0 or not os.path.isfile(out):
        raise SystemExit('%s does not load (exit %d): see campaign-rmc-error.txt in %s' % (name, code, folder))
    return open(out, encoding='utf-8', newline='').read()


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('edits', help='the zip "Export my changes" wrote')
    parser.add_argument('--repo', default=ROOT, help='the repository (default: this one)')
    parser.add_argument('--game', help='the exe that exports the current levels (default: <repo>/RawMetal.exe)')
    parser.add_argument('--dry-run', action='store_true', help='report only; install nothing')
    args = parser.parse_args()
    repo = os.path.abspath(args.repo)
    game = os.path.abspath(args.game or os.path.join(repo, 'RawMetal.exe'))
    overrides = os.path.join(repo, 'campaign-overrides')
    review = os.path.join(overrides, 'review')
    today = datetime.date.today().isoformat()
    lines = []

    def say(text=''):
        print(text)
        lines.append(text)

    with zipfile.ZipFile(args.edits) as z:
        manifest = z.read('manifest.txt').decode('utf-8').splitlines()
        author = next((l.split('|', 1)[1] for l in manifest if l.startswith('author|')), 'someone')
        exported = next((l.split('|', 1)[1] for l in manifest if l.startswith('exported|')), '?')
        levels = [l.split('|') for l in manifest if l.startswith('level|')]
        say('Edits from %s (exported %s): %d level(s)%s' % (author, exported, len(levels), ' [dry run]' if args.dry_run else ''))
        conflicts = 0
        with tempfile.TemporaryDirectory() as work:
            for _, chunk, version, base_hash, edit_hash, edit_file, base_file in (l + [''] * (7 - len(l)) for l in levels):
                chunk = int(chunk)
                theirs = z.read(edit_file).decode('utf-8')
                base = z.read(base_file).decode('utf-8') if base_file else None
                if data_hash(theirs) != edit_hash:
                    say('  %03d: SKIPPED - the file does not match its manifest hash' % chunk)
                    continue
                current = export_current(game, chunk, work)
                mine_hash = data_hash(current)
                # Compare what the files hold, not how an editor laid them out.
                theirs_canonical = canonical(game, theirs, work, 'theirs-%d' % chunk)
                edit_hash = data_hash(theirs_canonical)
                existing = [f for f in (os.listdir(overrides) if os.path.isdir(overrides) else []) if chunk_of(f) == chunk]
                if mine_hash == edit_hash:
                    say('  %03d %s: already merged' % (chunk, edit_file[4:-4]))
                    continue
                state = 'clean' if mine_hash == base_hash else 'both changed - review'
                say('  %03d %s: %s (based on %s)' % (chunk, edit_file[4:-4], state, version))
                say('      theirs: ' + diff(base, theirs_canonical))
                if state != 'clean':
                    conflicts += 1
                    say('      yours:  ' + diff(base, current))
                if args.dry_run:
                    continue
                os.makedirs(overrides, exist_ok=True)
                if state != 'clean':
                    os.makedirs(review, exist_ok=True)
                    # Yours as it plays now, and the file it came from when it is an override.
                    with open(os.path.join(review, '%03d-yours-%s.rmc' % (chunk, today)), 'w', encoding='utf-8', newline='') as f:
                        f.write(current)
                    for name in existing:
                        shutil.copyfile(os.path.join(overrides, name), os.path.join(review, '%03d-yours-%s-%s' % (chunk, today, name)))
                    if base is not None:
                        with open(os.path.join(review, '%03d-base-%s.rmc' % (chunk, version)), 'w', encoding='utf-8', newline='') as f:
                            f.write(base)
                    say('      kept yours in campaign-overrides/review/')
                for name in existing:  # one file per level: CMake gives each level one resource
                    if name != '%d.rmc' % chunk:
                        os.remove(os.path.join(overrides, name))
                with open(os.path.join(overrides, '%d.rmc' % chunk), 'w', encoding='utf-8', newline='') as f:
                    f.write(theirs)
                say('      installed as campaign-overrides/%d.rmc' % chunk)
        say('%d level(s), %d to review. Rebuild (Build.cmd) to embed them; the workspace game plays them already.' % (len(levels), conflicts))
    os.makedirs(os.path.join(repo, '.build'), exist_ok=True)
    with open(os.path.join(repo, '.build', 'merge-edits-%s.txt' % today), 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
