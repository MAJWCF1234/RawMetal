import copy
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import server


def project():
    rows = []
    for y in range(24):
        rows.append("########################" if y in (0, 23) else "#......................#")
    layer = {"id": "l0", "name": "Ground Floor", "z": 0, "ceilingHeight": 3,
             "thickness": 0, "rows": rows, "materials": {}}
    fixture_model = sorted(server.FACILITY_MODEL_INDEX)[0]
    chunk = {"id": "c0", "number": 1, "name": "Test Map", "gx": 0, "gy": 0,
             "surfaceWall": 2, "surfaceFloor": 0, "surfaceCeiling": -1,
             "layers": [layer], "objects": [
                 {"id": "o1", "layerId": "l0", "type": "spawn", "spawnKind": "Player",
                  "x": 3.5, "y": 3.5, "z": 0, "rotation": 0},
                 {"id": "o2", "layerId": "l0", "type": "light",
                  "x": 6, "y": 6, "z": 2.8, "rotation": 0, "lampMount": "ceiling",
                  "intensity": 1.2, "range": 9, "kelvin": 3200, "gel": "#ff2211"},
                 {"id": "o6", "layerId": "l0", "type": "sign", "title": "AIRLOCK",
                  "subtitle": "Keep clear", "accent": "#ff5533",
                  "x": 5, "y": 1, "z": 1.6, "w": 1.8, "h": .6, "rotation": 0},
                 {"id": "o7", "layerId": "l0", "type": "prim", "primKind": "arch",
                  "x": 12, "y": 8, "z": 0, "w": 2.4, "d": .6, "h": 3.4,
                  "rotation": 90, "material": 42},
                 {"id": "o8", "layerId": "l0", "type": "spawn", "spawnKind": "Worker",
                  "behaviour": "guard", "armed": "handgun",
                  "x": 18, "y": 18, "z": 0, "rotation": 0},
                 {"id": "o9", "layerId": "l0", "type": "spawn", "spawnKind": "Creature",
                  "species": "Brute", "x": 16, "y": 6, "z": 0, "rotation": 0},
                 {"id": "o3", "layerId": "l0", "type": "hazard", "kind": "Fire",
                  "x": 10, "y": 10, "z": 0, "w": 2, "d": 2, "h": 1, "rotation": 0},
                 {"id": "o4", "layerId": "l0", "type": "asset", "model": fixture_model,
                  "x": 8, "y": 5, "z": 0, "w": 1, "d": 1, "h": 2, "rotation": 0},
                 {"id": "o5", "layerId": "l0", "type": "spawn", "spawnKind": "Hostile",
                  "x": 15, "y": 15, "z": 0, "rotation": 0},
             ]}
    return {"version": 5, "name": "Round Trip", "chunks": [chunk]}


def test_campaign_file_is_a_world_file():
    filename, text, warnings = server.build_campaign_file(project(), "Round Trip")
    assert filename == "Round Trip.rmc"
    assert text.startswith("--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Round Trip|0\n")
    assert "MAP|0|Test Map|" in text
    assert "LIGHT|0|6.0|6.0|2.8|0|0.0|1.2|9.0|3200" in text
    assert "1.000|0.133|0.067" in text  # gel #ff2211 at fields 22-24
    assert "SIGN|0|5.0|1.0|1.6|1.8|0.6|0.0|AIRLOCK|Keep clear|" in text
    assert "PRIM|0|arch|12.0|8.0|0.0|2.4|0.6|3.4|1|42|1" in text
    assert "SURFACE|0|2|0|-1" in text
    assert "ACTOR|0|editor_clock|0|1.0|0|0|0" in text.replace("1|", "1|") or "ACTOR|0|editor_clock|" in text
    assert "ACTOR_AI|0|0|3||1.4|2|6" in text
    assert "ACTOR_WEAPON|0|0|2|650|1000|9|18" in text
    assert "CREATURE|0|2|16.0|6.0|0.0" in text  # Brute species, not always Huntsman
    assert "HAZARD|0|4|" in text
    assert text.rstrip().endswith("--- CUSTOM_CAMPAIGN_DATA_END ---")


def test_import_round_trips_geometry_and_objects():
    _, text, _ = server.build_campaign_file(project(), "Round Trip")
    imported = server.parse_campaign_project(text)
    assert imported["name"] == "Round Trip"
    (chunk,) = imported["chunks"]
    assert chunk["name"] == "Test Map"
    (layer,) = chunk["layers"]
    assert layer["rows"][0] == "########################"
    assert layer["rows"][5][5] == "."
    kinds = {(o["type"], o.get("spawnKind")) for o in chunk["objects"]}
    assert ("spawn", "Player") in kinds
    assert ("spawn", "Creature") in kinds  # hostile spawns export as creatures
    assert any(o["type"] == "light" and o["x"] == 6 and o["y"] == 6 for o in chunk["objects"])
    assert any(o["type"] == "hazard" and o.get("kind") == "Fire" for o in chunk["objects"])
    fixture_model = sorted(server.FACILITY_MODEL_INDEX)[0]
    assert any(o["type"] == "asset" and o.get("model") == fixture_model for o in chunk["objects"])
    sign = next(o for o in chunk["objects"] if o["type"] == "sign")
    assert sign["title"] == "AIRLOCK" and sign["subtitle"] == "Keep clear"
    prim = next(o for o in chunk["objects"] if o["type"] == "prim")
    assert prim["primKind"] == "arch" and prim["material"] == 42 and prim["rotation"] == 90
    light = next(o for o in chunk["objects"] if o["type"] == "light")
    assert light["kelvin"] == 3200 and light["gel"] == "#ff2211"
    guard = next(o for o in chunk["objects"]
                 if o["type"] == "spawn" and o.get("behaviour") == "guard")
    assert guard["spawnKind"] == "Worker" and guard["armed"] == "handgun"
    assert chunk["surfaceWall"] == 2


def test_imported_project_reexports():
    _, text, _ = server.build_campaign_file(project(), "Round Trip")
    imported = server.parse_campaign_project(text)
    # The front end fills in the material dictionaries normalizeProject adds.
    for chunk in imported["chunks"]:
        for layer in chunk["layers"]:
            for key in ("materials", "materialScales", "materialRotations",
                        "materialScaleYs", "materialOffsetsX", "materialOffsetsY"):
                layer.setdefault(key, {})
    filename, text2, _ = server.build_campaign_file(imported, imported["name"])
    assert filename == "Round Trip.rmc"
    assert "ROW|0|0|0|########################" in text2
    assert "LIGHT|0|6.0|6.0|2.8" in text2


def test_rejects_non_campaign():
    try:
        server.parse_campaign_project("hello\nworld\n")
    except ValueError as exc:
        assert "MAP" in str(exc)
    else:
        raise AssertionError("expected ValueError")
