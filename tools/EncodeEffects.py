"""Encode the PCM effect masters as RMS1 effect resources and point assets.rc at them.

RMS1 is a 12-byte header (magic, exact PCM frame count, decoder lead-in frames)
followed by an MP3 encoded with LAME VBR q2 and no Xing frame. The engine trims
the lead-in and the tail, so onsets and loop points match the WAV master to the
sample. The WAV files stay in the tree as masters.

Usage: python tools/EncodeEffects.py
"""
import argparse
import re
import struct
import subprocess
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RC = ROOT/'src'/'assets.rc'
# LAME's 576-frame encoder delay plus the 529-frame MP3 decoder delay. With no
# Xing frame this is exact for the engine's decoder.
LEAD = 1105
CODEC = ['-c:a', 'libmp3lame', '-q:a', '2', '-write_xing', '0']


def encode(wav, out):
    if struct.unpack_from('<H', wav.read_bytes(), 20)[0] != 1:
        return None  # already compressed; the engine decodes it as it is
    with wave.open(str(wav)) as w:
        frames = w.getnframes()
        if w.getsampwidth() != 2 or w.getframerate() != 44100:
            raise ValueError(f'{wav} is not 16-bit 44.1 kHz')
    mp3 = subprocess.run(['ffmpeg', '-v', 'error', '-i', str(wav), *CODEC, '-f', 'mp3', '-'],
                         capture_output=True, check=True).stdout
    out.write_bytes(struct.pack('<4sII', b'RMS1', frames, LEAD) + mp3)
    return frames, len(mp3)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    text = RC.read_text()
    pattern = re.compile(r'^(\d+) RCDATA "(assets/[^"]+)\.(wav|sfx)"$', re.M)
    encoded = set()
    for match in pattern.finditer(text):
        stem = match[2]
        wav = ROOT/'src'/(stem+'.wav')
        result = encode(wav, ROOT/'src'/(stem+'.sfx'))
        if result:
            print(f'{match[1]} {stem}: {result[0]} frames, {wav.stat().st_size} -> {result[1]+12} bytes')
            encoded.add(stem)
    text = pattern.sub(lambda m: f'{m[1]} RCDATA "{m[2]}.{"sfx" if m[2] in encoded else m[3]}"', text)
    count = len(encoded)
    with open(RC, 'w', newline='') as file:
        file.write(text)
    print(f'{count} effects encoded')


if __name__ == '__main__':
    main()
