"""Compare fixed-camera guard cover captures; differences are not quality scores."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument("directory", type=Path)
directory = parser.parse_args().directory.resolve()
for path in directory.glob("guard-*.ppm"):
    Image.open(path).save(path.with_suffix(".png"))

comparisons = {}
for before, after in (("guard-covered", "guard-reposition"),
                      ("guard-reposition", "guard-clear-fire")):
    a = np.asarray(Image.open(directory / f"{before}.png").convert("RGB"))
    b = np.asarray(Image.open(directory / f"{after}.png").convert("RGB"))
    if a.shape != b.shape or a.shape != (720, 1280, 3):
        raise ValueError("Expected matching 1280 x 720 Vulkan captures")
    delta = np.abs(a.astype(np.int16) - b.astype(np.int16)).max(axis=2)
    # Fixed observer at (22, 10.5), looking at the guard's original position.
    # The region includes the actor and low cover; HUD/hands are suppressed.
    roi = delta[130:680, 300:950]
    comparisons[f"{before} -> {after}"] = {
        "roi": [300, 130, 950, 680],
        "threshold_max_rgb_difference": 12,
        "changed_roi_pixels": int((roi > 12).sum()),
        "roi_pixels": int(roi.size),
        "mean_roi_max_rgb_difference": round(float(roi.mean()), 3),
        "feature_visible": bool((roi > 12).sum() > 100),
    }
    heat = np.zeros_like(a)
    heat[..., 0] = np.minimum(delta * 4, 255)
    Image.fromarray(heat).save(directory / f"{after}-difference.png")

result = {"comparisons": comparisons,
          "note": "Matched observer captures of movement/firing; not an overall rendering-quality percentage."}
(directory / "pixel-comparison.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
if not all(value["feature_visible"] for value in comparisons.values()):
    raise SystemExit("Expected repositioning/firing changes were not visible")
