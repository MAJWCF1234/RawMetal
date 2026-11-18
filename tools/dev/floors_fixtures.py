#!/usr/bin/env python3
"""Write the floors lab scenes (src/lab/tubes/Floors.cpp): lab/fixtures/floors.rmc and
floors-*.lab. One campaign: an indoor hall with stairs and a balcony (finishes), a raised
room and laid flooring; then the land, one map per footing (Footing.h) and one built the
old way, each with a room on its steepest slope.

  python tools/dev/floors_fixtures.py
"""
import math, os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'ashfall'))
from build_zone import Terrain  # noqa: E402

TERRAIN = dict(seed=7, base=1, amplitude=5, scale=16, roughness=.45)
OUT = os.path.join(ROOT, 'lab', 'fixtures')


def f(v):
    if isinstance(v, float):
        s = ('%.3f' % v).rstrip('0').rstrip('.')
        return '0' if s in ('-0', '') else s
    return str(v)


def rec(*fields):
    return '|'.join(f(x) for x in fields)


def rows(m, layer, tiles):
    return [rec('ROW', m, layer, y, ''.join(tiles.get((x, y), '.') for x in range(24))) for y in range(24)]


def sloping(t, ox, oy, w, d, want):
    """The footprint whose ground falls closest to `want` metres (a walkable slope)."""
    best, spot, got = 1e9, (8, 8), 0
    for y in range(3, 24 - d - 5):
        for x in range(5, 24 - w - 4):
            g = [t.ground(ox + x + i + .5, oy + y + j + .5) for i in range(w) for j in range(d)]
            if abs(max(g) - min(g) - want) < best:
                best, spot, got = abs(max(g) - min(g) - want), (x, y), max(g) - min(g)
    return spot, got


def steepest(t, ox, oy, w, d):
    best, spot = -1, (8, 8)
    for y in range(3, 24 - d - 5):
        for x in range(5, 24 - w - 4):
            g = [t.ground(ox + x + i + .5, oy + y + j + .5) for i in range(w) for j in range(d)]
            if max(g) - min(g) > best:
                best, spot = max(g) - min(g), (x, y)
    return spot, best


def main():
    t = Terrain(**TERRAIN)
    out = ['META_LEVEL_NAME: Floors lab', '# Written by tools/dev/floors_fixtures.py for the floors lab tube.',
           '--- CUSTOM_CAMPAIGN_DATA_START ---', 'FORMAT|2', 'CAMPAIGN|Floors lab|0',
           rec('TERRAIN', TERRAIN['seed'], float(TERRAIN['base']), float(TERRAIN['amplitude']), float(TERRAIN['scale']), TERRAIN['roughness'])]
    labs = {}

    # ---- Map 0: the hall. Stairs and a gallery (finishes), a raised room, laid flooring.
    hall = []
    tiles = {(x, y): '#' for x in range(24) for y in range(24) if x in (0, 23) or y in (0, 23)}
    for plain in (False, True):
        pass
    hall.append(rec('MAP', 0, 'Hall', 0, 0, 4, 12, 0, 0, 5, .55, 'industrial_night', 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0))
    hall.append('LAYER|0|0|Floor|0|0')
    # The raised room: walls x 13-20, y 13-20, a door on its north wall at x 16-18.
    room = {(x, y): '#' for x in range(13, 21) for y in range(13, 21) if x in (13, 20) or y in (13, 20)}
    for x in (16, 17):
        room.pop((x, 13), None)
    tiles.update(room)
    hall += rows(0, 0, tiles)
    hall.append(rec('LAYER', 0, 1, 'Gallery', 3, .3) + '|deck=51|edge=31|railing=19')
    deck = {(x, y): '=' for x in range(3, 11) for y in range(17, 22)}
    hall += rows(0, 1, deck)
    for x, y in ((4, 4), (12, 4), (20, 4), (4, 12), (12, 12), (20, 12), (4, 20), (12, 20), (20, 20)):
        hall.append(rec('LIGHT', 0, x, y, 4.85, 0, 0, 1.2, 12))
    # A stair up to the gallery with finishes: oak treads, brick risers, red stringers.
    hall.append(rec('STAIR', 0, 3, 13, 6, 17, 0, 3, 14, 1, 1, 0, .12, 1) + '|tread=51|riser=31|stringer=19')
    # The same stair plain, beside it (the look before finishes).
    hall.append(rec('STAIR', 0, 7, 13, 10, 17, 0, 3, 14, 1, 1, 0, .12, 1))
    # The raised room: 0.6 m up on a brick plinth with a stone cap; its door gets steps.
    hall.append(rec('STRUCT', 0, 13, 13, 21, 21, 0, .1, 0, 50) + '|footing=foundation|raise=0.6|base=31|cap=33|capwidth=0.12')
    hall.append(rec('DOOR', 0, 16, 18, 13.5, 0, 0, 0, 1, 0, -1, '', '', '', 1, '', '', '', '', '', '', 0))
    # Laid flooring: plank with a timber trim, tile, carpet, grating, a slab of cast concrete.
    hall.append(rec('STRUCT', 0, 13, 2, 17, 6, 0, .03, 0, 51) + '|trim=32|trimwidth=0.12')
    hall.append(rec('STRUCT', 0, 18, 2, 22, 6, 0, .03, 0, 50) + '|trim=33|trimwidth=0.1')
    hall.append(rec('STRUCT', 0, 13, 7, 17, 11, 0, .02, 0, 23))
    hall.append(rec('STRUCT', 0, 18, 7, 22, 11, 0, .05, 0, 46))
    out += ['', '# ==== Map 0: the hall ===='] + hall
    labs['floors-finishes'] = dict(map=0, camera=(12.5, 9.5, 2.4, 130.6, -12), room=None, shot='finishes')
    labs['floors-raised'] = dict(map=0, camera=(17, 7.6, 3.4, 90, -22), room=None, shot='raised')
    labs['floors-flooring'] = dict(map=0, camera=(17.5, 11.6, 2.6, -90, -32), room=None, shot='flooring')

    # ---- Maps 1-5: the land. The same room on each map's steepest slope.
    modes = [('old', None), ('pad', 'pad'), ('foundation', 'foundation'), ('stilts', 'stilts'), ('stepped', 'stepped')]
    for i, (name, mode) in enumerate(modes, 1):
        ox, oy = 24 * i, 0
        w, d = (12, 4) if mode == 'stepped' else (8, 6)
        (x0, y0), spread = steepest(t, ox, oy, w, d)
        m = [rec('MAP', i, 'Land ' + name, ox, oy, x0 - 2, y0 + d / 2, 0, 1, 2.8, .2, 'brutal_wasteland', 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0), rec('LAYER', i, 0, 'Ground', 0, 0)]
        walls = {}
        if mode not in ('stilts', 'stepped'):
            walls = {(x, y): '#' for x in range(x0, x0 + w) for y in range(y0, y0 + d) if x in (x0, x0 + w - 1) or y in (y0, y0 + d - 1)}
            # The door on the lowest side (south or north, whichever the land falls to).
            south = t.ground(ox + x0 + w / 2, oy + y0 + d + 1) < t.ground(ox + x0 + w / 2, oy + y0 - 1)
            dy = y0 + d - 1 if south else y0
            for x in (x0 + 3, x0 + 4):
                walls.pop((x, dy), None)
        m += rows(i, 0, walls)
        if mode is None:
            # The old way: the Floor tool's cells on the land, each grounded by itself.
            for y in range(y0 + 1, y0 + d - 1):
                for x in range(x0 + 1, x0 + w - 1):
                    m.append(rec('STRUCT', i, x, y, x + 1, y + 1, 0, .02, 0, 50))
        elif mode == 'pad':
            m.append(rec('STRUCT', i, x0, y0, x0 + w, y0 + d, 0, .1, 0, 50) + '|footing=pad|snap=mean')
        elif mode == 'foundation':
            m.append(rec('STRUCT', i, x0, y0, x0 + w, y0 + d, 0, .1, 0, 51) + '|footing=foundation|snap=high|raise=0.3|base=31|cap=33')
        elif mode == 'stilts':
            m.append(rec('STRUCT', i, x0, y0, x0 + w, y0 + d, 0, .12, 0, 51) + '|footing=stilts|snap=high|raise=0.4|base=32')
        else:
            m.append(rec('STRUCT', i, x0, y0, x0 + w, y0 + d, 0, .1, 0, 42) + '|footing=stepped|snap=mean|terrace=3|base=42')
        if walls:
            m.append(rec('DOOR', i, x0 + 3, x0 + 5, dy + .5, 0, 0, 0, 1, 0, -1, '', '', '', 1, '', '', '', '', '', '', 0))
        out += ['', '# ==== Map %d: the land, %s (ground spread %.1f m) ====' % (i, name, spread)] + m
        # The camera: off the footprint's low corner, looking across it.
        cx, cy = x0 + w / 2, y0 + d / 2
        # Downhill from the footprint, a little to the side: the low side and its edge in view.
        gx = t.height(ox + cx + 3, oy + cy) - t.height(ox + cx - 3, oy + cy)
        gy = t.height(ox + cx, oy + cy + 3) - t.height(ox + cx, oy + cy - 3)
        n = math.hypot(gx, gy) or 1
        ux, uy = -gx / n, -gy / n
        ex = min(23, max(1, cx + ux * 10 - uy * 5))
        ey = min(23, max(1, cy + uy * 10 + ux * 5))
        yaw = math.degrees(math.atan2(cy - ey, cx - ex))
        ground = max(t.ground(ox + ex, oy + ey), t.ground(ox + cx, oy + cy))
        if mode == 'stilts':
            # Low, under the deck's high side looking across its posts.
            ex, ey = min(23, max(1, cx + ux * 6)), min(23, max(1, cy + uy * 6))
            yaw = math.degrees(math.atan2(cy - ey, cx - ex))
            labs['floors-land-' + name] = dict(map=i, camera=(ex, ey, -999, yaw, 4), shot='land-' + name, room=None)
            continue
        labs['floors-land-' + name] = dict(map=i, camera=(ex, ey, ground + 4.5, yaw, -22), shot='land-' + name,
                                          room=(x0 + 1, y0 + 1, x0 + w - 1, y0 + d - 1) if mode in ('pad', 'foundation', None) else None,
                                          level=mode in ('pad', 'foundation'), sealed=mode in ('pad', 'foundation'))
    # ---- Map 6: paths and patios on the land: a path that follows the slope (drape) with a
    # stone edge, and a patio made level (a pad) with a brick edge.
    i = 6; ox, oy = 24 * i, 0
    (x0, y0), spread = sloping(t, ox, oy, 3, 12, 2)
    m = [rec('MAP', i, 'Land paths', ox, oy, 2, 2, 0, 1, 2.8, .2, 'brutal_wasteland', 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0),
         rec('LAYER', i, 0, 'Ground', 0, 0)] + rows(i, 0, {})
    m.append(rec('STRUCT', i, x0, y0, x0 + 2, y0 + 12, 0, .04, 0, 31) + '|drape=1|trim=33|trimwidth=0.2')
    px = x0 + 4 if x0 + 10 < 24 else x0 - 7
    m.append(rec('STRUCT', i, px, y0 + 3, px + 5, y0 + 8, 0, .15, 0, 50) + '|footing=pad|cap=31|capwidth=0.2')
    out += ['', '# ==== Map 6: paths and patios (ground spread %.1f m) ====' % spread] + m
    cx, cy = x0 + 1, y0 + 6
    # From the path's foot, up it, the patio beside.
    lo_end = y0 + 12 if t.ground(ox + x0 + 1, oy + y0 + 12) < t.ground(ox + x0 + 1, oy + y0) else y0
    ex, ey = x0 + 1 + (2.5 if px > x0 else -2.5), min(23, max(1, lo_end + (3 if lo_end > y0 else -3)))
    cy = y0 + 6
    labs['floors-land-paths'] = dict(map=i, camera=(ex, ey, t.ground(ox + ex, oy + ey) + 2.2, math.degrees(math.atan2(cy - ey, x0 + 1 - ex)), -14),
                                     shot='land-paths', room=None)
    out.append('--- CUSTOM_CAMPAIGN_DATA_END ---')
    with open(os.path.join(OUT, 'floors.rmc'), 'w', encoding='ascii', newline='\r\n') as fh:
        fh.write('\n'.join(out) + '\n')
    # The same campaign as it would be without this work (the "before" pictures): no named
    # finishes on stairs and storeys, no flooring, no footings in the hall or on the paths map.
    plain = []
    for line in out:
        cut = line.split('|')
        if cut[0] == 'STRUCT' and cut[1] in ('0', '6'):
            continue
        if cut[0] in ('STAIR', 'LAYER'):
            line = '|'.join(x for x in cut if '=' not in x)
        plain.append(line)
    with open(os.path.join(OUT, 'floors-plain.rmc'), 'w', encoding='ascii', newline='\r\n') as fh:
        fh.write('\n'.join(plain) + '\n')
    for name in ('floors-finishes', 'floors-flooring', 'floors-raised', 'floors-land-paths'):
        l = dict(labs[name]); l['plain'] = True; l['shot'] = l['shot'] + '-before'; labs[name + '-before'] = l
    for name, l in labs.items():
        x, y, z, yaw, pitch = l['camera']
        text = ['# %s (tools/dev/floors_fixtures.py). Tube: floors.' % name, 'world custom "lab/fixtures/%s"' % ('floors-plain.rmc' if l.get('plain') else 'floors.rmc'), 'map %d' % l['map'],
                'camera %s %s %s %s %s' % (f(float(x)), f(float(y)), f(float(z)), f(float(yaw)), f(round(math.radians(pitch) * 140, 1))), 'size 1280 720', 'frames 30',
                'floors-shot %s' % l['shot']]
        if l.get('room'):
            text.append('floors-room %s %s %s %s' % tuple(f(float(v)) for v in l['room']))
            if l.get('level'):
                text.append('floors-level 1')
            if l.get('sealed'):
                text.append('floors-sealed 1')
        with open(os.path.join(OUT, name + '.lab'), 'w', encoding='ascii', newline='\n') as fh:
            fh.write('\n'.join(text) + '\n')
    # The Starter Map's Hillside (make_starter_map.py hillside(), map 126): from below, and in the house.
    for name, cam, shot in (('floors-starter-hillside', (18, 23, -999, -133, -12), 'starter-hillside'),
                            ('floors-starter-house', (9.2, 12.4, -999, -146, -6), 'starter-house'),
                            ('floors-starter-stilts', (18.5, 16.5, -999, -128, 2), 'starter-stilts')):
        x, y, z, yaw, pitch = cam
        with open(os.path.join(OUT, name + '.lab'), 'w', encoding='ascii', newline='\n') as fh:
            fh.write('\n'.join(['# %s (tools/dev/floors_fixtures.py): the Starter Map Hillside. Tube: floors.' % name,
                                'world custom "custom maps/Starter Map.rmc"', 'map 126',
                                'camera %s %s %s %s %s' % (f(float(x)), f(float(y)), f(float(z)), f(float(yaw)), f(round(math.radians(pitch) * 140, 1))),
                                'size 1280 720', 'frames 30', 'floors-shot %s' % shot]) + '\n')
    print('wrote floors.rmc and %d scenes' % (len(labs) + 3))


if __name__ == '__main__':
    main()
