"""Build editor-assets.rmblob: the editor's registered asset blob.

Why: the game exe is ~57 MB because the engine bakes and compresses its
assets instead of shipping the 1.1 GB source tree (753 MB of that is
high-poly source FBX). The editor gets the same treatment. This script
registers every prefab and paint material the editor can use into one
blob file:

  - models: OBJ/GLB copied; FBX baked to OBJ with tools/level-editor/fbx2obj
    (the repo's ufbx, the same library the engine bakers use)
  - textures: downscaled to at most 512 px (preview only)
  - manifest.json inside the blob registers each asset's blob entry, like
    the engine's packed-resource table

server.py serves the catalog and bytes from the blob when it sits beside
the editor, so a standalone editor folder needs no src/assets tree.

Usage: py -3 tools/level-editor/build_asset_blob.py [--out PATH]
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import subprocess
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import server  # noqa: E402

ROOT = server.ROOT
EDITOR_ROOT = server.EDITOR_ROOT
CACHE = ROOT / ".build" / "editor-blob"
MAX_TEXTURE = 512


def build_converter() -> Path:
    exe = ROOT / ".build" / "fbx2obj.exe"
    if exe.exists():
        return exe
    vcvars = sorted(Path("C:/Program Files/Microsoft Visual Studio/2022").glob(
        "*/VC/Auxiliary/Build/vcvars64.bat"))
    if not vcvars:
        raise SystemExit("fbx2obj: Visual Studio 2022 vcvars64.bat not found")
    CACHE.mkdir(parents=True, exist_ok=True)
    cmd = (f'"{vcvars[0]}" >nul 2>&1 && cl /nologo /O2 /EHsc '
           f'/I src\\ThirdParty\\ufbx tools\\level-editor\\fbx2obj.cpp '
           f'src\\ThirdParty\\ufbx\\ufbx.c /Fe:.build\\fbx2obj.exe')
    result = subprocess.run(["cmd", "/c", cmd], cwd=ROOT, capture_output=True, text=True)
    if not exe.exists():
        raise SystemExit("fbx2obj build failed:\n" + result.stdout[-2000:] + result.stderr[-2000:])
    return exe


def convert_fbx(converter: Path, source: Path) -> bytes:
    digest = hashlib.sha1(str(source).encode()).hexdigest()[:12]
    out = CACHE / "obj" / f"{source.stem}-{digest}.obj"
    out.parent.mkdir(parents=True, exist_ok=True)
    if not out.exists():
        result = subprocess.run([str(converter), str(source), str(out)],
                                capture_output=True, text=True)
        if result.returncode != 0:
            raise ValueError(f"fbx2obj {source.name}: {result.stderr.strip()}")
    return out.read_bytes()


def shrink_texture(source: Path) -> tuple[bytes, str]:
    from PIL import Image
    image = Image.open(source)
    image.thumbnail((MAX_TEXTURE, MAX_TEXTURE))
    buffer = io.BytesIO()
    if image.mode in ("RGBA", "LA") or source.suffix.lower() == ".png":
        image.save(buffer, "PNG", optimize=True)
        return buffer.getvalue(), ".png"
    image.convert("RGB").save(buffer, "JPEG", quality=82)
    return buffer.getvalue(), ".jpg"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=str(EDITOR_ROOT / "editor-assets.rmblob"))
    args = parser.parse_args()
    converter = build_converter()
    server.BLOB_DISABLED = True  # always build from the loose source tree
    manifest = server.asset_manifest()
    blobs: dict[str, bytes] = {}
    registry: dict[str, str] = {}
    skipped: list[str] = []

    def register(source_rel: str, kind: str) -> str | None:
        if source_rel in registry:
            return registry[source_rel]
        source = ROOT / source_rel
        if not source.exists():
            skipped.append(source_rel)
            return None
        try:
            if kind == "model" and source.suffix.lower() == ".fbx":
                data, ext = convert_fbx(converter, source), ".obj"
            elif kind == "texture":
                data, ext = shrink_texture(source)
            else:
                data, ext = source.read_bytes(), source.suffix.lower()
        except Exception as exc:  # one bad asset must not sink the blob
            skipped.append(f"{source_rel} ({exc})")
            return None
        entry = f"files/{len(blobs):04d}{ext}"
        blobs[entry] = data
        registry[source_rel] = entry
        return entry

    prefabs = []
    for prefab in manifest["prefabs"]:
        entry = register(prefab["model"], "model")
        if entry is None:
            continue
        item = dict(prefab)
        item["modelUrl"] = "/__blob/" + entry
        item["ext"] = ".obj" if prefab["model"].lower().endswith(".fbx") else prefab["ext"]
        if prefab.get("texture"):
            texture_entry = register(prefab["texture"], "texture")
            item["textureUrl"] = "/__blob/" + texture_entry if texture_entry else None
        prefabs.append(item)
    materials = []
    for material in manifest["materials"]:
        entry = register(material["path"], "texture")
        if entry is None:
            continue
        item = dict(material)
        item["url"] = "/__blob/" + entry
        materials.append(item)

    catalog = {"format": "DepthWorksEditorAssets/1", "prefabs": prefabs,
               "materials": materials, "files": [], "threeReady": True,
               "count": len(blobs), "registry": registry}
    out_path = Path(args.out)
    with zipfile.ZipFile(out_path, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as blob:
        blob.writestr("manifest.json", json.dumps(catalog, separators=(",", ":")))
        for entry, data in blobs.items():
            blob.writestr(entry, data)
    print(f"{out_path}  {out_path.stat().st_size / 1e6:.1f} MB  "
          f"{len(prefabs)} prefabs  {len(materials)} materials  {len(blobs)} files")
    if skipped:
        print(f"skipped {len(skipped)}: " + "; ".join(skipped[:8]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
