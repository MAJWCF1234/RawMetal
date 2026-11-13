"""The Chapter 2 promo v2 shots that are not the kill in 96 (film96.py): environments, R-117's
returns, the Machine. Each shot is one live --film take: the chunk loaded with the states a
player has on arriving, stepped at 60 Hz, drawn every 2 ticks (30 fps) from a moving camera.

  python shots.py EXE [NAME ...]      (renders into .build/promo-1.4/env/.build/film/NAME_NNN.ppm)
"""
import math, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
C2 = ['ere_great_door', 'chapter2_open']
ARRIVE = {}
for i in range(68, 71): ARRIVE[i] = C2
for i in range(79, 85): ARRIVE[i] = C2 + ['proc_clearance', 'lab_chem_seal']
for i in range(71, 79): ARRIVE[i] = ARRIVE[79] + ['med_override']
for i in range(85, 104): ARRIVE[i] = ARRIVE[71] + ['med_morgue_release']
for i in range(104, 108): ARRIVE[i] = ARRIVE[85] + ['cont_power', 'cont_barricade']

# v2 fix pass: R-117's takes in true dark, lamps at 30% (a shape with lit edges, not a lit model)
DARK = ['dark', 'lights 0.3']

def ease(u): return .5 - .5 * math.cos(math.pi * max(0., min(1., u)))
def lerp(a, b, u): return a + (b - a) * u

# name: level, extra states, frames, camera start (x, y, feet, yaw, pitch), camera end, options
#   options: anchor=(x,y,feet,yaw,pitch) the player's place for the sim (default: the camera start),
#            pre=ticks before the first frame, lines=[extra --film lines after the stage], track=enemy name
SHOTS = {
    # Intake theatre (68): low between the back rows, the welcome film on the screen.
    'theatre': (68, [], 120, (20.35, 10.35, -12.5, -51, 0), (20.15, 10.2, -12.5, -47, 0), {}),
    # Medical Deep, the mortuary (78): along the tables at table height.
    'morgue': (78, [], 105, (1.7, 22.5, -15.45, -40, -2), (2.7, 21.5, -15.45, -48, -3), {}),
    # Energy Test Labs (93): the glass chambers under violet light, a crab-wise track.
    'labs': (93, [], 105, (1.2, 7.9, -16.3, 30, 0), (2.4, 6.9, -16.3, 42, 0), {}),
    # Containment B (98): the rock and the growth come through the wall, pushing in.
    'rock': (98, [], 105, (13.5, 0.9, -15.4, 100, 2), (13.3, 2.3, -15.4, 96, 0), {}),
    # Red corridor (100): the reader lamp, a door left open on the dark.
    'redhall': (100, [], 105, (1.3, 9.8, -15.3, 32, -1), (2.0, 10.9, -15.3, 27, -2), {}),
    # Containment's glass booth (101): a man in a suit in the cell under the lamps.
    'booth': (101, [], 105, (1.5, 22.6, -18.3, -36, 0), (2.4, 21.8, -18.3, -41, -1), {}),
    # The breach (102): growth across the floor, a body in the crystals.
    'breach': (102, [], 105, (11.4, 16.9, -17.6, -128, -8), (10.6, 16.1, -17.6, -121, -6), {}),
    # v3: the suit room (69), low along the hangers
    'suits': (69, [], 120, (9.6, 18.9, -12.6, 205, 2), (9.0, 18.5, -12.6, 200, 1), {}),
    # v3: the calibration pan (84): standing water under the gantry, a slow crane down
    'pan': (84, [], 120, (4.5, 10.5, -11.0, 42, -14), (5.0, 11.0, -11.3, 46, -10), {}),
    # v3: the Observation Ring (90), a slow drift along the glass
    'ring': (90, [], 120, (0.9, 7.9, -12.3, 35, -1), (1.6, 8.6, -12.3, 40, -2), {}),
    # v3: Deep Research (105), the far corner pushing in
    'deep': (105, [], 120, (22.0, 22.0, -15.3, -135, -2), (21.2, 21.2, -15.3, -132, -3), {}),
    # v3: Medical Front (72), a ward corridor
    'ward': (72, [], 110, (7.6, 22.0, -12.45, -54, 0), (8.3, 21.1, -12.45, -50, -1), {}),
    # v3: Observation (94), the gallery
    'gallery': (94, [], 110, (1.7, 21.8, -13.7, -35, -2), (2.5, 21.2, -13.7, -32, -3), {}),
    # v3: Containment approach (100), the far end
    'approach': (100, [], 110, (13.2, 12.9, -14.6, -136, -1), (12.5, 12.2, -14.6, -132, -2), {}),
    # Containment cages (99): R-117 walking behind the bars (its return), warm light.
    'cages': (99, ['cont_power', 'conb_sh_r117_out'], 150, (9, 16.8, -18.55, 63.6, 4), (9.1, 17.3, -18.55, 66, 4),
              {'anchor': (9, 16.8, -18, 63.6, -4), 'track': 'CHIMERA', 'pre': 20, 'lines': DARK}),
    # The return in 103: it crosses the hall at the player. A low camera a step off the player.
    'rush': (103, ['cont_power', 'conb_ec_pressure', 'cont_barricade'], 330, (11.0, 12.2, -15.75, 20, 4), (11.0, 12.2, -15.75, 20, 4),
             {'anchor': (12, 13.5, -15, 0, -4), 'track': 'CHIMERA', 'lines': DARK}),
    # The return in 101: down the hall to the player.
    'hall': (101, ['cont_power', 'conb_th_r117_out'], 300, (11.3, 9.4, -18.8, 60, 4), (11.3, 9.4, -18.8, 60, 4),
             {'anchor': (12, 10.6, -18, 90, -4), 'track': 'CHIMERA', 'lines': DARK}),
}

def write(name):
    level, sets, n, s, e, opt = SHOTS[name]
    states = ARRIVE.get(level, []) + sets
    anchor = opt.get('anchor', s)
    L = ['stage %d %g %g %g %g %g %s' % ((level,) + tuple(anchor) + (','.join(states),))]
    L += opt.get('lines', [])
    L.append('tick %d' % opt.get('pre', 60))
    for k in range(n):
        u = ease(k / max(1, n - 1)) * .7 + k / max(1, n - 1) * .3
        x, y, z, yaw, pitch = (lerp(a, b, u) for a, b in zip(s, e))
        L.append('f %s_%03d 2 %.3f %.3f %.3f %.2f %.1f' % (name, k, x, y, z, yaw, pitch))
    path = os.path.join(HERE, 'env', name + '.txt')
    open(path, 'w').write('\n'.join(L) + '\n')
    return path

def retrack(name, log):
    """Second pass for tracked shots: aim each frame at the creature's logged position."""
    import re
    level, sets, n, s, e, opt = SHOTS[name]
    pos = {}
    for l in open(log):
        m = re.match(r'(\S+)_(\d+) .*?%s ([\d.-]+) ([\d.-]+)' % opt['track'], l)
        if m and m.group(1) == name: pos[int(m.group(2))] = (float(m.group(3)), float(m.group(4)))
    path = os.path.join(HERE, 'env', name + '.txt')
    out, yaws = [], []
    for l in open(path):
        p = l.split()
        if p and p[0] == 'f':
            k = int(p[1].rsplit('_', 1)[1])
            if k in pos:
                x, y = float(p[3]), float(p[4])
                yaw = math.degrees(math.atan2(pos[k][1] - y, pos[k][0] - x))
                if yaws:  # an operator's lag: never more than 9 degrees a frame, smoothed
                    d = (yaw - yaws[-1] + 180) % 360 - 180
                    yaw = yaws[-1] + max(-9, min(9, d * .55))
                yaws.append(yaw); p[6] = '%.2f' % yaw
            l = ' '.join(p) + '\n'
        out.append(l)
    open(path, 'w').write(''.join(out))

def main():
    exe = os.path.abspath(sys.argv[1]); names = sys.argv[2:] or list(SHOTS)
    run = os.path.join(HERE, 'env'); os.makedirs(run, exist_ok=True)
    for name in names:
        path = write(name)
        subprocess.run([exe, '--film=' + path], cwd=run, timeout=1200)
        log = os.path.join(run, 'log-%s.txt' % name)
        os.replace(os.path.join(run, 'film-log.txt'), log)
        if 'track' in SHOTS[name][5]:
            retrack(name, log)
            subprocess.run([exe, '--film=' + path], cwd=run, timeout=1200)
            os.replace(os.path.join(run, 'film-log.txt'), log)
        print(name, 'done', flush=True)

if __name__ == '__main__':
    main()
