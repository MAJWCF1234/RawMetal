"""Chapter 2 promo v3: the score and sound, mixed with numpy at 48 kHz stereo, placed from compose_v3's edit.

  python score_v3.py OUT.wav        (writes cues.txt beside OUT: every cue, its frame and the cut it lands on)

Act 1: "No Light Returns" (its opening section, Tracks/Split) under a Pure Horror drone, with room tone per shot.
Act 2: "Storm - dark - heavy - lurking" from its top, the cuts on its 1.02 s pulse, the game's own sounds under it
(each gameplay take logs its sound events per frame: film-sounds.txt, mapped to the game's audio files).
Act 3: silence and a line, then "What Eyes Can't See" placed so its drop (0:44.65) lands on the kill's first cut,
cut dead at the hard stop. Title sting, the Machine's tease, the end drone. Purchased packs and the game's own audio.
"""
import os, re, subprocess, sys, wave
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import compose_v3 as C

SR = 48000
FPS = C.FPS
A = 'O:/retro official/Audio/'
GAME = 'N:/rawmetal/src/assets/audio/'
M = {k: v / FPS for k, v in C.MARK.items()}
END = C.END / FPS
N = int(END * SR) + SR
music = np.zeros((N, 2), np.float32); sfx = np.zeros((N, 2), np.float32); game = np.zeros((N, 2), np.float32); tail = np.zeros((N, 2), np.float32)
CUES = []
_cache = {}

def db(v): return 10 ** (v / 20)

def load(p):
    if p not in _cache:
        raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', p, '-ac', '2', '-ar', str(SR), '-f', 'f32le', '-'], capture_output=True).stdout
        a = np.frombuffer(raw, np.float32).reshape(-1, 2).copy()
        if not len(a): raise SystemExit('cannot read ' + p)
        _cache[p] = a
    return _cache[p]

def place(bus, p, t, gain=0., peak=False, start=0., length=None, fin=0., fout=0., pan=0., what='', log=True, rate=1.0):
    a = load(p)[int(start * SR):]
    if length is not None: a = a[:int(length * SR)]
    if rate != 1.0:
        idx = np.arange(0, len(a) - 1, rate); a = a[idx.astype(int)]
    a = a * db(gain)
    if fin: n = min(len(a), int(fin * SR)); a[:n] *= np.linspace(0, 1, n)[:, None]
    if fout: n = min(len(a), int(fout * SR)); a[len(a) - n:] *= np.linspace(1, 0, n)[:, None]
    if pan: a = a * np.array([1 - max(0, pan), 1 + min(0, pan)], np.float32)
    off = t - (np.argmax(np.abs(a).sum(1)) / SR if peak else 0)
    i = int(round(off * SR)); j0 = max(0, -i); i = max(0, i); n = min(len(a) - j0, N - i)
    if n > 0: bus[i:i + n] += a[j0:j0 + n]
    if log: CUES.append((t, what or os.path.basename(p), gain, 'peak' if peak else 'start'))

def seg_start(src, first=None):
    for a, z, kind, s, f0, o in C.EDIT:
        if s == src and (first is None or f0 == first): return a / FPS, z / FPS
    raise KeyError(src)

# ---- ACT 1 -------------------------------------------------------------------------------------------
place(music, A + 'Tracks/Split/ost_no_light_returns_mx_1_beginning.ogg', 0., 2., fin=4., length=M['act2'] + .6, fout=1.6, what='MUSIC act 1: "No Light Returns" (opening section) from 0:00')
place(music, A + 'OGG/Pure Horror/One-shots/drone_long_ph_1.ogg', 0., -10., fin=3., length=M['act2'], fout=2., what='drone_long_ph_1 under act 1')
t, z = seg_start('theatre'); place(sfx, A + 'Sound Effects/Machines/old_machine_mx_1_loop.wav', t, -22., length=z - t, fin=.6, fout=.6, what='projector hum (theatre)')
t, z = seg_start('morgue'); place(sfx, A + 'OGG/Impacts/impact_distant_3.ogg', t, -12., peak=True, what='impact_distant_3 (morgue cut)')
t, z = seg_start('ward'); place(sfx, A + 'Sound Effects/Machines/old_machine_mx_2_loop.wav', t, -28., length=z - t, fin=.5, fout=.8, what='an old machine running (ward)')
t, z = seg_start('labs'); place(sfx, A + 'Sound Effects/Machines/retro_computer_calculating_mx_1_loop.wav', t, -25., length=z - t, fin=.4, fout=.6, what='lab computers')
t, z = seg_start('pan'); place(sfx, A + 'OGG/Pure Horror/Loops/sub_tones_loop_1.ogg', t, -20., length=z - t, fin=.6, fout=.8, what='sub tones (the pan)')
t, z = seg_start('redhall'); place(sfx, A + 'OGG/alarm_distant_1.ogg', t + .3, -20., length=z - t, fin=.4, fout=.8, what='a distant alarm (red hall)')
t, z = seg_start('rock'); place(sfx, A + 'OGG/Impacts/impact_distant_5.ogg', t, -12., peak=True, what='impact_distant_5 (the rock)')
t, z = seg_start('breach'); place(sfx, A + 'OGG/Pure Sci-Fi/sci-fi_frequencies_1.ogg', t, -23., length=z - t, fin=.5, fout=.8, what='crystal hum (the breach)')
t, z = seg_start('gallery'); place(sfx, A + 'OGG/Impacts/impact_distant_6.ogg', t + .6, -14., peak=True, what='a distant impact (gallery)')
for a, z, kind, s, f0, o in C.EDIT:
    if kind == 'black' and o.get('line'):
        place(sfx, A + 'OGG/Impacts/impact_distant_8.ogg', a / FPS + .2, -14., peak=True, what='line: ' + o['line'])

# ---- ACT 2 -------------------------------------------------------------------------------------------
act2_len = M['act3'] - M['act2']
place(music, A + 'Tracks/Storm - dark - heavy - lurking.wav', M['act2'] - .16, -3., length=act2_len + .16 + .3, fout=1.2, what='MUSIC act 2: "Storm - dark - heavy - lurking" from 0:00 (its first bar on the first cut)')
CUES.append((M['act2'], 'ACT 2: first person', 0, '-'))

NAMES = [l.strip() for l in open(os.path.join(HERE, 'sound_names.txt'))]
def game_file(name):
    s1 = re.sub(r'(?<!^)(?=[A-Z0-9])', '-', name).lower(); s2 = s1.replace('-', '_'); s3 = name.lower()
    for c in (s1, s2, s3, s1.replace('-', '')):
        p = GAME + c + '.wav'
        if os.path.exists(p): return p
    return None
# v3.1: nothing spoken and no creature voice that is not that creature's own: WorkerDying carries every NPC
# voice line (speech), Hurt is Claire's voice, RadioSquelch keys the radio lines, and the Transformed and the
# mutated men borrow the Brute's calls in the game, so those go too.
SKIP = re.compile(r'^(Music|Amb|Zone|AreaTitle|Pa[A-Z]|One|Sting|WorkerDying|Hurt|RadioSquelch|Brute|Spider|Wasp)')
FOOT = re.compile(r'^(Metal|Concrete|Step)')
placed = set()
for a, z, kind, src, first, o in C.EDIT:
    if kind not in ('fp', 'rep'): continue
    name = src.rsplit('_', 1)[0]
    logf = os.path.join(HERE, 'gp', 'snd-%s-a.txt' % name)
    if not os.path.exists(logf): continue
    n = z - a; last = first + int(n * o.get('step', 1))
    for l in open(logf):
        p = l.split()
        if len(p) < 7 or p[1] == '-': continue
        rf, sid, gain, pitch, dist = int(p[2]), int(p[3]), float(p[4]), float(p[5]), float(p[6])
        if not first <= rf < last or sid >= len(NAMES): continue
        sname = NAMES[sid]
        if SKIP.match(sname): continue
        f = game_file(sname)
        if not f: continue
        key = (a, rf, sid, round(gain, 3))
        if key in placed: continue
        placed.add(key)
        att = 1.0 / (1.0 + dist / 5.0)
        g = 20 * np.log10(max(1e-4, gain * att)) + (-14 if FOOT.match(sname) else -4) + (-2 if kind == 'rep' else 0)
        t = (a + rf - first) / FPS
        place(game, f, t, g, rate=pitch if .5 < pitch < 2 else 1.0, what='game: %s (%s)' % (sname, name), log=sname in ('Shot', 'PistolShot', 'DetectSting', 'LockerBang', 'LockerSting', 'BruteDeath', 'WorkerDying'))
# a hit on every first-person-to-replay change (a tape drop)
for a, z, kind, src, first, o in C.EDIT:
    if kind == 'rep': place(sfx, A + 'Sound Effects/Cinematic vol. 1/sfx_impact_mx_17.ogg', a / FPS, -16., peak=True, what='replay cut (%s)' % src)
t, z = seg_start('cages', 40); place(sfx, A + 'OGG/Creatures/Monster/monster_roar_distant_3.ogg', t + .2, -15., what='R-117 behind the bars (distant growl)')
t, z = seg_start('mtop'); place(sfx, GAME + 'machine.wav', t, -9., fin=.3, length=z - t, fout=.6, what='the Machine (game sound)')
place(sfx, A + 'Sound Effects/Misc/electric_shock_mx_1.ogg', t + 1.1, -5., peak=True, what='HIT: the Machine wakes on live current')
place(sfx, A + 'Sound Effects/Cinematic vol. 1/sfx_impact_mx_4.ogg', t + 1.1, -9., peak=True, what='boom under the Machine')

# ---- ACT 3 -------------------------------------------------------------------------------------------
CUES.append((M['act3'], 'ACT 3: silence, the line', 0, '-'))
drop = M['drop']; eyes_in = M['eyes_in']
place(music, A + 'Tracks/ost_what_eyes_cant_see_mx_1_full.ogg', eyes_in, 0., start=44.65 - (drop - eyes_in), length=M['stop'] - eyes_in + .2, fin=1.5, what='MUSIC act 3: "What Eyes Can\'t See" from %.2f s (drop 0:44.65 on the kill)' % (44.65 - (drop - eyes_in)))
CUES.append((drop, 'MUSIC drop', 0, '-'))
place(sfx, A + 'Sound Effects/Cinematic vol. 1/sfx_swell_mx_1.ogg', drop - .05, -9., peak=True, what='riser into the drop')
def cut(src, first): return seg_start(src, first)[0]
place(sfx, A + 'Sound Effects/Cinematic vol. 1/sfx_impact_mx_1.ogg', drop, -3., peak=True, what='HIT: drop / R-117 out of the door')
place(sfx, A + 'OGG/Creatures/Monster/monster_roar_3.ogg', drop + .35, -5., peak=True, what='roar')
place(sfx, A + 'Sound Effects/Entities/dog_hybrid_mx_1.ogg', cut('bite2', 149) + .3, -9., peak=True, what='snarl behind the post man')
place(sfx, A + 'Sound Effects/Impact & Break/Body/impact_body_ufx_1.ogg', cut('pounce', 161), -5., peak=True, what='HIT: pounce')
place(sfx, A + 'OGG/Creatures/Monster/monster_roar_5.ogg', cut('pounce', 161) + .05, -6., peak=True, what='roar (pounce)')
tb = cut('bite', 199) + 8 / FPS
place(sfx, A + 'Sound Effects/Impact & Break/Body/impact_body_ufx_2.ogg', tb, -2., peak=True, what='HIT: the bite (flash, blood on the lens)')
place(sfx, A + 'Sound Effects/Cinematic vol. 1/sfx_impact_mx_4.ogg', tb, -6., peak=True, what='boom under the bite')
place(sfx, A + 'Sound Effects/Impact & Break/Body/impact_body_ufx_2.ogg', cut('feedw', 259) + .5, -15., peak=True, what='feeding (wet body impact, no voice)')
place(sfx, A + 'Sound Effects/Cinematic vol. 1/sfx_impact_mx_13.ogg', cut('rush', 84), -5., peak=True, what='HIT: blood wipe into the return (103)')
place(sfx, A + 'OGG/Creatures/Monster/monster_roar_7.ogg', cut('rush', 148) - .15, -4., peak=True, what='roar (it reaches us)')
place(sfx, A + 'Sound Effects/Cinematic vol. 1/sfx_impact_mx_16.ogg', cut('rush', 148), -4., peak=True, what='HIT: shake on the rush')
place(sfx, A + 'OGG/Creatures/Monster/monster_roar_2.ogg', cut('hall', 165) + .05, -4., peak=True, what='roar (over the floor camera, 101)')
place(sfx, A + 'Sound Effects/Impact & Break/Body/impact_body_ufx_1.ogg', cut('hall', 165), -6., peak=True, what='HIT: pounce at the floor camera')
place(sfx, A + 'OGG/Creatures/Monster/monster_roar_6.ogg', cut('turn', 476) - .1, -4., peak=True, what='roar into the last pounce')
place(sfx, A + 'Sound Effects/Cinematic vol. 1/sfx_impact_mx_1.ogg', cut('turn', 476), -3., peak=True, what='HIT: last pounce at the camera')
CUES.append((M['stop'], 'HARD STOP: silence', 0, '-'))

# ---- title, tease, end ----------------------------------------------------------------------------------
place(tail, A + 'Sound Effects/Cinematic vol. 1/sfx_stinger_mx_2.ogg', M['title'], -2., peak=True, what='STING: title')
place(tail, A + 'Sound Effects/Cinematic vol. 1/sfx_gong_mx_1.ogg', M['title'], -9., peak=True, what='gong under the title')
place(tail, A + 'OGG/Impacts/impact_distant_2.ogg', C.ENDCARD / FPS, -17., peak=True, what='CHAPTER 1 line')
place(tail, A + 'OGG/Pure Horror/One-shots/drone_ph_2.ogg', M['title'] + 2., -16., fin=1., length=END - M['title'] - 2., fout=2., what='end drone')
place(tail, GAME + 'machine.wav', M['button'], -11., fin=.2, length=2., fout=.4, what='the Machine (game sound)')
place(tail, A + 'OGG/Pure Horror/One-shots/stinger_ph_1.ogg', M['button'] + .4, -9., peak=True, what='the lamp-eye in the dark')
place(tail, A + 'Sound Effects/Misc/electric_shock_mx_1.ogg', M['button'] + 1.9, -6., peak=True, what='STING: the floor goes live')
place(tail, A + 'Sound Effects/Cinematic vol. 1/sfx_impact_mx_16.ogg', M['end'] - 2.0, -8., peak=True, what='HIT: cut to black')

# ---- master -------------------------------------------------------------------------------------------
mix = music * db(-2.) + sfx + game * db(-1.)
i0 = int(M['stop'] * SR); ramp = int(.012 * SR)
mix[i0:i0 + ramp] *= np.linspace(1, 0, ramp)[:, None]; mix[i0 + ramp:] = 0
mix += tail
mix = mix[:int(END * SR)]
mix[-int(.05 * SR):] *= np.linspace(1, 0, int(.05 * SR))[:, None]

def write_wav(p, a):
    x = (np.clip(a, -1, 1) * 32767).astype('<i2')
    with wave.open(p, 'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes(x.tobytes())

def lufs(a):
    p = os.path.join(HERE, 'audio', '_meas.wav'); write_wav(p, a)
    r = subprocess.run(['ffmpeg', '-hide_banner', '-nostats', '-i', p, '-af', 'ebur128=peak=true', '-f', 'null', '-'], capture_output=True, text=True).stderr
    return float(r.rsplit('I:', 1)[1].split('LUFS')[0]), float(r.rsplit('Peak:', 1)[1].split('dBFS')[0])

def limit(a, ceil=db(-1.5), look=int(.004 * SR), release=int(.08 * SR)):
    pk = np.abs(a).max(1); need = np.minimum(1, ceil / np.maximum(pk, 1e-9))
    from numpy.lib.stride_tricks import sliding_window_view
    ahead = sliding_window_view(np.concatenate([need, np.ones(look)]), look + 1).min(1)[:len(need)]
    g = np.empty_like(ahead); cur = 1.0; k = 1 - 1 / release
    for i in range(len(ahead)):
        cur = min(ahead[i], cur * k + (1 - k)); g[i] = cur
    return a * g[:, None]

if __name__ == '__main__':
    out = sys.argv[1]
    I, tp = lufs(mix); mix = limit(mix * db(-14 - I))
    for _ in range(2):
        I, tp = lufs(mix)
        if abs(I + 14) < .3: break
        mix = limit(mix * db(-14 - I))
    write_wav(out, mix)
    I, tp = lufs(mix)
    print('integrated %.1f LUFS, true peak %.1f dBFS, %.2f s' % (I, tp, len(mix) / SR))
    print('stop-to-title silence peak', np.abs(mix[int((M['stop'] + .03) * SR):int((M['title'] - .03) * SR)]).max())
    with open(os.path.join(os.path.dirname(os.path.abspath(out)), 'cues.txt'), 'w', encoding='utf-8') as fh:
        for t, what, g, how in sorted(CUES):
            fh.write('%6.2f s  f%04d  %-70s %5.1f dB  %s\n' % (t, round(t * FPS), what, g, how))
