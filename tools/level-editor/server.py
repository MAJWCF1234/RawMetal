from __future__ import annotations

import json
import mimetypes
import re
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
        if route != "/__depthworks_save_project":
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
            filename = project_filename(payload.get("file") or project.get("name") or "untitled")
            PROJECT_ROOT.mkdir(parents=True, exist_ok=True)
            path = PROJECT_ROOT / filename
            path.write_text(json.dumps(project, indent=2) + "\n", encoding="utf-8")
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
