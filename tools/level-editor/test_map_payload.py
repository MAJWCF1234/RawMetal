from __future__ import annotations

import server
import re


def rows(fill: str = ".") -> list[str]:
    return [fill * 24 for _ in range(24)]


def test_payload() -> None:
    ground = rows(".")
    ground[0] = "#" * 24
    ground[23] = "#" * 24
    upper = ["_" * 24 for _ in range(24)]
    upper[4] = "_" * 8 + "====#===" + "_" * 8

    project = {
        "name": "Payload Self Test",
        "version": 5,
        "chunks": [{
            "id": "area-a",
            "number": 7,
            "name": "Pump Annex Test",
            "gx": 0,
            "gy": 0,
            "layers": [
                {"id": "ground", "name": "Ground", "z": -9, "ceilingHeight": 3.4, "thickness": 0, "rows": ground, "materials": {}},
                {"id": "upper", "name": "Upper", "z": -4, "ceilingHeight": 3.0, "thickness": .25, "rows": upper, "materials": {}},
            ],
            "objects": [
                {"id": "player", "layerId": "ground", "type": "spawn", "spawnKind": "Player", "x": 3.5, "y": 2.5, "z": -9, "rotation": 0},
                {"id": "exit", "layerId": "ground", "type": "door", "name": "Transfer", "x": 20.5, "y": 23.5, "z": -9, "w": 2, "d": .18, "h": 2.4, "wallAxis": "horizontal", "rotation": 0},
                {"id": "stairs", "layerId": "ground", "type": "stairs", "name": "Stairs", "x": 10, "y": 10, "z": -9, "w": 1.2, "d": 8, "h": 5, "steps": 28, "rotation": 0, "targetLayerId": "upper"},
                {"id": "cab", "layerId": "ground", "type": "asset", "name": "Wall Cabinet", "x": 5, "y": 5, "z": -9, "w": .67, "d": .20, "h": .91, "rotation": 90, "scale": 1, "model": "src/assets/facility/source/wall_box_2.fbx"},
                {"id": "light", "layerId": "upper", "type": "light", "x": 12, "y": 6, "z": -1.3, "intensity": 1, "radius": 5},
                {"id": "hazard", "layerId": "ground", "type": "hazard", "name": "Arc", "kind": "Electricity", "x": 15, "y": 12, "z": -9, "w": 2, "d": 2, "h": 1},
            ],
        }],
    }

    payload, warnings = server.build_map_payload(project, "area-a", 7, "Pump Annex Test", "MAIN")
    assert "META_LEVEL_ID: 7" in payload
    assert "META_LEVEL_NAME: Pump Annex Test" in payload
    assert "META_DEFAULT_TARGET: MAIN" in payload
    assert "--- MAP_CODE_START ---" in payload and "--- MAP_CODE_END ---" in payload
    assert "if(m_level==7)" in payload
    assert "m_layers={" in payload
    assert "m_doors.push_back" in payload and ",true,false,0.0f}" in payload
    assert "stairs.push_back" in payload
    assert "m_fixtures.push_back({8," in payload
    assert "m_lights.push_back" in payload
    assert "Hazard::Kind::Electricity" in payload
    assert not warnings

    custom, _ = server.build_map_payload(project, "area-a", 2, "Standalone Test", "CUSTOM")
    assert "META_LEVEL_ID: 2" in custom and "META_DEFAULT_TARGET: CUSTOM" in custom

    try:
        server.build_map_payload(project, "area-a", 2, "Bad Main", "MAIN")
    except ValueError:
        pass
    else:
        raise AssertionError("MAIN payload below dynamic level 6 was accepted")


def test_open_stair_exports() -> None:
    # Both supported export formats must preserve thin open treads, including
    # rotated stairs and legacy projects that have no new staircase fields.
    for rotation in (0, 90, 180, 270):
        for properties, expected_open, expected_thickness in (
            ({}, False, .12),
            ({"openUnderside": True, "treadThickness": .16, "sideRails": True}, True, .16),
            ({"openUnderside": True, "treadThickness": 0}, True, .025),
            ({"openUnderside": True, "treadThickness": 8}, True, .5),
        ):
            project = {"name": "Open Stairs", "chunks": [{
                "id": "stairs", "name": "Open Stairs", "gx": 0, "gy": 0,
                "layers": [
                    {"id": "lower", "name": "Lower", "z": -9, "rows": rows()},
                    {"id": "upper", "name": "Upper", "z": -4, "rows": rows("_")},
                ],
                "objects": [{"id": "spawn", "type": "spawn", "spawnKind": "Player", "layerId": "lower", "x": 3, "y": 3},
                    {"id": "flight", "type": "stairs", "layerId": "lower",
                    "x": 10, "y": 10, "w": 1.2, "d": 8, "h": 99, "steps": 28,
                    "rotation": rotation, "targetLayerId": "upper", **properties}],
            }]}
            payload, warnings = server.build_map_payload(project, "stairs", 11, "Open Stairs", "MAIN")
            assert not warnings
            flight = re.search(r"stairs\.push_back\(\{([^}]+)\}\)", payload)
            assert flight, payload
            fields = flight.group(1).split(",")
            assert fields[4:7] == ["-9.0f", "-4.0f", "28"]  # target floor, not stale authored height
            assert fields[7] == str(rotation in (0, 180)).lower()
            assert fields[8] == str(rotation in (0, 270)).lower()
            assert fields[9] == str(expected_open).lower()
            assert float(fields[10].removesuffix("f")) == expected_thickness
            assert fields[11] == str(bool(properties.get("sideRails",False))).lower()

            campaign, warnings = server.build_runtime_campaign(project, "Open Stairs")
            assert not warnings
            flight = next(line for line in campaign.splitlines() if line.startswith("STAIR|"))
            fields = flight.split("|")
            assert [float(value) for value in fields[6:8]] == [-9, -4]
            assert int(fields[8]) == 28
            assert int(fields[9]) == int(rotation in (0, 180))
            assert int(fields[10]) == int(rotation in (0, 270))
            assert int(fields[11]) == int(expected_open)
            assert float(fields[12]) == expected_thickness
            assert int(fields[13]) == int(bool(properties.get("sideRails",False)))


def test_industrial_fixture_exports() -> None:
    models = (("extraction-fan.obj", 20), ("packing-crate.fbx", 21),
              ("control-console.fbx", 22), ("forklift.obj", 23))
    project = {"chunks": [{"id": "cargo", "name": "Cargo", "gx": 0, "gy": 0,
        "layers": [{"id": "floor", "z": 0, "rows": rows()}],
        "objects": [{"id": "spawn", "type": "spawn", "spawnKind": "Player", "layerId": "floor", "x": 3, "y": 3}]
            + [{"id": name, "type": "asset", "layerId": "floor",
            "model": "src/assets/facility/cargo/" + name, "x": 4 + i * 4,
            "y": 8, "w": 1, "d": 1, "h": 1} for i, (name, _) in enumerate(models)],
    }]}
    payload, warnings = server.build_map_payload(project, "cargo", 11, "Cargo", "MAIN")
    assert not warnings
    for _, fixture in models:
        assert f"m_fixtures.push_back({{{fixture}," in payload
    campaign, warnings = server.build_runtime_campaign(project, "Cargo")
    assert not warnings
    exported = [int(line.split("|")[2]) for line in campaign.splitlines() if line.startswith("FIXTURE|")]
    assert exported == [fixture for _, fixture in models]


if __name__ == "__main__":
    test_payload()
    test_open_stair_exports()
    test_industrial_fixture_exports()
    print("level-editor map payload export: PASS")
