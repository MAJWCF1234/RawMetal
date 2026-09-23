from __future__ import annotations

import server


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


if __name__ == "__main__":
    test_payload()
    print("level-editor map payload export: PASS")
