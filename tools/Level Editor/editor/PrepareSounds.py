"""Cut the editor's building sounds from the purchased Complex pack.

The clips, offsets, rates and loudness are the ones Creating Constructs uses
(N:/Finished/Monolith maker/tools/prepare_audio.py), mastered the same way:
highpass 55 Hz, lowpass, loudnorm to the target LUFS, 7 ms fades and a 0.251
peak ceiling. The output is plain 16-bit mono WAV in .build/sounds, which is
build output and never committed: purchased audio stays out of git.
"""
from pathlib import Path
import subprocess
import sys

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path(r'O:\retro official\Complex')
OUT = ROOT / '.build' / 'sounds'
# name, path in the pack, offset, duration, sample rate, LUFS
CLIPS = [
    ('placeStone', 'SFX/Movement/Footsteps/Asphalt/FS-Asphalt_1.wav', 0, .45, 16000, -23),
    ('placeWood', 'SFX/Movement/Footsteps/Wood/FS-Wood_1.wav', 0, .45, 16000, -23),
    ('placeMetal', 'SFX/Movement/Footsteps/Stairs/Metal/MetalStairs_FS1.wav', 0, .45, 16000, -24),
    ('breakStone', 'SFX/Cutscenes/StatueHeadFall_Ending_2.wav', 0, .65, 16000, -24),
    ('breakWood', 'SFX/LadderStepOn.wav', 0, .65, 16000, -24),
    ('breakMetal', 'SFX/Cutscenes/MetalClash.wav', 0, .8, 16000, -26),
    ('click', 'SFX/UI/UI_Click.wav', 0, .22, 16000, -26),
    ('hover', 'SFX/UI/UI_Hover.wav', 0, .14, 16000, -30),
    ('back', 'SFX/UI/UI_Back.wav', 0, .26, 16000, -26),
    ('slider', 'SFX/UI/UI_Slider.wav', 0, .12, 16000, -30),
    ('musicKey', 'Music/BassPluckLoop.wav', 0, .85, 16000, -26),
]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, rel, offset, duration, rate, lufs in CLIPS:
        source = SOURCE / rel
        if not source.exists():
            sys.exit(f'Missing purchased clip: {source}')
        args = ['ffmpeg', '-v', 'error', '-ss', str(offset), '-i', str(source), '-t', str(duration),
                '-vn', '-af', f'highpass=f=55,lowpass=f={min(rate * .44, 6800)},loudnorm=I={lufs}:TP=-9:LRA=9',
                '-ac', '1', '-ar', str(rate), '-f', 'f32le', '-']
        samples = np.frombuffer(subprocess.check_output(args), dtype='<f4').copy()
        n = min(int(.007 * rate), len(samples) // 4)
        samples[:n] *= np.linspace(0, 1, n)
        samples[-n:] *= np.linspace(1, 0, n)
        samples *= min(1., .251 / max(1e-9, float(np.max(np.abs(samples)))))
        dest = OUT / (name + '.wav')
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'f32le', '-ar', str(rate), '-ac', '1', '-i', '-',
                        '-c:a', 'pcm_s16le', str(dest)], input=samples.astype('<f4').tobytes(), check=True)
        print(f'{name}: {len(samples) / rate:.2f} s -> {dest}')


if __name__ == '__main__':
    main()
