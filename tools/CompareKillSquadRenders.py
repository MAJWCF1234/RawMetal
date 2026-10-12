"""Compare matched Vulkan encounter captures; pixel differences are not quality scores."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

parser = argparse.ArgumentParser()
parser.add_argument("directory", type=Path)
directory = parser.parse_args().directory.resolve()
names = ("cleanup-ready", "cleanup-grenade", "cleanup-fire", "cleanup-pursuit")
images = {}
for name in names:
    with Image.open(directory / (name + ".ppm")) as source:
        image = source.convert("RGB")
    if image.size != (1280, 720):
        raise ValueError("Expected 1280 x 720 Vulkan captures")
    image.save(directory / (name + ".png"))
    images[name] = image
for path in directory.glob("cleanup-*-close.ppm"):
    with Image.open(path) as source:
        source.save(path.with_suffix(".png"))
with Image.open(directory / "cleanup-engineer-throw.ppm") as source:
    source.save(directory / "cleanup-engineer-throw.png")
with Image.open(directory / "cleanup-drone.ppm") as source:
    source.save(directory / "cleanup-drone.png")
roles = ("engineer-throw", "assault-close", "sharpshooter-close", "heavy-close", "medic-close", "rusher-close", "drone")
role_sheet = Image.new("RGB", (1920, 1152), (18, 18, 18))
role_draw = ImageDraw.Draw(role_sheet)
for index, role in enumerate(roles):
    x, y = (index % 3) * 640, (index // 3) * 384
    with Image.open(directory / ("cleanup-" + role + ".png")) as source:
        role_sheet.paste(source.resize((640, 360)), (x, y + 24))
    role_draw.text((x + 8, y + 6), role, fill=(230, 230, 230))
role_sheet.save(directory / "full-squad-observation.png")
medical_sheet=Image.new("RGB",(1920,384),(18,18,18))
medical_draw=ImageDraw.Draw(medical_sheet)
for index,name in enumerate(("cleanup-medkit-opening","cleanup-medkit-treatment","cleanup-medkit-healed")):
    with Image.open(directory/(name+".ppm")) as source:
        source.save(directory/(name+".png"))
        medical_sheet.paste(source.resize((640,360)),(index*640,24))
    medical_draw.text((index*640+8,6),name,fill=(230,230,230))
medical_sheet.save(directory/"medic-treatment-observation.png")

comparisons = {}
for after in names[1:3]:
    a = np.asarray(images[names[0]], dtype=np.int16)
    b = np.asarray(images[after], dtype=np.int16)
    delta = np.abs(a - b).max(axis=2)
    roi = delta[180:690, 250:1050]
    comparisons[f"{names[0]} -> {after}"] = {
        "roi": [250, 180, 1050, 690],
        "threshold_max_rgb_difference": 12,
        "changed_roi_pixels": int((roi > 12).sum()),
        "roi_pixels": int(roi.size),
        "mean_roi_max_rgb_difference": round(float(roi.mean()), 3),
    }
    heat = np.zeros((720, 1280, 3), dtype=np.uint8)
    heat[..., 0] = np.minimum(delta * 4, 255)
    Image.fromarray(heat).save(directory / (after + "-difference.png"))

sheet = Image.new("RGB", (1280, 768), (18, 18, 18))
draw = ImageDraw.Draw(sheet)
for index, name in enumerate(names):
    x, y = (index % 2) * 640, (index // 2) * 384
    sheet.paste(images[name].resize((640, 360)), (x, y + 24))
    draw.text((x + 8, y + 6), name, fill=(230, 230, 230))
sheet.save(directory / "squad-observation.png")
result = {
    "comparisons": comparisons,
    "note": "Ready, grenade and fire use the same camera. Pursuit uses a different observer and is excluded from pixel comparison. Differences show behavior and pose changes, not a rendering-quality improvement percentage.",
}
(directory / "pixel-comparison.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps(result, indent=2))
if any(value["changed_roi_pixels"] < 100 for value in comparisons.values()):
    raise SystemExit("Expected visible encounter changes were absent")
