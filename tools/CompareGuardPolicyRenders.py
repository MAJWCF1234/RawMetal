"""Check visible squad movement in matched Vulkan captures, not visual quality."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

parser = argparse.ArgumentParser()
parser.add_argument("directory", type=Path)
directory = parser.parse_args().directory.resolve()
names = ("squad-activated", "squad-alert", "squad-fire", "squad-reload-cover")
frames = {}
sheet = Image.new("RGB", (1280, 760), "#111111")
draw = ImageDraw.Draw(sheet)
for index, name in enumerate(names):
    frame = Image.open(directory / f"{name}.ppm").convert("RGB")
    if frame.size != (1280, 720):
        raise ValueError("Expected 1280 x 720 Vulkan capture")
    frame.save(directory / f"{name}.png")
    frames[name] = np.asarray(frame)
    x, y = (index % 2) * 640, (index // 2) * 380
    sheet.paste(frame.resize((640, 360)), (x, y))
    draw.text((x + 8, y + 362), name, fill="#eeeeee")
sheet.save(directory / "squad-contact-sheet.png")

comparisons = {}
for before, after in ((names[0], names[2]), (names[2], names[3])):
    delta = np.abs(frames[before].astype(np.int16) - frames[after].astype(np.int16)).max(axis=2)
    # Same observer (22.5, 10), looking toward (21.5, 15) after each real milestone.
    # Includes both guards, the target and the supported retreat panel.
    roi = delta[50:710, 80:1200]
    changed = int((roi > 12).sum())
    comparisons[f"{before} -> {after}"] = {
        "roi": [80, 50, 1200, 710],
        "threshold_max_rgb_difference": 12,
        "changed_roi_pixels": changed,
        "roi_pixels": int(roi.size),
        "mean_roi_max_rgb_difference": round(float(roi.mean()), 3),
        "feature_visible": changed > 100,
    }
    heat = np.zeros_like(frames[before])
    heat[..., 0] = np.minimum(delta * 4, 255)
    Image.fromarray(heat).save(directory / f"{after}-difference.png")

result = {"comparisons": comparisons,
          "note": "Matched camera states show recognition, firing and retreat. Pixel differences do not measure rendering quality."}
(directory / "pixel-comparison.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps(result, indent=2))
if not all(item["feature_visible"] for item in comparisons.values()):
    raise SystemExit("Expected squad movement was not visible")
