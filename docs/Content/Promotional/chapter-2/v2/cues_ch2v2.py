"""The cue sheet: every sound cue (audio/cues.txt from score_ch2v2.py) against the nearest picture cut.

  python cues_ch2v2.py OUT.txt
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import compose_ch2v2 as C

cuts = {a: (s, f) for a, z, s, f, o in C.EDIT}
cuts.update({C.TITLE: ('TITLE', 0), C.ENDCARD: ('END CARD', 0), C.STOP: ('STOP', 0)})
out = []
for l in open(os.path.join(HERE, 'audio', 'cues.txt'), encoding='utf-8'):
    l = l.rstrip(); fr = int(l.split()[2][1:])
    near = min(cuts, key=lambda c: abs(c - fr)); s, f = cuts[near]
    pic = '%s_%03d' % (s, f) if s and s.islower() else (s or 'black')
    out.append('%s | nearest cut f%04d %s (%+d fr)' % (l, near, pic, fr - near))
open(sys.argv[1], 'w', encoding='utf-8').write('\n'.join(out) + '\n')
print('\n'.join(o for o in out if 'HIT' in o or 'STING' in o or 'STOP' in o or 'drop' in o))
