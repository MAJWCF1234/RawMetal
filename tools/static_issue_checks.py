"""Fast source/content integrity checks for the external issue hunter."""
import hashlib, math, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NUMBER = re.compile(r"^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$")

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def number(value):
    if not NUMBER.match(value.strip()): return None
    value = float(value)
    return value if math.isfinite(value) else None

def run():
    failures, warnings = [], []
    authored = ROOT / "src/assets/campaigns"
    installed = ROOT / "custom maps"
    name = "RawMetal-Ashfall-Exclusion-Zone.txt"
    a, b = authored / name, installed / name
    if not a.exists() or not b.exists(): failures.append("MAP MIRROR missing")
    elif sha(a) != sha(b): failures.append("MAP MIRROR mismatch: authored and installed Ashfall maps differ")
    seen, files = set(), sorted(authored.glob("*.txt"))
    for path in files:
        for line_no, raw in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            if not raw or raw.startswith("#"): continue
            fields = raw.split("|")
            for index, field in enumerate(fields[1:], 1):
                if field.strip().lower() in {"nan", "+nan", "-nan", "inf", "+inf", "-inf", "infinity", "+infinity", "-infinity"}:
                    failures.append(f"NONFINITE {path.name}:{line_no} field {index}")
            if fields[0] != "CREATURE" or len(fields) < 6: continue
            vals = [number(fields[i]) for i in (1, 3, 4, 5)]
            if any(v is None for v in vals):
                failures.append(f"BAD CREATURE numeric data {path.name}:{line_no}"); continue
            map_id, x, y, z = vals
            if not 0 <= map_id <= 179 or not -1 <= x <= 25 or not -1 <= y <= 25 or not -80 <= z <= 40:
                failures.append(f"CREATURE out of bounds {path.name}:{line_no} map={map_id} pos={x},{y},{z}")
            key = (path.name, int(map_id), round(x, 4), round(y, 4), round(z, 4))
            if key in seen: failures.append(f"DUPLICATE CREATURE spawn {path.name}:{line_no} map={map_id} pos={x},{y},{z}")
            seen.add(key)
    for path in (ROOT / "src").rglob("*.cpp"):
        text = path.read_text(encoding="utf-8", errors="replace")
        if '"Incoming"' in text or "'Incoming'" in text: warnings.append(str(path.relative_to(ROOT)))
    if failures:
        for item in failures: print("FAIL " + item)
        print(f"FAIL static-integrity {len(failures)} issue(s)"); return 1
    print(f"PASS static-integrity maps={len(files)} duplicate-spawns=0 nonfinite=0")
    for item in warnings: print("DETAIL Incoming warning token present in: " + item)
    print("ALL PASSED 0"); return 0

if __name__ == "__main__": sys.exit(run())
