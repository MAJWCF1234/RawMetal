from __future__ import annotations

import json
import mimetypes
import threading
import time
import webbrowser
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = ROOT / "src" / "assets"
HOST = "127.0.0.1"
PORT = 8008

MODEL_EXTENSIONS = {".obj", ".fbx", ".glb", ".gltf"}
TEXTURE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".webp", ".bmp", ".tga"}
AUDIO_EXTENSIONS = {".wav", ".ogg", ".mp3", ".flac"}

def asset_manifest() -> dict:
    items = []
    if ASSET_ROOT.exists():
        for path in sorted(ASSET_ROOT.rglob("*")):
            if not path.is_file():
                continue
            rel = path.relative_to(ROOT).as_posix()
            ext = path.suffix.lower()
            kind = "model" if ext in MODEL_EXTENSIONS else "texture" if ext in TEXTURE_EXTENSIONS else "audio" if ext in AUDIO_EXTENSIONS else "other"
            items.append({"name": path.name, "path": rel, "url": "/" + rel, "kind": kind, "ext": ext, "bytes": path.stat().st_size})
    return {"root": "src/assets", "count": len(items), "items": items}

class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def do_GET(self):
        route = urlparse(self.path).path
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

    def log_message(self, fmt, *args):
        print("[LevelEditor] " + fmt % args)

def open_browser():
    time.sleep(0.4)
    webbrowser.open(f"http://{HOST}:{PORT}/level-editor/")

def main() -> int:
    mimetypes.add_type("model/gltf-binary", ".glb")
    mimetypes.add_type("model/gltf+json", ".gltf")
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
