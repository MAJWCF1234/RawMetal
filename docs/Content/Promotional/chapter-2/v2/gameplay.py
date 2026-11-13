"""Chapter 2 promo v3: the gameplay beats. Each beat is one deterministic --film take run twice:
pass A first person with the hands and HUD (NAME_a_NNNN), pass B the same take from a fixed replay
camera (NAME_b_NNNN, the body orbit aimed at a point; the HUD is cropped off in the edit).

  python gameplay.py EXE [NAME ...]        (renders into gp/.build/film/, logs in gp/log-NAME-[ab].txt)
"""
import os, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, 'gp')

C2 = ['ere_great_door', 'chapter2_open', 'proc_clearance', 'lab_chem_seal', 'med_override', 'med_morgue_release']

def head(level, x=12, y=12, z=-12, extra=()):
    return ['game %d %g %g %g 0 0 %s' % (level, x, y, z, ','.join(C2 + list(extra))), 'console god', 'hud 1', 'play']

# Each beat: (lines before recording, recording lines, replay camera line for pass B)
BEATS = {}

def kill(level, enemy, weapon, dist, deg, cam, shots, hp=55, gap=40, run=30, z=-12, extra=(), pre=(), turn=40, exit_run=36, sprint=False):
    """Run up to it (sprint), stop, put it down in `shots` rounds, then turn off and run out of the replay camera's view."""
    lines = head(level, z=z, extra=extra) + ['weapon ' + weapon, 'tick 20', 'near %s %g %g' % (enemy, dist, deg), 'hp %s %g' % (enemy, hp), 'tick 20'] + list(pre) + ['census']
    rec = ['look %s 12 w' % enemy, 'track %s %d w%s' % (enemy, run, ',sprint' if sprint else ''), 'track %s 6' % enemy]
    for i in range(shots):
        rec += ['track %s 1 fire' % enemy, 'track %s %d %s' % (enemy, gap, 's' if i < shots - 1 else '-'), 'census']
    rec += ['in 18 - %g 0' % turn, 'in %d w,sprint' % exit_run, 'census']
    return lines, rec, cam

def write(name, which):
    pre, rec, cam = BEATS[name]
    L = list(pre)
    if which == 'b': L.append(cam)
    L.append('rec %s_%s' % (name, which))
    L += rec
    L.append('stop')
    path = os.path.join(RUN, '%s-%s.txt' % (name, which))
    open(path, 'w').write('\n'.join(L) + '\n')
    return path

# creature kills: each a different room, creature and weapon
BEATS['k93'] = kill(93, 'e0', 'shotgun', 11.5, 180, 'cam rel 60 3.5', 2, run=30)
BEATS['k79'] = kill(79, 'e0', 'shotgun', 11.5, 180, 'cam rel -60 3.5', 2, hp=50, run=30)
BEATS['k98'] = kill(98, 'e0', 'handgun', 8.0, 180, 'cam rel 60 3.5', 4, hp=48, gap=16, z=-15)
BEATS['k104'] = kill(104, 'e0', 'shotgun', 7.5, 90, 'cam auto 3.0 -30 30 -60 60 -100 100', 2, z=-15, turn=-40, run=4)
BEATS['k86'] = kill(86, 'e0', 'handgun', 8.0, 90, 'cam rel 60 3.5', 4, hp=48, gap=16, z=-20)

# a Cleanup man makes the player out (the '?' filling, the sting, ALERT) and goes down to the handgun
BEATS['g83'] = (head(83) + ['weapon handgun', 'tick 20', 'near n91 5.0 0', 'tick 10', 'census'],
    ['look g91 20 w', 'track g91 70 -', 'census', 'track g91 1 fire', 'track g91 14', 'track g91 1 fire', 'track g91 14',
     'track g91 1 fire', 'track g91 14', 'track g91 1 fire', 'track g91 50', 'census', 'tick 1', 'in 18 - -40 0', 'in 30 w,sprint'],
    'cam rel 70 3.0')
# crouched in the dark of Ward C's corridor while a sharpshooter walks his round
BEATS['s71'] = (head(71) + ['weapon none', 'tick 20', 'near n79 8 180', 'in 10 crouch', 'census'],
    ['look g79 30 crouch', 'track g79 120 crouch,a', 'census'],
    'cam rel 120 2.5')
# a door eased open by hand, the gap peered through
BEATS['d71'] = (head(71) + ['weapon none', 'tick 20', 'pos 9.45 4.45 -12 90 0', 'tick 10', 'census'],
    ['in 30 -', 'in 1 use', 'in 40 -', 'in 30 use', 'in 40 w', 'census'],
    'cam rel -150 2.0')
# hiding in a staff locker; the nearest guard walks up, tears it open and hauls her out
BEATS['l71'] = (head(71) + ['weapon none', 'tick 20', 'pos 18.86 21.85 -12 90 0', 'tick 10', 'census'],
    ['reach', 'tick 170', 'yank', 'tick 170', 'census'],
    'cam rel 200 3.0')
# the Lab 4 door reader: a terminal read in the dark
BEATS['t83'] = (head(83) + ['weapon none', 'tick 20', 'pos 14.98 7.2 -12 90 0', 'tick 10', 'census'],
    ['look 14.98,8.2,-11.3 20', 'in 1 use', 'in 90 -', 'census'],
    'cam rel 160 2.5')

BEATS['g95'] = (head(95, z=-15) + ['weapon handgun', 'tick 20', 'near n114 6.0 180', 'hurt n114 30', 'tick 10', 'census'],
    ['look g114 20 w', 'track g114 70 -', 'census', 'track n114 1 fire', 'track n114 14', 'track n114 1 fire', 'track n114 14',
     'track n114 1 fire', 'track n114 14', 'track n114 1 fire', 'track n114 14', 'track n114 1 fire', 'track g114 40', 'census', 'in 18 - -40 0', 'in 30 w,sprint'],
    'cam auto 3.5 -60 -90 60 90 -130 130')
BEATS['g71'] = (head(71) + ['weapon handgun', 'tick 20', 'near n78 6.0 180', 'hurt n78 30', 'tick 10', 'census'],
    ['look g78 20 w', 'track g78 40 -', 'census'] + sum([['track n78 1 fire', 'track n78 14'] for _ in range(3)], []) +
    ['track n78 50', 'census', 'in 18 - -40 0', 'in 30 w,sprint'],
    'cam auto 3.0 -70 70 -110 110 150 -150')

# more fights (v3 second batch): each a different room and creature
BEATS['k105'] = kill(105, 'e0', 'handgun', 8.0, 0, 'cam auto 3.5', 4, hp=48, gap=16, z=-15)
BEATS['k99'] = kill(99, 'e0', 'shotgun', 11.0, 180, 'cam auto 3.5', 2, run=30, z=-15)
BEATS['k107'] = kill(107, 'e2', 'shotgun', 11.0, 0, 'cam auto 3.5', 2, hp=50, run=30, z=-15)
BEATS['k81'] = kill(81, 'e0', 'handgun', 8.0, 90, 'cam auto 3.5', 4, hp=48, gap=16)
BEATS['g91'] = (head(91) + ['weapon shotgun', 'tick 20', 'near n106 6.5 180', 'hurt n106 25', 'tick 10', 'census'],
    ['look g106 18 w', 'track n106 30 w', 'track n106 20 -', 'census', 'track n106 1 fire', 'track n106 40', 'track n106 1 fire', 'track n106 50', 'census', 'in 18 - 40 0', 'in 30 w,sprint'],
    'cam auto 3.5')

# the torch: a dark room swept by the flashlight, something in it (88)
BEATS['f88'] = (head(88) + ['weapon handgun', 'torch', 'tick 20', 'near e1 9 0', 'tick 10', 'census'],
    ['in 30 -', 'look e1 40 w', 'track e1 50 w', 'census'],
    'cam auto 3.0')
# Containment Intake again from the other side: crouched behind the crates while the squad talks (95)
BEATS['s95'] = (head(95, z=-15) + ['weapon none', 'tick 20', 'pos 4.0 9.0 -15 45 0', 'in 10 crouch', 'census'],
    ['look g114 30 crouch', 'track g114 90 crouch,d', 'census'],
    'cam auto 2.5')
BEATS['k97'] = kill(97, 'e0', 'handgun', 9.0, 0, 'cam auto 3.5', 4, hp=48, gap=16, z=-15)

BEATS['k85'] = kill(85, 'e0', 'shotgun', 11.0, 180, 'cam auto 3.5', 2, run=30)
BEATS['k106'] = kill(106, 'e0', 'handgun', 9.0, 0, 'cam auto 3.5', 4, hp=48, gap=16, z=-15)

def main():
    exe = os.path.abspath(sys.argv[1]); names = sys.argv[2:] or list(BEATS)
    passes = os.environ.get('PASSES', 'ab')
    os.makedirs(RUN, exist_ok=True)
    for name in names:
        for which in passes:
            path = write(name, which)
            subprocess.run([exe, '--film=' + path], cwd=RUN, timeout=1800)
            shutil.copy(os.path.join(RUN, 'film-log.txt'), os.path.join(RUN, 'log-%s-%s.txt' % (name, which)))
            shutil.copy(os.path.join(RUN, 'film-sounds.txt'), os.path.join(RUN, 'snd-%s-%s.txt' % (name, which)))
        print(name, 'done', flush=True)

if __name__ == '__main__':
    main()
