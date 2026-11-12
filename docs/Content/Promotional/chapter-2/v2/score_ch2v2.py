"""Chapter 2 promo v2: the score edit and sound design, mixed with numpy to 48 kHz stereo.

  python score_ch2v2.py OUT.wav          (also prints the cue sheet and writes cues.txt)

Bed: "What Eyes Can't See" (O:/retro official/Audio/Tracks/ost_what_eyes_cant_see_mx_1_full.ogg),
17.65 s to 67.62 s, so its drop (44.65 s in the track) lands on the trailer's drop (27.0 s, frame 810)
and its pulse (0.6965 s) runs under the cuts; cut dead at the hard stop (49.967 s). Hits and creature
sounds are aligned by their loudest sample to the cut they belong to. Purchased packs only; nothing
here leaves this machine.
"""
import os, subprocess, sys
import numpy as np

SR = 48000
A = 'O:/retro official/Audio/'
TRACK = A + 'Tracks/ost_what_eyes_cant_see_mx_1_full.ogg'
GAME = 'N:/rawmetal/src/assets/audio/'
FPS = 30
DROP, STOP, TITLE, ENDCARD, BUTTON, END = 27.0, 1499 / FPS, 1530 / FPS, 1650 / FPS, 1710 / FPS, 1785 / FPS
BEAT = .6965
TRACK_IN = 44.65 - DROP

_cache = {}
def load(path):
    if path not in _cache:
        raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-ac', '2', '-ar', str(SR), '-f', 'f32le', '-'], capture_output=True).stdout
        a = np.frombuffer(raw, np.float32).reshape(-1, 2).copy()
        if not len(a): raise SystemExit('cannot read ' + path)
        _cache[path] = a
    return _cache[path]

N = int(END * SR) + SR
music = np.zeros((N, 2), np.float32)
sfx = np.zeros((N, 2), np.float32)
tail = np.zeros((N, 2), np.float32)   # what plays after the stop (the title and the end)
CUES = []

def db(v): return 10 ** (v / 20)

def place(bus, path, t, gain=0., peak=False, start=0., length=None, fin=0., fout=0., pan=0., what=''):
    a = load(path)
    a = a[int(start * SR):]
    if length is not None: a = a[:int(length * SR)]
    a = a * db(gain)
    if fin: a[:int(fin * SR)] *= np.linspace(0, 1, int(fin * SR))[:len(a), None]
    if fout:
        n = min(len(a), int(fout * SR)); a[len(a) - n:] *= np.linspace(1, 0, n)[:, None]
    if pan: a = a * np.array([1 - max(0, pan), 1 + min(0, pan)], np.float32)
    off = t
    if peak: off = t - np.argmax(np.abs(a).sum(1)) / SR
    i = int(round(off * SR))
    j0 = max(0, -i); i = max(0, i)
    n = min(len(a) - j0, N - i)
    if n > 0: bus[i:i + n] += a[j0:j0 + n]
    CUES.append((t, what or os.path.basename(path), gain, 'peak' if peak else 'start'))

# ---- the bed: "What Eyes Can't See" ------------------------------------------------------------------
place(music, TRACK, 0., -1., start=TRACK_IN, length=STOP + .2, fin=3.0, what='MUSIC in at 17.65 s ("What Eyes Can\'t See")')
CUES.append((DROP, 'MUSIC drop (track 44.65 s)', 0, '-'))
place(music, A + 'OGG/Pure Horror/One-shots/drone_long_ph_1.ogg', 0., -9., fin=2.5, length=DROP - .05, fout=1.5, what='drone_long_ph_1 (cold open drone)')

# ---- act 1: the levels as they were left -------------------------------------------------------------
S = sfx
place(S, A + 'Sound Effects/Machines/old_machine_mx_1_loop.wav', 3.0, -24., length=3.5, fin=.6, fout=.8, what='projector hum (theatre)')
place(S, A + 'OGG/Impacts/impact_distant_3.ogg', 6.5, -12., peak=True, what='impact_distant_3 (morgue cut)')
place(S, A + 'Sound Effects/Machines/retro_computer_calculating_mx_1_loop.wav', 10.0, -26., length=3.0, fin=.4, fout=.6, what='lab computers (test labs)')
place(S, A + 'OGG/static_loop_1.ogg', 13.0, -28., length=3.0, fin=.3, fout=.8, what='radio static (red hall)')
place(S, A + 'OGG/containment_breach_risk_smx_1.ogg', 13.6, -9., what='PA: "containment breach risk"')
place(S, A + 'OGG/Impacts/impact_distant_5.ogg', 16.0, -12., peak=True, what='impact_distant_5 (the rock)')
place(S, A + 'OGG/Pure Sci-Fi/sci-fi_frequencies_1.ogg', 19.0, -24., length=2.6, fin=.5, fout=1.0, what='crystal hum (the breach)')
place(S, A + 'OGG/Creatures/Monster/monster_talking_1.ogg', 21.8, -15., what='R-117 behind the bars')
place(S, A + 'Sound Effects/Entities/unknown_entity_distant_mx_2.ogg', 24.2, -15., what='something in the hall')
place(S, A + 'Sound Effects/Cinematic vol. 1/sfx_swell_mx_1.ogg', DROP - .05, -9., peak=True, what='riser into the drop')

# ---- the drop: the kill in 96 ------------------------------------------------------------------------
def f(n): return n / FPS
def b(n): return round(810 + BEAT * FPS * n) / FPS
place(S, A + 'Sound Effects/Cinematic vol. 1/sfx_impact_mx_1.ogg', DROP, -3., peak=True, what='HIT: drop / it rears (wake)')
place(S, A + 'OGG/Creatures/Monster/monster_roar_3.ogg', DROP + .35, -5., peak=True, what='roar (wake)')
place(S, A + 'Sound Effects/Entities/dog_hybrid_mx_1.ogg', b(2) + .3, -9., peak=True, what='snarl (behind the post man)')
place(S, A + 'Sound Effects/Impact & Break/Body/impact_body_ufx_1.ogg', b(3), -5., peak=True, what='HIT: pounce')
place(S, A + 'OGG/Creatures/Monster/monster_roar_5.ogg', b(3) + .05, -6., peak=True, what='roar (pounce)')
place(S, A + 'Sound Effects/Impact & Break/Body/impact_body_ufx_2.ogg', b(4) + f(8), -2., peak=True, what='HIT: the bite (head off, flash)')
place(S, A + 'Sound Effects/Cinematic vol. 1/sfx_impact_mx_4.ogg', b(4) + f(8), -6., peak=True, what='boom under the bite')
place(S, A + 'OGG/Creatures/Monster/monster_talking_3.ogg', b(5) + .2, -11., what='feeding')
place(S, A + 'Sound Effects/Cinematic vol. 1/sfx_impact_mx_13.ogg', b(9), -5., peak=True, what='HIT: blood wipe into the rush (103)')
place(S, A + 'OGG/Creatures/Monster/monster_roar_7.ogg', b(12) - .15, -4., peak=True, what='roar (the rush reaches us)')
place(S, A + 'Sound Effects/Cinematic vol. 1/sfx_impact_mx_16.ogg', b(12), -4., peak=True, what='HIT: shake on the rush')
place(S, A + 'OGG/Creatures/Monster/monster_roar_distant_2.ogg', b(13) + .2, -13., what='distant roar (cages)')
place(S, A + 'OGG/Creatures/Monster/monster_roar_2.ogg', b(17) + .05, -4., peak=True, what='roar (it is over us, 101)')
place(S, A + 'Sound Effects/Impact & Break/Body/impact_body_ufx_1.ogg', b(17), -6., peak=True, what='HIT: pounce at the floor camera')
place(S, A + 'Sound Effects/Misc/electric_shock_mx_1.ogg', b(18), -4., peak=True, what='HIT: the Machine on live current')
place(S, A + 'Sound Effects/Misc/electric_shock_mx_1.ogg', b(19) + .05, -9., peak=True, pan=.3, what='current, second arc')
place(S, A + 'OGG/Impacts/impact_distant_8.ogg', b(20), -10., peak=True, what='hit (the breach)')
hits = ['sfx_impact_mx_5', 'sfx_impact_mx_19', 'sfx_impact_mx_6', 'sfx_impact_mx_20', 'sfx_impact_mx_21', 'sfx_impact_mx_9', 'sfx_impact_mx_5', 'sfx_impact_mx_19']
for i in range(0, 16, 2):
    t = int(round(round(810 + BEAT * FPS * 21) + BEAT * FPS / 2 * i)) / FPS   # the compose script's run cuts, to the frame
    place(S, A + 'Sound Effects/Cinematic vol. 1/%s.ogg' % hits[i // 2], t, -11. + i * .4, peak=True, what='HIT: run cut %d' % (i + 1))
place(S, A + 'OGG/Creatures/Monster/monster_roar_6.ogg', STOP - .7, -4., peak=True, what='roar into the last pounce')
place(S, A + 'Sound Effects/Cinematic vol. 1/sfx_impact_mx_1.ogg', f(1478), -3., peak=True, what='HIT: last pounce at the camera')
CUES.append((STOP, 'HARD STOP: silence', 0, '-'))

# ---- the title, the button, the end --------------------------------------------------------------------
T = tail
place(T, A + 'Sound Effects/Cinematic vol. 1/sfx_stinger_mx_2.ogg', TITLE, -2., peak=True, what='STING: title')
place(T, A + 'Sound Effects/Cinematic vol. 1/sfx_gong_mx_1.ogg', TITLE, -9., peak=True, what='gong under the title')
place(T, A + 'OGG/Impacts/impact_distant_8.ogg', f(1600), -15., peak=True, what='COMING SOON')
place(T, A + 'OGG/Impacts/impact_distant_2.ogg', ENDCARD, -17., peak=True, what='end card: CHAPTER 1 line')
place(T, A + 'OGG/Pure Horror/One-shots/drone_ph_2.ogg', ENDCARD - 1.0, -15., fin=.8, length=END - ENDCARD + 1.0, fout=2.0, what='end drone')
place(T, GAME + 'machine.wav', BUTTON, -11., fin=.2, length=1.6, fout=.4, what='the Machine (game sound)')
place(T, A + 'OGG/Pure Horror/One-shots/stinger_ph_1.ogg', BUTTON + .35, -10., peak=True, what='the lamp-eye in the dark')
place(T, A + 'Sound Effects/Misc/electric_shock_mx_1.ogg', f(1739), -6., peak=True, what='STING: the floor goes live, the Machine seen')
place(T, A + 'Sound Effects/Cinematic vol. 1/sfx_impact_mx_16.ogg', f(1758), -8., peak=True, what='HIT: cut to black')

# ---- master ---------------------------------------------------------------------------------------------
mix = music * db(-2.) + sfx
i0, i1 = int(STOP * SR), int(TITLE * SR) - int(.02 * SR)
ramp = int(.012 * SR)
mix[i0:i0 + ramp] *= np.linspace(1, 0, ramp)[:, None]
mix[i0 + ramp:] = 0
mix += tail
mix = mix[:int(END * SR)]
mix[-int(.05 * SR):] *= np.linspace(1, 0, int(.05 * SR))[:, None]

def lufs(a):
    p = 'N:/rawmetal/.build/promo-1.4/audio/_meas.wav'
    write_wav(p, a)
    r = subprocess.run(['ffmpeg', '-hide_banner', '-nostats', '-i', p, '-af', 'ebur128=peak=true', '-f', 'null', '-'], capture_output=True, text=True).stderr
    I = float(r.rsplit('I:', 1)[1].split('LUFS')[0]); tp = float(r.rsplit('Peak:', 1)[1].split('dBFS')[0])
    return I, tp

def write_wav(p, a):
    import wave
    x = (np.clip(a, -1, 1) * 32767).astype('<i2')
    with wave.open(p, 'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes(x.tobytes())

def limit(a, ceil=db(-1.5), look=int(.004 * SR), release=int(.08 * SR)):
    """A look-ahead peak limiter: gain follows the loudest coming sample, recovers slowly."""
    pk = np.abs(a).max(1)
    need = np.minimum(1, ceil / np.maximum(pk, 1e-9))
    # look ahead: the minimum over the next `look` samples
    from numpy.lib.stride_tricks import sliding_window_view
    padded = np.concatenate([need, np.ones(look)])
    ahead = sliding_window_view(padded, look + 1).min(1)[:len(need)]
    g = np.empty_like(ahead); cur = 1.0; k = 1 - 1 / release
    for i in range(len(ahead)):
        cur = min(ahead[i], cur * k + (1 - k))
        g[i] = cur
    return a * g[:, None]

if __name__ == '__main__':
    out = sys.argv[1]
    I, tp = lufs(mix)
    mix = mix * db(-14 - I)
    mix = limit(mix)
    for _ in range(2):
        I, tp = lufs(mix)
        if abs(I + 14) < .3: break
        mix = limit(mix * db(-14 - I))
    write_wav(out, mix)
    I, tp = lufs(mix)
    print('integrated %.1f LUFS, true peak %.1f dBFS, %.2f s' % (I, tp, len(mix) / SR))
    sil = np.abs(mix[int((STOP + .03) * SR):int((TITLE - .03) * SR)]).max()
    print('stop-to-title silence peak', sil)
    with open(os.path.join(os.path.dirname(os.path.abspath(out)), 'cues.txt'), 'w') as fh:
        for t, what, g, how in sorted(CUES):
            line = '%6.2f s  f%04d  %-55s %5.1f dB  %s' % (t, round(t * FPS), what, g, how)
            print(line); fh.write(line + '\n')
