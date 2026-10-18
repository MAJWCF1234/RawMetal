"""Encode explicitly selected surface textures with the existing shared model.

Offline only. Edge padding preserves odd-sized atlases without rescaling UVs.
Writes source/reconstruction previews and quality metrics for human review.
"""
import argparse
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import struct

import numpy as np
from PIL import Image
import torch

from train_texture_upscaler import ROOT, TextureNet, image_from_tensor, quantize, ssim, tensor


def read_model(path):
    data = path.read_bytes()
    if len(data) != 2204 or struct.unpack_from('<4s5I', data) != (b'NTM1', 1, 8, 2, 1084, 0):
        raise ValueError('Invalid model')
    model = TextureNet()
    offset = 24
    with torch.no_grad():
        for layer in model.layers:
            scale, = struct.unpack_from('<f', data, offset)
            offset += 4
            count = layer.weight.numel() + layer.bias.numel()
            values = np.frombuffer(data, dtype='<i2', count=count, offset=offset).astype(np.float32) * scale
            offset += count * 2
            shape = (layer.kernel_size[0], layer.kernel_size[1], layer.in_channels, layer.out_channels)
            weights = values[:layer.weight.numel()].reshape(shape).transpose(3, 2, 0, 1).copy()
            layer.weight.copy_(torch.from_numpy(weights))
            layer.bias.copy_(torch.from_numpy(values[layer.weight.numel():].copy()))
    return model.eval()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--model', type=Path, default=ROOT/'src/assets/ai/texture-x2.ntm')
    args = parser.parse_args()
    torch.set_num_threads(4)
    model = read_model(args.model)
    args.out.mkdir(parents=True, exist_ok=True)
    rows = []
    for item in json.loads(args.plan.read_text()):
        image = Image.open(ROOT/'src'/item['source']).convert('RGBA')
        if image.getextrema()[3] != (255, 255):
            raise ValueError(f"Refusing to remove alpha from {item['source']}")
        image = image.convert('RGB')
        w, h = image.size
        factor = item['factor']
        if factor not in (2, 4) or min(w, h) < 4 or max(w, h) > 1024:
            raise ValueError('Unsupported neural texture dimensions/factor')
        lw, lh = (w+factor-1)//factor, (h+factor-1)//factor
        if min(lw, lh) < 2:
            raise ValueError('Reduced image too small')
        padded = Image.fromarray(np.pad(np.asarray(image), ((0, lh*factor-h), (0, lw*factor-w), (0, 0)), mode='edge'))
        low = padded.resize((lw, lh), Image.Resampling.BICUBIC)
        stream = io.BytesIO()
        low.save(stream, format='WEBP', lossless=True, quality=100, method=6, exact=True)
        packet = struct.pack('<4s7I', b'NTX1', w, h, lw, lh, 348, factor, 0) + stream.getvalue()
        destination = args.out/str(item['id'])
        destination.mkdir(exist_ok=True)
        (destination/f'x{factor}.ntx').write_bytes(packet)
        image.save(destination/'original.png')
        low.save(destination/f'low-x{factor}.png')
        with torch.no_grad():
            output = tensor(low)
            for _ in range(int(math.log2(factor))):
                output = quantize(model(output))
        reconstruction = image_from_tensor(output).crop((0, 0, w, h))
        reconstruction.save(destination/f'neural-x{factor}.png')
        error = np.asarray(image, dtype=np.float64) - np.asarray(reconstruction, dtype=np.float64)
        mse = float((error**2).mean())
        rows.append(dict(id=item['id'], source=item['source'], factor=factor, width=w, height=h, low_width=lw, low_height=lh,
                         packet_bytes=len(packet), mae=float(np.abs(error).mean()), psnr=10*math.log10(255**2/max(mse, 1e-12)),
                         ssim=ssim(image, reconstruction), source_sha256=hashlib.sha256((ROOT/'src'/item['source']).read_bytes()).hexdigest()))
        print(f"{item['id']} x{factor}: {w}x{h} -> {lw}x{lh}, {len(packet)} bytes; PSNR {rows[-1]['psnr']:.2f}, SSIM {rows[-1]['ssim']:.3f}", flush=True)
    with (args.out/'metrics.csv').open('w', newline='') as file:
        writer = csv.DictWriter(file, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)
    (args.out/'model-sha256.txt').write_text(hashlib.sha256(args.model.read_bytes()).hexdigest()+'\n')


if __name__ == '__main__':
    main()
