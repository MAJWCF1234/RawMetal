from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import threading
import unittest
import urllib.error
import urllib.request

import server
from runtime_records import MAX_RECORDS, MAX_RECORD_BYTES, MAX_TOTAL_BYTES, runtime_records


RECORDS = [
    "TIMED_SEQUENCE|0|test-timer|test-done|2|2|22|22|0|3|1000|1000|0|0|0|0|1|1",
    "ACTOR|0|test-timer|0|1|1|0|0",
    "ACTOR_KEY|0|0|0|5|5|0|0|1|0",
    "ACTOR_AI|0|0|3||1.4|2|6",
    "ACTOR_GUARD|0|0|12|0.35|500|3000|test-alert",
    "ACTOR_WEAPON|0|0|1|650|1000|9|18",
    "ACTOR_FACTION|0|0|2|test-security-squad",
    "ACTOR_TACTICS|0|0|1|2",
    "ACTOR_PATROL|0|0|5|5|0|700",
    "ACTOR_PATROL|0|0|5|9|0|500",
    "SURFACE|0|5|8|5",
]


def project() -> dict:
    return {"name": "Reusable Guard Playground", "runtimeRecords": list(RECORDS),
            "chunks": [{"id": "area", "number": 1, "name": "Area", "gx": 0, "gy": 0,
                        "layers": [{"id": "ground", "z": 0, "rows": ["." * 24] * 24}],
                        "objects": []}]}


class RuntimeRecordsTests(unittest.TestCase):
    def test_optional_default_and_exact_preservation(self) -> None:
        self.assertEqual(runtime_records({}), [])
        original = project()
        records = runtime_records(original)
        self.assertEqual(records, RECORDS)
        self.assertIsNot(records, original["runtimeRecords"])
        self.assertEqual(runtime_records(json.loads(json.dumps(original))), RECORDS)

    def test_rejects_invalid_types_and_geometry(self) -> None:
        for value in (None, "ACTOR|0", {}, ("ACTOR|0",), [2], [None],
                      [""], ["ACTOR"], ["ACTOR |0"], ["UNKNOWN|0"],
                      ["MAP|0|Replacement"], ["CAMPAIGN|Replacement|0"],
                      ["STRUCT|0|0|0|1|1|0|3|0|0"], ["SEQUENCE_LOOP|0|0"]):
            with self.subTest(value=value), self.assertRaises(ValueError):
                runtime_records({"runtimeRecords": value})

    def test_rejects_multiline_controls_and_markers(self) -> None:
        for suffix in ("\nMAP|0", "\rMAP|0", "\r\nMAP|0", "\vMAP|0", "\fMAP|0",
                       "\x00", "\x1f", "\x7f", "\u0085MAP|0", "\u2028MAP|0",
                       "\u2029MAP|0", "\u0085", "\u2028", "\u2029", "--- CUSTOM_CAMPAIGN_DATA_END ---",
                       "--- CUSTOM_CAMPAIGN_DATA_START ---"):
            with self.subTest(suffix=suffix), self.assertRaises(ValueError):
                runtime_records({"runtimeRecords": ["ACTOR|0|" + suffix]})

    def test_bounds_are_byte_based(self) -> None:
        with self.assertRaises(ValueError):
            runtime_records({"runtimeRecords": ["ACTOR|0"] * (MAX_RECORDS + 1)})
        exact = "ACTOR|" + "x" * (MAX_RECORD_BYTES - len("ACTOR|"))
        self.assertEqual(runtime_records({"runtimeRecords": [exact]}), [exact])
        for record in (exact + "x", "ACTOR|" + "é" * (MAX_RECORD_BYTES // 2)):
            with self.subTest(record_length=len(record)), self.assertRaises(ValueError):
                runtime_records({"runtimeRecords": [record]})
        with self.assertRaises(ValueError):
            runtime_records({"runtimeRecords": [exact] * (MAX_TOTAL_BYTES // (MAX_RECORD_BYTES + 1) + 1)})

    def test_payload_suffix_order_and_no_mutation(self) -> None:
        data = project()
        before = copy.deepcopy(data)
        campaign, _ = server.build_runtime_campaign(data, data["name"])
        self.assertEqual(campaign.splitlines()[-len(RECORDS):], RECORDS)
        payload, _ = server.build_map_payload(data, "area", 1, data["name"], "CUSTOM")
        inner = payload.split("--- CUSTOM_CAMPAIGN_DATA_START ---\n", 1)[1].split("\n--- CUSTOM_CAMPAIGN_DATA_END ---", 1)[0]
        self.assertEqual(inner, campaign)
        self.assertEqual(data, before)
        # This helper deliberately does not duplicate the engine's syntax parser.
        malformed = project()
        malformed["runtimeRecords"] = ["ACTOR|bad-fields"]
        self.assertTrue(server.build_runtime_campaign(malformed, "Syntax checked by engine")[0].endswith("ACTOR|bad-fields"))

    def test_old_projects_export_without_script_records(self) -> None:
        data = project()
        del data["runtimeRecords"]
        omitted = server.build_runtime_campaign(data, data["name"])
        data["runtimeRecords"] = []
        self.assertEqual(omitted, server.build_runtime_campaign(data, data["name"]))

    def test_http_save_load_export_and_rejection(self) -> None:
        class QuietHandler(server.Handler):
            def log_message(self, fmt, *args):
                pass

        with tempfile.TemporaryDirectory() as folder:
            original_root = server.PROJECT_ROOT
            server.PROJECT_ROOT = Path(folder)
            httpd = server.ThreadingHTTPServer(("127.0.0.1", 0), QuietHandler)
            thread = threading.Thread(target=httpd.serve_forever, daemon=True)
            thread.start()
            url = f"http://127.0.0.1:{httpd.server_port}"

            def post(route: str, data: dict) -> dict:
                request = urllib.request.Request(url + route, data=json.dumps(data).encode(),
                                                 headers={"Content-Type": "application/json"})
                with urllib.request.urlopen(request, timeout=5) as response:
                    return json.load(response)

            try:
                data = project()
                saved = post("/__depthworks_save_project", {"file": "roundtrip", "project": data})
                self.assertTrue(saved["ok"])
                path = Path(folder) / saved["file"]
                original_bytes = path.read_bytes()
                with urllib.request.urlopen(url + "/__depthworks_project.json?name=roundtrip", timeout=5) as response:
                    loaded = json.load(response)["project"]
                self.assertEqual(loaded, data)
                exported = post("/__depthworks_map_payload", {"project": loaded, "chunk": "area", "level": 1,
                                 "name": data["name"], "target": "CUSTOM"})
                self.assertEqual(exported["payload"], server.build_map_payload(data, "area", 1, data["name"], "CUSTOM")[0])
                bad = project()
                bad["runtimeRecords"] = ["MAP|0|injection"]
                for route in ("/__depthworks_save_project", "/__depthworks_map_payload"):
                    with self.subTest(route=route), self.assertRaises(urllib.error.HTTPError) as rejected:
                        post(route, {"file": "roundtrip", "project": bad, "chunk": "area", "level": 1, "target": "CUSTOM"})
                    self.assertEqual(rejected.exception.code, 400)
                    rejected.exception.close()
                self.assertEqual(path.read_bytes(), original_bytes)
                # Loading/saving older editor projects fills the optional field.
                old = project()
                del old["runtimeRecords"]
                post("/__depthworks_save_project", {"file": "old", "project": old})
                self.assertEqual(json.loads((Path(folder) / "old.json").read_text())["runtimeRecords"], [])
            finally:
                httpd.shutdown()
                httpd.server_close()
                thread.join(timeout=5)
                server.PROJECT_ROOT = original_root


if __name__ == "__main__":
    unittest.main()
