"""Train/export the shared neural texture models.

NTM2 (the current model, texture-n2.ntm): two heads in one blob, x2 and x4.
Each head reads the stored low-res RGB, runs a small residual CNN (3x3 convs,
residual blocks) and adds a learned luma detail residual, pixel-shuffled to
full size, on top of a bilinear upsample. It is trained here, locally, on the
game's own opaque textures (every PNG/JPG assets.rc embeds, the purchased
library packs included), with a fixed hashed holdout never seen in training,
and with VP8-compressed low-res inputs mixed in so a head also cleans up the
lossy storage the encoder may choose.

NTM1 (texture-x2.ntm, 1,084 parameters, x2 applied twice for x4) is kept
readable: TextureNet and export_model below still describe it, and packets
that reference it decode as before.

PyTorch/Pillow are offline tools only. Nothing here leaves this machine: the
runtime uses the quantized weights in native C++ (src/core/NeuralTexture.cpp).

  python tools/train_texture_upscaler.py --steps 6000 --out .build/neural-textures
"""
import argparse
import hashlib
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
# Heads in an NTM2 blob: factor -> (channels, residual blocks). Sized so a
# head costs about as much per output texel at x4 as at x2.
HEADS = {2: (12, 1), 4: (16, 2)}


# ---------------------------------------------------------------- NTM1 (legacy)
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


def export_model(model, path):
    blob = bytearray(struct.pack("<4s5I", b"NTM1", 1, 8, 2, 1084, 0))
    with torch.no_grad():
        for layer in model.layers:
            blob += quantize_layer(layer)
    path.write_bytes(blob)


# ---------------------------------------------------------------- NTM2
def conv(i, o):
    return nn.Conv2d(i, o, 3, padding=1, padding_mode="replicate")


class Head(nn.Module):
    def __init__(self, factor, channels, blocks):
        super().__init__()
        self.factor = factor
        self.inp = conv(3, channels)
        self.blocks = nn.ModuleList([nn.ModuleList([conv(channels, channels), conv(channels, channels)]) for _ in range(blocks)])
        self.out = conv(channels, factor * factor)
        nn.init.zeros_(self.out.weight); nn.init.zeros_(self.out.bias)
        for a, b in self.blocks:
            b.weight.data *= .1; nn.init.zeros_(b.bias)

    def layers(self):
        return [self.inp] + [c for pair in self.blocks for c in pair] + [self.out]

    def forward(self, rgb):
        h = F.relu(self.inp(rgb - .5))
        for a, b in self.blocks:
            h = h + b(F.relu(a(h)))
        residual = F.pixel_shuffle(self.out(h), self.factor)
        return F.interpolate(rgb, scale_factor=self.factor, mode="bilinear", align_corners=False) + residual


class TextureNet2(nn.Module):
    def __init__(self, heads=HEADS):
        super().__init__()
        self.heads = nn.ModuleDict({str(f): Head(f, c, b) for f, (c, b) in heads.items()})

    def forward(self, rgb, factor):
        return self.heads[str(factor)](rgb)


def quantize_layer(layer):
    """int16 weights with one float scale per layer; writes the restored
    values back so the PyTorch model is exactly what the runtime runs."""
    weight = layer.weight.detach().cpu().numpy().transpose(2, 3, 1, 0).copy()  # ky kx in out
    bias = layer.bias.detach().cpu().numpy().copy()
    values = np.concatenate((weight.reshape(-1), bias))
    scale = np.float32(max(float(np.abs(values).max()) / 32767, 1e-12))
    q = np.clip(np.rint(values / scale), -32767, 32767).astype("<i2")
    restored = q.astype(np.float32) * scale
    n = weight.size
    layer.weight.data.copy_(torch.from_numpy(restored[:n].reshape(weight.shape).transpose(3, 2, 0, 1).copy()))
    layer.bias.data.copy_(torch.from_numpy(restored[n:].copy()))
    return struct.pack("<f", scale) + q.tobytes()


def export_model2(model, path):
    heads = sorted(model.heads.values(), key=lambda h: h.factor)
    blob = bytearray(struct.pack("<4s2I", b"NTM2", 2, len(heads)))
    with torch.no_grad():
        for head in heads:
            channels = head.inp.out_channels
            count = sum(l.weight.numel() + l.bias.numel() for l in head.layers())
            blob += struct.pack("<5I", head.factor, 3, channels, len(head.blocks), count)
            for layer in head.layers():
                blob += quantize_layer(layer)
    path.write_bytes(blob)
    return len(blob)


def read_model2(path_or_bytes):
    data = path_or_bytes if isinstance(path_or_bytes, (bytes, bytearray)) else Path(path_or_bytes).read_bytes()
    magic, version, count = struct.unpack_from("<4s2I", data)
    if magic != b"NTM2" or version != 2 or not 1 <= count <= 4:
        raise ValueError("Invalid NTM2 model")
    at, heads = 12, {}
    for _ in range(count):
        factor, inputs, channels, blocks, params = struct.unpack_from("<5I", data, at); at += 20
        if inputs != 3:
            raise ValueError("NTM2 heads read RGB")
        heads[factor] = (channels, blocks)
        at_layers = at
        head = Head(factor, channels, blocks)
        with torch.no_grad():
            for layer in head.layers():
                scale, = struct.unpack_from("<f", data, at_layers); at_layers += 4
                n = layer.weight.numel() + layer.bias.numel()
                values = np.frombuffer(data, dtype="<i2", count=n, offset=at_layers).astype(np.float32) * scale
                at_layers += n * 2
                shape = (3, 3, layer.in_channels, layer.out_channels)
                layer.weight.copy_(torch.from_numpy(values[:layer.weight.numel()].reshape(shape).transpose(3, 2, 0, 1).copy()))
                layer.bias.copy_(torch.from_numpy(values[layer.weight.numel():].copy()))
        if sum(l.weight.numel() + l.bias.numel() for l in head.layers()) != params:
            raise ValueError("NTM2 parameter count mismatch")
        at = at_layers
        heads[factor] = head
    if at != len(data):
        raise ValueError("NTM2 trailing bytes")
    model = TextureNet2({f: (h.inp.out_channels, len(h.blocks)) for f, h in heads.items()})
    for f, h in heads.items():
        model.heads[str(f)].load_state_dict(h.state_dict())
    return model.eval()


# ---------------------------------------------------------------- shared helpers
def tensor(image):
    return torch.from_numpy(np.asarray(image, dtype=np.float32).copy()).permute(2, 0, 1).unsqueeze(0) / 255


def quantize(image):
    return torch.clamp(torch.floor(image * 255 + .5), 0, 255) / 255


def image_from_tensor(value):
    return Image.fromarray(torch.floor(value.clamp(0, 1)[0].permute(1, 2, 0)*255+.5).to(torch.uint8).numpy())


def ssim(a, b):
    # Block luminance SSIM (8x8 blocks), the metric every neural round reports.
    a = np.asarray(a, dtype=np.float64)[..., :3] @ np.array([.2126, .7152, .0722])
    b = np.asarray(b, dtype=np.float64)[..., :3] @ np.array([.2126, .7152, .0722])
    h, w = a.shape
    a = a[:h//8*8, :w//8*8].reshape(h//8, 8, w//8, 8).transpose(0, 2, 1, 3).reshape(-1, 64)
    b = b[:h//8*8, :w//8*8].reshape(h//8, 8, w//8, 8).transpose(0, 2, 1, 3).reshape(-1, 64)
    ma, mb = a.mean(1), b.mean(1)
    va, vb = a.var(1), b.var(1)
    covariance = ((a-ma[:, None])*(b-mb[:, None])).mean(1)
    return float((((2*ma*mb+6.5025)*(2*covariance+58.5225))/((ma*ma+mb*mb+6.5025)*(va+vb+58.5225))).mean())


def downsample(rgb, factor):
    """Encoder's reduction: edge-pad to a multiple of the factor, bicubic."""
    a = np.asarray(rgb)
    h, w = a.shape[:2]
    lw, lh = (w+factor-1)//factor, (h+factor-1)//factor
    padded = Image.fromarray(np.pad(a, ((0, lh*factor-h), (0, lw*factor-w), (0, 0)), mode="edge"))
    return padded, padded.resize((lw, lh), Image.Resampling.BICUBIC)


def lossy(image, quality):
    stream = io.BytesIO()
    image.save(stream, format="WEBP", quality=quality, method=6, use_sharp_yuv=True)
    return Image.open(io.BytesIO(stream.getvalue())).convert("RGB")


def holdout(name):
    return name in HOLDOUT or hashlib.sha256(name.encode()).digest()[0] % 10 == 0


def texture_sources():
    """Every opaque colour texture the game embeds (masters, not packets)."""
    seen = set()
    for number, relative in re.findall(r'^(\d+)\s+RCDATA\s+"([^"]+)"', (ROOT/"src/assets.rc").read_text(encoding="cp1252"), re.M):
        if relative.endswith(".ntx"):
            relative = re.sub(r"-(x\d|n)\.ntx$", ".png", relative)
        if not relative.endswith((".png", ".jpg")) or relative in seen:
            continue
        seen.add(relative)
        low = relative.lower()
        if any(w in low for w in ("normal", "emission", "_nrm", "/ui/", "font")):
            continue
        yield int(number), relative


PACK_SKIP = re.compile(r"normal|_nrm|_n\.|rough|metal(ic|ness)|_ao|occlusion|height|disp|emiss|mask|spec|gloss|_orm|_arm|opacity|alpha|icon|logo|thumb|preview|screenshot|font|ui", re.I)


def pack_sources(folder, count):
    """A deterministic sample of colour textures from a purchased pack folder
    on this machine; brought to <=256 px as the importer does."""
    files = [p for p in Path(folder).rglob("*") if p.suffix.lower() in (".png", ".jpg", ".jpeg") and not PACK_SKIP.search(p.name)
             and not any(part in ("Audio", "Animations", "archives") for part in p.parts)]
    files.sort(key=lambda p: hashlib.sha256(str(p.relative_to(folder)).encode()).digest())
    return files[:count]


def inventory(limit_side=512, extra=None, extra_count=0):
    data = []
    digests = set()
    sources = [(n, ROOT/"src"/r, r, None) for n, r in texture_sources()]
    if extra:
        sources += [(0, p, "pack:"+p.name, 256) for p in pack_sources(extra, extra_count)]
    for number, path, relative, side in sources:
        if not path.exists():
            continue
        digest = hashlib.sha256(path.read_bytes()).digest()
        if digest in digests:
            continue
        digests.add(digest)
        try:
            image = Image.open(path).convert("RGBA")
        except Exception:
            continue
        if side and max(image.size) > side:
            scale = side/max(image.size)
            image = image.resize((max(1, round(image.size[0]*scale)), max(1, round(image.size[1]*scale))), Image.Resampling.LANCZOS)
        w, h = image.size
        if min(w, h) < 64 or image.getextrema()[3] != (255, 255):
            continue
        rgb = image.convert("RGB")
        if max(w, h) > limit_side:  # big masters: train on a centred tile
            x, y = max(0, (w-limit_side)//2), max(0, (h-limit_side)//2)
            rgb = rgb.crop((x, y, x+min(w, limit_side), y+min(h, limit_side)))
        item = dict(id=number, relative=relative, name=path.name, high=np.asarray(rgb), unseen=holdout(path.name) and not side, pack=bool(side))
        for f in (2, 4):
            padded, low = downsample(rgb, f)
            item[f"high{f}"] = np.asarray(padded)
            item[f"low{f}"] = np.asarray(low)
            item[f"lossy{f}"] = np.asarray(lossy(low, 85 if f == 2 else 90))
        data.append(item)
    return data


def as_tensor(array):
    return torch.from_numpy(array.astype(np.float32)/255).permute(2, 0, 1).unsqueeze(0)


def batch(train, factor, size, count, rng):
    lows, highs = [], []
    for _ in range(count):
        item = train[rng.randrange(len(train))]
        low = item[f"lossy{factor}"] if rng.random() < .4 else item[f"low{factor}"]
        high = item[f"high{factor}"]
        lh, lw = low.shape[:2]
        s = min(size, lh, lw)
        y, x = rng.randrange(lh-s+1), rng.randrange(lw-s+1)
        l = low[y:y+s, x:x+s]; h = high[y*factor:(y+s)*factor, x*factor:(x+s)*factor]
        k = rng.randrange(8)
        l, h = np.rot90(l, k % 4), np.rot90(h, k % 4)
        if k >= 4: l, h = l[:, ::-1], h[:, ::-1]
        if s < size:  # tiny textures: repeat to the patch size
            l = np.pad(l, ((0, size-s), (0, size-s), (0, 0)), mode="wrap"); h = np.pad(h, ((0, (size-s)*factor), (0, (size-s)*factor), (0, 0)), mode="wrap")
        lows.append(as_tensor(np.ascontiguousarray(l))); highs.append(as_tensor(np.ascontiguousarray(h)))
    return torch.cat(lows), torch.cat(highs)


def psnr(mse):
    return 10*math.log10(1/max(mse, 1e-12))


def evaluate(model, data, factor, key="low"):
    """Mean PSNR (dB) of quantized output vs the master, and bilinear's."""
    net, plain = [], []
    with torch.no_grad():
        for item in data:
            low = as_tensor(item[f"{key}{factor}"]); high = as_tensor(item[f"high{factor}"])
            out = quantize(model(low, factor)) if isinstance(model, TextureNet2) else quantize(model(low))
            base = quantize(F.interpolate(low, scale_factor=factor, mode="bilinear", align_corners=False))
            net.append(psnr(float(((out-high)**2).mean()))); plain.append(psnr(float(((base-high)**2).mean())))
    return sum(net)/len(net), sum(plain)/len(plain)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, default=ROOT/".build/neural-textures")
    parser.add_argument("--steps", type=int, default=6000)
    parser.add_argument("--threads", type=int, default=12)
    parser.add_argument("--extra", type=Path, help="a purchased pack folder on this machine to add training textures from")
    parser.add_argument("--extra-count", type=int, default=1200)
    parser.add_argument("--heads", default="", help="e.g. 2:12x1,4:16x2 (channels x residual blocks)")
    args = parser.parse_args()
    heads = dict(HEADS)
    if args.heads:
        heads = {int(f): tuple(int(v) for v in spec.split("x")) for f, spec in (p.split(":") for p in args.heads.split(","))}
    args.out.mkdir(parents=True, exist_ok=True)
    rng = random.Random(1337); np.random.seed(1337); torch.manual_seed(1337)
    torch.set_num_threads(args.threads)
    start = time.perf_counter()
    data = inventory(extra=args.extra, extra_count=args.extra_count)
    train = [d for d in data if not d["unseen"]]
    test = [d for d in data if d["unseen"]]
    print(f"{len(data)} textures ({sum(d['pack'] for d in data)} from the pack folder; {len(train)} training, {len(test)} held out) in {time.perf_counter()-start:.0f}s", flush=True)
    model = TextureNet2(heads)
    params = sum(p.numel() for p in model.parameters())
    optimizer = torch.optim.Adam(model.parameters(), lr=2e-3)
    schedule = torch.optim.lr_scheduler.OneCycleLR(optimizer, max_lr=2e-3, total_steps=args.steps, pct_start=.05)
    for step in range(args.steps):
        factor = 2 if step % 2 == 0 else 4
        low, high = batch(train, factor, 32 if factor == 2 else 16, 24, rng)
        out = model(low, factor)
        b = 2*factor
        error = (out-high)[..., b:-b, b:-b]
        loss = torch.sqrt(error*error + 1e-6).mean()  # Charbonnier
        optimizer.zero_grad(); loss.backward(); optimizer.step(); schedule.step()
        if (step+1) % 500 == 0 or step+1 == args.steps:
            line = []
            for f in (2, 4):
                n, p = evaluate(model, test, f)
                line.append(f"x{f} {n:.2f} dB (bilinear {p:.2f})")
            print(f"step {step+1}/{args.steps}: held out " + ", ".join(line) + f"; {time.perf_counter()-start:.0f}s", flush=True)
    size = export_model2(model, args.out/"texture-n2.ntm")
    torch.save(model.state_dict(), args.out/"training-checkpoint-n2.pt")
    summary = dict(seed=1337, steps=args.steps, pack_folder=str(args.extra) if args.extra else None, pack_textures=sum(d["pack"] for d in data), heads={str(f): dict(channels=c, blocks=b) for f, (c, b) in heads.items()},
                   parameters=params, model_bytes=size, training_textures=len(train), held_out_textures=len(test),
                   held_out_policy="sha256(file name)[0] % 10 == 0, plus panel-metal/water/wall-painted; never trained",
                   lossy_inputs="40% of patches use VP8 (sharp YUV) low-res: q85 at x2, q90 at x4")
    for f in (2, 4):
        n, p = evaluate(model, test, f); nl, pl = evaluate(model, test, f, "lossy")
        summary[f"x{f}"] = dict(held_out_psnr=n, bilinear_psnr=p, held_out_psnr_lossy_input=nl, bilinear_psnr_lossy_input=pl)
    (args.out/"training-n2.json").write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
