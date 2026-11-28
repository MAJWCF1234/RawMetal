"""Remove FBX animation objects when the runtime shares another rig's actions.
Geometry, bind transforms, skinning, UVs and materials remain unchanged.
"""
from dataclasses import dataclass, field
import argparse
from pathlib import Path
import struct

@dataclass
class Node:
    name: bytes
    props: list[bytes] = field(default_factory=list)
    children: list = field(default_factory=list)
    terminated: bool = False

def parse(data):
    if len(data) < 27 or data[:23] != b"Kaydara FBX Binary  \x00\x1a\x00":
        raise ValueError("Expected a binary FBX")
    version = struct.unpack_from("<I", data, 23)[0]
    if version != 7400:
        raise ValueError(f"Only FBX 7400 is supported, got {version}")
    def node(pos, limit):
        if pos + 13 > limit:
            raise ValueError("Truncated FBX node")
        end, count, size, length = struct.unpack_from("<IIIB", data, pos)
        start = pos + 13 + length
        stop = start + size
        if not pos < end <= limit or stop > end:
            raise ValueError("Invalid FBX node bounds")
        props, cursor = [], start
        for _ in range(count):
            begin = cursor
            if cursor >= stop:
                raise ValueError("Truncated FBX property")
            kind = chr(data[cursor]); cursor += 1
            if kind in "fdlibc":
                if cursor + 12 > stop:
                    raise ValueError("Truncated FBX array")
                _, encoding, n = struct.unpack_from("<III", data, cursor)
                if encoding not in (0, 1):
                    raise ValueError("Invalid FBX array encoding")
                cursor += 12 + n
            elif kind in "YCFDIL":
                cursor += {"Y": 2, "C": 1, "F": 4, "D": 8, "I": 4, "L": 8}[kind]
            elif kind in "SR":
                if cursor + 4 > stop:
                    raise ValueError("Truncated FBX string")
                n = struct.unpack_from("<I", data, cursor)[0]
                cursor += 4 + n
            else:
                raise ValueError(f"Unknown FBX property {kind!r}")
            if cursor > stop:
                raise ValueError("FBX property exceeds its node")
            props.append(data[begin:cursor])
        if cursor != stop:
            raise ValueError("FBX property count mismatch")
        result = Node(data[pos+13:start], props)
        cursor = stop
        while cursor < end:
            if data[cursor:cursor+13] == bytes(13):
                result.terminated = True; cursor += 13
                if cursor != end:
                    raise ValueError("Unexpected FBX trailing node data")
                break
            child, cursor = node(cursor, end)
            result.children.append(child)
        return result, end
    roots, pos = [], 27
    while pos + 13 <= len(data) and data[pos:pos+13] != bytes(13):
        item, pos = node(pos, len(data)); roots.append(item)
    return data[:27], roots, data[pos:]

def serialize(header, roots, footer):
    out = bytearray(header)
    def write(n):
        start = len(out)
        props = b"".join(n.props)
        out.extend(bytes(13)); out.extend(n.name); out.extend(props)
        for c in n.children: write(c)
        if n.terminated: out.extend(bytes(13))
        struct.pack_into("<IIIB", out, start, len(out), len(n.props), len(props), len(n.name))
    for n in roots: write(n)
    out.extend(footer)
    return bytes(out)

def strip_animation(data):
    header, roots, footer = parse(data)
    if serialize(header, roots, footer) != data:
        raise ValueError("FBX parser did not preserve the source")
    objects = next(n for n in roots if n.name == b"Objects")
    removed = [n for n in objects.children if n.name.startswith(b"Animation")]
    if not removed:
        raise ValueError("No animation objects to share")
    ids = {struct.unpack_from("<q", n.props[0], 1)[0] for n in removed}
    objects.children = [n for n in objects.children if not n.name.startswith(b"Animation")]
    connections = next(n for n in roots if n.name == b"Connections")
    connections.children = [
        n for n in connections.children
        if n.name != b"C" or not any(
            struct.unpack_from("<q", p, 1)[0] in ids for p in n.props[1:3] if p[:1] == b"L"
        )
    ]
    definitions = next((n for n in roots if n.name == b"Definitions"), None)
    if definitions:
        definitions.children = [
            n for n in definitions.children
            if not (n.name == b"ObjectType" and n.props[0][5:].startswith(b"Animation"))
        ]
        for n in definitions.children:
            if n.name == b"Count":
                old = struct.unpack_from("<i", n.props[0], 1)[0]
                n.props[0] = b"I" + struct.pack("<i", old-len(removed))
    return serialize(header, roots, footer)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.source.resolve() == args.output.resolve():
        parser.error("Use a separate output; preserve the animated source")
    source = args.source.read_bytes()
    result = strip_animation(source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)
    print(f"{len(source):,} -> {len(result):,} bytes; geometry and skin data retained")

if __name__ == "__main__":
    main()
