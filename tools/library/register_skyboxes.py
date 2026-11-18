#!/usr/bin/env python3
"""Register paid Brutal Skyboxes as neural packets for the engine.

Original 4096x1024 PNGs stay under src/assets/Skyboxes/Brutal/ as the donor
masters. The engine embeds assets/ai/brutal/<stem>-n.ntx packets that
reconstruct 2048x512 panoramas (legacy wasteland resolution) through the
shared NTM2 model (texture-n2.ntm, resource 1999): the smallest x4 or x2
packet that passes tools/encode_neural_textures.py's quality gate, or the
legacy NTX1 x2 packet (texture-x2, resource 348) if none does. Run from any
directory. --check verifies donors, packets and generated registration
without writing.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import shutil
import struct

import sys

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

DEFAULT_SOURCE = Path(
    r"O:\retro official\Brutal Skyboxes v1.4.1\Brutal Skyboxes"
    r"\Brutal Skyboxes\Textures\Skyboxes"
)
FIRST_RESOURCE = 2800
DONOR_SIZE = (4096, 1024)
# Match the legacy wasteland sky (resource 252) so the 47-pack fits the cart.
EMBED_SIZE = (2048, 512)
NEURAL_FACTOR = 2
NEURAL_MODEL_RESOURCE = 348
NEURAL2_MODEL_RESOURCE = 1999
STEMS = tuple(
    [f"skybox_emx_{i}" for i in range(1, 36) if i not in (7, 22)]
    + [f"skybox_plain_emx_{i}" for i in range(1, 15)]
)
BEGIN = "// BEGIN GENERATED BRUTAL SKYBOXES"
END = "// END GENERATED BRUTAL SKYBOXES"
# The skies tools/library/make_skies.py makes (2026-11-17): five from the purchased packs
# (packets under the gitignored assets/library/skies/), five made there (assets/ai/skies/).
# haze is the sky's own air, used when the MAP leaves its haze fields empty. light is its own
# outdoor light: sun (or moon) azimuth and elevation in degrees, intensity and colour, then the
# ambient strength and colour (World::sun, World::outdoorAmbient, World::skyAmbientTone), then the
# ceiling on the eye's auto exposure (World::skyExposure): a night is not metered back up to day.
EXTRA_FIRST_RESOURCE = 2900
EXTRA_BEGIN = "// BEGIN GENERATED EXTRA SKIES"
EXTRA_END = "// END GENERATED EXTRA SKIES"
EXTRA_SKIES = [
    {"id": "psx_clear_day", "label": "Clear day", "ntx": "assets/library/skies/psx_clear_day-n.ntx", "haze": (.66, .74, .84, .005), "light": (120, 50, 2.3, 1, 0.96, 0.88, 0.3, 0.92, 0.97, 1.08, 1)},
    {"id": "grey_overcast", "label": "Grey overcast", "ntx": "assets/ai/skies/grey_overcast-n.ntx", "haze": (.62, .64, .66, .011), "light": (60, 62, 0.55, 0.95, 0.97, 1, 0.46, 0.97, 0.99, 1.02, 1)},
    {"id": "cold_dawn", "label": "Cold dawn", "ntx": "assets/ai/skies/cold_dawn-n.ntx", "haze": (.62, .50, .50, .008), "light": (95, 4, 1.25, 1, 0.62, 0.42, 0.2, 0.86, 0.82, 1, 0.85)},
    {"id": "dusk_afterglow", "label": "Dusk afterglow", "ntx": "assets/library/skies/dusk_afterglow-n.ntx", "haze": (.24, .24, .32, .009), "light": (90, 3, 0.7, 1, 0.55, 0.36, 0.15, 0.74, 0.78, 1.08, 0.7)},
    {"id": "storm_front", "label": "Storm front", "ntx": "assets/ai/skies/storm_front-n.ntx", "haze": (.27, .29, .32, .016), "light": (110, 35, 0.3, 0.74, 0.8, 0.9, 0.19, 0.84, 0.9, 1, 0.75)},
    {"id": "psx_starry_night", "label": "Starry night", "ntx": "assets/library/skies/psx_starry_night-n.ntx", "haze": (.06, .09, .16, .008), "light": (58, 41, 0.42, 0.55, 0.68, 1, 0.075, 0.55, 0.68, 1.06, 0.5)},
    {"id": "cloudy_night", "label": "Cloudy night", "ntx": "assets/library/skies/cloudy_night-n.ntx", "haze": (.08, .09, .12, .011), "light": (90, 50, 0.14, 0.6, 0.66, 0.85, 0.085, 0.6, 0.68, 0.96, 0.45)},
    {"id": "treeline_night", "label": "Treeline night", "ntx": "assets/library/skies/treeline_night-n.ntx", "haze": (.10, .15, .24, .009), "light": (90, 40, 0.28, 0.55, 0.68, 1, 0.075, 0.55, 0.7, 1.06, 0.5)},
    {"id": "ember_eclipse", "label": "Ember eclipse", "ntx": "assets/ai/skies/ember_eclipse-n.ntx", "haze": (.30, .14, .09, .013), "light": (70, 24, 0.5, 1, 0.32, 0.12, 0.09, 1.1, 0.55, 0.45, 0.6)},
    {"id": "resonance_aurora", "label": "Resonance aurora", "ntx": "assets/ai/skies/resonance_aurora-n.ntx", "haze": (.10, .07, .15, .010), "light": (90, 30, 0.28, 0.7, 0.45, 1, 0.075, 0.76, 0.6, 1.12, 0.5)},
]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def png_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Not a PNG with an IHDR header: {path}")
    return struct.unpack(">II", header[16:24])


def label(stem: str) -> str:
    family = "plain" if stem.startswith("skybox_plain_") else "sky"
    return f"Brutal {family} {stem.rsplit('_', 1)[1]}"


def ntx_name(stem: str) -> str:
    # Under assets/ai/brutal/ so paid packets never overwrite the legacy
    # wasteland sky (assets/ai/skybox_plain_emx_14-x2.ntx, resource 252).
    return f"{stem}-n.ntx"


def ntx_relative(stem: str) -> str:
    return f"assets/ai/brutal/{ntx_name(stem)}"


def ntx_header(path: Path) -> tuple[int, int, int, int, int, int]:
    data = path.read_bytes()
    if len(data) < 32 or data[:4] not in (b"NTX1", b"NTX2"):
        raise ValueError(f"Invalid neural sky packet: {path}")
    return struct.unpack_from("<6I", data, 4)


def sky_packet(image: Image.Image) -> bytes:
    """An embedded-size RGB panorama as the smallest NTX2 packet that passes
    the neural quality gate, else the legacy lossless NTX1 x2 packet."""
    from encode_neural_textures import neural_packet
    packet, _ = neural_packet(image)
    if packet is not None:
        return packet
    width, height = image.size
    low_w = (width + NEURAL_FACTOR - 1) // NEURAL_FACTOR
    low_h = (height + NEURAL_FACTOR - 1) // NEURAL_FACTOR
    padded = Image.fromarray(np.pad(np.asarray(image), ((0, low_h * NEURAL_FACTOR - height), (0, low_w * NEURAL_FACTOR - width), (0, 0)), mode="edge"))
    low = padded.resize((low_w, low_h), Image.Resampling.BICUBIC)
    stream = io.BytesIO()
    low.save(stream, format="WEBP", lossless=True, quality=100, method=6, exact=True)
    return struct.pack("<4s7I", b"NTX1", width, height, low_w, low_h, NEURAL_MODEL_RESOURCE, NEURAL_FACTOR, 0) + stream.getvalue()


def valid_sky_header(path: Path) -> bool:
    width, height, low_w, low_h, model, factor = ntx_header(path)
    legacy = path.read_bytes()[:4] == b"NTX1"
    return ((width, height) == EMBED_SIZE and factor in ((NEURAL_FACTOR,) if legacy else (2, 4))
            and model == (NEURAL_MODEL_RESOURCE if legacy else NEURAL2_MODEL_RESOURCE)
            and low_w == EMBED_SIZE[0] // factor and low_h == EMBED_SIZE[1] // factor)


def encode_neural_packet(png: Path) -> bytes:
    image = Image.open(png).convert("RGBA")
    if image.getextrema()[3] != (255, 255):
        raise ValueError(f"Refusing to strip alpha from sky panorama: {png}")
    image = image.convert("RGB")
    if image.size != DONOR_SIZE:
        raise ValueError(f"Expected donor {DONOR_SIZE}, got {image.size}: {png}")
    return sky_packet(image.resize(EMBED_SIZE, Image.Resampling.LANCZOS))


def cpp_float(v: float) -> str:
    text = f"{v:g}"
    return text + ("f" if "." in text or "e" in text else ".f")


def catalog_text() -> str:
    rows = [
        '    {"industrial_night", "Industrial night", 0, false},',
        '    {"zone_overcast", "Zone overcast", 0, true},',
        '    {"brutal_wasteland", "Brutal wasteland", 252, true},',
    ]
    rows += [
        f'    {{"{stem}", "{label(stem)}", {FIRST_RESOURCE + i}, true}},'
        for i, stem in enumerate(STEMS)
    ]
    rows += [
        f'    {{"{sky["id"]}", "{sky["label"]}", {EXTRA_FIRST_RESOURCE + i}, true, '
        + ", ".join(cpp_float(v) for v in (*sky["haze"], *sky["light"])) + "},"
        for i, sky in enumerate(EXTRA_SKIES)
    ]
    return (
        "#pragma once\n"
        "// Authored sky IDs shared by the engine, map exporter and editor.\n"
        "// Generated by tools/library/register_skyboxes.py; legacy IDs remain stable.\n"
        "#include <array>\n#include <string_view>\n\nnamespace retro {\n"
        "struct SkyboxDef {\n    std::string_view id, label;\n"
        "    int resource;\n    bool terrain;\n"
        "    // The sky's own air when the MAP leaves its haze empty; density < 0 keeps the default.\n"
        "    float hazeR = 0, hazeG = 0, hazeB = 0, hazeDensity = -1;\n"
        "    // Its own outdoor light; sunIntensity < 0 keeps the default daylight. Azimuth and\n"
        "    // elevation are degrees, as the panorama is drawn (0 = +x, 90 = +y).\n"
        "    float sunAzimuth = 0, sunElevation = 0, sunIntensity = -1, sunR = 1, sunG = 1, sunB = 1;\n"
        "    float ambient = -1, ambientR = 1, ambientG = 1, ambientB = 1;\n"
        "    // How far the eye may open under it (1 = as by day): a night stays dark.\n"
        "    float exposure = 1;\n};\n"
        f"inline constexpr std::array<SkyboxDef, {len(rows)}> SkyboxCatalog{{{{\n"
        + "\n".join(rows)
        + "\n}};\n"
        "inline constexpr const SkyboxDef* findSkybox(std::string_view id) {\n"
        "    for (const auto& sky : SkyboxCatalog)\n"
        "        if (sky.id == id) return &sky;\n    return nullptr;\n}\n"
        "inline constexpr bool terrainSkybox(std::string_view id) {\n"
        "    const auto* sky = findSkybox(id);\n"
        "    return sky && sky->terrain;\n}\n}\n"
    )


def resource_text(existing: str) -> str:
    block = "\n".join(
        [BEGIN]
        + [
            f'{FIRST_RESOURCE + i} RCDATA "{ntx_relative(stem)}"'
            for i, stem in enumerate(STEMS)
        ]
        + [END]
    )
    pattern = re.compile(re.escape(BEGIN) + r".*?" + re.escape(END), re.DOTALL)
    stripped, count = pattern.subn("", existing)
    if count > 1 or (BEGIN in stripped or END in stripped):
        raise ValueError("Malformed or duplicate generated skybox resource block")
    for match in re.finditer(r"(?m)^\s*(\d+)\s+\w+\s+", stripped):
        resource = int(match.group(1))
        if FIRST_RESOURCE <= resource < FIRST_RESOURCE + len(STEMS):
            raise ValueError(f"Resource ID {resource} is already used outside the skybox block")
    if count:
        return pattern.sub(block, existing)
    return existing.rstrip() + "\n" + block + "\n"


def extra_resource_text(existing: str) -> str:
    block = "\n".join(
        [EXTRA_BEGIN]
        + [f'{EXTRA_FIRST_RESOURCE + i} RCDATA "{sky["ntx"]}"' for i, sky in enumerate(EXTRA_SKIES)]
        + [EXTRA_END]
    )
    pattern = re.compile(re.escape(EXTRA_BEGIN) + r".*?" + re.escape(EXTRA_END), re.DOTALL)
    stripped, count = pattern.subn("", existing)
    for match in re.finditer(r"(?m)^\s*(\d+)\s+\w+\s+", stripped):
        resource = int(match.group(1))
        if EXTRA_FIRST_RESOURCE <= resource < EXTRA_FIRST_RESOURCE + len(EXTRA_SKIES):
            raise ValueError(f"Resource ID {resource} is already used outside the extra sky block")
    if count:
        return pattern.sub(block, existing)
    return existing.rstrip() + "\n" + block + "\n"


def provenance_text(source: Path, records: list[dict]) -> str:
    png_total = sum(record["bytes"] for record in records)
    ntx_total = sum(record["ntx_bytes"] for record in records)
    rows = [
        "# Brutal Skyboxes source assets",
        "",
        "Imported from the user's paid local Brutal Skyboxes v1.4.1 package:",
        f"`{source}`.",
        "",
        "Donor masters stay as original lossless 4096 x 1024 PNGs in this folder.",
        "The engine embeds neural `assets/ai/brutal/<stem>-n.ntx` packets that",
        "reconstruct 2048 x 512 panoramas through `texture-n2.ntm` (resource 1999),",
        "x4 or x2, lossy or lossless low-res, whichever is smallest and passes the",
        "quality gate in tools/encode_neural_textures.py; the same resolution as the",
        "legacy wasteland sky.",
        "No donor PNG is modified.",
        "Legacy resource 252 is encoded from its own master by tools/encode_neural_textures.py.",
        "No license text was present in the supplied extracted skybox directory; purchase",
        "and distribution rights remain governed by the original asset package terms.",
        "",
        f"Original PNG payload: {png_total:,} bytes ({png_total / 1024 / 1024:.2f} MiB).",
        f"Embedded neural payload: {ntx_total:,} bytes ({ntx_total / 1024 / 1024:.2f} MiB).",
        "Resource IDs 2800 through 2846 are stable and follow natural filename order.",
        "",
        "Rebuild registration with `python tools/library/register_skyboxes.py`.",
        "Verify it without changes with `python tools/library/register_skyboxes.py --check`.",
        "",
        "| Resource | Original filename | Neural packet | PNG bytes | NTX bytes | PNG SHA-256 |",
        "| --- | --- | --- | ---: | ---: | --- |",
    ]
    rows += [
        f'| {record["resource"]} | {record["stem"]}.png | {ntx_name(record["stem"])} | '
        f'{record["bytes"]} | {record["ntx_bytes"]} | {record["sha256"]} |'
        for record in records
    ]
    return "\n".join(rows) + "\n"


def write_text(path: Path, wanted: str, check: bool) -> None:
    actual = path.read_text(encoding="utf-8") if path.exists() else None
    if actual == wanted:
        return
    if check:
        raise ValueError(f"Generated registration is missing or stale: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(wanted)


def write_bytes(path: Path, wanted: bytes, check: bool) -> None:
    actual = path.read_bytes() if path.exists() else None
    if actual == wanted:
        return
    if check:
        raise ValueError(f"Neural sky packet is missing or stale: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as stream:
        stream.write(wanted)


def register(root: Path, source: Path, check: bool) -> dict:
    actual = {path.stem for path in source.glob("*.png")}
    if actual != set(STEMS):
        raise ValueError(
            f"Expected exactly the 47 paid skyboxes; missing={sorted(set(STEMS) - actual)}, "
            f"unexpected={sorted(actual - set(STEMS))}"
        )
    donors = root / "src/assets/Skyboxes/Brutal"
    neural = root / "src/assets/ai/brutal"
    records = []
    for index, stem in enumerate(STEMS):
        donor = source / f"{stem}.png"
        size = png_size(donor)
        if size != DONOR_SIZE:
            raise ValueError(f"Expected original {DONOR_SIZE} resolution, got {size}: {donor}")
        records.append({
            "stem": stem,
            "resource": FIRST_RESOURCE + index,
            "bytes": donor.stat().st_size,
            "sha256": sha256(donor),
        })
    rc_path = root / "src/assets.rc"
    rc = resource_text(rc_path.read_text(encoding="utf-8"))
    if not check:
        donors.mkdir(parents=True, exist_ok=True)
        neural.mkdir(parents=True, exist_ok=True)
    ntx_total = 0
    for record in records:
        donor = source / f'{record["stem"]}.png'
        imported = donors / donor.name
        equal = (
            imported.exists()
            and imported.stat().st_size == record["bytes"]
            and sha256(imported) == record["sha256"]
        )
        if not equal:
            if check:
                raise ValueError(f"Imported PNG is missing or differs from the original: {imported}")
            shutil.copyfile(donor, imported)
        if png_size(imported) != DONOR_SIZE or sha256(imported) != record["sha256"]:
            raise ValueError(f"Post-import verification failed: {imported}")
        packet = encode_neural_packet(imported)
        packet_path = neural / ntx_name(record["stem"])
        write_bytes(packet_path, packet, check)
        if not valid_sky_header(packet_path):
            raise ValueError(f"Neural sky header mismatch: {packet_path}")
        record["ntx_bytes"] = packet_path.stat().st_size
        ntx_total += record["ntx_bytes"]
    write_text(root / "src/world/SkyboxCatalog.h", catalog_text(), check)
    write_text(rc_path, rc, check)
    write_text(donors / "SOURCES.md", provenance_text(source, records), check)
    return {
        "mode": "check" if check else "register",
        "count": len(records),
        "donor_width": DONOR_SIZE[0],
        "donor_height": DONOR_SIZE[1],
        "embed_width": EMBED_SIZE[0],
        "embed_height": EMBED_SIZE[1],
        "png_bytes": sum(record["bytes"] for record in records),
        "ntx_bytes": ntx_total,
        "resources": [FIRST_RESOURCE, FIRST_RESOURCE + len(records) - 1],
        "verified_sha256": len(records),
        "neural_model": NEURAL2_MODEL_RESOURCE,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=None)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    source = args.source.resolve() if args.source else None
    if source is None:
        donors = root / "src/assets/Skyboxes/Brutal"
        source = donors if donors.exists() and any(donors.glob("skybox_*.png")) else DEFAULT_SOURCE.resolve()
    print(json.dumps(register(root, source, args.check), sort_keys=True))


if __name__ == "__main__":
    main()
