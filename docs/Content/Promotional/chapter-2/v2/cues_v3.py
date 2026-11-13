"""The v3 cue sheet: every sound cue (audio/cues.txt from score_v3.py) against the nearest picture cut.

  python cues_v3.py OUT.txt
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import compose_v3 as C

cuts = {a: (s or k, f) for a, z, k, s, f, o in C.EDIT}
cuts.update({C.MARK['title']: ('TITLE', 0), C.MARK['stop']: ('STOP', 0), C.ENDCARD: ('END CARD', 0)})
out = ['# time  frame  cue  level  alignment | nearest picture cut (offset in frames)']
for l in open(os.path.join(HERE, 'audio', 'cues.txt'), encoding='utf-8'):
    l = l.rstrip(); fr = int(l.split()[2][1:])
    near = min(cuts, key=lambda c: abs(c - fr)); s, f = cuts[near]
    out.append('%s | cut f%04d %s%s (%+d fr)' % (l, near, s, '_%d' % f if f else '', fr - near))
open(sys.argv[1], 'w', encoding='utf-8').write('\n'.join(out) + '\n')
print('\n'.join(o for o in out if 'HIT' in o or 'STING' in o or 'STOP' in o or 'MUSIC' in o))
