"""Join clips baked onto Claire into one animation bank.

Every bank must come from the same Claire.fbx (the same corners and the same
skinned-vertex order). Each SOURCE is BANK.bin, or BANK.bin:FIRST:END for its
clips FIRST up to (not including) END; they are joined in the order given, and
their hand anchors (.anchors) with them.

  python tools/player/append_claire_clips.py OUT.bin SOURCE [SOURCE ...]

claire-animation.bin as shipped (40 clips): the 32-clip bank of v1.1.14 with its
turns (28-31) baked again with :drift, then the clips 32-39 of the third-person
hold bake (docs/Systems/player-body.md has both commands):

  python tools/player/append_claire_clips.py claire-animation.bin base.bin:0:28 turns.bin:2:6 base.bin:28:28 holds.bin
"""
import struct, sys


def read(path):
    data = open(path, 'rb').read()
    magic, corners, total, clips, unique = struct.unpack_from('<4s4I', data)
    if magic != b'RMA3':
        raise SystemExit(f'{path}: not an RMA3 bank')
    remap = data[20:20 + corners * 2]
    counts = list(struct.unpack_from(f'<{clips}I', data, 20 + corners * 2))
    start = 20 + corners * 2 + clips * 4
    stride = unique * 6
    if len(data) != start + total * stride:
        raise SystemExit(f'{path}: carries a trailer this script does not keep')
    anchors = open(path + '.anchors', 'rb').read()
    if len(anchors) != total * 24:
        raise SystemExit(f'{path}.anchors: wrong size')
    return (corners, unique, remap), counts, data[start:], anchors, stride


def main():
    out, sources = sys.argv[1], sys.argv[2:]
    topology, counts, payload, anchors = None, [], b'', b''
    for source in sources:
        parts = source.rsplit(':', 2)
        path, first, end = (parts[0], int(parts[1]), int(parts[2])) if len(parts) == 3 and parts[1].isdigit() else (source, None, None)
        shape, n, p, a, stride = read(path)
        if topology is None:
            topology = shape
        elif shape != topology:
            raise SystemExit(f'{path}: baked from a different mesh')
        first = 0 if first is None else first
        end = len(n) if end is None else end
        skip, take = sum(n[:first]), sum(n[first:end])
        counts += n[first:end]
        payload += p[skip * stride:(skip + take) * stride]
        anchors += a[skip * 24:(skip + take) * 24]
    if len(counts) > 64:
        raise SystemExit('a bank holds at most 64 clips')
    corners, unique, remap = topology
    head = struct.pack('<4s4I', b'RMA3', corners, sum(counts), len(counts), unique)
    with open(out, 'wb') as f:
        f.write(head + remap + struct.pack(f'<{len(counts)}I', *counts) + payload)
    with open(out + '.anchors', 'wb') as f:
        f.write(anchors)
    print(f'{out}: {len(counts)} clips, {sum(counts)} samples')


main()
