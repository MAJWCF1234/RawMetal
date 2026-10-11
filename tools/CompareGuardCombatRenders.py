"""Compare matched Vulkan guard captures; changes are evidence, not a quality score."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument("directory", type=Path)
args = parser.parse_args()
directory = args.directory.resolve()
for path in directory.glob("guard-*.ppm"):
    Image.open(path).save(path.with_suffix(".png"))

comparisons = {}
for before, after in (("guard-unarmed-control", "guard-low-ready"),
                      ("guard-aim", "guard-fire")):
    a = np.asarray(Image.open(directory / f"{before}.png").convert("RGB"))
    b = np.asarray(Image.open(directory / f"{after}.png").convert("RGB"))
    if a.shape != b.shape or a.shape != (720, 1280, 3):
        raise ValueError("Expected matching 1280 x 720 Vulkan captures")
    delta = np.abs(a.astype(np.int16) - b.astype(np.int16)).max(axis=2)
    # Exclude the HUD and first-person hands; this is the NPC/weapon region
    # from the fixed oblique camera used by --guard-combat-inspection.
    roi = delta[230:670, 350:760]
    changed = roi > 12
    comparisons[f"{before} -> {after}"] = {
        "roi": [350, 230, 760, 670],
        "threshold_max_rgb_difference": 12,
        "changed_roi_pixels": int(changed.sum()),
        "roi_pixels": int(changed.size),
        "mean_roi_max_rgb_difference": round(float(roi.mean()), 3),
        "feature_visible": bool(changed.sum() > 100),
    }
    heat = np.zeros_like(a)
    heat[..., 0] = np.minimum(delta * 4, 255)
    Image.fromarray(heat).save(directory / f"{after}-difference.png")

result = {"comparisons": comparisons,
          "note": "Matched camera feature comparisons; not an overall rendering-quality percentage."}
(directory / "pixel-comparison.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
if not all(value["feature_visible"] for value in comparisons.values()):
    raise SystemExit("Expected weapon/pose and firing effects were not visible")
