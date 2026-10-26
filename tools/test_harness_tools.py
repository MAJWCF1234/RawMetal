"""Focused tests for collision hashing, safe traversal and edge-height sampling."""
import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
(HERE.parent / ".build").mkdir(exist_ok=True)
spec = importlib.util.spec_from_file_location("state_ids", HERE / "check_state_ids.py")
state_ids = importlib.util.module_from_spec(spec)
spec.loader.exec_module(state_ids)

class HarnessToolsTest(unittest.TestCase):
    def test_hash_native_reference_values(self):
        # Independently emitted by the native content-integrity test.
        self.assertEqual(state_ids.state_id(b"receiving_cleanup"), 4679438)
        self.assertEqual(state_ids.state_id(b"receiving_breach"), 2964111)
        self.assertEqual(state_ids.state_id(b""), 1875397)

    def test_cpp_literal_bytes(self):
        self.assertEqual(state_ids.cpp_literal(r'"\xff\377\?"'), b"\xff\xff?")
        self.assertEqual(state_ids.cpp_literal(r'"\u00e9"'), "é".encode())

    def test_distinct_colliding_names_are_reported(self):
        with tempfile.TemporaryDirectory(dir=HERE.parent / ".build") as directory:
            root = Path(directory)
            (root / "src").mkdir()
            # Find a real collision in the 23-bit namespace, deterministically.
            seen = {}
            for i in range(100000):
                name = ("collision_fixture_%d" % i).encode()
                value = state_ids.state_id(name)
                if value in seen:
                    first, second = seen[value], name
                    break
                seen[value] = name
            else:
                self.fail("collision fixture search exhausted")
            (root / "src/test.cpp").write_text('stateId("%s");stateId("%s");' % (first.decode(), second.decode()))
            previous_root, previous_argv = state_ids.ROOT, sys.argv
            try:
                state_ids.ROOT, sys.argv = root, ["check_state_ids.py"]
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    self.assertEqual(state_ids.main(), 1)
                self.assertIn("COLLISION id=", output.getvalue())
                self.assertIn(first.decode(), output.getvalue())
                self.assertIn(second.decode(), output.getvalue())
            finally:
                state_ids.ROOT, sys.argv = previous_root, previous_argv

    def test_edge_height_and_all_border_cells(self):
        with tempfile.TemporaryDirectory(dir=HERE.parent / ".build") as directory:
            root = Path(directory)
            # Map 34's first planned edge is sourced from the real plan.
            sys.path.insert(0, str(HERE / "campaign"))
            import plan
            chunk = next(c for c in plan.CHUNKS if c["idx"] == 34)
            records = {}
            for edge in chunk["edges"]:
                import math
                for t in range(int(edge["a"]), math.ceil(edge["b"])):
                    side = edge["side"]
                    x, y = (t, 0) if side == "N" else (t, 23) if side == "S" else (0, t) if side == "W" else (23, t)
                    records[x, y, edge["z"]] = "."
            audit = root / "map-audit-34.txt"
            def write(extra=""):
                audit.write_text(("."*24+"\n")*24 + "".join("BORDER %d %d %g %s\n" % (*key, mark) for key, mark in records.items()) + extra)
            def run():
                return subprocess.run([sys.executable, str(HERE / "campaign/edge_check.py"), str(root), "34", "34"], text=True, capture_output=True)
            write()
            self.assertEqual(run().returncode, 0)
            key = next(iter(records))
            write("BORDER %d %d %g .\n" % (key[0], key[1], key[2]-2))
            result = run()
            self.assertEqual(result.returncode, 1)
            self.assertIn("pit under the edge", result.stdout)
            shifted = {(x, y, z-2): mark for (x, y, z), mark in records.items()}
            records = shifted
            write()
            result = run()
            self.assertEqual(result.returncode, 1)
            self.assertIn('border tiles "o', result.stdout)

if __name__ == "__main__":
    unittest.main()
