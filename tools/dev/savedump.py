"""List what a RawMetal save holds: its header, and from format 28 each section's name, version,
flags, size and whether its checksum holds (docs/Systems/save-format.md).

  python tools/dev/savedump.py PATH [PATH...]
"""
import struct, sys

def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & 0xffffffff
    return h

def dump(path):
    data = open(path, 'rb').read()
    split = data.find(b'\n')
    if split < 0:
        print('%s: no header line' % path); return 1
    head = data[:split].decode('ascii', 'replace').split()
    payload = data[split + 1:]
    if len(head) != 3 or head[0] != 'RAWMETAL_SAVE':
        print('%s: not a RawMetal save' % path); return 1
    version, checksum = int(head[1]), int(head[2])
    print('%s: format %d, %d bytes, payload checksum %s' % (path, version, len(data), 'ok' if fnv(payload) == checksum else 'MISMATCH'))
    if version < 28:
        print('  positional text archive (formats 1-27): refitted to format 28 on load'); return 0
    at, bad = 0, 0
    while at < len(payload):
        n = payload[at]
        if n == 0 or at + 1 + n + 12 > len(payload):
            print('  frame cut off or damaged at byte %d' % at); return 1
        name = payload[at + 1:at + 1 + n].decode('ascii', 'replace'); at += 1 + n
        sv, flags, size, check = struct.unpack_from('<HHII', payload, at); at += 12
        body = payload[at:at + size]; ok = len(body) == size and fnv(body) == check; bad += not ok
        print('  %-12s v%-3d %8d bytes%s%s' % (name, sv, size, '  required' if flags & 1 else '', '' if ok else '  DAMAGED'))
        at += size
    return 1 if bad else 0

if __name__ == '__main__':
    if len(sys.argv) < 2: print(__doc__); sys.exit(2)
    sys.exit(max(dump(p) for p in sys.argv[1:]))
