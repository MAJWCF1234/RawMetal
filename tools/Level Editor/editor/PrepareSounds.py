"""Make the Campaign Builder's interface sounds: soft, warm and short.

Every clip is synthesized here with numpy (no purchased audio, no samples):
wooden taps, small marimba and kalimba notes, a felt knock and a low soft
tone for a refusal. Nothing is a game cue, nothing is louder than a quiet
click, nothing sits in the sub-bass, and nothing rings longer than a second.
The output is 22.05 kHz 16-bit mono WAV in tools/Level Editor/.build/sounds,
which the editor reads at start (sfx::load in NativeEditor.cpp).

  python "tools/Level Editor/editor/PrepareSounds.py"          write the set
  python "tools/Level Editor/editor/PrepareSounds.py" --check  write, then prove
      every clip the editor names exists, is short, quiet, soft at the start
      and has no low rumble (the old room sting was a 31 Hz bass drop).
"""
from pathlib import Path
import re
import sys
import wave

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / '.build' / 'sounds'
RATE = 22050
PEAK = .2  # every clip's ceiling; the editor scales again by about 0.7


def note(name):
    """'C5' -> Hz (A4 = 440)."""
    steps = {'C': -9, 'D': -7, 'E': -5, 'F': -4, 'G': -2, 'A': 0, 'B': 2}
    semis = steps[name[0]] + (1 if '#' in name else 0) + 12 * (int(name[-1]) - 4)
    return 440. * 2 ** (semis / 12)


def t_for(seconds):
    return np.arange(int(seconds * RATE)) / RATE


def smooth(x, cutoff):
    """One-pole lowpass: rounds the edges off a tap."""
    a = np.exp(-2 * np.pi * cutoff / RATE)
    y = np.empty_like(x)
    acc = 0.
    for i, v in enumerate(x):
        acc = (1 - a) * v + a * acc
        y[i] = acc
    return y


def envelope(t, attack, decay):
    rise = np.clip(t / max(attack, 1e-4), 0, 1)
    rise = .5 - .5 * np.cos(np.pi * rise)  # a raised-cosine onset: no click
    return rise * np.exp(-t / decay)


def modal(freq, partials, seconds, attack=.002):
    """A struck thing: (ratio, level, decay seconds) partials over freq."""
    t = t_for(seconds)
    out = np.zeros_like(t)
    for ratio, level, decay in partials:
        out += level * np.sin(2 * np.pi * freq * ratio * t) * envelope(t, attack, decay)
    return out


def tap_noise(seconds, cutoff, decay, level, seed):
    t = t_for(seconds)
    rng = np.random.default_rng(seed)
    return smooth(rng.standard_normal(len(t)), cutoff) * np.exp(-t / decay) * level


def wood(freq, seconds=.16, seed=1):
    """A wooden block: a hollow knock, two inharmonic modes and a breath of contact."""
    body = modal(freq, [(1, 1., .045), (2.57, .32, .02), (4.1, .08, .01)], seconds, .0015)
    return body + tap_noise(seconds, 2500, .004, .25, seed)


def felt(freq, seconds=.2, seed=2):
    """A felt mallet on a wooden box: rounder and lower than a wood block."""
    body = modal(freq, [(1, 1., .07), (2.0, .18, .03), (3.0, .05, .015)], seconds, .004)
    return smooth(body + tap_noise(seconds, 900, .006, .18, seed), 2400)


def marimba(freq, seconds=.45):
    return modal(freq, [(1, 1., .22), (4., .22, .05), (9.9, .04, .012)], seconds, .003)


def kalimba(freq, seconds=.7, seed=3):
    body = modal(freq, [(1, 1., .32), (5.9, .14, .04), (2.0, .05, .12)], seconds, .002)
    return body + tap_noise(seconds, 3500, .003, .08, seed)


def bell(freq, seconds=.42):
    """A small soft bell, struck lightly: a tinkle, not a clang."""
    return smooth(modal(freq, [(1, 1., .16), (2.76, .2, .05), (5.4, .05, .02)], seconds, .004), 5000)


def seq(*parts):
    """Notes laid out in time: (start seconds, clip, level)."""
    end = max(int(start * RATE) + len(clip) for start, clip, _ in parts)
    out = np.zeros(end)
    for start, clip, level in parts:
        at = int(start * RATE)
        out[at:at + len(clip)] += clip * level
    return out


def low_tone(seconds=.34):
    """A refusal: a soft low hum that dips a little, like a polite 'hm'."""
    t = t_for(seconds)
    f = 233. - 30. * np.clip(t / seconds, 0, 1)
    phase = 2 * np.pi * np.cumsum(f) / RATE
    tone = np.sin(phase) + .22 * np.sin(2 * phase) + .05 * np.sin(3 * phase)
    return smooth(tone * envelope(t, .03, .11), 1800)


def clips():
    c5, d5, e5, g5, g4 = note('C5'), note('D5'), note('E5'), note('G5'), note('G4')
    return {
        # Something set down: a felt knock (floors, blocks, rooms), a wood block (furniture,
        # people, props), a small bell (lamps, pipes, machines).
        'placeStone': felt(196.),
        'placeWood': wood(523.),
        'placeMetal': bell(1047.) * .6,
        # Something taken away: the same voice, two light taps falling away, quieter.
        'breakStone': seq((0, felt(220., .16, 4), .8), (.07, felt(165., .2, 5), .55)),
        'breakWood': seq((0, wood(587., .12, 6), .7), (.065, wood(440., .16, 7), .5)),
        'breakMetal': seq((0, bell(1175., .3), .45), (.08, bell(880., .38), .35)),
        # The interface: tiny ticks.
        'click': wood(1320., .07, 8) * .55,
        'hover': wood(1760., .045, 9) * .25,
        'slider': wood(990., .04, 10) * .3,
        # Undo steps back down, redo steps up again.
        'back': seq((0, marimba(g5, .3), .6), (.075, marimba(d5, .35), .55)),
        'redo': seq((0, marimba(d5, .3), .55), (.075, marimba(g5, .35), .6)),
        # Saved: two kalimba notes, a fifth apart. A finished room: three, rising.
        'save': seq((0, kalimba(c5, .55), .7), (.11, kalimba(g5, .6), .55)),
        'celebrate': seq((0, kalimba(c5, .5), .55), (.1, kalimba(e5, .5), .5), (.2, kalimba(g5, .62), .55)),
        # Walk in (F5): a soft lift.
        'play': seq((0, marimba(g4, .35), .5), (.09, marimba(c5, .45), .55)),
        # Refused: a low soft tone, never a sting.
        'error': low_tone(),
    }


# How loud each clip is against the ceiling: the ticks you hear constantly are the quietest.
LEVEL = {'hover': .3, 'slider': .4, 'click': .55, 'breakStone': .7, 'breakWood': .7, 'breakMetal': .6,
         'placeMetal': .75, 'back': .7, 'redo': .7, 'play': .7, 'error': .75}


def master(x, level=1.):
    n = min(int(.004 * RATE), len(x) // 4)
    x = x.copy()
    x[:n] *= np.linspace(0, 1, n)
    m = min(int(.012 * RATE), len(x) // 3)
    x[-m:] *= np.linspace(1, 0, m)
    return x * (PEAK * level / max(1e-9, float(np.max(np.abs(x)))))


def write(path, x):
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1), w.setsampwidth(2), w.setframerate(RATE)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype('<i2').tobytes())


def read(path):
    with wave.open(str(path), 'rb') as w:
        return np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(float) / 32768, w.getframerate()


def editor_names():
    """The clip names NativeEditor.cpp loads (sfx::names)."""
    text = (ROOT / 'editor' / 'NativeEditor.cpp').read_text(encoding='utf-8')
    block = re.search(r'names\[\]\s*=\s*\{(.*?)\};', text, re.S)
    return re.findall(r'"(\w+)"', block.group(1)) if block else []


def check():
    problems = []
    names = editor_names()
    if not names:
        problems.append('could not read sfx::names from NativeEditor.cpp')
    for name in names:
        path = OUT / (name + '.wav')
        if not path.exists():
            problems.append(f'{name}: the editor names it but it was not made')
            continue
        x, rate = read(path)
        seconds = len(x) / rate
        peak = float(np.max(np.abs(x)))
        spectrum = np.abs(np.fft.rfft(x)) ** 2
        freqs = np.fft.rfftfreq(len(x), 1 / rate)
        low = float(spectrum[freqs < 120].sum() / max(1e-12, spectrum.sum()))
        onset = float(np.max(np.abs(x[:int(.001 * rate)])))
        line = f'{name:11s} {seconds:.2f} s  peak {peak:.3f}  below 120 Hz {low * 100:4.1f}%'
        print(line)
        if seconds > 1.:
            problems.append(f'{name}: {seconds:.2f} s is too long for an interface sound')
        if peak > .25:
            problems.append(f'{name}: peak {peak:.3f} is louder than the set allows')
        if low > .05:
            problems.append(f'{name}: {low * 100:.1f}% of its energy is low rumble')
        if onset > .5 * peak:
            problems.append(f'{name}: starts with a hard click')
    for p in problems:
        print('FAIL', p)
    print('PASS: the editor sounds are soft, short and quiet' if not problems else f'FAIL: {len(problems)} problems')
    return not problems


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    made = clips()
    missing = [n for n in editor_names() if n not in made]
    if missing:
        sys.exit(f'NativeEditor.cpp names clips this script does not make: {missing}')
    for name, x in made.items():
        write(OUT / (name + '.wav'), master(x, LEVEL.get(name, 1.)))
    for old in OUT.glob('*.wav'):  # the purchased clips this set replaces (musicKey was the room sting)
        if old.stem not in made:
            old.unlink()
    print(f'{len(made)} editor sounds -> {OUT}')
    if '--check' in sys.argv[1:]:
        sys.exit(0 if check() else 1)


if __name__ == '__main__':
    main()
