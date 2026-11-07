import json
import subprocess
import sys
import zipfile
from pathlib import Path

HERE = Path(__file__).parent


def test_blob_registers_preview_assets():
    out = HERE / "editor-assets.rmblob"
    if not out.exists():
        result = subprocess.run(
            [sys.executable, str(HERE / "build_asset_blob.py"), "--out", str(out)],
            capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
    with zipfile.ZipFile(out) as blob:
        catalog = json.loads(blob.read("manifest.json"))
        assert catalog["format"] == "DepthWorksEditorAssets/1"
        assert len(catalog["prefabs"]) > 400
        assert len(catalog["materials"]) > 50
        prefab = catalog["prefabs"][0]
        assert prefab["modelUrl"].startswith("/__blob/files/")
        entry = prefab["modelUrl"][len("/__blob/"):]
        assert blob.getinfo(entry).file_size > 100  # a real baked mesh
        registry = catalog["registry"]
        assert prefab["model"] in registry
        assert registry[prefab["model"]] == entry
    # The whole point: ~800 MB of referenced source becomes tens of MB.
    assert out.stat().st_size < 80 * 1024 * 1024
