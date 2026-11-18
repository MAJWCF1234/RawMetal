"""Encode the textures tools/neural_texture_plan.json marks as NTX2 neural packets.

Plan entries (one JSON list):
  {"id": 117, "source": "assets/scissors.png", "factor": 2}
      one resource by id and master (the original form; rounds 1-4)
  {"library": "bld_*", "factor": "auto"}
      every library texture (src/assets/library, by file name or a glob on
      its path under the library folder, e.g. "textures/surface_*") that
      assets.rc embeds; library entries are how new parts get marked
factor is 2, 4 or "auto" (try x4, then x2). "lossy": false keeps an entry's
stored texels lossless (first-person arms and the player's body are seen up
close). A later entry overrides an earlier one for the same texture.

For each marked texture the encoder reduces the master (edge-padded bicubic),
refines the reduced texels by gradient descent through the shared NTM2 head so
the reconstruction, not the reduction, matches the master, and then tries the
low-res storage choices (lossless VP8L, then VP8 at falling quality). The
smallest packet that passes the quality gate wins:

  PSNR >= 32 dB and block luminance SSIM >= 0.90 over the visible texels,
  the worst 1% of 8x8 blocks at SSIM >= 0.50 (local damage: text, grain),
  at least 20% and 2 KB smaller than the lossless WebP of the master, and x4
  only where the stored image keeps at least 32 texels a side; each choice
  keeps 90% of the detail (luma gradient energy) the x2 lossless one does
  (a sky panorama may instead lose under 0.4 luma levels per texel more than
  it: a flat sky's dither is not detail, and a sky is seen far off). A smooth image
  (over half its 8x8 blocks flat to 2 levels: skies, glows) keeps lossless
  texels, since VP8's blocks would show as bands across its gradients.

Alpha (if any) is kept exact in a full-size lossless plane. A texture with no
passing candidate stays raw (or keeps the NTX1 packet it already ships).
Passing packets are written as <stem>-n.ntx (factor in its header) beside their master and
assets.rc is pointed at them; failures are pointed back at the master PNG.

Offline and local only: nothing is sent anywhere. Writes metrics.csv and
previews (original | reconstruction | 4x error) for review.

  python tools/encode_neural_textures.py --out <dir> [--apply] [--only GLOB]
"""
import argparse
import csv
import fnmatch
import hashlib
import io
import json
import math
from pathlib import Path
import re
import struct
import sys
import time

import numpy as np
from PIL import Image
import torch
from torch.nn import functional as F

from train_texture_upscaler import ROOT, TextureNet, image_from_tensor, quantize, read_model2, ssim, tensor

PLAN = ROOT/'tools'/'neural_texture_plan.json'
RC = ROOT/'src'/'assets.rc'
MODEL = ROOT/'src'/'assets'/'ai'/'texture-n2.ntm'
MODEL_ID = 1999
GATE = dict(psnr=32.0, ssim=0.90, block_p1=0.50, ratio=0.80, saving=2048, x4_min_side=32, smooth=0.5, detail=0.9, detail_loss=0.4)
QUALITIES = (None, 97, 94, 90, 85, 80)  # None = lossless VP8L
LIBRARY = 'assets/library/'


def read_model(path):
    """The legacy NTM1 model (texture-x2.ntm), for comparisons."""
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
            layer.weight.copy_(torch.from_numpy(values[:layer.weight.numel()].reshape(shape).transpose(3, 2, 0, 1).copy()))
            layer.bias.copy_(torch.from_numpy(values[layer.weight.numel():].copy()))
    return model.eval()


def rc_entries(text):
    return {int(n): p for n, p in re.findall(r'^(\d+)\s+RCDATA\s+"([^"]+)"', text, re.M)}


def master_of(path):
    """The master a resource path stands for (packets map back to PNGs)."""
    return re.sub(r'-(x\d|n)\.ntx$', '.png', path)


def resolve(plan, entries):
    """Plan entries -> [(id, master relative path, factors)] in resource order."""
    chosen = {}
    for item in plan:
        factors = (4, 2) if item.get('factor') == 'auto' else (int(item['factor']),)
        spec = (factors, bool(item.get('lossy', True)))
        if 'library' in item:
            pattern = item['library']
            for number, path in entries.items():
                master = master_of(path)
                if not master.startswith(LIBRARY) or not master.endswith('.png'):
                    continue
                name = master[len(LIBRARY):]
                if name.startswith('skies/'):
                    continue  # panoramas: tools/library/make_skies.py encodes those at their embedded size
                if fnmatch.fnmatch(name, pattern) or fnmatch.fnmatch(name, pattern + '.png'):
                    chosen[number] = (master, spec)
        else:
            chosen[int(item['id'])] = (item['source'], spec)
    return sorted((n, m, f) for n, (m, f) in chosen.items())


def webp(image, quality):
    stream = io.BytesIO()
    if quality is None:
        image.save(stream, format='WEBP', lossless=True, quality=100, method=6, exact=True)
    else:
        image.save(stream, format='WEBP', quality=quality, method=6, use_sharp_yuv=True)
    data = stream.getvalue()
    return data, np.asarray(Image.open(io.BytesIO(data)).convert('RGB'))


def block_ssims(a, b, mask):
    a = a.astype(np.float64)[..., :3] @ np.array([.2126, .7152, .0722])
    b = b.astype(np.float64)[..., :3] @ np.array([.2126, .7152, .0722])
    h, w = a.shape
    def blocks(v):
        return v[:h//8*8, :w//8*8].reshape(h//8, 8, w//8, 8).transpose(0, 2, 1, 3).reshape(-1, 64)
    a, b, m = blocks(a), blocks(b), blocks(mask.astype(np.float64))
    keep = m.mean(1) >= .5
    if not keep.any():
        return np.ones(1)
    a, b = a[keep], b[keep]
    ma, mb = a.mean(1), b.mean(1)
    va, vb = a.var(1), b.var(1)
    cov = ((a-ma[:, None])*(b-mb[:, None])).mean(1)
    return ((2*ma*mb+6.5025)*(2*cov+58.5225))/((ma*ma+mb*mb+6.5025)*(va+vb+58.5225))


def flat_fraction(original):
    """Share of visible 8x8 blocks whose luma barely moves (std under 2 levels)."""
    y = original[..., :3].astype(np.float64) @ np.array([.2126, .7152, .0722])
    h, w = y.shape
    if h < 8 or w < 8:
        return 1.0
    blocks = y[:h//8*8, :w//8*8].reshape(h//8, 8, w//8, 8).transpose(0, 2, 1, 3).reshape(-1, 64)
    seen = (original[:h//8*8, :w//8*8, 3] >= 128).reshape(h//8, 8, w//8, 8).transpose(0, 2, 1, 3).reshape(-1, 64).mean(1) >= .5
    return float((blocks[seen].std(1) < 2).mean()) if seen.any() else 1.0


def measure(original, recon, mask):
    # Colour error weighted by alpha (what shows); blocks at least half visible.
    weight = original[..., 3].astype(np.float64)/255
    error = (original[..., :3].astype(np.float64)-recon[..., :3])
    total = max(float(weight.sum()), 1e-9)
    mse = float(((error**2).mean(-1)*weight).sum()/total)
    blocks = block_ssims(original, recon, original[..., 3] >= 128)
    return dict(psnr=10*math.log10(255**2/max(mse, 1e-12)), ssim=float(blocks.mean()), block_p1=float(np.percentile(blocks, 1)),
                mae=float((np.abs(error).mean(-1)*weight).sum()/total), detail=detail(original, recon))


def gradient_energy(a, seen=None):
    """Mean luma gradient magnitude (levels per texel) over visible texels."""
    y = a[..., :3].astype(np.float64) @ np.array([.2126, .7152, .0722])
    g = np.abs(np.diff(y, axis=0))[:, :-1] + np.abs(np.diff(y, axis=1))[:-1, :]
    return float(g[seen].mean()) if seen is not None and seen.any() else float(g.mean())


def detail(original, recon):
    """Luma gradient energy kept, reconstruction over master, on visible texels."""
    seen = original[:-1, :-1, 3] >= 128
    base = gradient_energy(original, seen)
    return gradient_energy(recon, seen)/base if base > 1e-9 else 1.0


def keeps_detail(m, reference, base, sky=False):
    """Within 90% of the detail x2 lossless keeps; a sky may instead lose
    under 0.4 luma levels of gradient per texel more (its flat dither)."""
    return m['detail'] >= GATE['detail']*reference or (sky and (reference-m['detail'])*base <= GATE['detail_loss'])


def looks_right(m):
    return m['psnr'] >= GATE['psnr'] and m['ssim'] >= GATE['ssim'] and m['block_p1'] >= GATE['block_p1']


def passes(m, packet, baseline):
    return looks_right(m) and packet <= baseline*GATE['ratio'] and baseline-packet >= GATE['saving']


def refine(head, low, target, weight, steps):
    """Choose the stored texels: descend on the low-res so the head's
    reconstruction matches the master (straight-through 8-bit rounding)."""
    if steps <= 0:
        return low
    x = low.clone().requires_grad_(True)
    opt = torch.optim.Adam([x], lr=2/255)
    for _ in range(steps):
        q = x + (quantize(x.clamp(0, 1))-x).detach()
        out = head(q)[..., :target.shape[-2], :target.shape[-1]]
        loss = (((out-target)**2)*weight).mean()
        opt.zero_grad(); loss.backward(); opt.step()
    return quantize(x.detach().clamp(0, 1))


def packet(w, h, lw, lh, factor, colour, lossy, alpha):
    flags = (1 if lossy else 0) | (2 if alpha is not None else 0)
    body = struct.pack('<4s7I', b'NTX2', w, h, lw, lh, MODEL_ID, factor, flags) + struct.pack('<I', len(colour)) + colour
    if alpha is not None:
        body += struct.pack('<I', len(alpha)) + alpha
    return body


def encode(model, source, factors, steps, sky=False, lossy=True):
    rgba = (source if isinstance(source, Image.Image) else Image.open(source)).convert('RGBA')
    w, h = rgba.size
    original = np.asarray(rgba)
    opaque = original[..., 3].min() == 255
    mask = original[..., 3] > 0
    alpha = None
    if not opaque:
        a = original[..., 3]
        alpha, _ = webp(Image.fromarray(np.stack([a, a, a], -1)), None)
    raw, _ = webp(rgba, None)  # what the packer would store (lossless VP8L)
    baseline = len(raw)
    weight = torch.from_numpy(original[..., 3].astype(np.float32)/255)[None, None]
    smooth = flat_fraction(original) > GATE['smooth']
    energy = gradient_energy(original, original[:-1, :-1, 3] >= 128)
    target = tensor(Image.fromarray(original[..., :3]))
    candidates = []
    reference = None  # detail the x2 lossless reconstruction keeps
    for factor in sorted(factors):
        lw, lh = (w+factor-1)//factor, (h+factor-1)//factor
        if min(lw, lh) < (GATE["x4_min_side"] if factor == 4 else 2) or max(lw, lh) > 2048 or max(w, h) > 4096:
            continue
        padded = np.pad(original[..., :3], ((0, lh*factor-h), (0, lw*factor-w), (0, 0)), mode='edge')
        low = Image.fromarray(padded).resize((lw, lh), Image.Resampling.BICUBIC)
        head = model.heads[str(factor)]
        with torch.enable_grad():
            stored = refine(head, tensor(low), target, weight, steps)
        stored = image_from_tensor(stored)

        def attempt(quality):
            colour, decoded = webp(stored, quality)
            data = packet(w, h, lw, lh, factor, colour, quality is not None, alpha)
            with torch.no_grad():
                out = image_from_tensor(quantize(head(tensor(Image.fromarray(decoded)))))
            recon = np.asarray(out.crop((0, 0, w, h)))
            m = measure(original, recon, mask)
            nonlocal reference
            if reference is None:
                reference = m['detail']
            ok = passes(m, len(data), baseline) and keeps_detail(m, reference, energy, sky)
            candidates.append(dict(factor=factor, quality=quality or 0, bytes=len(data), data=data, recon=recon, ok=ok, **m))
            return candidates[-1]
        # Lossless is this factor's best quality: if it fails, every lossy
        # choice fails too. Otherwise climb from the smallest (lowest VP8
        # quality) and stop at the first that passes. Every choice must keep
        # 90% of the detail (luma gradient energy) x2 lossless keeps (a sky:
        # or lose under 0.4 levels per texel more): VP8 and x4 smear ridges
        # and grain that SSIM and PSNR forgive.
        first = attempt(None)
        if not looks_right(first) or not keeps_detail(first, reference, energy, sky) or smooth or not lossy:
            continue  # smooth images keep lossless texels: VP8 blocks show as bands in a gradient
        for quality in QUALITIES[:0:-1]:
            if attempt(quality)['ok']:
                break
    winners = [c for c in candidates if c['ok']]
    best = min(winners, key=lambda c: c['bytes']) if winners else (max(candidates, key=lambda c: (c['psnr'] >= GATE['psnr'], c['ssim'])) if candidates else None)
    return dict(width=w, height=h, alpha=not opaque, baseline=baseline, best=best, passed=bool(winners), original=original, mask=mask,
                candidates=[{k: v for k, v in c.items() if k not in ('data', 'recon')} for c in candidates])


_shared = None


def neural_packet(image, factors=(4, 2), sky=True):
    """(NTX2 packet, metrics) for a PIL image under the same gate, or (None,
    metrics) when nothing passes. The sky tools (tools/library) use this."""
    global _shared
    if _shared is None:
        _shared = read_model2(MODEL)
        for p in _shared.parameters():
            p.requires_grad_(False)
    result = encode(_shared, image, factors, 0, sky)
    best = {k: v for k, v in (result['best'] or {}).items() if k not in ('data', 'recon')}
    return (result['best']['data'] if result['passed'] else None), best


def preview(path, original, recon, mask):
    o = original[..., :3].astype(np.int16); r = recon[..., :3].astype(np.int16)
    err = np.clip(np.abs(o-r)*4, 0, 255).astype(np.uint8)
    err[~mask] = 0
    sheet = np.concatenate([original[..., :3], np.full((original.shape[0], 4, 3), 255, np.uint8), recon[..., :3], np.full((original.shape[0], 4, 3), 255, np.uint8), err], 1)
    image = Image.fromarray(sheet)
    scale = max(1, 512 // max(original.shape[:2]))
    if scale > 1:
        image = image.resize((image.width*scale, image.height*scale), Image.Resampling.NEAREST)
    image.save(path)


_worker = None


def _start_worker(model_path, threads):
    global _worker
    torch.set_num_threads(threads)
    _worker = read_model2(model_path)
    for p in _worker.parameters():
        p.requires_grad_(False)


def _encode_job(job):
    path, (factors, lossy), steps = job
    try:
        return encode(_worker, Path(path), factors, steps, lossy=lossy)
    except ValueError as error:
        return str(error)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--plan', type=Path, default=PLAN)
    parser.add_argument('--out', type=Path, required=True, help='metrics.csv and previews')
    parser.add_argument('--model', type=Path, default=MODEL)
    parser.add_argument('--apply', action='store_true', help='write packets beside their masters and update assets.rc')
    parser.add_argument('--only', help='glob on the master path')
    parser.add_argument('--refine-steps', type=int, default=0, help='descend on the stored texels first (smaller error, larger lossless packets)')
    parser.add_argument('--threads', type=int, default=2, help='per worker')
    parser.add_argument('--jobs', type=int, default=6, help='textures encoded at once')
    args = parser.parse_args()
    torch.set_num_threads(args.threads)
    model = read_model2(args.model)
    for p in model.parameters():
        p.requires_grad_(False)
    text = RC.read_bytes().decode('cp1252')
    entries = rc_entries(text)
    if entries.get(MODEL_ID) != 'assets/ai/texture-n2.ntm':
        raise SystemExit(f'resource {MODEL_ID} must be assets/ai/texture-n2.ntm in assets.rc')
    todo = resolve(json.loads(args.plan.read_text()), entries)
    if args.only:
        todo = [t for t in todo if fnmatch.fnmatch(t[1], args.only)]
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out/'previews').mkdir(exist_ok=True)
    rows, cache, changes, tried = [], {}, {}, []
    start = time.perf_counter()
    jobs = {}
    for number, master, factors in todo:
        source = ROOT/'src'/master
        if source.exists():
            jobs.setdefault((hashlib.sha256(source.read_bytes()).hexdigest(), factors), str(source))
    from concurrent.futures import ProcessPoolExecutor
    with ProcessPoolExecutor(args.jobs, initializer=_start_worker, initargs=(str(args.model), args.threads)) as pool:
        keys = list(jobs)
        for i, (key, result) in enumerate(zip(keys, pool.map(_encode_job, [(jobs[k], k[1], args.refine_steps) for k in keys]))):
            cache[key] = result
            if (i+1) % 25 == 0:
                print(f'{i+1}/{len(keys)} distinct textures encoded ({time.perf_counter()-start:.0f}s)', flush=True)
    for number, master, factors in todo:
        source = ROOT/'src'/master
        current = entries.get(number)
        if current is None or not source.exists():
            print(f'{number} {master}: not embedded or master missing; skipped', flush=True)
            continue
        key = (hashlib.sha256(source.read_bytes()).hexdigest(), factors)
        result = cache[key]
        if isinstance(result, str):
            print(f'{number} {master}: {result}', flush=True)
            continue
        best = result['best']
        ntx1 = current.endswith(('-x2.ntx', '-x4.ntx'))
        old_bytes = (ROOT/'src'/current).stat().st_size if ntx1 else None
        if result['passed'] and (old_bytes is None or best['bytes'] < old_bytes):
            status = 'neural'
            target = f"{master[:-4]}-n.ntx"
            changes[number] = target
            if args.apply:
                (ROOT/'src'/target).write_bytes(best['data'])
        elif ntx1:
            status = 'kept-ntx1'  # keeps the screened NTX1 packet it already ships
        else:
            status = 'raw'
            changes[number] = master
        row = dict(id=number, master=master, width=result['width'], height=result['height'], alpha=int(result['alpha']),
                   raw_bytes=result['baseline'], ntx1_bytes=old_bytes or '', status=status,
                   factor=best['factor'] if best else '', storage=('lossless' if best and not best['quality'] else f"vp8 q{best['quality']}") if best else '',
                   packet_bytes=best['bytes'] if best else '', psnr=round(best['psnr'], 3) if best else '', ssim=round(best['ssim'], 4) if best else '',
                   block_p1=round(best['block_p1'], 4) if best else '', mae=round(best['mae'], 3) if best else '',
                   saved=((old_bytes or result['baseline'])-best['bytes']) if status == 'neural' else 0)
        rows.append(row)
        tried.extend(dict(id=number, master=master, raw_bytes=result['baseline'], **c) for c in result['candidates'])
        if best is not None:
            row['_preview'] = (result['original'], best['recon'], result['mask'])
        print(f"{number} {master}: {row['status']} x{row['factor']} {row['storage']} {row['packet_bytes']}/{row['raw_bytes']} B, "
              f"PSNR {row['psnr']} SSIM {row['ssim']} p1 {row['block_p1']} ({time.perf_counter()-start:.0f}s)", flush=True)
    # Previews of the five weakest shipped reconstructions, and the five
    # weakest rejects, for review.
    shipped = sorted((r for r in rows if r['status'] == 'neural'), key=lambda r: r['ssim'])
    rejected = sorted((r for r in rows if r['status'] != 'neural' and '_preview' in r), key=lambda r: -float(r['ssim'] or 0))
    for label, group in (('worst-shipped', shipped[:5]), ('closest-rejected', rejected[:5])):
        for i, r in enumerate(group):
            preview(args.out/'previews'/f"{label}-{i+1}-{r['id']}-{Path(r['master']).stem}.png", *r['_preview'])
    for r in rows:
        r.pop('_preview', None)
    if rows:
        with (args.out/'metrics.csv').open('w', newline='') as file:
            writer = csv.DictWriter(file, fieldnames=list(rows[0].keys()))
            writer.writeheader(); writer.writerows(rows)
    if tried:
        with (args.out/'candidates.csv').open('w', newline='') as file:
            writer = csv.DictWriter(file, fieldnames=list(tried[0].keys()))
            writer.writeheader(); writer.writerows(tried)
    summary = dict(gate=GATE, model=str(args.model.relative_to(ROOT)), model_sha256=hashlib.sha256(args.model.read_bytes()).hexdigest(),
                   marked=len(rows), neural=sum(r['status'] == 'neural' for r in rows), raw=sum(r['status'] == 'raw' for r in rows),
                   kept_ntx1=sum(r['status'] == 'kept-ntx1' for r in rows), bytes_saved=sum(r['saved'] for r in rows),
                   x4=sum(r['status'] == 'neural' and r['factor'] == 4 for r in rows), lossy_storage=sum(r['status'] == 'neural' and r['storage'] != 'lossless' for r in rows))
    (args.out/'summary.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary, indent=2))
    if args.apply and changes:
        def swap(match):
            number = int(match.group(1))
            return f'{number} RCDATA "{changes[number]}"' if number in changes else match.group(0)
        text = RC.read_bytes().decode('cp1252')  # as it is now, not as it was an encode ago
        updated = re.sub(r'^(\d+) RCDATA "[^"]+"', swap, text, flags=re.M)
        if updated != text:
            RC.write_bytes(updated.encode('cp1252'))
            print(f'assets.rc: {sum(1 for n in changes if entries[n] != changes[n])} resources repointed')


if __name__ == '__main__':
    main()
