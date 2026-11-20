"""How far a player body's waist pops through a turn (the `spineturn` lab tube writes the data).

  python tools/dev/lab.py run --feature=spineturn --frames=480 --tune spineturn.character=N   (N 0 Claire, 1 Limitator, 2 Leon)
  python tools/dev/spine_pop.py [DIR] [N ...]

The drawn faces of the torso are cut into 5 cm height bands from the pelvis up to the chest. For each band and each pair of
frames the rotation about the vertical of its faces (their centres, the same faces in both frames) is measured: omega,
degrees a frame. A soft torso turns its bands by neighbouring amounts, a snapping one turns two neighbours by very
different amounts (the whole twist sits between them) or jumps from one frame to the next.
  shear    the largest difference of omega between neighbouring bands in one frame (deg per frame)
  jump     the largest change of one band's omega from one frame to the next (deg per frame squared, x1)
  share    in the frame of the largest total twist, the largest share of that twist any two neighbouring bands carry
Frames are 1/60 s. Scenes: spot (frames 70-240, two turns on the spot) and walk (240-420, turning while walking).
"""
import os, struct, sys
import numpy as np

def load(base):
    with open(base + '.bin', 'rb') as f:
        n, frames = struct.unpack('<ii', f.read(8))
        tags, pts = [], []
        while True:
            t = f.read(4)
            if len(t) < 4: break
            tags.append(struct.unpack('<f', t)[0]); pts.append(np.frombuffer(f.read(n * 12), dtype='<f4').reshape(n, 3))
    with open(base + '.w', 'rb') as f:
        n2, b = struct.unpack('<ii', f.read(8)); w = np.frombuffer(f.read(n2 * b * 4), dtype='<f4').reshape(n2, b)
    return np.array(tags), np.array(pts), w

BONES = ['Hips', 'Spine1', 'Spine2', 'Spine3', 'Neck', 'Head', 'Shoulder_L', 'Shoulder_R', 'Arm_L', 'Arm_R', 'LegUp_L', 'LegUp_R']

def rot(a, b):
    """angle of the rotation about the vertical taking points a (k,2) to b (k,2), the translation removed"""
    a = a - a.mean(0); b = b - b.mean(0)
    return np.degrees(np.arctan2((a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]).sum(), (a * b).sum()))

def analyse(base, step=.05):
    tags, pts, w = load(base)
    col = {k: i for i, k in enumerate(BONES)}
    torso = w[:, col['Hips']] + w[:, col['Spine1']] + w[:, col['Spine2']] + w[:, col['Spine3']] + w[:, col['Shoulder_L']] + w[:, col['Shoulder_R']]
    arms = w[:, col['Arm_L']] + w[:, col['Arm_R']] + w[:, col['Neck']] + w[:, col['Head']] + w[:, col['LegUp_L']] + w[:, col['LegUp_R']]
    ok = (torso >= .6) & (arms < .2)
    rest = pts[min(40, len(pts) - 1)]
    # from the pelvis (the hips' own faces) up to the chest (the third spine bone's)
    pelvis = np.nanpercentile(rest[ok, 2], 5)
    chest = np.nanpercentile(rest[ok & (w[:, col['Spine2']] + w[:, col['Spine3']] > .3), 2], 90)
    # equal bands of 5 cm (the bodies are all 1.7 m tall from outside), from the pelvis' faces (never lower than 0.8 m) to the chest
    edges = np.arange(max(pelvis, .8), chest + 1e-6, step); bands = len(edges) - 1
    band = np.digitize(rest[:, 2], edges) - 1
    omega = np.full((len(pts) - 1, bands), np.nan)
    for t in range(len(pts) - 1):
        for k in range(bands):
            sel = ok & (band == k) & ~np.isnan(pts[t][:, 0]) & ~np.isnan(pts[t + 1][:, 0])
            if sel.sum() >= 6: omega[t, k] = rot(pts[t][sel, :2], pts[t + 1][sel, :2])
    return tags, omega, (pelvis, chest)

def scene(omega, lo, hi):
    o = omega[lo:hi]
    shear = np.nanmax(np.abs(np.diff(o, axis=1)))
    jump = np.nanmax(np.abs(np.diff(o, axis=0)))
    # twist relative to the lowest band, summed over the scene's frames: the profile up the torso
    theta = np.nancumsum(o, axis=0); rel = theta - theta[:, :1]
    peak = int(np.nanargmax(np.nanmax(np.abs(rel), axis=1)))
    total = np.nanmax(np.abs(rel[peak])); step = np.nanmax(np.abs(np.diff(rel[peak])))
    return shear, jump, (step / total if total > 1 else float('nan')), total

if __name__ == '__main__':
    args = sys.argv[1:]
    d = args[0] if args and not args[0].isdigit() else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '.build', 'lab-out')
    who = [int(a) for a in args if a.isdigit()] or [0, 1, 2]
    names = {0: 'Claire', 1: 'Limitator', 2: 'Leon'}
    print('%-10s %-5s %8s %8s %8s %8s' % ('body', 'scene', 'shear', 'jump', 'share', 'twist'))
    for n in who:
        base = os.path.join(d, 'spineturn-%d' % n)
        if not os.path.exists(base + '.bin'): continue
        tags, omega, (pz, cz) = analyse(base)
        for nm, lo, hi in (('spot', 70, 240), ('walk', 240, 419)):
            s, j, sh, tw = scene(omega, lo, min(hi, len(omega)))
            print('%-10s %-5s %8.2f %8.2f %8.2f %8.1f' % (names[n], nm, s, j, sh, tw))
