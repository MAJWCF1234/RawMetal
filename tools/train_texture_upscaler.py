"""Train/export a tiny shared x2 subpixel CNN on the project's surface textures.

PyTorch/Pillow are offline tools only. Runtime uses the quantized weights in
native C++ and never needs Python, a DL framework, or original high-res pixels.
"""
import argparse
import csv
import io
import json
import math
from pathlib import Path
import random
import re
import struct
import time

import numpy as np
from PIL import Image
import torch
from torch import nn
from torch.nn import functional as F

ROOT = Path(__file__).resolve().parents[1]
LUMA = torch.tensor([.2126, .7152, .0722]).view(1, 3, 1, 1)
HOLDOUT = {"wall-painted.png", "panel-metal.png", "water.png"}


class TextureNet(nn.Module):
    def __init__(self):
        super().__init__()
        self.layers = nn.ModuleList([
            nn.Conv2d(1, 8, 5, padding=2, padding_mode="replicate"),
            nn.Conv2d(8, 8, 3, padding=1, padding_mode="replicate"),
            nn.Conv2d(8, 4, 3, padding=1, padding_mode="replicate"),
        ])
        nn.init.zeros_(self.layers[-1].weight)
        nn.init.zeros_(self.layers[-1].bias)

    def forward(self, rgb):
        y = (rgb * LUMA).sum(1, keepdim=True) - .5
        y = F.relu(self.layers[0](y))
        y = F.relu(self.layers[1](y))
        residual = F.pixel_shuffle(self.layers[2](y), 2)
        return F.interpolate(rgb, scale_factor=2, mode="bilinear", align_corners=False) + residual


def tensor(image):
    return torch.from_numpy(np.asarray(image, dtype=np.float32).copy()).permute(2, 0, 1).unsqueeze(0) / 255


def quantize(image):
    return torch.clamp(torch.floor(image * 255 + .5), 0, 255) / 255


def inventory():
    result = []
    for number, relative in re.findall(r'(\d+)\s+RCDATA\s+"([^"]+\.png)"', (ROOT / "src/assets.rc").read_text()):
        if not (relative.startswith(("assets/materials/", "assets/environment/", "assets/pressureworks/")) or relative in {"assets/wall.png", "assets/metal.png"}):
            continue
        if any(word in relative.lower() for word in ("normal", "emission", "sign", "grating", "gate")):
            continue
        path = ROOT / "src" / relative
        image = Image.open(path).convert("RGBA")
        w, h = image.size
        if min(w, h) < 128 or max(w, h) > 1024 or w & (w-1) or h & (h-1) or image.getextrema()[3] != (255, 255):
            continue
        rgb = image.convert("RGB")
        low = rgb.resize((w//2, h//2), Image.Resampling.BICUBIC)
        result.append(dict(id=int(number), relative=relative, name=path.stem, rgb=rgb, high=tensor(rgb), low=tensor(low), low4=tensor(rgb.resize((w//4,h//4),Image.Resampling.BICUBIC)), unseen=path.name in HOLDOUT))
    return result


def validation(model, data):
    losses, baseline = [], []
    with torch.no_grad():
        for item in data:
            target = item["high"]
            start = target.shape[-1] * 3 // 4 + 8
            for factor in (2,4):
                x = item["low"] if factor==2 else item["low4"]
                output = model(x)
                if factor==4: output=model(quantize(output))
                plain = F.interpolate(x, size=target.shape[-2:], mode="bilinear", align_corners=False)
                losses.append(float(((output[..., start:] - target[..., start:])**2).mean()))
                baseline.append(float(((plain[..., start:] - target[..., start:])**2).mean()))
    return sum(losses)/len(losses), sum(baseline)/len(baseline)


def export_model(model, path):
    blob = bytearray(struct.pack("<4s5I", b"NTM1", 1, 8, 2, 1084, 0))
    with torch.no_grad():
        for layer in model.layers:
            weight = layer.weight.detach().cpu().numpy().transpose(2, 3, 1, 0).copy()
            bias = layer.bias.detach().cpu().numpy().copy()
            values = np.concatenate((weight.reshape(-1), bias))
            scale = np.float32(max(float(np.abs(values).max()) / 32767, 1e-12))
            q = np.clip(np.rint(values / scale), -32767, 32767).astype("<i2")
            blob += struct.pack("<f", scale) + q.tobytes()
            restored = q.astype(np.float32) * scale
            n = weight.size
            layer.weight.copy_(torch.from_numpy(restored[:n].reshape(weight.shape).transpose(3, 2, 0, 1).copy()))
            layer.bias.copy_(torch.from_numpy(restored[n:].copy()))
    path.write_bytes(blob)


def ssim(a, b):
    # Same block luminance metric used in the preceding native comparison.
    a = np.asarray(a, dtype=np.float64) @ np.array([.2126, .7152, .0722])
    b = np.asarray(b, dtype=np.float64) @ np.array([.2126, .7152, .0722])
    h, w = a.shape
    a = a[:h//8*8, :w//8*8].reshape(h//8, 8, w//8, 8).transpose(0, 2, 1, 3).reshape(-1, 64)
    b = b[:h//8*8, :w//8*8].reshape(h//8, 8, w//8, 8).transpose(0, 2, 1, 3).reshape(-1, 64)
    ma, mb = a.mean(1), b.mean(1)
    va, vb = a.var(1), b.var(1)
    covariance = ((a-ma[:, None])*(b-mb[:, None])).mean(1)
    return float((((2*ma*mb+6.5025)*(2*covariance+58.5225))/((ma*ma+mb*mb+6.5025)*(va+vb+58.5225))).mean())


def image_from_tensor(value):
    return Image.fromarray(torch.floor(value.clamp(0, 1)[0].permute(1, 2, 0)*255+.5).to(torch.uint8).numpy())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, default=ROOT / ".build/neural-textures")
    parser.add_argument("--steps", type=int, default=1200)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    random.seed(1337); np.random.seed(1337); torch.manual_seed(1337)
    torch.set_num_threads(4)
    data = inventory()
    train = [item for item in data if not item["unseen"]]
    if not train:
        raise RuntimeError("No training surfaces")
    model = TextureNet()
    optimizer = torch.optim.Adam(model.parameters(), lr=.001)
    best, baseline = validation(model, data)
    best_state = {name: p.clone() for name, p in model.state_dict().items()}
    print(f"{len(data)} surfaces; {len(train)} training, {len(data)-len(train)} unseen assets; 1084 parameters. Baseline validation MSE {baseline:.8f}", flush=True)
    start = time.perf_counter()
    for step in range(args.steps):
        lr_patches, hr_patches = [], []
        factor = 2 if step%2==0 else 4
        for _ in range(16):
            item = random.choice(train)
            low, high = (item["low"] if factor==2 else item["low4"]), item["high"]
            x = random.randrange(max(1, low.shape[-1]*3//4 - 32 - 8))
            y = random.randrange(low.shape[-2] - 32 + 1)
            lr_patches.append(low[..., y:y+32, x:x+32])
            hr_patches.append(high[..., y*factor:(y+32)*factor, x*factor:(x+32)*factor])
        low, target = torch.cat(lr_patches), torch.cat(hr_patches)
        output = model(low)
        if factor==4:
            # Straight-through quantization trains the exact two-pass runtime.
            output = output + (quantize(output)-output).detach()
            output = model(output)
        border = 8 if factor==2 else 16
        output = output[..., border:-border, border:-border]
        target = target[..., border:-border, border:-border]
        error = output-target
        loss = (error**2).mean() + .05*((error[..., 1:, :]-error[..., :-1, :])**2).mean() + .05*((error[..., :, 1:]-error[..., :, :-1])**2).mean()
        optimizer.zero_grad(); loss.backward(); optimizer.step()
        if step == args.steps*2//3:
            for group in optimizer.param_groups: group["lr"] = .0002
        if step == 0 or (step+1)%100 == 0 or step+1 == args.steps:
            score, _ = validation(model, data)
            if score < best:
                best = score
                best_state = {name: p.clone() for name, p in model.state_dict().items()}
            print(f"step {step+1}/{args.steps}: validation {score:.8f}, best {best:.8f}, {time.perf_counter()-start:.1f}s", flush=True)
    model.load_state_dict(best_state)
    export_model(model, args.out/"texture-x2.ntm")
    torch.save(model.state_dict(), args.out/"training-checkpoint.pt")
    score, baseline = validation(model, data)
    rows = []
    with torch.no_grad():
        for item in data:
            directory = args.out/"candidates"/str(item["id"])
            directory.mkdir(parents=True, exist_ok=True)
            reference = np.asarray(item["rgb"], dtype=np.float64)
            w, h = item["rgb"].size
            for factor in (2, 4):
                low_image = item["rgb"].resize((w//factor, h//factor), Image.Resampling.BICUBIC)
                low_image.save(directory/f"low-x{factor}.png")
                stream = io.BytesIO(); low_image.save(stream, format="WEBP", lossless=True, quality=100, method=6, exact=True)
                packet = struct.pack("<4s7I", b"NTX1", w, h, w//factor, h//factor, 348, factor, 0) + stream.getvalue()
                (directory/f"x{factor}.ntx").write_bytes(packet)
                output = tensor(low_image)
                for _ in range(int(math.log2(factor))): output = quantize(model(output))
                neural = image_from_tensor(output)
                neural.save(directory/f"neural-x{factor}.png")
                # The native comparison baseline uses the same stored samples.
                plain = image_from_tensor(F.interpolate(tensor(low_image), size=(h, w), mode="bilinear", align_corners=False))
                plain.save(directory/f"bilinear-x{factor}.png")
                error = reference-np.asarray(neural, dtype=np.float64)
                mse = float((error**2).mean())
                plain_error = reference-np.asarray(plain, dtype=np.float64)
                heldout_start = w*3//4+8
                heldout_mse = float((error[:, heldout_start:]**2).mean())
                rows.append(dict(id=item["id"], source=item["relative"], factor=factor, unseen_asset=item["unseen"], packet_bytes=len(packet), mae=float(np.abs(error).mean()), psnr=10*math.log10(255**2/max(mse, 1e-12)), ssim=ssim(item["rgb"], neural), heldout_psnr=10*math.log10(255**2/max(heldout_mse, 1e-12)), bilinear_mse=float((plain_error**2).mean()), neural_mse=mse))
    with (args.out/"candidate-metrics.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=rows[0].keys()); writer.writeheader(); writer.writerows(rows)
    (args.out/"training.json").write_text(json.dumps(dict(seed=1337, steps=args.steps, model_bytes=(args.out/"texture-x2.ntm").stat().st_size, parameters=1084, heldout_policy="right quarter + 8-pixel margin never used for training; wall-painted/panel-metal/water entirely unseen", validation_mse=score, bilinear_mse=baseline, train_assets=[item["relative"] for item in train], all_assets=[item["relative"] for item in data]), indent=2))
    print(f"Exported {len(rows)} candidates; shared model {(args.out/'texture-x2.ntm').stat().st_size} bytes; quantized validation MSE {score:.8f} vs bilinear {baseline:.8f}", flush=True)


if __name__ == "__main__":
    main()
