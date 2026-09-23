from __future__ import annotations

import json
import mimetypes
import math
import re
import shutil
import threading
import time
import urllib.request
import webbrowser
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = ROOT / "src" / "assets"
EDITOR_ROOT = ROOT / "tools" / "level-editor"
VENDOR_ROOT = EDITOR_ROOT / "vendor"
PROJECT_ROOT = EDITOR_ROOT / "projects"
HOST = "127.0.0.1"
PORT = 8008

MODEL_EXTENSIONS = {".obj", ".fbx", ".glb", ".gltf"}
TEXTURE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".webp", ".bmp", ".tga"}
AUDIO_EXTENSIONS = {".wav", ".ogg", ".mp3", ".flac"}

VENDOR = {
    "three.module.js": "https://cdn.jsdelivr.net/npm/three@0.180.0/build/three.module.js",
    "three.core.js": "https://cdn.jsdelivr.net/npm/three@0.180.0/build/three.core.js",
    "controls/OrbitControls.js": "https://cdn.jsdelivr.net/npm/three@0.180.0/examples/jsm/controls/OrbitControls.js",
    "loaders/OBJLoader.js": "https://cdn.jsdelivr.net/npm/three@0.180.0/examples/jsm/loaders/OBJLoader.js",
    "loaders/FBXLoader.js": "https://cdn.jsdelivr.net/npm/three@0.180.0/examples/jsm/loaders/FBXLoader.js",
    "loaders/GLTFLoader.js": "https://cdn.jsdelivr.net/npm/three@0.180.0/examples/jsm/loaders/GLTFLoader.js",
    "curves/NURBSCurve.js": "https://cdn.jsdelivr.net/npm/three@0.180.0/examples/jsm/curves/NURBSCurve.js",
    "curves/NURBSUtils.js": "https://cdn.jsdelivr.net/npm/three@0.180.0/examples/jsm/curves/NURBSUtils.js",
    "utils/BufferGeometryUtils.js": "https://cdn.jsdelivr.net/npm/three@0.180.0/examples/jsm/utils/BufferGeometryUtils.js",
    "libs/fflate.module.js": "https://cdn.jsdelivr.net/npm/three@0.180.0/examples/jsm/libs/fflate.module.js",
}

TEXTURE_OVERRIDES = {
    "src/assets/models/PSX-Weapon-Pack/Remington-870/Remington-870.fbx": "src/assets/remington.png",
    "src/assets/models/huntsman.fbx": "src/assets/spider.png",
    "src/assets/models/wasp.fbx": "src/assets/wasp.png",
    "src/assets/models/scissors.fbx": "src/assets/scissors.png",
    "src/assets/models/arms_rig.fbx": "src/assets/arms/arms_gloves_01.png",
    "src/assets/environment/generator.fbx": "src/assets/facility/generator_1.png",
    "src/assets/clutter/bottle.obj": "src/assets/clutter/Bottles.png",
    "src/assets/reactor/control-panel.obj": "src/assets/reactor/electronics.png",
    "src/assets/reactor/stalker.obj": "src/assets/reactor/stalker.png",
    "src/assets/facility/source/wall_6.fbx": "src/assets/facility/wall_6.png",
    "src/assets/facility/source/wall_8.fbx": "src/assets/facility/wall_8.png",
    "src/assets/facility/source/column_6.fbx": "src/assets/facility/metal_6.png",
    "src/assets/facility/source/vent_fps_1.fbx": "src/assets/facility/vent_1.png",
    "src/assets/facility/source/ceiling_lamp_fps_1.fbx": "src/assets/facility/ceiling_lamp_1.png",
    "src/assets/facility/source/doorway_wide_1.fbx": "src/assets/facility/door_1.png",
    "src/assets/facility/source/metal_shelf_1.fbx": "src/assets/facility/metal_4.png",
    "src/assets/facility/source/wall_box_2.fbx": "src/assets/facility/wall_box_2.png",
    "src/assets/facility/source/computer_1.fbx": "src/assets/facility/pc_1.png",
}

FRIENDLY = {
    "Remington-870": "Remington 870",
    "huntsman": "Huntsman",
    "wasp": "Flying Creature",
    "scissors": "Scissor Fiend",
    "arms_rig": "Player Arms Rig",
    "first-aid": "First Aid Kit",
    "shells": "12 Gauge Shells",
    "trash_1": "Trash",
    "mre_1": "MRE",
    "bottle": "Bottle",
    "power_supply_1": "Power Supply",
    "pcb_2": "Circuit Board",
    "floppy_disc_2": "Floppy Disk",
    "control-panel": "Reactor Control Panel",
    "stalker": "Stalker",
    "wall_6": "Facility Wall 6",
    "wall_8": "Facility Wall 8",
    "column_6": "Facility Column",
    "vent_fps_1": "Vent",
    "ceiling_lamp_fps_1": "Ceiling Lamp",
    "doorway_wide_1": "Wide Doorway",
    "metal_shelf_1": "Metal Shelf",
    "wall_box_2": "Wall Cabinet",
    "computer_1": "Facility Computer",
}

# The editor catalog is intentionally curated. Crowbar and its matching atlas
# came from an unwanted third-party kit; keep the runtime copy available for
# old maps, but never expose it as a placeable editor asset or material.
EDITOR_EXCLUDED_STEMS = {"crowbar"}

DEFAULT_MOUNT = {
    # Obvious architectural defaults. These are only editor conveniences;
    # the artist can change Floor / Wall / Ceiling after placement.
    "ceiling_lamp_fps_1": "ceiling",
    "vent_fps_1": "ceiling",
    "wall_box_2": "wall",
}

DEFAULT_SIZE = {
    "pump": (1.10, 0.66, 1.35),
    "compressor": (1.00, 0.62, 1.12),
    "pipe": (4.20, 0.30, 0.30),
    "gate": (2.00, 0.25, 2.40),
    "barrel": (0.65, 0.65, 1.10),
    "crate": (0.90, 0.90, 0.85),
    "generator": (1.90, 1.10, 1.70),
    "metal_shelf_1": (2.05, 0.61, 1.44),
    "wall_box_2": (0.67, 0.20, 0.91),
    "computer_1": (0.70, 0.55, 1.00),
    "doorway_wide_1": (2.60, 0.30, 2.70),
    "vent_fps_1": (0.70, 0.15, 0.70),
    "ceiling_lamp_fps_1": (1.00, 0.30, 0.18),
    "column_6": (0.30, 0.30, 3.00),
    "first-aid": (0.65, 0.45, 0.40),
    "shells": (0.48, 0.36, 0.36),
    "trash_1": (0.55, 0.55, 0.35),
    "mre_1": (0.45, 0.30, 0.10),
    "bottle": (0.18, 0.18, 0.35),
    # Meter-based calibration: a power supply is a floor appliance, not a
    # palm-sized pickup. Keeping it around 0.6 m tall makes creature scale
    # comparisons in the 3D preview read correctly.
    "power_supply_1": (0.35, 0.45, 0.60),
    "pcb_2": (0.35, 0.25, 0.04),
    "floppy_disc_2": (0.28, 0.28, 0.03),
    "control-panel": (0.70, 0.45, 1.20),
    "stalker": (0.70, 0.70, 1.80),
    "huntsman": (1.30, 1.30, 0.75),
    "wasp": (1.20, 1.20, 1.00),
    "scissors": (1.10, 1.10, 1.80),
    "Remington-870": (1.10, 0.20, 0.20),
    "arms_rig": (1.20, 0.70, 1.20),
}

def ensure_vendor() -> None:
    VENDOR_ROOT.mkdir(parents=True, exist_ok=True)
    for relative, url in VENDOR.items():
        target = VENDOR_ROOT / relative
        if target.exists() and target.stat().st_size > 100:
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        try:
            print(f"[LevelEditor] caching {relative}")
            request = urllib.request.Request(url, headers={"User-Agent": "Depthworks-Level-Editor/1"})
            with urllib.request.urlopen(request, timeout=12) as response:
                target.write_bytes(response.read())
        except Exception as exc:
            print(f"[LevelEditor] 3D dependency unavailable ({relative}): {exc}")
            try:
                target.unlink(missing_ok=True)
            except OSError:
                pass

def pretty_name(path: Path) -> str:
    stem = path.stem
    if stem in FRIENDLY:
        return FRIENDLY[stem]
    return re.sub(r"\s+", " ", re.sub(r"[_-]+", " ", stem)).strip().title()

def category_for(path: Path) -> str:
    p = path.as_posix().lower()
    if "/pressureworks/" in p:
        return "Machinery"
    if "/facility/source/" in p:
        return "Architecture"
    if "/environment/" in p:
        return "Environment"
    if "/clutter/" in p:
        return "Clutter"
    if "/pickups/" in p:
        return "Pickups"
    if "/reactor/" in p:
        return "Reactor"
    if "/models/" in p:
        if any(x in p for x in ("huntsman", "wasp", "scissors")):
            return "Creatures"
        if "weapon" in p or "remington" in p:
            return "Weapons"
        return "Actors"
    return "Other"

def asset_manifest() -> dict:
    files = []
    if ASSET_ROOT.exists():
        for path in sorted(ASSET_ROOT.rglob("*")):
            if not path.is_file():
                continue
            if path.stem.lower() in EDITOR_EXCLUDED_STEMS:
                continue
            rel = path.relative_to(ROOT).as_posix()
            ext = path.suffix.lower()
            kind = "model" if ext in MODEL_EXTENSIONS else "texture" if ext in TEXTURE_EXTENSIONS else "audio" if ext in AUDIO_EXTENSIONS else "other"
            files.append({
                "name": path.name,
                "label": pretty_name(path),
                "path": rel,
                "url": "/" + rel,
                "kind": kind,
                "ext": ext,
                "bytes": path.stat().st_size,
            })

    by_path = {item["path"].lower(): item for item in files}
    textures = [item for item in files if item["kind"] == "texture"]
    prefabs = []
    for model in (item for item in files if item["kind"] == "model"):
        model_path = Path(model["path"])
        texture_path = TEXTURE_OVERRIDES.get(model["path"])
        if not texture_path:
            candidates = []
            stem = model_path.stem.lower()
            for texture in textures:
                tp = Path(texture["path"])
                if tp.stem.lower() == stem and tp.parent == model_path.parent:
                    candidates.append(texture["path"])
            if candidates:
                texture_path = candidates[0]
        texture = by_path.get(texture_path.lower()) if texture_path else None
        stem = model_path.stem
        size = DEFAULT_SIZE.get(stem, (1.0, 1.0, 1.0))
        if model_path.stem.lower() in EDITOR_EXCLUDED_STEMS:
            continue
        prefabs.append({
            "id": model["path"],
            "name": FRIENDLY.get(stem, model["label"]),
            "category": category_for(model_path),
            "model": model["path"],
            "modelUrl": model["url"],
            "texture": texture["path"] if texture else None,
            "textureUrl": texture["url"] if texture else None,
            "ext": model["ext"],
            "w": size[0], "d": size[1], "h": size[2],
            "mount": DEFAULT_MOUNT.get(stem, "floor"),
        })

    # The paint palette is intentionally conservative. Most PNGs beside a
    # model are UV skins, not building finishes. Exposing those as paint made
    # the artist choose things like medkit, creature, keyboard, and pump skins
    # as if they were wall materials.
    def paintable_material(texture: dict) -> tuple[bool, str]:
        path = texture["path"].lower()
        name = Path(path).name
        if "/materials/" in path:
            return True, "Construction"
        if path in {"src/assets/floor.png", "src/assets/wall.png", "src/assets/metal.png"}:
            return True, "Construction"
        if "/pressureworks/" in path and name in {"floor.png", "wall.png", "metal.png"}:
            return True, "Pressure Works"
        if "/facility/" in path and (
            name.startswith("wall_")
            or name.startswith("floor_")
            or name.startswith("ceiling_")
            or name.startswith("metal_")
            or name == "scifi_texture_1.png"
        ):
            return True, "Facility"
        if "/environment/" in path and name in {
            "panel-metal.png",
            "straight-hazard-stripes.png",
            "sign-rust.png",
        }:
            return True, "Markings"
        return False, ""

    materials = []
    for texture in textures:
        allowed, category = paintable_material(texture)
        if not allowed:
            continue
        materials.append({
            "id": texture["path"],
            "name": texture["label"],
            "category": category,
            "path": texture["path"],
            "url": texture["url"],
        })

    return {
        "root": "src/assets",
        "count": len(files),
        "prefabs": prefabs,
        "materials": materials,
        "files": files,
        "threeReady": (VENDOR_ROOT / "three.module.js").exists(),
    }


FACILITY_MODEL_INDEX = {
    "src/assets/facility/source/wall_6.fbx": 0,
    "src/assets/facility/source/wall_8.fbx": 1,
    "src/assets/facility/source/column_6.fbx": 2,
    "src/assets/facility/source/vent_fps_1.fbx": 3,
    "src/assets/facility/source/ceiling_lamp_fps_1.fbx": 4,
    "src/assets/facility/source/doorway_wide_1.fbx": 5,
    "src/assets/environment/generator.fbx": 6,
    "src/assets/facility/source/metal_shelf_1.fbx": 7,
    "src/assets/facility/source/wall_box_2.fbx": 8,
    "src/assets/facility/source/computer_1.fbx": 11,
    "src/assets/facility/service/machinery_mx_1.fbx": 12,
    "src/assets/facility/service/electrical_equipment_1.fbx": 13,
    "src/assets/facility/service/tank_system_mx_1.fbx": 14,
}
WORLD_PROP_KIND = {
    "src/assets/pressureworks/pump.fbx": 0,
    "src/assets/pressureworks/compressor.fbx": 1,
    "src/assets/pressureworks/pipe.fbx": 2,
    "src/assets/pressureworks/gate.fbx": 3,
}
CLUTTER_KIND = {
    "src/assets/clutter/trash_1.obj": 0,
    "src/assets/clutter/mre_1.obj": 1,
    "src/assets/clutter/bottle.obj": 2,
    "src/assets/clutter/power_supply_1.obj": 3,
    "src/assets/clutter/pcb_2.obj": 4,
    "src/assets/clutter/floppy_disc_2.obj": 5,
}
PICKUP_KIND = {
    "src/assets/pickups/first-aid.fbx": "Health",
    "src/assets/pickups/shells.fbx": "Ammo",
}
CREATURE_MODEL_KIND = {
    "src/assets/models/huntsman.fbx": "Huntsman",
    "src/assets/models/wasp.fbx": "Wasp",
    "src/assets/models/scissors.fbx": "Brute",
    "src/assets/reactor/stalker.obj": "Warden",
}
HAZARD_KINDS = {"Electricity","Steam","Crusher","Toxic","Fire","FallingDebris","Pressure","Anomaly"}

def _number(value, default=0.0) -> float:
    try:
        result = float(value)
        return result if math.isfinite(result) else float(default)
    except (TypeError, ValueError):
        return float(default)

def _cpp_float(value) -> str:
    n = _number(value)
    if abs(n) < 0.0005:
        n = 0.0
    text = f"{n:.3f}".rstrip("0").rstrip(".")
    if "." not in text:
        text += ".0"
    return text + "f"

def _cpp_string(value) -> str:
    return json.dumps(str(value or ""), ensure_ascii=False)

def _cpp_id(value) -> str:
    name = re.sub(r"[^A-Za-z0-9_]+", "_", str(value or "Layer")).strip("_")
    if not name:
        name = "Layer"
    if name[0].isdigit():
        name = "_" + name
    return name

def _normalize_model_path(value) -> str:
    return str(value or "").replace("\\", "/").lower()

def _resolve_editor_object(chunk: dict, obj: dict, by_id: dict, seen=None) -> dict:
    seen = set() if seen is None else seen
    oid = obj.get("id")
    if not oid or oid in seen or not obj.get("parentId"):
        return {
            "x": _number(obj.get("x")),
            "y": _number(obj.get("y")),
            "z": _number(obj.get("z")),
            "rotation": _number(obj.get("rotation")),
        }
    seen.add(oid)
    parent = by_id.get(obj.get("parentId"))
    if not parent:
        return {
            "x": _number(obj.get("x")),
            "y": _number(obj.get("y")),
            "z": _number(obj.get("z")),
            "rotation": _number(obj.get("rotation")),
        }
    pt = _resolve_editor_object(chunk, parent, by_id, seen)
    angle = math.radians(pt["rotation"])
    lx, ly = _number(obj.get("localX")), _number(obj.get("localY"))
    rx = lx * math.cos(angle) - ly * math.sin(angle)
    ry = lx * math.sin(angle) + ly * math.cos(angle)
    base = pt["z"]
    if obj.get("surface") == "top":
        base += _number(parent.get("h"))
    elif obj.get("surface") == "custom":
        base += _number(obj.get("surfaceHeight"))
    base += _number(obj.get("heightOffset"))
    return {
        "x": pt["x"] + rx,
        "y": pt["y"] + ry,
        "z": base,
        "rotation": pt["rotation"] + _number(obj.get("rotation")),
    }

def _opening_cells(obj: dict) -> list[dict]:
    axis = str(obj.get("wallAxis") or "horizontal")
    width = max(.25, min(12.0, _number(obj.get("w"), 1.0)))
    x, y = _number(obj.get("x")), _number(obj.get("y"))
    along = x if axis == "horizontal" else y
    fixed = y if axis == "horizontal" else x
    start, end = along - width / 2.0, along + width / 2.0
    first = math.floor(start + 1e-7)
    last = math.ceil(end - 1e-7) - 1
    out = []
    for a in range(first, last + 1):
        ix = a if axis == "horizontal" else math.floor(fixed)
        iy = math.floor(fixed) if axis == "horizontal" else a
        if ix < 0 or iy < 0 or ix >= 24 or iy >= 24:
            continue
        out.append({
            "x": ix, "y": iy, "axis": axis,
            "start": max(0.0, start - a), "end": min(1.0, end - a),
        })
    return out

def build_map_payload(project: dict, chunk_id: str, level_id: int, level_name: str, default_target: str) -> tuple[str,list[str]]:
    if not isinstance(project, dict):
        raise ValueError("Invalid Depthworks project")
    chunks = project.get("chunks")
    if not isinstance(chunks, list) or not chunks:
        raise ValueError("Project has no plan areas")
    chunk = next((c for c in chunks if str(c.get("id")) == str(chunk_id)), None)
    if chunk is None:
        raise ValueError("Selected plan area no longer exists")
    if level_id < 0:
        raise ValueError("Level ID must be zero or greater")
    default_target = str(default_target or "MAIN").upper()
    if default_target not in {"MAIN","CUSTOM"}:
        raise ValueError("Default target must be MAIN or CUSTOM")
    if default_target == "MAIN" and level_id < 6:
        raise ValueError("Main campaign payloads use dynamic level slots 6 and above")

    layers = list(chunk.get("layers") or [])
    if not layers:
        raise ValueError("Selected plan area has no floors")
    layers.sort(key=lambda layer: _number(layer.get("z")))
    base_layer = layers[0]
    base_z = _number(base_layer.get("z"))
    by_layer = {str(layer.get("id")): layer for layer in layers}
    objects = list(chunk.get("objects") or [])
    by_id = {str(o.get("id")): o for o in objects if o.get("id")}
    warnings: list[str] = []

    # Copy the 24x24 sheets, then cut smart-door/window openings. Upper editor
    # floors become '=' decks because RawMetal's additional MapLayer sheets are
    # structural decks, while their '#' cells are emitted as explicit walls.
    exported_rows: dict[str,list[list[str]]] = {}
    opening_by_cell: dict[tuple[str,int,int],tuple[dict,dict]] = {}
    for index, layer in enumerate(layers):
        src = layer.get("rows") or []
        grid = []
        for y in range(24):
            raw = str(src[y]) if y < len(src) else ""
            row = list((raw + "_" * 24)[:24])
            if index > 0:
                row = ["_" if ch == "_" else "=" for ch in row]
            grid.append(row)
        exported_rows[str(layer.get("id"))] = grid

    for obj in objects:
        if obj.get("type") not in {"door","window"}:
            continue
        layer = by_layer.get(str(obj.get("layerId")))
        if not layer:
            continue
        lid = str(layer.get("id"))
        source_rows = layer.get("rows") or []
        for cell in _opening_cells(obj):
            x, y = cell["x"], cell["y"]
            raw = str(source_rows[y]) if y < len(source_rows) else ""
            if x >= len(raw) or raw[x] != "#":
                continue
            exported_rows[lid][y][x] = "." if layer is base_layer else "="
            opening_by_cell[(lid, x, y)] = (obj, cell)

    ident_base = _cpp_id(level_name or chunk.get("name") or project.get("name") or "Map")
    layer_names = {}
    lines = [
        f"META_LEVEL_ID: {level_id}",
        f"META_LEVEL_NAME: {level_name}",
        f"META_DEFAULT_TARGET: {default_target}",
        f"META_EDITOR_PROJECT: {project.get('name','Depthworks Level')}",
        f"META_EDITOR_PLAN_AREA: {chunk.get('name','Plan Area')}",
        "",
        "--- MAP_CODE_START ---",
        f"  if(m_level=={level_id}){{",
        f"   // Generated by Depthworks Level Editor from {chunk.get('name','Plan Area')}.",
    ]

    player_spawns = [o for o in objects if o.get("type") == "spawn" and o.get("spawnKind") == "Player"]
    if player_spawns:
        p = _resolve_editor_object(chunk, player_spawns[0], by_id)
        lines.append(f"   // Player start marker: {{{_cpp_float(p['x'])},{_cpp_float(p['y'])},{_cpp_float(p['z'])}}}.")
        if len(player_spawns) > 1:
            warnings.append("More than one Player Start exists; only the first is recorded as a comment. Campaign spawn position still comes from WorldDefinition.h.")
    else:
        warnings.append("No Player Start marker is present. Campaign spawn position still comes from WorldDefinition.h.")

    for index, layer in enumerate(layers):
        lid = str(layer.get("id"))
        ident = f"{ident_base}_L{index}_{_cpp_id(layer.get('name') or 'Floor')}"
        layer_names[lid] = ident
        lines.append(f"   static constexpr MapRows {ident} = {{")
        for row in exported_rows[lid]:
            lines.append(f'    "{"".join(row)}",')
        lines.append("   };")

    lines.append("")
    lines.append("   m_layers={")
    for index, layer in enumerate(layers):
        lid = str(layer.get("id"))
        name = f"{level_name} / {layer.get('name') or ('Floor '+str(index+1))}"
        thickness = _number(layer.get("thickness"))
        if index > 0 and thickness <= 0:
            thickness = .25
        lines.append(f"    {{{_cpp_string(name)},{_cpp_float(layer.get('z'))},{_cpp_float(thickness)},{layer_names[lid]}}},")
    lines.append("   };")

    # Upper-floor walls do not come from tile(), so emit them as normal world
    # structures. Openings get side pieces and window sill/header pieces.
    for index, layer in enumerate(layers):
        lid, floor_z = str(layer.get("id")), _number(layer.get("z"))
        ceiling = floor_z + max(.5, _number(layer.get("ceilingHeight"), 3.0))
        source_rows = layer.get("rows") or []
        if index > 0:
            for y in range(24):
                raw = str(source_rows[y]) if y < len(source_rows) else ""
                for x in range(min(24, len(raw))):
                    if raw[x] != "#" or (lid, x, y) in opening_by_cell:
                        continue
                    lines.append(f"   wall({_cpp_float(x)},{_cpp_float(y)},{_cpp_float(x+1)},{_cpp_float(y+1)},{_cpp_float(floor_z)},{_cpp_float(ceiling)});")

        for (cell_lid, x, y), (opening, cell) in opening_by_cell.items():
            if cell_lid != lid:
                continue
            axis, a, b = cell["axis"], cell["start"], cell["end"]
            if axis == "horizontal":
                if a > .001:
                    lines.append(f"   wall({_cpp_float(x)},{_cpp_float(y)},{_cpp_float(x+a)},{_cpp_float(y+1)},{_cpp_float(floor_z)},{_cpp_float(ceiling)});")
                if b < .999:
                    lines.append(f"   wall({_cpp_float(x+b)},{_cpp_float(y)},{_cpp_float(x+1)},{_cpp_float(y+1)},{_cpp_float(floor_z)},{_cpp_float(ceiling)});")
            else:
                if a > .001:
                    lines.append(f"   wall({_cpp_float(x)},{_cpp_float(y)},{_cpp_float(x+1)},{_cpp_float(y+a)},{_cpp_float(floor_z)},{_cpp_float(ceiling)});")
                if b < .999:
                    lines.append(f"   wall({_cpp_float(x)},{_cpp_float(y+b)},{_cpp_float(x+1)},{_cpp_float(y+1)},{_cpp_float(floor_z)},{_cpp_float(ceiling)});")
            if opening.get("type") == "window":
                sill = max(0.0, _number(opening.get("sill"), .95))
                height = max(.2, _number(opening.get("h"), 1.15))
                header = min(ceiling, floor_z + sill + height)
                if sill > .001:
                    lines.append(f"   wall({_cpp_float(x)},{_cpp_float(y)},{_cpp_float(x+1)},{_cpp_float(y+1)},{_cpp_float(floor_z)},{_cpp_float(floor_z+sill)});")
                if header < ceiling - .001:
                    lines.append(f"   wall({_cpp_float(x)},{_cpp_float(y)},{_cpp_float(x+1)},{_cpp_float(y+1)},{_cpp_float(header)},{_cpp_float(ceiling)});")

    # Interactive horizontal smart doors. Vertical smart doors still carve a
    # valid passage, but Door currently has no orientation field.
    for obj in objects:
        if obj.get("type") != "door":
            continue
        layer = by_layer.get(str(obj.get("layerId")))
        if not layer:
            continue
        p = _resolve_editor_object(chunk, obj, by_id)
        axis = str(obj.get("wallAxis") or "horizontal")
        if axis != "horizontal":
            warnings.append(f"Vertical smart door '{obj.get('name','Door')}' was exported as an open passage because the runtime Door type is horizontal-only.")
            lines.append(f"   // Vertical smart door {_cpp_string(obj.get('name','Door'))}: opening preserved; runtime Door has no vertical orientation yet.")
            continue
        width = max(.25, min(12.0, _number(obj.get("w"), 1.0)))
        left, right = p["x"] - width / 2, p["x"] + width / 2
        entry = p["y"] <= 1.25
        transfer = p["y"] >= 22.75
        zoff = _number(layer.get("z")) - base_z
        lines.append(f"   m_doors.push_back({{{_cpp_float(left)},{_cpp_float(right)},{_cpp_float(p['y'])},0,false,{str(transfer).lower()},{str(entry).lower()},{_cpp_float(zoff)}}});")
        if not entry and not transfer:
            lines.append("   m_doors.back().swinging=true;")

    # Ordinary editor objects.
    creature_lines, pickup_lines, clutter_lines = [], [], []
    for obj in objects:
        kind = obj.get("type")
        if kind in {"door","window","dimension","label","stairs","light","terminal","hazard","box","spawn"}:
            continue
        if kind != "asset":
            continue
        layer = by_layer.get(str(obj.get("layerId"))) or base_layer
        p = _resolve_editor_object(chunk, obj, by_id)
        model = _normalize_model_path(obj.get("model"))
        yaw = math.radians(p["rotation"])
        w = max(.05, _number(obj.get("w"), 1.0) * max(.01, _number(obj.get("scale"), 1.0)))
        d = max(.05, _number(obj.get("d"), 1.0) * max(.01, _number(obj.get("scale"), 1.0)))
        h = max(.05, _number(obj.get("h"), 1.0) * max(.01, _number(obj.get("scale"), 1.0)))
        base = p["z"] - base_z
        if model in FACILITY_MODEL_INDEX:
            mi = FACILITY_MODEL_INDEX[model]
            solid = mi not in {3,4,5}
            lines.append(f"   m_fixtures.push_back({{{mi},{{{_cpp_float(p['x'])},{_cpp_float(p['y'])}}},{_cpp_float(base)},{_cpp_float(w)},{_cpp_float(d)},{_cpp_float(h)},{_cpp_float(yaw)},{str(solid).lower()}}});")
        elif model in WORLD_PROP_KIND:
            pk = WORLD_PROP_KIND[model]
            lines.append(f"   m_props.push_back({{{pk},{{{_cpp_float(p['x'])},{_cpp_float(p['y'])}}},{_cpp_float(h)},{_cpp_float(max(w,d))},{_cpp_float(yaw)},{{{_cpp_float(w/2)},{_cpp_float(d/2)}}},{_cpp_float(base)}}});")
        elif model in CLUTTER_KIND:
            ck = CLUTTER_KIND[model]
            z = -999.0 if abs(p["z"] - base_z) < .03 else p["z"]
            clutter_lines.append(f"{{{ck},{{{_cpp_float(p['x'])},{_cpp_float(p['y'])}}},{_cpp_float(z)},{_cpp_float(yaw)}}}")
        elif model in PICKUP_KIND:
            pk = PICKUP_KIND[model]
            pickup_lines.append(f"{{{{{_cpp_float(p['x'])},{_cpp_float(p['y'])}}},PickupKind::{pk}}}")
            if abs(p["z"] - base_z) > .25:
                warnings.append(f"Pickup '{obj.get('name','Pickup')}' is above the base floor; PickupSpawn has no authored Z and will settle on floorHeight().")
        elif model in CREATURE_MODEL_KIND:
            ck = CREATURE_MODEL_KIND[model]
            creature_lines.append(f"{{CreatureKind::{ck},{{{_cpp_float(p['x'])},{_cpp_float(p['y'])}}},{_cpp_float(p['z'])}}}")
        else:
            warnings.append(f"Asset '{obj.get('name') or model}' has no runtime placement mapping yet and was preserved only as an export comment.")
            lines.append(f"   // Unsupported editor asset: {_cpp_string(obj.get('name') or model)} ({model})")

    for obj in objects:
        kind = obj.get("type")
        layer = by_layer.get(str(obj.get("layerId"))) or base_layer
        p = _resolve_editor_object(chunk, obj, by_id)
        if kind == "box":
            rotation = int(round((_number(p["rotation"]) % 360) / 90.0)) % 4
            w, d = max(.05, _number(obj.get("w"), 1)), max(.05, _number(obj.get("d"), 1))
            if rotation % 2:
                w, d = d, w
            if abs((_number(p["rotation"]) % 90)) > .01:
                warnings.append(f"Block '{obj.get('name','Block')}' uses a non-90-degree rotation; collision exported as its axis-aligned footprint.")
            lines.append(f"   m_structures.push_back({{{_cpp_float(p['x']-w/2)},{_cpp_float(p['y']-d/2)},{_cpp_float(p['x']+w/2)},{_cpp_float(p['y']+d/2)},{_cpp_float(p['z'])},{_cpp_float(p['z']+max(.05,_number(obj.get('h'),1)))},false,2}});")
        elif kind == "stairs":
            rot = int(round((_number(p["rotation"]) % 360) / 90.0)) % 4
            if abs((_number(p["rotation"]) % 90)) > .01:
                warnings.append(f"Stairs '{obj.get('name','Stairs')}' were snapped to the nearest 90-degree runtime direction.")
            w, d = max(.4, _number(obj.get("w"),1.2)), max(1.0, _number(obj.get("d"),5))
            bottom = _number(layer.get("z"))
            target = by_layer.get(str(obj.get("targetLayerId")))
            top = _number(target.get("z")) if target else bottom + max(.25, _number(obj.get("h"),3))
            steps = max(3, min(64, int(round(_number(obj.get("steps"),17)))))
            if rot in {0,2}:
                x1,x2,y1,y2 = p["x"]-w/2,p["x"]+w/2,p["y"]-d/2,p["y"]+d/2
                along_y, ascending = True, rot == 0
            else:
                x1,x2,y1,y2 = p["x"]-d/2,p["x"]+d/2,p["y"]-w/2,p["y"]+w/2
                along_y, ascending = False, rot == 3
            lines.append(f"   stairs.push_back({{{_cpp_float(x1)},{_cpp_float(y1)},{_cpp_float(x2)},{_cpp_float(y2)},{_cpp_float(bottom)},{_cpp_float(top)},{steps},{str(along_y).lower()},{str(ascending).lower()}}});")
        elif kind == "light":
            lines.append(f"   m_lights.push_back({{{{{_cpp_float(p['x'])},{_cpp_float(p['y'])}}},{_cpp_float(p['z'])}}});")
        elif kind == "terminal":
            title = obj.get("title") or obj.get("name") or "TERMINAL"
            lines.append(f"   m_terminals.push_back({{{{{_cpp_float(p['x'])},{_cpp_float(p['y'])}}},{_cpp_string(title)},\"AUTHORED IN LEVEL EDITOR.\",\"LOCAL TERMINAL.\",{_cpp_float(p['z']-base_z)},false}});")
        elif kind == "hazard":
            hk = str(obj.get("kind") or "Electricity")
            if hk not in HAZARD_KINDS:
                warnings.append(f"Hazard '{obj.get('name','Hazard')}' uses unknown kind '{hk}'; exported as Electricity.")
                hk = "Electricity"
            w,d,h = max(.1,_number(obj.get("w"),2)),max(.1,_number(obj.get("d"),2)),max(.1,_number(obj.get("h"),1))
            lines.append(f"   m_hazards.push_back({{Hazard::Kind::{hk},{_cpp_float(p['x']-w/2)},{_cpp_float(p['y']-d/2)},{_cpp_float(p['x']+w/2)},{_cpp_float(p['y']+d/2)},{_cpp_float(p['z'])},{_cpp_float(p['z']+h)},20}});")
        elif kind == "spawn":
            role = str(obj.get("spawnKind") or "Worker")
            if role in {"Creature","Hostile"}:
                creature_lines.append(f"{{CreatureKind::Huntsman,{{{_cpp_float(p['x'])},{_cpp_float(p['y'])}}},{_cpp_float(p['z'])}}}")
                if role == "Hostile":
                    warnings.append("Hostile dummy exported as Huntsman; the editor does not yet choose a runtime hostile species.")
            elif role == "Player":
                pass
            else:
                warnings.append(f"{role} dummy '{obj.get('name',role)}' is blueprint-only because RawMetal has no runtime NPC spawn type for that role yet.")

    if creature_lines:
        lines.append("   m_creatureSpawns={"+",".join(creature_lines)+"};")
    if pickup_lines:
        lines.append("   m_pickupSpawns={"+",".join(pickup_lines)+"};")
    if clutter_lines:
        lines.append("   m_clutterSpawns={"+",".join(clutter_lines)+"};")

    # Keep the payload self-contained and explicit about editor-only omissions.
    if len(chunks) > 1:
        warnings.append(f"Project contains {len(chunks)} plan areas. This payload contains only '{chunk.get('name','Plan Area')}', because one campaign slot is one 24x24 World chunk.")
    if any((layer.get("materials") or {}) for layer in layers):
        warnings.append("Painted editor finishes are not encoded by the current World map payload API yet; geometry is exported, but per-tile material paint remains editor-only.")
    if any(o.get("type") == "window" for o in objects):
        warnings.append("Smart windows export as collision-correct wall apertures; the current runtime has no dedicated glass/window entity.")

    lines.append("  }")
    lines.append("--- MAP_CODE_END ---")
    if warnings:
        lines.extend(["", "--- EDITOR_EXPORT_WARNINGS ---"])
        lines.extend(f"- {warning}" for warning in warnings)
    lines.append("")
    return "\n".join(lines), warnings


def project_filename(value: str) -> str:
    stem = Path(str(value or "untitled")).stem
    stem = re.sub(r"[^A-Za-z0-9 _.-]+", "", stem).strip().replace(" ", "-")
    stem = stem[:80] or "untitled"
    return stem + ".json"

def project_index() -> list[dict]:
    PROJECT_ROOT.mkdir(parents=True, exist_ok=True)
    result = []
    for path in sorted(PROJECT_ROOT.glob("*.json"), key=lambda p: p.stat().st_mtime, reverse=True):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
            title = str(data.get("name") or path.stem) if isinstance(data, dict) else path.stem
            chunks = len(data.get("chunks", [])) if isinstance(data, dict) else 0
        except Exception:
            title, chunks = path.stem, 0
        result.append({"file": path.name, "name": title, "chunks": chunks, "modified": int(path.stat().st_mtime)})
    return result

class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def send_json(self, payload, status=200):
        data = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        parsed = urlparse(self.path)
        route = parsed.path
        if route == "/__depthworks_projects.json":
            self.send_json({"projects": project_index()})
            return
        if route == "/__depthworks_project.json":
            name = parse_qs(parsed.query).get("name", [""])[0]
            path = PROJECT_ROOT / project_filename(name)
            if not path.is_file():
                self.send_json({"error": "Project not found"}, 404)
                return
            try:
                self.send_json({"file": path.name, "project": json.loads(path.read_text(encoding="utf-8"))})
            except Exception as exc:
                self.send_json({"error": str(exc)}, 500)
            return
        if route.startswith('/__three/'):
            relative = route[len('/__three/'):]
            target = VENDOR_ROOT / relative
            if target.is_file() and target.resolve().is_relative_to(VENDOR_ROOT.resolve()):
                data = target.read_bytes()
                self.send_response(200)
                self.send_header('Content-Type', 'text/javascript; charset=utf-8')
                self.send_header('Cache-Control', 'public, max-age=31536000, immutable')
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)
                return
            self.send_error(404)
            return
        # The editor is exposed at /level-editor/ for convenience, while its
        # pinned modules live under tools/level-editor/vendor. Alias that
        # public path so import maps work in browsers that resolve relative
        # module URLs against the friendly route.
        if route.startswith('/level-editor/vendor/'):
            self.path = '/tools/level-editor/vendor/' + route[len('/level-editor/vendor/'):]
            return super().do_GET()
        if route == "/__depthworks_assets.json":
            payload = json.dumps(asset_manifest(), separators=(",", ":")).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return
        if route in {"/", "/level-editor", "/level-editor/"}:
            self.path = "/tools/level-editor/index.html"
        return super().do_GET()

    def do_POST(self):
        route = urlparse(self.path).path
        if route not in {"/__depthworks_save_project", "/__depthworks_map_payload"}:
            self.send_json({"error": "Not found"}, 404)
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0 or length > 16 * 1024 * 1024:
                raise ValueError("Invalid project payload size")
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            project = payload.get("project")
            if not isinstance(project, dict) or not isinstance(project.get("chunks"), list) or not project["chunks"]:
                raise ValueError("Invalid Depthworks project")
            if route == "/__depthworks_map_payload":
                level = int(payload.get("level"))
                name = str(payload.get("name") or project.get("name") or "Depthworks Map").strip()
                if not name:
                    raise ValueError("Map name is required")
                target = str(payload.get("target") or "MAIN").upper()
                text_payload, warnings = build_map_payload(
                    project,
                    str(payload.get("chunk") or ""),
                    level,
                    name,
                    target,
                )
                filename = f"Map_{level:02d}_{re.sub(r'[^A-Za-z0-9_-]+', '_', name).strip('_') or 'Map'}.txt"
                self.send_json({"ok": True, "file": filename, "payload": text_payload, "warnings": warnings})
                return

            filename = project_filename(payload.get("file") or project.get("name") or "untitled")
            PROJECT_ROOT.mkdir(parents=True, exist_ok=True)
            path = PROJECT_ROOT / filename
            temp = path.with_suffix(path.suffix + ".tmp")
            backup = path.with_suffix(path.suffix + ".bak")
            temp.write_text(json.dumps(project, indent=2) + "\n", encoding="utf-8")
            if path.exists():
                shutil.copy2(path, backup)
            temp.replace(path)
            self.send_json({"ok": True, "file": path.name})
        except Exception as exc:
            self.send_json({"error": str(exc)}, 400)

    def log_message(self, fmt, *args):
        print("[LevelEditor] " + fmt % args)

def open_browser():
    time.sleep(0.5)
    webbrowser.open(f"http://{HOST}:{PORT}/level-editor/")

def main() -> int:
    mimetypes.add_type("model/gltf-binary", ".glb")
    mimetypes.add_type("model/gltf+json", ".gltf")
    ensure_vendor()
    PROJECT_ROOT.mkdir(parents=True, exist_ok=True)
    threading.Thread(target=open_browser, daemon=True).start()
    try:
        with ThreadingHTTPServer((HOST, PORT), Handler) as server:
            print(f"Depthworks Level Editor: http://{HOST}:{PORT}/level-editor/")
            print("Assets are read directly from src/assets. Press Ctrl+C to stop.")
            server.serve_forever()
    except KeyboardInterrupt:
        print("\nLevel editor stopped.")
        return 0
    except OSError as exc:
        print(f"Could not start level editor server on port {PORT}: {exc}")
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
