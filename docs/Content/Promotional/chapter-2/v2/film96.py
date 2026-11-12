"""R-117's kill in Containment A (chunk 96), several cameras on the one live take.

  python film96.py OUT.txt [LOG]

Writes a --film list: the scene stepped every 2 ticks (30 fps) and, at each step, a frame from
every camera that is rolling then. The camera aims follow the animal (positions from LOG, a
film-log.txt of an earlier take of the same scene; the sim is deterministic with the player held
at the same anchor). Frames: .build/film/<cam>_NNN.ppm, NNN = 30 fps frame index from t=0.
"""
import math, re, sys

ANCHOR = (6.4, 12.6, -15, -4, -6)       # where --chimera-kill-frames holds the player
FLOOR = -15.0

def track_from(log):
    pts = []
    for l in open(log):
        m = re.search(r't=([\d.]+).*?CHIMERA ([\d.-]+) ([\d.-]+) ([\d.-]+)', l)
        if m: pts.append(tuple(map(float, m.groups())))
    return pts

def at(pts, t):
    if t <= pts[0][0]: return pts[0][1:3]
    for a, b in zip(pts, pts[1:]):
        if a[0] <= t <= b[0]:
            u = (t - a[0]) / max(1e-6, b[0] - a[0])
            return a[1] + (b[1] - a[1]) * u, a[2] + (b[2] - a[2]) * u
    return pts[-1][1:3]

def smooth_aim(pts, cam, t, lag=.35, lead=(0, 0)):
    """Yaw (deg) toward the animal as an operator would: a little behind it."""
    xs = [at(pts, t - lag * k / 4) for k in range(5)]
    x = sum(p[0] for p in xs) / 5 + lead[0]; y = sum(p[1] for p in xs) / 5 + lead[1]
    return math.degrees(math.atan2(y - cam[1], x - cam[0]))

def lerp(a, b, u): return a + (b - a) * u
def ease(u): return .5 - .5 * math.cos(math.pi * max(0, min(1, u)))

# name: (t0, t1, camera(t, pts) -> (x, y, feet, yaw, pitch))
def cam_wake(t, pts):      # low in the dark of the room's south end, looking into the service run
    u = ease((t - 2.4) / 3.0); c = (lerp(9.6, 9.9, u), lerp(18.6, 18.3, u))
    return c[0], c[1], FLOOR - .85, smooth_aim(pts, c, t, .5), 2
def cam_prowl(t, pts):     # floor level behind the post: it comes down the room at us
    u = ease((t - 3.4) / 3.0); c = (lerp(8.9, 9.1, u), lerp(16.4, 16.6, u))
    return c[0], c[1], FLOOR - 1.0, smooth_aim(pts, c, t, .45), 6
def cam_pounce(t, pts):    # behind the post man, at hip height: it comes up over him at us
    c = (7.6, 14.2)
    return c[0], c[1], FLOOR - .55, smooth_aim(pts, c, t, .2, (-.3, -.3)), 10
def cam_bite(t, pts):      # tight, from the window side, on the head
    u = ease((t - 5.6) / 2.0); c = (lerp(9.9, 9.7, u), lerp(14.4, 14.6, u))
    return c[0], c[1], FLOOR - .3, smooth_aim(pts, c, t, .1, (-.6, -.4)), 6
def cam_feed(t, pts):      # close and low by the wall, a slow push in on the feeding
    u = ease((t - 7.0) / 6.5); c = (lerp(7.6, 8.0, u), lerp(18.2, 17.8, u))
    return c[0], c[1], FLOOR - 1.0, smooth_aim(pts, c, t, .6, (-.5, -.3)), -6
def cam_feed3(t, pts):     # mid-close, low, behind the body: the feeding, head and all
    u = ease((t - 7.0) / 6.5); c = (lerp(7.5, 8.1, u), lerp(14.5, 15.0, u))
    return c[0], c[1], FLOOR - .9, smooth_aim(pts, c, t, .6, (-.35, -.25)), 9
def cam_feedw(t, pts):     # from low by the bench, wider: the head down in the body
    u = ease((t - 7.0) / 6.5); c = (lerp(6.8, 7.25, u), lerp(13.5, 13.9, u))
    return c[0], c[1], FLOOR - .6, smooth_aim(pts, c, t, .6, (-.55, -.35)), 5
def cam_bite2(t, pts):     # the strike seen low past the post man's legs
    c = (8.4, 14.2)
    return c[0], c[1], FLOOR - .95, smooth_aim(pts, c, t, .15, (-.5, -.4)), 12
def cam_feed2(t, pts):     # from the floor at the body's feet
    c = (7.3, 14.9)
    return c[0], c[1], FLOOR - 1.15, smooth_aim(pts, c, t, .6, (-.4, -.2)), 4
def cam_turn(t, pts):      # it gets up and comes our way: low, by the anchor
    c = (5.7, 13.5)
    return c[0], c[1], FLOOR - .9, smooth_aim(pts, c, t, .3), 4

CAMS = [('wake', 2.4, 5.6, cam_wake), ('pounce', 4.6, 6.8, cam_pounce),
        ('bite', 5.6, 7.8, cam_bite), ('feed3', 7.0, 13.8, cam_feed3), ('feedw', 7.0, 13.8, cam_feedw), ('bite2', 5.0, 7.8, cam_bite2), ('turn', 13.6, 18.0, cam_turn)]

def main():
    out = sys.argv[1]; pts = track_from(sys.argv[2])
    L = ['stage 96 %g %g %g %g %g' % ANCHOR] + (['torch'] if len(sys.argv) > 3 and sys.argv[3] == 'torch' else [])
    # v2 fix pass: the room taken down to a true dark (no fill), its lamps at 30%, and a red work lamp in the
    # service-run door behind the animal, so it is seen as a shape with lit edges, not a lit model.
    L += ['dark', 'lights 0.3', 'lamp 12.0 23.1 -13.3 0.9 4 2400 1 .08 .05']
    end = max(c[2] for c in CAMS)
    n = int(end * 30) + 1
    for k in range(n):
        t = (k + 1) * 2 / 60.0          # elapsed after this step
        first = True
        for name, t0, t1, cam in CAMS:
            if not t0 <= t <= t1: continue
            x, y, z, yaw, pitch = cam(t, pts)
            L.append('f %s_%03d %d %.3f %.3f %.3f %.2f %.1f' % (name, k, 2 if first else 0, x, y, z, yaw, pitch))
            first = False
        if first: L.append('tick 2')
    open(out, 'w').write('\n'.join(L) + '\n')

if __name__ == '__main__':
    main()
