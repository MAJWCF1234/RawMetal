"""Encode the screened release textures as lossy WebP and point assets.rc at them.

Each plan entry names a resource id, its PNG master and the WebP quality that
passed screening (PSNR >= 38 dB and block luminance SSIM >= 0.95 against the
master, alpha exact, at least 25% and 2 KB smaller than the lossless packing,
then inspected side by side). The PNG masters stay in the tree; the packer
stores the WebP as transform 15 and the engine decodes it to the same RMT1
pixels the embedded check verifies.

Usage: python tools/encode_lossy_textures.py
"""
import io
import json
import math
import re
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT/'tools'/'lossy_texture_plan.json'
RC = ROOT/'src'/'assets.rc'


def main():
    plan = json.loads(PLAN.read_text())
    text = RC.read_text()
    for item in plan:
        source = ROOT/'src'/item['source']
        image = Image.open(source).convert('RGBA')
        stream = io.BytesIO()
        image.save(stream, 'WEBP', quality=item['quality'], method=6, alpha_quality=100, exact=True, use_sharp_yuv=True)
        data = stream.getvalue()
        original = np.asarray(image)
        decoded = np.asarray(Image.open(io.BytesIO(data)).convert('RGBA'))
        if (decoded[:, :, 3] != original[:, :, 3]).any():
            raise ValueError(f"{item['source']}: alpha changed")
        error = (original[:, :, :3].astype(float)-decoded[:, :, :3])[original[:, :, 3] > 0]
        psnr = 10*math.log10(255**2/max((error**2).mean(), 1e-9))
        if psnr < 38:
            raise ValueError(f"{item['source']}: {psnr:.2f} dB is under the screening bar")
        target = item['source'][:-4]+'-q.webp'
        (ROOT/'src'/target).write_bytes(data)
        line = re.compile(rf'^{item["id"]} RCDATA "[^"]+"$', re.M)
        if not line.search(text):
            raise ValueError(f"resource {item['id']} is not in assets.rc")
        text = line.sub(f'{item["id"]} RCDATA "{target}"', text)
        print(f"{item['id']} {target}: q{item['quality']}, {len(data)} bytes, {psnr:.2f} dB")
    with open(RC, 'w', newline='') as file:
        file.write(text)
    print(f'{len(plan)} lossy textures encoded')


if __name__ == '__main__':
    main()
