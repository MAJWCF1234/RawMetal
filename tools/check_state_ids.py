"""Audit literal state names without traversing licensed assets or junctions.

Run from any directory: python tools/check_state_ids.py [--cpp-values REPORT]
The optional report is emitted by RawMetal.exe --content-integrity-test.
"""
import argparse
import itertools
import os
from pathlib import Path
import re
from urllib.parse import unquote_to_bytes

ROOT = Path(__file__).resolve().parents[1]
PRUNE = {"library", "source-assets", ".git", ".build", "node_modules", "__pycache__"}
SOURCE = {".cpp", ".h", ".hpp", ".cc", ".cxx", ".inl"}
# Fields passed to stateId(decode(...)) or scriptName in CustomCampaign.cpp.
STATE_FIELDS = {
 "OBJECTIVE_TEXT": (2,), "SCRIPT_EVENT": (2, 9), "SCRIPT_ACTION": (2,),
 "DOOR": (11,), "LIGHT": (11,), "DARK": (8,), "TERMINAL": (9, 11),
 "CARGO_LIFT": (9, 10, 11, 12, 13, 14), "TIMED_SEQUENCE": (2, 3),
 "ACTOR": (2,), "ACTOR_AI": (4,), "ACTOR_FACTION": (4,),
 "ACTOR_GUARD": (7,), "ACTOR_HEALTH": (4, 5), "SEQUENCE_CUE": (2,),
}

def state_id(value):
    h = 2166136261
    for byte in value:
        h = ((h ^ byte) * 16777619) & 0xffffffff
    return (h & 0x7fffff) or 1



def cpp_literal(token):
    """Decode ordinary C++ UTF-8 string literals, including byte escapes."""
    result = bytearray()
    body = token[1:-1]
    i = 0
    simple = {"n": 10, "r": 13, "t": 9, "a": 7, "b": 8, "f": 12, "v": 11,
              "\\": 92, '"': 34, "'": 39, "?": 63}
    while i < len(body):
        if body[i] != "\\":
            result.extend(body[i].encode("utf-8")); i += 1; continue
        i += 1
        char = body[i]
        if char in simple:
            result.append(simple[char]); i += 1
        elif char in "01234567":
            match = re.match(r"[0-7]{1,3}", body[i:])[0]
            result.append(int(match, 8) & 255); i += len(match)
        elif char == "x":
            i += 1
            match = re.match(r"[0-9a-fA-F]+", body[i:])[0]
            result.append(int(match, 16) & 255); i += len(match)
        elif char in "uU":
            size = 4 if char == "u" else 8
            result.extend(chr(int(body[i+1:i+1+size], 16)).encode("utf-8")); i += size+1
        elif char == "\n":
            i += 1
        else:
            raise ValueError("unsupported C++ escape: " + char)
    return bytes(result)


def safe_files(root):
    for folder, dirs, files in os.walk(root, followlinks=False):
        # Prune before descent, including Windows junctions on Python < 3.12.
        dirs[:] = sorted(d for d in dirs if d.lower() not in PRUNE and not
                         (os.lstat(Path(folder) / d).st_file_attributes & 0x400
                          if os.name == "nt" else (Path(folder) / d).is_symlink()))
        for name in sorted(files):
            path = Path(folder) / name
            if path.suffix.lower() not in SOURCE | {".txt"}:
                continue
            if path.is_symlink() or (os.name == "nt" and os.lstat(path).st_file_attributes & 0x400):
                continue
            if path.suffix.lower() in SOURCE | {".txt"}:
                yield path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp-values", type=Path)
    args = parser.parse_args()
    if args.cpp_values:
        values = [line.split() for line in args.cpp_values.read_text().splitlines() if line.startswith("HASH ")]
        if len(values) < 2:
            parser.error("C++ report must contain at least two HASH records")
        for _, name, expected in values:
            if state_id(name.encode()) != int(expected):
                parser.error("C++ hash mismatch: " + name)
        print("C++ hash parity:", len(values), "values")
    names = {}
    source_count = campaign_count = 0
    def add(value, location):
        names.setdefault(value, set()).add(location)
    literal = re.compile(r'\bstateId\s*\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)')
    for path in safe_files(ROOT):
        text = path.read_text(encoding="utf-8-sig", errors="replace")
        relative = path.relative_to(ROOT)
        if path.suffix.lower() in SOURCE and relative.parts[0] == "src":
            source_count += 1
            # Remove comments to avoid treating documentation as authored states.
            text = re.sub(r'"(?:[^"\\]|\\.)*"|//[^\n]*|/\*.*?\*/', lambda m: m[0] if m[0].startswith(chr(34)) else "\n" * m[0].count("\n"), text, flags=re.S)
            for match in literal.finditer(text):
                value = b"".join(cpp_literal(token) for token in re.findall(r'"(?:[^"\\]|\\.)*"', match[1]))
                add(value, f"{relative}:{text.count(chr(10), 0, match.start()) + 1}")
        elif path.suffix.lower() == ".txt" and "CUSTOM_CAMPAIGN_DATA_START" in text:
            campaign_count += 1
            for number, line in enumerate(text.splitlines(), 1):
                f = line.strip().split("|")
                indices = STATE_FIELDS.get(f[0], ())
                if f[0] == "SCRIPT_ACTION" and len(f) > 3 and f[3] in {"STATE", "OBJECTIVE", "ITEM"}:
                    indices += (4,)
                for i in indices:
                    if i < len(f) and f[i]:
                        add(unquote_to_bytes(f[i]), f"{relative}:{number} field {i}")
    by_id = {}
    for name in names:
        by_id.setdefault(state_id(name), []).append(name)
    collisions = 0
    for value, group in sorted(by_id.items()):
        for a, b in itertools.combinations(sorted(group), 2):
            collisions += 1
            print(f"COLLISION id={value}: {a!r} vs {b!r}")
            for name in (a, b):
                print(" ", repr(name), ":", "; ".join(sorted(names[name])))
    print(f"{source_count} source files; {campaign_count} campaign text files; {len(names)} distinct names; {collisions} collision pairs")
    return int(collisions > 0)

if __name__ == "__main__":
    raise SystemExit(main())
