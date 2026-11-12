"""Chapter 2's Research area score (resource 2412, assets/library/audio/music_research.mp3).

The first Research score was a plucked music-box arpeggio. The user, 2026-11-12: "the music box
is inappropriate". The research levels now sit on a low, unbroken bed from the purchased
Ambiences pack ("The Other Side"): no tune to follow, nothing that resolves, a plant working
under a strain it was not built for. Levelled under the old track so the rooms' own sound and
the silences between scores carry more.

  python tools/audio/chapter2_scores.py [PACK_AUDIO_FOLDER]
"""
import subprocess, sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'src'/'assets'/'library'/'audio'
PACK = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('O:/retro official/Audio')
RATE = 44100

SCORES = {
    # out name: (source under the pack folder, dBFS RMS)
    'music_research': ('Ambiences/ambience_the_other_side_mx_1_loop.ogg', -21.0),
}


def decode(path):
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', str(path), '-ac', '2', '-ar', str(RATE), '-f', 'f32le', '-'],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, np.float32).reshape(-1, 2).copy()


def main():
    for name, (member, rms_db) in SCORES.items():
        pcm = decode(PACK/member)
        rms = float(np.sqrt((pcm**2).mean()))
        gain = min(10**(rms_db/20)/max(rms, 1e-6), 0.89/max(float(np.abs(pcm).max()), 1e-6))
        pcm = np.clip(pcm*gain, -1, 1)
        out = OUT/f'{name}.mp3'
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'f32le', '-ar', str(RATE), '-ac', '2', '-i', '-',
                        '-c:a', 'libmp3lame', '-q:a', '3', str(out)], input=pcm.astype(np.float32).tobytes(), check=True)
        print(f'{out.name}: {len(pcm)/RATE:.1f} s, rms {20*np.log10(np.sqrt((pcm**2).mean())):.1f} dBFS, '
              f'peak {20*np.log10(np.abs(pcm).max()):.1f} dBFS, {out.stat().st_size} bytes')


if __name__ == '__main__':
    main()
