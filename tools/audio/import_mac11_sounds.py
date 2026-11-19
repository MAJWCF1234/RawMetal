"""The MAC-11's sounds and the spent cases' clinks, from the purchased packs.

Reads the packs in place (nothing is unpacked beside them) and writes, for
each entry of SOUNDS, a mono 16-bit 44.1 kHz master src/assets/audio/<name>.wav
(trimmed, peak-levelled, a short fade out) and its RMS1 effect <name>.sfx
(tools/EncodeEffects.py). assets.rc names the .sfx files (resources 3346 on,
in the order of src/audio/Sound.h's MacShot block).

  python tools/audio/import_mac11_sounds.py
"""
import array
import subprocess
import sys
import wave
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
import EncodeEffects  # noqa: E402

PACKS = Path('O:/retro official/Audio')
FIREARMS = PACKS/'Sound Effects'/'Firearms'
HORROR = PACKS/'Horror SFX Complete (ogg files).zip'
OUT = ROOT/'src'/'assets'/'audio'

# (name, source (a path, or 'zip:' a member of HORROR), start s, length s, peak 0-1, fade s)
SOUNDS = [
    ('mac11-shot',    FIREARMS/'Fire'/'smg_fire_ufx_1.ogg',                    0.035, 0.34, 0.95, 0.12),
    ('mac11-dry',     FIREARMS/'Misc'/'smg_universal_empty_barrel_ufx_1.ogg',  0.08,  0.25, 0.70, 0.08),
    ('mac11-mag-out', FIREARMS/'Reloading'/'smg_reloading_mag_out_ufx_2.ogg',  0.10,  0.50, 0.70, 0.10),
    ('mac11-mag-in',  FIREARMS/'Reloading'/'smg_reloading_mag_in_ufx_1.ogg',   0.14,  0.40, 0.75, 0.10),
    ('mac11-rack',    FIREARMS/'Racking'/'smg_racking_ufx_1.ogg',              0.10,  0.50, 0.80, 0.10),
    ('casing-brass',  'zip:Horror SFX Complete OGG/Guns/Gun_shell_hitting_ground.ogg',         0.02, 0.32, 0.45, 0.10),
    ('casing-shell',  'zip:Horror SFX Complete OGG/Guns/Gun_shotgun_shell_hitting_ground.ogg', 0.02, 0.40, 0.45, 0.12),
]


def decode(source):
    if isinstance(source, str) and source.startswith('zip:'):
        data = zipfile.ZipFile(HORROR).read(source[4:])
        args = ['ffmpeg', '-v', 'error', '-i', '-', '-ac', '1', '-ar', '44100', '-f', 's16le', '-']
        raw = subprocess.run(args, input=data, capture_output=True, check=True).stdout
    else:
        args = ['ffmpeg', '-v', 'error', '-i', str(source), '-ac', '1', '-ar', '44100', '-f', 's16le', '-']
        raw = subprocess.run(args, capture_output=True, check=True).stdout
    samples = array.array('h'); samples.frombytes(raw)
    return samples


def main():
    for name, source, start, length, peak, fade in SOUNDS:
        s = decode(source)
        a, n = int(start*44100), int(length*44100)
        clip = [float(x) for x in s[a:a+n]]
        top = max(1.0, max(abs(x) for x in clip))
        gain = peak*32767/top
        f = int(fade*44100)
        out = array.array('h')
        for i, x in enumerate(clip):
            k = gain
            if i < 44: k *= i/44.0                       # a 1 ms fade in (no click at the cut)
            if i > len(clip)-f: k *= max(0.0, (len(clip)-i)/f)
            out.append(int(max(-32767, min(32767, x*k))))
        wav = OUT/(name+'.wav')
        with wave.open(str(wav), 'wb') as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100); w.writeframes(out.tobytes())
        frames, size = EncodeEffects.encode(wav, OUT/(name+'.sfx'))
        print(f'{name}: {frames} frames, {wav.stat().st_size} -> {size+12} bytes')


if __name__ == '__main__':
    main()
