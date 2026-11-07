from __future__ import annotations

import argparse
import json
import mimetypes
import math
import re
import shutil
import sys
import threading
import time
import urllib.request
import webbrowser
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, quote, urlparse

from runtime_records import ALLOWED_TAGS, runtime_records

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
    "src/assets/facility/cargo/chair-wooden.fbx": "src/assets/facility/cargo/pallet.png",
    "src/assets/facility/cargo/table-wooden.fbx": "src/assets/facility/cargo/pallet.png",
    "src/assets/facility/cargo/extraction-fan.obj": "src/assets/facility/cargo/extraction-fan.png",
    "src/assets/facility/cargo/packing-crate.fbx": "src/assets/facility/cargo/packing-crate.png",
    "src/assets/facility/cargo/control-console.fbx": "src/assets/facility/cargo/control-console.png",
    "src/assets/facility/cargo/service-van-assembled.obj": "src/assets/facility/cargo/service-van.png",
    "src/assets/facility/service/electrical_equipment_1.fbx": "src/assets/facility/service/transformer_box_hr_2.png",
    "src/assets/facility/service/tank_system_mx_1.fbx": "src/assets/facility/service/metal_hr_6_1.png",
    "src/assets/mutant/MonsterPSX.fbx": "src/assets/mutant/MonsterPSX_Diffuse.png",
    "src/assets/actors/freight-worker.obj": "src/assets/actors/worker-body.png",
    "src/assets/actors/source/freight-worker.fbx": "src/assets/actors/worker-body.png",
}

FRIENDLY = {
    "Remington-870": "Remington 870",
    "huntsman": "Huntsman",
    "wasp": "Flying Creature",
    "scissors": "Scissor Fiend",
    "arms_rig": "Player Arms Rig",
    "first-aid": "First Aid Kit",
    "shells": "12 Gauge Shells",
    "9mm": "9mm Rounds",
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
    "chair-wooden": "Wooden Chair",
    "table-wooden": "Wooden Table",
    "extraction-fan": "Caged Extraction Fan",
    "packing-crate": "Painted Shipping Crate",
    "control-console": "Industrial Control Console",
    "forklift": "Warehouse Forklift",
    "pallet": "Wooden Pallet",
    "wooden-crate": "Wooden Storage Crate",
    "service-van": "Service Van",
    "service-van-assembled": "Service Van (Parked)",
    "electrical_equipment_1": "Electrical Transformer Box",
    "machinery_mx_1": "Industrial Generator Machinery",
    "tank_system_mx_1": "Pressurized Tank System",
    "MonsterPSX": "Mutant (Infected)",
    "freight-worker": "Freight Worker",
    "barrel": "Explosive Barrel",
    "crate": "Breakable Wooden Crate",
    "broken-bottle": "Broken Glass Bottle",
    "crushed-can": "Crushed Metal Can",
    "garbage-bag": "Garbage Bag",
    "newspapers": "Discarded Newspapers",
    "bent-metal": "Bent Scrap Metal",
}

# The editor catalog is intentionally curated. Crowbar and its matching atlas
# came from an unwanted third-party kit; keep the runtime copy available for
# old maps, but never expose it as a placeable editor asset or material.
EDITOR_EXCLUDED_STEMS = {"crowbar"}

DEFAULT_MOUNT = {
    # Obvious architectural defaults. These are only editor conveniences;
    # the artist can change Floor / Wall / Ceiling after placement.
    "ceiling_lamp_fps_1": "ceiling",
    "extraction-fan": "ceiling",
    "vent_fps_1": "ceiling",
    "wall_box_2": "wall",
    "electrical_equipment_1": "wall",
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
    "extraction-fan": (1.40, 1.40, 0.234),
    "packing-crate": (1.681, 1.680, 1.20),
    "control-console": (0.718, 0.453, 1.20),
    "forklift": (1.164, 3.620, 2.20),
    "pallet": (1.20, 0.80, 0.15),
    "chair-wooden": (0.417, 0.525, 0.95),
    "table-wooden": (1.402, 0.742, 0.80),
    "wooden-crate": (0.90, 0.90, 0.85),
    "service-van": (4.50, 2.00, 2.10),
    "service-van-assembled": (4.50, 2.00, 2.10),
    "electrical_equipment_1": (0.91, 0.50, 2.20),
    "machinery_mx_1": (2.45, 1.30, 1.40),
    "tank_system_mx_1": (2.40, 2.65, 2.20),
    "doorway_wide_1": (2.60, 0.30, 2.70),
    "vent_fps_1": (0.70, 0.15, 0.70),
    "ceiling_lamp_fps_1": (1.00, 0.30, 0.18),
    "column_6": (0.30, 0.30, 3.00),
    "first-aid": (0.65, 0.45, 0.40),
    "shells": (0.48, 0.36, 0.36),
    "9mm": (0.34, 0.14, 0.08),
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
    "MonsterPSX": (1.20, 1.20, 1.85),
    "freight-worker": (0.70, 0.70, 1.80),
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
    if "/facility/cargo/" in p:
        return "Cargo"
    if "/facility/service/" in p:
        return "Machinery"
    if "/facility/source/" in p:
        return "Architecture"
    if "/pressureworks/" in p:
        return "Machinery"
    if "/environment/" in p:
        return "Environment"
    if "/clutter/" in p:
        return "Clutter"
    if "/pickups/" in p:
        return "Pickups"
    if "/reactor/" in p:
        return "Reactor"
    if "/mutant/" in p:
        return "Creatures"
    if "/actors/" in p:
        return "Actors"
    if "/models/" in p:
        if any(x in p for x in ("huntsman", "wasp", "scissors")):
            return "Creatures"
        if "weapon" in p or "remington" in p:
            return "Weapons"
        return "Actors"
    return "Other"

BLOB_PATH = EDITOR_ROOT / "editor-assets.rmblob"
BLOB_DISABLED = False  # build_asset_blob.py sets this to read the loose tree
_blob_cache: dict | None = None


def blob_catalog() -> dict | None:
    """Catalog from editor-assets.rmblob (see build_asset_blob.py), if present."""
    global _blob_cache
    if BLOB_DISABLED or not BLOB_PATH.exists():
        return None
    if _blob_cache is None:
        import zipfile
        archive = zipfile.ZipFile(BLOB_PATH)
        catalog = json.loads(archive.read("manifest.json"))
        _blob_cache = {"archive": archive, "catalog": catalog}
    return _blob_cache["catalog"]


def blob_entry(name: str) -> tuple[bytes, str] | None:
    if blob_catalog() is None:
        return None
    import mimetypes
    try:
        data = _blob_cache["archive"].read(name)
    except KeyError:
        return None
    return data, mimetypes.guess_type(name)[0] or "application/octet-stream"


def asset_manifest() -> dict:
    catalog = blob_catalog()
    if catalog is not None:
        result = dict(catalog)
        result["threeReady"] = (VENDOR_ROOT / "three.module.js").exists()
        return result
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
        if "/facility/cargo/" in path and name in {"office-carpet.png", "van-metal.png", "pallet.png"}:
            return True, "Cargo & Finishes"
        if "/facility/service/" in path and name in {"metal_hr_6_1.png", "transformer_box_hr_2.png"}:
            return True, "Service & Metals"
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
    "src/assets/facility/source/computer_1_002.fbx": 11,
    "src/assets/facility/service/machinery_mx_1.fbx": 12,
    "src/assets/facility/source/machinery_2.fbx": 12,
    "src/assets/facility/service/electrical_equipment_1.fbx": 13,
    "src/assets/facility/service/tank_system_mx_1.fbx": 14,
    "src/assets/facility/cargo/pallet.fbx": 15,
    "src/assets/facility/cargo/service-van-assembled.obj": 16,
    "src/assets/facility/cargo/service-van.fbx": 16,
    "src/assets/facility/cargo/service-van.obj": 16,
    "src/assets/facility/cargo/wooden-crate.obj": 17,
    "src/assets/facility/cargo/chair-wooden.fbx": 18,
    "src/assets/facility/cargo/table-wooden.fbx": 19,
    "src/assets/facility/cargo/extraction-fan.obj": 20,
    "src/assets/facility/cargo/packing-crate.fbx": 21,
    "src/assets/facility/cargo/control-console.fbx": 22,
    "src/assets/reactor/control-panel.obj": 22,
    "src/assets/facility/cargo/forklift.obj": 23,
    "src/assets/office/desk.obj": 24,
    "src/assets/office/desk-corner.obj": 25,
    "src/assets/office/chair.obj": 26,
    "src/assets/office/crt-monitor.obj": 27,
    "src/assets/office/keyboard.obj": 28,
    "src/assets/office/pc-tower.obj": 29,
    "src/assets/office/cctv-tv.obj": 30,
    "src/assets/office/file-cabinet-wide.obj": 31,
    "src/assets/office/file-cabinet-tall.obj": 32,
    "src/assets/office/desk-phone.obj": 33,
    "src/assets/office/mug.obj": 34,
    "src/assets/office/water-cooler.obj": 35,
    "src/assets/office/coffee-machine.obj": 36,
    "src/assets/office/bookcase.obj": 37,
    "src/assets/office/binder.obj": 38,
    "src/assets/facility/industrial/converter.obj": 39,
    "src/assets/facility/industrial/flue.obj": 40,
    "src/assets/facility/industrial/stock-cage.obj": 41,
    "src/assets/facility/industrial/pendant-lamp.obj": 42,
    "src/assets/facility/industrial/ibeam.obj": 43,
    "src/assets/facility/industrial/pipe-riser.obj": 44,
    "src/assets/facility/industrial/valve-wheel.obj": 45,
    "src/assets/facility/industrial/turbine-casing.obj": 46,
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
    "src/assets/clutter/crushed-can.obj": 0,
    "src/assets/clutter/garbage-bag.obj": 0,
    "src/assets/clutter/newspapers.obj": 0,
    "src/assets/clutter/bent-metal.obj": 0,
    "src/assets/clutter/broken-bottle.obj": 2,
}
PICKUP_KIND = {
    "src/assets/pickups/first-aid.fbx": "Health",
    "src/assets/pickups/shells.fbx": "Ammo",
    "src/assets/pickups/9mm.fbx": "HandgunAmmo",
}
# The engine's PickupKind values (WorldDefinition.h); they are saved, so append only.
PICKUP_KIND_INDEX = {"Health": 0, "Ammo": 1, "HandgunAmmo": 2}
CREATURE_MODEL_KIND = {
    "src/assets/models/huntsman.fbx": "Huntsman",
    "src/assets/models/wasp.fbx": "Wasp",
    "src/assets/models/scissors.fbx": "Brute",
    "src/assets/reactor/stalker.obj": "Warden",
    "src/assets/mutant/monsterpsx.fbx": "Mutant",
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
        f"META_CAMPAIGN_NAME: {project.get('name') or level_name}",
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
        elif model in {"src/assets/environment/barrel.fbx", "barrel.fbx"}:
            ix, iy = int(math.floor(p["x"])), int(math.floor(p["y"]))
            lid = str(layer.get("id"))
            if 0 <= ix < 24 and 0 <= iy < 24 and lid in exported_rows:
                exported_rows[lid][iy][ix] = "B"
                lines.append(f"   // Placed explosive barrel at ({ix}, {iy}).")
            else:
                warnings.append(f"Explosive barrel at ({p['x']}, {p['y']}) is outside grid bounds.")
        elif model in {"src/assets/environment/crate.fbx", "crate.fbx"}:
            ix, iy = int(math.floor(p["x"])), int(math.floor(p["y"]))
            lid = str(layer.get("id"))
            if 0 <= ix < 24 and 0 <= iy < 24 and lid in exported_rows:
                exported_rows[lid][iy][ix] = "C"
                lines.append(f"   // Placed breakable crate at ({ix}, {iy}).")
            else:
                warnings.append(f"Breakable crate at ({p['x']}, {p['y']}) is outside grid bounds.")
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
            open_under=bool(obj.get("openUnderside",False));tread=max(.025,min(.5,_number(obj.get("treadThickness"),.12)))
            side_rails=bool(obj.get("sideRails",False))
            lines.append(f"   stairs.push_back({{{_cpp_float(x1)},{_cpp_float(y1)},{_cpp_float(x2)},{_cpp_float(y2)},{_cpp_float(bottom)},{_cpp_float(top)},{steps},{str(along_y).lower()},{str(ascending).lower()},{str(open_under).lower()},{_cpp_float(tread)},{str(side_rails).lower()}}});")
        elif kind == "light":
            lines.append(f"   m_lights.push_back({{{{{_cpp_float(p['x'])},{_cpp_float(p['y'])}}},{_cpp_float(p['z'])}}});")
        elif kind == "terminal":
            title = obj.get("title") or obj.get("name") or "TERMINAL"
            yaw = math.radians(p["rotation"]) if obj.get("rotation") is not None else math.pi
            lines.append(f"   m_terminals.push_back({{{{{_cpp_float(p['x'])},{_cpp_float(p['y'])}}},{_cpp_string(title)},\"AUTHORED IN LEVEL EDITOR.\",\"LOCAL TERMINAL.\",{_cpp_float(p['z']-base_z)},false,0,0,false,{_cpp_float(yaw)}}});")
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
    if len(chunks) > 1 and default_target == "MAIN":
        warnings.append(f"Project contains {len(chunks)} plan areas. The MAIN campaign code slot contains only '{chunk.get('name','Plan Area')}'. The runtime custom-campaign block still contains the complete project.")
    if any((layer.get("materials") or {}) for layer in layers):
        warnings.append("Painted editor finishes are not encoded by the current World map payload API yet; geometry is exported, but per-tile material paint remains editor-only.")
    if any(o.get("type") == "window" for o in objects):
        warnings.append("Smart windows export as collision-correct wall apertures; the current runtime has no dedicated glass/window entity.")

    lines.append("  }")
    lines.append("--- MAP_CODE_END ---")
    runtime_text,runtime_warnings=build_runtime_campaign(project,project.get("name") or level_name)
    warnings.extend(runtime_warnings)
    lines.extend(["","--- CUSTOM_CAMPAIGN_DATA_START ---"])
    lines.extend(runtime_text.splitlines())
    lines.append("--- CUSTOM_CAMPAIGN_DATA_END ---")
    if warnings:
        # Preserve order but avoid repeating the same limitation once for the
        # compile-time map block and again for the runtime campaign block.
        warnings=list(dict.fromkeys(warnings))
        lines.extend(["", "--- EDITOR_EXPORT_WARNINGS ---"])
        lines.extend(f"- {warning}" for warning in warnings)
    lines.append("")
    return "\n".join(lines), warnings



def _runtime_string(value) -> str:
    return quote(str(value or ""), safe=" _-./")

def _runtime_rows(layer: dict, upper: bool) -> list[list[str]]:
    result = []
    source = layer.get("rows") or []
    for y in range(24):
        raw = str(source[y]) if y < len(source) else ""
        row = list((raw + "_" * 24)[:24])
        if upper:
            row = ["_" if ch == "_" else "=" for ch in row]
        result.append(row)
    return result

def build_runtime_campaign(project: dict, campaign_name: str) -> tuple[str,list[str]]:
    authored_records = runtime_records(project)
    chunks = list(project.get("chunks") or [])
    if not chunks:
        raise ValueError("Project has no plan areas")
    if len(chunks) > 32:
        raise ValueError("A runtime custom campaign currently supports at most 32 map chunks")
    chunks.sort(key=lambda c: (int(c.get("number") or 9999), int(c.get("gy") or 0), int(c.get("gx") or 0), str(c.get("name") or "")))
    runtime_index = {str(c.get("id")): i for i, c in enumerate(chunks)}
    grid = {(int(c.get("gx") or 0), int(c.get("gy") or 0)): i for i, c in enumerate(chunks)}
    warnings = []
    start_map = 0
    for i,c in enumerate(chunks):
        if any(o.get("type") == "spawn" and o.get("spawnKind") == "Player" for o in c.get("objects") or []):
            start_map = i
            break
    lines = [f"CAMPAIGN|{_runtime_string(campaign_name)}|{start_map}"]
    skybox = str(project.get("skybox") or "industrial_night")

    for map_index, chunk in enumerate(chunks):
        layers = list(chunk.get("layers") or [])
        if not layers:
            raise ValueError(f"{chunk.get('name','Plan Area')} has no floors")
        layers.sort(key=lambda l: float(l.get("z") or 0))
        base = layers[0]
        base_z = float(base.get("z") or 0)
        by_layer = {str(layer.get("id")): layer for layer in layers}
        objects = list(chunk.get("objects") or [])
        by_id = {str(o.get("id")): o for o in objects if o.get("id")}
        gx,gy = int(chunk.get("gx") or 0),int(chunk.get("gy") or 0)
        origin_x,origin_y = gx*24,gy*24
        player = next((o for o in objects if o.get("type") == "spawn" and o.get("spawnKind") == "Player"), None)
        if player:
            pt = _resolve_editor_object(chunk,player,by_id)
            spawn_x,spawn_y,spawn_z = pt["x"],pt["y"],pt["z"]
        else:
            spawn_x=spawn_y=3.5
            source=base.get("rows") or []
            found=False
            for y in range(min(24,len(source))):
                row=str(source[y])
                for x,ch in enumerate(row[:24]):
                    if ch not in {"#","_"}:
                        spawn_x,spawn_y=x+.5,y+.5;found=True;break
                if found:break
            spawn_z=base_z
        top = max(float(layer.get("z") or 0)+max(.5,float(layer.get("ceilingHeight") or 3)) for layer in layers)
        environment = 1 if skybox in {"ashfall","brutal_wasteland"} else 0
        north,south,west,east = (gx,gy-1) in grid,(gx,gy+1) in grid,(gx-1,gy) in grid,(gx+1,gy) in grid
        lines.append("MAP|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|0".format(
            map_index,_runtime_string(chunk.get("name") or f"Map {map_index}"),
            origin_x,origin_y,_cpp_float(spawn_x)[:-1],_cpp_float(spawn_y)[:-1],_cpp_float(spawn_z)[:-1],
            environment,_cpp_float(top)[:-1],"0.27",_runtime_string(skybox),
            int(north),int(south),int(west),int(east)))

        exported = {}
        openings = {}
        for li,layer in enumerate(layers):
            exported[str(layer.get("id"))] = _runtime_rows(layer,li>0)
        for obj in objects:
            if obj.get("type") not in {"door","window"}: continue
            layer=by_layer.get(str(obj.get("layerId")))
            if not layer: continue
            lid=str(layer.get("id"));src=layer.get("rows") or []
            for cell in _opening_cells(obj):
                x,y=cell["x"],cell["y"];raw=str(src[y]) if y<len(src) else ""
                if x>=len(raw) or raw[x]!="#":continue
                exported[lid][y][x]="." if layer is base else "="
                openings[(lid,x,y)]=(obj,cell)

        for li,layer in enumerate(layers):
            lid=str(layer.get("id"));thickness=float(layer.get("thickness") or 0)
            if li>0 and thickness<=0:thickness=.25
            lines.append(f"LAYER|{map_index}|{li}|{_runtime_string(layer.get('name') or ('Floor '+str(li+1)))}|{_cpp_float(layer.get('z'))[:-1]}|{_cpp_float(thickness)[:-1]}")
            for y,row in enumerate(exported[lid]):
                lines.append(f"ROW|{map_index}|{li}|{y}|{''.join(row)}")

        for li,layer in enumerate(layers):
            lid=str(layer.get("id"));floor_z=float(layer.get("z") or 0);ceiling=floor_z+max(.5,float(layer.get("ceilingHeight") or 3));src=layer.get("rows") or []
            if li>0:
                for y in range(24):
                    raw=str(src[y]) if y<len(src) else ""
                    for x,ch in enumerate(raw[:24]):
                        if ch=="#" and (lid,x,y) not in openings:
                            lines.append(f"STRUCT|{map_index}|{x}|{y}|{x+1}|{y+1}|{floor_z}|{ceiling}|0|3")
            for (ol,x,y),(opening,cell) in list(openings.items()):
                if ol!=lid:continue
                axis,a,b=cell["axis"],float(cell["start"]),float(cell["end"])
                pieces=[]
                if axis=="horizontal":
                    if a>.001:pieces.append((x,y,x+a,y+1))
                    if b<.999:pieces.append((x+b,y,x+1,y+1))
                else:
                    if a>.001:pieces.append((x,y,x+1,y+a))
                    if b<.999:pieces.append((x,y+b,x+1,y+1))
                for x1,y1,x2,y2 in pieces:lines.append(f"STRUCT|{map_index}|{x1}|{y1}|{x2}|{y2}|{floor_z}|{ceiling}|0|3")
                if opening.get("type")=="window":
                    sill=max(0,float(opening.get("sill") or .95));height=max(.2,float(opening.get("h") or 1.15));header=min(ceiling,floor_z+sill+height)
                    if sill>.001:lines.append(f"STRUCT|{map_index}|{x}|{y}|{x+1}|{y+1}|{floor_z}|{floor_z+sill}|0|3")
                    if header<ceiling-.001:lines.append(f"STRUCT|{map_index}|{x}|{y}|{x+1}|{y+1}|{header}|{ceiling}|0|3")
                    if axis=="horizontal":gx1,gx2,gy1,gy2=x+a,x+b,y+.47,y+.53
                    else:gx1,gx2,gy1,gy2=x+.47,x+.53,y+a,y+b
                    if min(gx1,gy1)>=.3 and max(gx2,gy2)<=23.7:
                        lines.append(f"BREAKABLE|{map_index}|{gx1}|{gy1}|{gx2}|{gy2}|{floor_z+sill}|{header}|17|0|{int(bool(opening.get('holds',True)))}||")
                    else:warnings.append("Window glazing crosses a chunk seam; inset the window at least 0.3 m.")

        actor_lines=[];actor_indices=[]
        for obj in objects:
            kind=obj.get("type");layer=by_layer.get(str(obj.get("layerId"))) or base;pt=_resolve_editor_object(chunk,obj,by_id)
            if kind=="asset" and obj.get("physical"):
                yaw=math.radians(float(pt["rotation"]));scale=max(.01,float(obj.get("scale") or 1));w=float(obj.get("w") or 1)*scale;d=float(obj.get("d") or 1)*scale;h=float(obj.get("h") or 1)*scale
                mi=int(obj.get("physicsModel",-1))
                if mi<0:raise ValueError("A loose model prop needs a valid model slot")
                lines.append(f"CLUTTER|{map_index}|7|{pt['x']}|{pt['y']}|{pt['z']}|{-yaw}|{mi}|{w}|{d}|{h}|{math.radians(float(obj.get('pitch') or 0))}|{math.radians(float(obj.get('roll') or 0))}|{_runtime_string(obj.get('breakState') or '')}")
                continue
            if kind=="box" and obj.get("breakMaterial"):
                w=float(obj.get("w") or 1);d=float(obj.get("d") or 1);h=float(obj.get("h") or 1);material=int(obj["breakMaterial"])
                lines.append(f"BREAKABLE|{map_index}|{pt['x']-w/2}|{pt['y']-d/2}|{pt['x']+w/2}|{pt['y']+d/2}|{pt['z']}|{pt['z']+h}|{material}|{float(obj.get('breakHealth') or 0)}|{int(bool(obj.get('holds',False)))}|{_runtime_string(obj.get('breakState') or '')}|{_runtime_string(obj.get('breakTrigger') or '')}")
                continue
            if kind=="door":
                axis=str(obj.get("wallAxis") or "horizontal")
                if axis!="horizontal":
                    warnings.append(f"{chunk.get('name','Map')}: vertical smart door '{obj.get('name','Door')}' is an open passage at runtime because Door is horizontal-only.")
                    continue
                width=max(.25,min(12,float(obj.get("w") or 1)));left,right=pt["x"]-width/2,pt["x"]+width/2
                entry=pt["y"]<=1.25;transfer=pt["y"]>=22.75;zoff=float(layer.get("z") or 0)-base_z;swing=not entry and not transfer
                lines.append(f"DOOR|{map_index}|{left}|{right}|{pt['y']}|{zoff}|{int(entry)}|{int(transfer)}|{int(swing)}|0|-1")
            elif kind=="box":
                rot=int(round((float(pt["rotation"])%360)/90))%4;w=max(.05,float(obj.get("w") or 1));d=max(.05,float(obj.get("d") or 1))
                if rot%2:w,d=d,w
                h=max(.05,float(obj.get("h") or 1))
                lines.append(f"STRUCT|{map_index}|{pt['x']-w/2}|{pt['y']-d/2}|{pt['x']+w/2}|{pt['y']+d/2}|{pt['z']}|{pt['z']+h}|0|2")
            elif kind=="stairs":
                rot=int(round((float(pt["rotation"])%360)/90))%4;w=max(.4,float(obj.get("w") or 1.2));d=max(1,float(obj.get("d") or 5));bottom=float(layer.get("z") or 0)
                target=by_layer.get(str(obj.get("targetLayerId")));topz=float(target.get("z")) if target else bottom+max(.25,float(obj.get("h") or 3));steps=max(3,min(64,int(round(float(obj.get("steps") or 17)))))
                if rot in {0,2}:x1,x2,y1,y2,along,asc=pt["x"]-w/2,pt["x"]+w/2,pt["y"]-d/2,pt["y"]+d/2,1,int(rot==0)
                else:x1,x2,y1,y2,along,asc=pt["x"]-d/2,pt["x"]+d/2,pt["y"]-w/2,pt["y"]+w/2,0,int(rot==3)
                open_under=int(bool(obj.get("openUnderside",False)));tread=max(.025,min(.5,float(obj.get("treadThickness",.12))))
                lines.append(f"STAIR|{map_index}|{x1}|{y1}|{x2}|{y2}|{bottom}|{topz}|{steps}|{along}|{asc}|{open_under}|{tread}|{int(bool(obj.get('sideRails',False)))}")
            elif kind=="light":
                line=f"LIGHT|{map_index}|{pt['x']}|{pt['y']}|{pt['z']}"
                gel=obj.get("gel") or obj.get("gelColor")
                if any(obj.get(k) not in (None,"") for k in ("lampMount","intensity","range","kelvin")) or gel:
                    mounts={"ceiling":0,"wall":1,"bare":2}
                    yaw=math.radians(float(pt["rotation"]))
                    tail=[str(mounts.get(str(obj.get("lampMount") or "ceiling"),0)),str(yaw),
                          str(float(obj.get("intensity") or 1)),str(float(obj.get("range") or 6)),
                          str(int(float(obj.get("kelvin") or 0)))]
                    if gel:
                        hexv=str(gel).lstrip("#");rgb=[int(hexv[i:i+2],16)/255 for i in (0,2,4)] if len(hexv)>=6 else [1,1,1]
                        tail+=[""]*12+[f"{c:.3f}" for c in rgb]  # fields 11..21 empty, gel at 22..24
                    line+="|"+"|".join(tail)
                lines.append(line)
            elif kind in {"sign","label"}:
                title=str(obj.get("title") or obj.get("text") or obj.get("name") or "SIGN")
                subtitle=str(obj.get("subtitle") or "")
                accent=str(obj.get("accent") or "#d8a03c").lstrip("#")
                try:accent_int=int(accent[:6],16)
                except ValueError:accent_int=0xd8a03c
                yaw=math.radians(float(pt["rotation"]))
                lines.append(f"SIGN|{map_index}|{pt['x']}|{pt['y']}|{pt['z']}|{float(obj.get('w') or 1.4)}|{float(obj.get('h') or .5)}|{yaw}|{_runtime_string(title)}|{_runtime_string(subtitle)}|{accent_int}")
            elif kind=="prim":
                facing=int(round((float(pt["rotation"])%360)/90))%4
                lines.append(f"PRIM|{map_index}|{_runtime_string(obj.get('primKind') or 'box')}|{pt['x']}|{pt['y']}|{pt['z']}|{float(obj.get('w') or 0)}|{float(obj.get('d') or 0)}|{float(obj.get('h') or 0)}|{facing}|{int(float(obj.get('material') or 40))}|1")
            elif kind=="terminal":
                title=obj.get("title") or obj.get("name") or "TERMINAL"
                yaw=math.radians(float(pt["rotation"])) if obj.get("rotation") is not None else math.pi
                lines.append(f"TERMINAL|{map_index}|{pt['x']}|{pt['y']}|{pt['z']-base_z}|0|{_runtime_string(title)}|{_runtime_string('AUTHORED IN LEVEL EDITOR.')}|{_runtime_string('LOCAL TERMINAL.')}||0||{yaw}")
            elif kind=="hazard":
                hazard_ids={"Electricity":0,"Steam":1,"Crusher":2,"Toxic":3,"Fire":4,"FallingDebris":5,"Pressure":6,"Anomaly":7}
                hk=hazard_ids.get(str(obj.get("kind") or "Electricity"),0);w=max(.1,float(obj.get("w") or 2));d=max(.1,float(obj.get("d") or 2));h=max(.1,float(obj.get("h") or 1))
                lines.append(f"HAZARD|{map_index}|{hk}|{pt['x']-w/2}|{pt['y']-d/2}|{pt['x']+w/2}|{pt['y']+d/2}|{pt['z']}|{pt['z']+h}|20")
            elif kind=="spawn":
                role=str(obj.get("spawnKind") or "Worker")
                if role in {"Creature","Hostile"}:
                    species={"Huntsman":0,"Wasp":1,"Brute":2,"Warden":3,"Mutant":4}
                    lines.append(f"CREATURE|{map_index}|{species.get(str(obj.get('species') or 'Huntsman'),0)}|{pt['x']}|{pt['y']}|{pt['z']}")
                elif role not in {"Player"}:
                    visuals={"Worker":0,"Cargo":3,"Cleanup":4,"Security":4,"Scientist":10,"SeniorScientist":11,"FemaleScientist":12,"FemaleDoctor":13,"LabTech":14,"NPC":15,"Investigator":15}
                    if role not in visuals:
                        warnings.append(f"{chunk.get('name','Map')}: {role} dummy '{obj.get('name',role)}' remains blueprint-only; there is no runtime NPC type for that role.")
                    else:
                        idx=len(actor_indices);actor_indices.append(idx)
                        yaw=math.radians(float(pt["rotation"]));scale=float(obj.get("personScale") or 1)
                        actor_lines.append(f"ACTOR|{map_index}|editor_clock|{visuals[role]}|{scale}|0|0|{idx}")
                        actor_lines.append(f"ACTOR_KEY|{map_index}|{idx}|0|{pt['x']}|{pt['y']}|{pt['z']}|{yaw}|0|0")
                        actor_lines.append(f"ACTOR_KEY|{map_index}|{idx}|60000|{pt['x']}|{pt['y']}|{pt['z']}|{yaw}|0|1")
                        behaviour=str(obj.get("behaviour") or "stand")
                        if behaviour=="civilian":
                            actor_lines.append(f"ACTOR_AI|{map_index}|{idx}|1||1.4|2|6")
                        elif behaviour=="guard":
                            actor_lines.append(f"ACTOR_AI|{map_index}|{idx}|3||1.4|2|6")
                            armed=str(obj.get("armed") or "none")
                            if armed in {"handgun","shotgun"}:
                                actor_lines.append(f"ACTOR_WEAPON|{map_index}|{idx}|{2 if armed=='handgun' else 1}|650|1000|9|18")
            elif kind=="asset":
                model=_normalize_model_path(obj.get("model"));yaw=math.radians(float(pt["rotation"]));scale=max(.01,float(obj.get("scale") or 1));w=max(.05,float(obj.get("w") or 1)*scale);d=max(.05,float(obj.get("d") or 1)*scale);h=max(.05,float(obj.get("h") or 1)*scale);baseoff=pt["z"]-base_z
                if model in FACILITY_MODEL_INDEX:
                    mi=FACILITY_MODEL_INDEX[model];solid=int(mi not in {3,4,5});lines.append(f"FIXTURE|{map_index}|{mi}|{pt['x']}|{pt['y']}|{baseoff}|{w}|{d}|{h}|{yaw}|{solid}")
                elif model in WORLD_PROP_KIND:
                    pk=WORLD_PROP_KIND[model];lines.append(f"PROP|{map_index}|{pk}|{pt['x']}|{pt['y']}|{h}|{max(w,d)}|{yaw}|{w/2}|{d/2}|{baseoff}")
                elif model in CLUTTER_KIND:
                    ck=CLUTTER_KIND[model];z=-999 if abs(pt["z"]-base_z)<.03 else pt["z"];lines.append(f"CLUTTER|{map_index}|{ck}|{pt['x']}|{pt['y']}|{z}|{yaw}")
                elif model in PICKUP_KIND:
                    pk=PICKUP_KIND_INDEX[PICKUP_KIND[model]];lines.append(f"PICKUP|{map_index}|{pk}|{pt['x']}|{pt['y']}")
                elif model in CREATURE_MODEL_KIND:
                    ids={"Huntsman":0,"Wasp":1,"Brute":2,"Warden":3,"Mutant":4};lines.append(f"CREATURE|{map_index}|{ids[CREATURE_MODEL_KIND[model]]}|{pt['x']}|{pt['y']}|{pt['z']}")
                elif model in {"src/assets/environment/barrel.fbx", "barrel.fbx"}:
                    ix, iy = int(math.floor(pt["x"])), int(math.floor(pt["y"]))
                    lid = str(layer.get("id"))
                    if 0 <= ix < 24 and 0 <= iy < 24 and lid in exported:
                        exported[lid][iy][ix] = "B"
                    else:
                        warnings.append(f"{chunk.get('name','Map')}: explosive barrel at ({pt['x']:.2f},{pt['y']:.2f}) is outside grid bounds.")
                elif model in {"src/assets/environment/crate.fbx", "crate.fbx"}:
                    ix, iy = int(math.floor(pt["x"])), int(math.floor(pt["y"]))
                    lid = str(layer.get("id"))
                    if 0 <= ix < 24 and 0 <= iy < 24 and lid in exported:
                        exported[lid][iy][ix] = "C"
                    else:
                        warnings.append(f"{chunk.get('name','Map')}: breakable crate at ({pt['x']:.2f},{pt['y']:.2f}) is outside grid bounds.")
                else:warnings.append(f"{chunk.get('name','Map')}: asset '{obj.get('name') or model}' has no RawMetal runtime placement mapping.")

        lines.extend(actor_lines)
        surface=[int(float(chunk.get(k) if chunk.get(k) not in (None,"") else -1)) for k in ("surfaceWall","surfaceFloor","surfaceCeiling")]
        if any(v!=-1 for v in surface):
            lines.append(f"SURFACE|{map_index}|{surface[0]}|{surface[1]}|{surface[2]}")
        if any((layer.get("materials") or {}) for layer in layers):
            warnings.append(f"{chunk.get('name','Map')}: painted per-tile finishes remain editor-only until the World runtime has a material override table.")


    lines.extend(authored_records)
    return "\n".join(lines),warnings


def build_campaign_file(project: dict, campaign_name: str) -> tuple[str, str, list[str]]:
    """The playable world file: the runtime campaign in its markers, named .rmc.

    This is what the game loads from "custom maps". The installer payload
    (build_map_payload) remains for legacy MAIN-campaign slots.
    """
    runtime_text, warnings = build_runtime_campaign(project, campaign_name)
    name = re.sub(r'[<>:"/\\|?*]+', "", str(campaign_name or "Depthworks Campaign")).strip() or "Depthworks Campaign"
    text = ("--- CUSTOM_CAMPAIGN_DATA_START ---\n" + runtime_text.rstrip() +
            "\n--- CUSTOM_CAMPAIGN_DATA_END ---\n")
    return name + ".rmc", text, warnings


def parse_campaign_project(text: str) -> dict:
    """Read a .rmc world file back into an editor project.

    Round-trips what build_runtime_campaign writes: maps, layers, rows,
    doors, stairs, lights, terminals, hazards, fixtures, props, pickups and
    creature spawns. Script records with no editor control are preserved in
    runtimeRecords so re-exporting does not silently drop them.
    """
    from urllib.parse import unquote
    start = text.find("--- CUSTOM_CAMPAIGN_DATA_START ---")
    end = text.find("--- CUSTOM_CAMPAIGN_DATA_END ---")
    if start >= 0 and end > start:
        text = text[start:end]
    maps: dict[int, dict] = {}
    campaign_name = "Imported Campaign"
    counter = [0]

    def oid() -> str:
        counter[0] += 1
        return f"imp{counter[0]}"

    def num(value, default=0.0):
        try:
            return float(value)
        except (TypeError, ValueError):
            return default

    fixture_by_index = {v: k for k, v in FACILITY_MODEL_INDEX.items()}
    prop_by_kind = {v: k for k, v in WORLD_PROP_KIND.items()}
    clutter_by_kind = {v: k for k, v in CLUTTER_KIND.items()}
    pickup_by_index = {}
    for path, kind_name in PICKUP_KIND.items():
        pickup_by_index[PICKUP_KIND_INDEX[kind_name]] = path
    hazard_names = {0: "Electricity", 1: "Steam", 2: "Crusher", 3: "Toxic",
                    4: "Fire", 5: "FallingDebris", 6: "Pressure", 7: "Anomaly"}
    creature_names = {0: "Huntsman", 1: "Wasp", 2: "Brute", 3: "Warden", 4: "Mutant"}
    pending_structs: list[tuple[int, list[str]]] = []
    pending_doors: list[tuple[int, list[str]]] = []

    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "|" not in line:
            continue
        f = [unquote(part) for part in line.split("|")]
        tag = f[0]
        try:
            if tag == "CAMPAIGN":
                campaign_name = f[1] if len(f) > 1 else "Imported Campaign"
            elif tag == "MAP" and len(f) >= 12:
                idx = int(num(f[1]))
                maps[idx] = {
                    "id": f"map{idx}", "number": idx + 1, "name": f[2] or f"Map {idx + 1}",
                    "gx": int(num(f[3])) // 24, "gy": int(num(f[4])) // 24,
                    "layers": [], "objects": [],
                    "_spawn": (num(f[5], 3.5), num(f[6], 3.5), num(f[7])),
                    "_skybox": f[11],
                }
            elif tag == "LAYER" and len(f) >= 6:
                chunk = maps.get(int(num(f[1])))
                if chunk is not None:
                    chunk["layers"].append({
                        "id": f"m{f[1]}l{f[2]}", "name": f[3] or f"Floor {int(num(f[2])) + 1}",
                        "z": num(f[4]), "ceilingHeight": 3.0, "thickness": num(f[5]),
                        "rows": ["_" * 24 for _ in range(24)],
                        "materials": {}, "materialScales": {}, "materialRotations": {},
                        "materialScaleYs": {}, "materialOffsetsX": {}, "materialOffsetsY": {},
                    })
            elif tag == "ROW" and len(f) >= 5:
                chunk = maps.get(int(num(f[1])))
                li, y = int(num(f[2])), int(num(f[3]))
                if chunk is not None and 0 <= li < len(chunk["layers"]) and 0 <= y < 24:
                    row = (f[4] + "_" * 24)[:24]
                    if li > 0:
                        row = row.replace("=", ".")
                    chunk["layers"][li]["rows"][y] = row
            elif tag == "STRUCT":
                pending_structs.append((int(num(f[1])), f))
            elif tag == "DOOR":
                pending_doors.append((int(num(f[1])), f))
            elif tag == "SURFACE" and len(f) >= 5:
                chunk = maps.get(int(num(f[1])))
                if chunk is not None:
                    chunk["surfaceWall"] = int(num(f[2], -1))
                    chunk["surfaceFloor"] = int(num(f[3], -1))
                    chunk["surfaceCeiling"] = int(num(f[4], -1))
            elif tag in ("LIGHT", "TERMINAL", "HAZARD", "STAIR", "FIXTURE", "PROP",
                         "PICKUP", "CREATURE", "BREAKABLE", "CLUTTER", "SIGN", "PRIM",
                         "ACTOR", "ACTOR_KEY", "ACTOR_AI", "ACTOR_WEAPON"):
                chunk = maps.get(int(num(f[1])))
                if chunk is not None:
                    chunk.setdefault("_records", []).append(f)
        except (IndexError, ValueError):
            continue

    # Script records belong to the whole campaign, not one map.
    # (Collected in a second pass so ordering does not matter.)
    project_records: list[str] = []
    in_scope = False
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("--- CUSTOM_CAMPAIGN_DATA_START"):
            in_scope = True
            continue
        if line.startswith("--- CUSTOM_CAMPAIGN_DATA_END"):
            in_scope = False
        if not in_scope and ("CUSTOM_CAMPAIGN_DATA_START" in text):
            continue
        tag = line.split("|", 1)[0] if "|" in line else ""
        if tag in ALLOWED_TAGS and tag not in {"CLUTTER", "BREAKABLE"} and not tag.startswith("ACTOR"):
            project_records.append(line)

    chunks = []
    for idx in sorted(maps):
        chunk = maps[idx]
        layers = chunk["layers"] or [{
            "id": f"m{idx}l0", "name": "Ground Floor", "z": 0, "ceilingHeight": 3.0,
            "thickness": 0, "rows": ["_" * 24 for _ in range(24)],
            "materials": {}, "materialScales": {}, "materialRotations": {},
            "materialScaleYs": {}, "materialOffsetsX": {}, "materialOffsetsY": {},
        }]
        for li in range(len(layers) - 1):
            gap = layers[li + 1]["z"] - layers[li]["z"]
            if gap > 0.5:
                layers[li]["ceilingHeight"] = round(gap, 3)
        base_z = layers[0]["z"]

        def layer_for(z, records_map=chunk):
            return min(layers, key=lambda l: abs(l["z"] - z))

        objects = []
        sx, sy, sz = chunk.get("_spawn", (3.5, 3.5, base_z))
        objects.append({"id": oid(), "layerId": layer_for(sz)["id"], "type": "spawn",
                        "spawnKind": "Player", "name": "Player Start",
                        "x": sx, "y": sy, "z": sz, "rotation": 0, "mount": "floor",
                        "w": .65, "d": .65, "h": 1.75})
        door_cells = set()
        for midx, f in pending_doors:
            if midx != idx or len(f) < 6:
                continue
            left, right, y, zoff = num(f[2]), num(f[3]), num(f[4]), num(f[5])
            layer = layer_for(base_z + zoff)
            objects.append({"id": oid(), "layerId": layer["id"], "type": "door",
                            "name": "Door", "x": (left + right) / 2, "y": y,
                            "z": layer["z"], "rotation": 0, "mount": "wall",
                            "wallAxis": "horizontal", "tileX": int((left + right) / 2),
                            "tileY": int(y), "w": max(.25, right - left), "d": .18, "h": 2.2})
            door_cells.add((int((left + right) / 2), int(y)))
        for midx, f in pending_structs:
            if midx != idx or len(f) < 8:
                continue
            x1, y1, x2, y2, zb, zt = (num(f[i]) for i in range(2, 8))
            upper = [l for l in layers[1:] if abs(l["z"] - zb) < .06]
            cell_aligned = all(abs(v - round(v)) < 1e-6 for v in (x1, y1, x2, y2))
            if upper and cell_aligned and 0 <= x1 < x2 <= 24 and 0 <= y1 < y2 <= 24:
                rows = upper[0]["rows"]
                for cy in range(int(y1), int(y2)):
                    row = list(rows[cy])
                    for cx in range(int(x1), int(x2)):
                        row[cx] = "#"
                    rows[cy] = "".join(row)
                continue
            if (int(x1), int(y1)) in door_cells and abs(zb - base_z) < .1:
                continue  # a door/window opening piece; the door object recreates it
            if x2 - x1 < .05 or y2 - y1 < .05 or zt - zb < .05:
                continue
            objects.append({"id": oid(), "layerId": layer_for(zb)["id"], "type": "box",
                            "name": "Block", "x": (x1 + x2) / 2, "y": (y1 + y2) / 2,
                            "z": zb, "rotation": 0, "mount": "floor",
                            "w": x2 - x1, "d": y2 - y1, "h": zt - zb})
        for f in chunk.get("_records", []):
            tag = f[0]
            if tag == "LIGHT" and len(f) >= 5:
                z = num(f[4])
                light = {"id": oid(), "layerId": layer_for(z)["id"], "type": "light",
                         "name": "Light", "x": num(f[2]), "y": num(f[3]), "z": z,
                         "rotation": 0, "mount": "floor", "w": .3, "d": .3, "h": .3}
                if len(f) > 5 and f[5] != "":
                    light["lampMount"] = {0: "ceiling", 1: "wall", 2: "bare"}.get(int(num(f[5])), "ceiling")
                    light["rotation"] = math.degrees(num(f[6])) if len(f) > 6 else 0
                if len(f) > 7 and f[7] != "":
                    light["intensity"] = num(f[7])
                if len(f) > 8 and f[8] != "":
                    light["range"] = num(f[8])
                if len(f) > 9 and f[9] != "":
                    light["kelvin"] = int(num(f[9]))
                if len(f) >= 25 and all(f[i] != "" for i in (22, 23, 24)):
                    light["gel"] = "#%02x%02x%02x" % tuple(
                        max(0, min(255, round(num(f[i]) * 255))) for i in (22, 23, 24))
                objects.append(light)
            elif tag == "SIGN" and len(f) >= 11:
                z = num(f[4])
                objects.append({"id": oid(), "layerId": layer_for(z)["id"], "type": "sign",
                                "name": f[8] or "Sign", "title": f[8], "subtitle": f[9],
                                "accent": "#%06x" % (int(num(f[10])) & 0xFFFFFF),
                                "x": num(f[2]), "y": num(f[3]), "z": z,
                                "rotation": math.degrees(num(f[7])), "mount": "wall",
                                "w": num(f[5], 1.4), "d": .1, "h": num(f[6], .5)})
            elif tag == "PRIM" and len(f) >= 11:
                z = num(f[5])
                objects.append({"id": oid(), "layerId": layer_for(z)["id"], "type": "prim",
                                "name": f[2], "primKind": f[2],
                                "x": num(f[3]), "y": num(f[4]), "z": z,
                                "rotation": int(num(f[9])) * 90, "mount": "floor",
                                "material": int(num(f[10], 40)),
                                "w": num(f[6], 1), "d": num(f[7], 1), "h": num(f[8], 1)})
            elif tag == "TERMINAL" and len(f) >= 5:
                z = base_z + num(f[4])
                objects.append({"id": oid(), "layerId": layer_for(z)["id"], "type": "terminal",
                                "name": f[6] if len(f) > 6 else "Terminal",
                                "x": num(f[2]), "y": num(f[3]), "z": z,
                                "rotation": 0, "mount": "floor", "w": .55, "d": .35, "h": 1.1})
            elif tag == "HAZARD" and len(f) >= 10:
                kind = hazard_names.get(int(num(f[2])), "Electricity")
                objects.append({"id": oid(), "layerId": layer_for(num(f[7]))["id"], "type": "hazard",
                                "name": "Hazard", "kind": kind,
                                "x": (num(f[3]) + num(f[5])) / 2, "y": (num(f[4]) + num(f[6])) / 2,
                                "z": num(f[7]), "rotation": 0, "mount": "floor",
                                "w": num(f[5]) - num(f[3]), "d": num(f[6]) - num(f[4]),
                                "h": max(.1, num(f[8]) - num(f[7]))})
            elif tag == "STAIR" and len(f) >= 10:
                x1, y1, x2, y2, bottom, top = (num(f[i]) for i in range(2, 8))
                along, asc = int(num(f[9])) if len(f) > 9 else 1, int(num(f[10])) if len(f) > 10 else 1
                rotation = (0 if asc else 180) if along else (270 if asc else 90)
                objects.append({"id": oid(), "layerId": layer_for(bottom)["id"], "type": "stairs",
                                "name": "Stairs", "x": (x1 + x2) / 2, "y": (y1 + y2) / 2,
                                "z": bottom, "rotation": rotation, "mount": "floor",
                                "w": (x2 - x1) if along else (y2 - y1),
                                "d": (y2 - y1) if along else (x2 - x1),
                                "h": max(.25, top - bottom),
                                "steps": int(num(f[8], 17)) if len(f) > 8 else 17})
            elif tag == "FIXTURE" and len(f) >= 11:
                model = fixture_by_index.get(int(num(f[2])))
                if model:
                    z = base_z + num(f[5])
                    objects.append({"id": oid(), "layerId": layer_for(z)["id"], "type": "asset",
                                    "name": Path(model).stem, "model": model,
                                    "x": num(f[3]), "y": num(f[4]), "z": z,
                                    "rotation": math.degrees(num(f[9])), "mount": "floor",
                                    "w": num(f[6], 1), "d": num(f[7], 1), "h": num(f[8], 1)})
            elif tag == "PROP" and len(f) >= 8:
                model = prop_by_kind.get(int(num(f[2])))
                if model:
                    objects.append({"id": oid(), "layerId": layer_for(base_z)["id"], "type": "asset",
                                    "name": Path(model).stem, "model": model,
                                    "x": num(f[3]), "y": num(f[4]), "z": base_z,
                                    "rotation": math.degrees(num(f[7])), "mount": "floor",
                                    "w": num(f[8], 1) * 2 if len(f) > 8 else 1,
                                    "d": num(f[9], 1) * 2 if len(f) > 9 else 1,
                                    "h": num(f[5], 1)})
            elif tag == "PICKUP" and len(f) >= 5:
                model = pickup_by_index.get(int(num(f[2])))
                if model:
                    objects.append({"id": oid(), "layerId": layers[0]["id"], "type": "asset",
                                    "name": Path(model).stem, "model": model,
                                    "x": num(f[3]), "y": num(f[4]), "z": base_z,
                                    "rotation": 0, "mount": "floor", "w": .5, "d": .5, "h": .5})
            elif tag == "CREATURE" and len(f) >= 6:
                kind = creature_names.get(int(num(f[2])), "Huntsman")
                objects.append({"id": oid(), "layerId": layer_for(num(f[5]))["id"], "type": "spawn",
                                "spawnKind": "Creature", "name": kind + " Spawn",
                                "x": num(f[3]), "y": num(f[4]), "z": num(f[5]),
                                "rotation": 0, "mount": "floor", "w": .65, "d": .65, "h": 1.2})
            elif tag == "BREAKABLE" and len(f) >= 10:
                objects.append({"id": oid(), "layerId": layer_for(num(f[7]))["id"], "type": "box",
                                "name": "Breakable", "breakMaterial": int(num(f[9])),
                                "x": (num(f[3]) + num(f[5])) / 2, "y": (num(f[4]) + num(f[6])) / 2,
                                "z": num(f[7]), "rotation": 0, "mount": "floor",
                                "w": num(f[5]) - num(f[3]), "d": num(f[6]) - num(f[4]),
                                "h": max(.05, num(f[8]) - num(f[7]))})
            elif tag == "CLUTTER" and len(f) >= 7:
                objects.append({"id": oid(), "layerId": layer_for(base_z)["id"], "type": "box",
                                "name": "Clutter", "x": num(f[3]), "y": num(f[4]), "z": base_z,
                                "rotation": 0, "mount": "floor", "w": .6, "d": .6, "h": .6})
        # People: ACTOR tracks become spawn dummies at their first key.
        actors: dict[int, dict] = {}
        for f in chunk.get("_records", []):
            if f[0] == "ACTOR" and len(f) >= 8:
                actors[int(num(f[7]))] = {"visual": int(num(f[3])), "scale": num(f[4], 1),
                                          "x": None, "behaviour": "stand", "armed": "none"}
            elif f[0] == "ACTOR_KEY" and len(f) >= 8:
                actor = actors.get(int(num(f[2])))
                if actor is not None and actor["x"] is None:
                    actor.update({"x": num(f[4]), "y": num(f[5]), "z": num(f[6]),
                                  "rotation": math.degrees(num(f[7]))})
            elif f[0] == "ACTOR_AI" and len(f) >= 4:
                actor = actors.get(int(num(f[2])))
                if actor is not None:
                    actor["behaviour"] = {1: "civilian", 3: "guard"}.get(int(num(f[3])), "stand")
            elif f[0] == "ACTOR_WEAPON" and len(f) >= 4:
                actor = actors.get(int(num(f[2])))
                if actor is not None:
                    actor["armed"] = {1: "shotgun", 2: "handgun"}.get(int(num(f[3])), "none")
        role_names = {0: "Worker", 3: "Cargo", 4: "Security", 10: "Scientist",
                      11: "SeniorScientist", 12: "FemaleScientist", 13: "FemaleDoctor",
                      14: "LabTech", 15: "NPC"}
        for actor in actors.values():
            if actor["x"] is None:
                continue
            role = role_names.get(actor["visual"], "Worker")
            objects.append({"id": oid(), "layerId": layer_for(actor["z"])["id"], "type": "spawn",
                            "spawnKind": role, "name": role + " Dummy",
                            "x": actor["x"], "y": actor["y"], "z": actor["z"],
                            "rotation": actor.get("rotation", 0), "mount": "floor",
                            "behaviour": actor["behaviour"], "armed": actor["armed"],
                            "personScale": actor["scale"], "w": .65, "d": .65, "h": 1.75})
        chunk["objects"] = objects
        chunk.pop("_spawn", None)
        chunk.pop("_records", None)
        kept = {k: chunk[k] for k in ("id", "number", "name", "gx", "gy", "layers", "objects")}
        for key in ("surfaceWall", "surfaceFloor", "surfaceCeiling"):
            if key in chunk:
                kept[key] = chunk[key]
        chunks.append(kept)

    if not chunks:
        raise ValueError("No MAP records found. Is this a .rmc campaign file?")
    skybox = maps[sorted(maps)[0]].get("_skybox", "industrial_night") if maps else "industrial_night"
    project = {"version": 5, "name": campaign_name,
               "skybox": skybox, "chunks": chunks}
    if project_records:
        project["runtimeRecords"] = project_records
    return project

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
                project = json.loads(path.read_text(encoding="utf-8"))
                project["runtimeRecords"] = runtime_records(project)
                self.send_json({"file": path.name, "project": project})
            except Exception as exc:
                self.send_json({"error": str(exc)}, 500)
            return
        if route.startswith('/src/assets/') and blob_catalog() is not None:
            entry = (blob_catalog().get("registry") or {}).get(route[1:])
            found = blob_entry(entry) if entry else None
            if found is None:
                self.send_error(404)
                return
            data, content_type = found
            self.send_response(200)
            self.send_header('Content-Type', content_type)
            self.send_header('Cache-Control', 'public, max-age=31536000, immutable')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        if route.startswith('/__blob/'):
            found = blob_entry(route[len('/__blob/'):])
            if found is None:
                self.send_error(404)
                return
            data, content_type = found
            self.send_response(200)
            self.send_header('Content-Type', content_type)
            self.send_header('Cache-Control', 'public, max-age=31536000, immutable')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)
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
        if route not in {"/__depthworks_save_project", "/__depthworks_map_payload",
                         "/__depthworks_campaign", "/__depthworks_import_campaign"}:
            self.send_json({"error": "Not found"}, 404)
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0 or length > 16 * 1024 * 1024:
                raise ValueError("Invalid project payload size")
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            if route == "/__depthworks_import_campaign":
                text = payload.get("text")
                if not isinstance(text, str) or not text.strip():
                    raise ValueError("No campaign text received")
                self.send_json({"ok": True, "project": parse_campaign_project(text)})
                return
            project = payload.get("project")
            if not isinstance(project, dict) or not isinstance(project.get("chunks"), list) or not project["chunks"]:
                raise ValueError("Invalid Depthworks project")
            project["runtimeRecords"] = runtime_records(project)
            if route == "/__depthworks_campaign":
                name = str(payload.get("name") or project.get("name") or "Depthworks Campaign").strip()
                if not name:
                    raise ValueError("Campaign name is required")
                filename, text_payload, warnings = build_campaign_file(project, name)
                self.send_json({"ok": True, "file": filename, "payload": text_payload, "warnings": warnings})
                return
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
            print(f"Raw Metal Campaign Editor: http://{HOST}:{PORT}/level-editor/")
            print(("Assets served from editor-assets.rmblob." if blob_catalog() is not None
                   else "Assets are read directly from src/assets.") +
                  " Press Ctrl+C to stop.")
            server.serve_forever()
    except KeyboardInterrupt:
        print("\nLevel editor stopped.")
        return 0
    except OSError as exc:
        print(f"Could not start level editor server on port {PORT}: {exc}")
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
