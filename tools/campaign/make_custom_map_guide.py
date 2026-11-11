"""Write custom maps/GUIDE.html: how to make a custom map, with diagrams and the Starter Map tour.

  python tools/campaign/make_custom_map_guide.py [--shots]

--shots renders the guide's screenshots first (RawMetal.exe --zone-pack on the Starter
Map) into custom maps/guide/. Without it the existing images are reused. The diagrams
are inline SVG and the tables come from the same data as tools/campaign/make_starter_map.py,
so regenerate both together whenever models are added.
"""
import html, json, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_starter_map as starter  # noqa: E402

ROOT = starter.ROOT
MAPS = os.path.join(ROOT, 'custom maps')
IMAGES = os.path.join(MAPS, 'guide')
PACK = os.path.join(MAPS, 'Starter Map.rmc')

# name, map name, x, y, yaw degrees, pitch, lift: map-local views of the Starter Map.
SHOTS = [
    ('welcome', 'Welcome', 12, 2.2, 90, -.05, .3, 'Spawn: the welcome room. Material pillars, stairs, doors, water and pickups.'),
    ('lights', 'Lights', 12, 15, -90, 0, 0, 'Lights: one booth per kind of lamp. Walk in to see each one alone.'),
    ('hazards', 'Hazards', 1.5, 12, 0, -.12, 1.0, 'Hazards: the eight hazard kinds fenced off, with a compactor, smoke, glass and cables.'),
    ('life', 'Life', 12, 8.6, 90, -.1, 0, 'Life: every creature asleep in a pen (they wake if hurt) and every actor body behind.'),
    ('actors', 'Life', 12, 15.2, 90, 0, 0, 'Actor bodies stand in a row, each labelled with its ACTOR visual number.'),
    ('levels', 'Levels', 2.5, 17, -15, .02, .4, 'Levels: a deck (layer 1, 3.5 m) over the ground floor (layer 0), the stair between them.'),
    ('props', 'Props', 12, 1.2, 90, -.25, 1.2, 'Props: every loose object you can pick up and throw, on plinths.'),
    ('medical', 'Medical', 12, 1.2, 90, -.2, 1.0, 'A model row (Medical). The HUD top-left names the model you stand at.'),
    ('industrial', 'Industrial', 12, 1.2, 90, -.1, 2.0, 'Industrial: whole buildings, shown at reduced scale (the sign says by how much).'),
    ('ground', 'Ground paint', 12, 1.5, 90, -.5, 4.0, 'Outdoors: the nine GROUND paints on a TERRAIN_FLAT pad, under the zone sky.'),
    ('terrain', 'Terrain', 2.5, 21, -40, -.06, 1.2, 'Terrain: rolling TERRAIN noise, a plateau and a road levelled with TERRAIN_FLAT.'),
    ('grove', 'Grove', 12, 1.0, 90, -.05, .5, 'Grove: trees and undergrowth as FIXTURE records with base 0, standing on the ground.'),
    ('seams', 'Seams', 12, 7, -90, -.3, 2.5, 'A north-south seam: the dirt strip is the edge between two maps, open on both.'),
    ('seam-ew', 'Seams', 15, 12, 0, -.2, 2.0, 'An east-west seam (dirt strip), and a closed edge (mud) at the far end of the next map.'),
    ('doorway', 'Ground paint', 1.5, 6, -90, .05, .3, 'Indoors to outdoors: the model row keeps its wall, with # tiles, except a doorway.'),
    ('scripts', 'Scripts', 8, 12.5, -90, -.22, 1.0, 'Scripts: three pads ADD to a counter and a MESSAGE shows it; the lamp lights at 3.'),
    ('scripts-vault', 'Scripts', 18.5, 13.5, -90, -.08, .4, 'The vault door opens on the lever AND three crystals: SCRIPT_REQUIRE.'),
    ('scripts-south', 'Scripts', 12, 10.2, 92, -.38, .6, 'Match any (two switches), a repeat doormat, a Huntsman pen (@kills, @remaining) and a gas strip (@health).'),
]

SECTION_ROWS = [('WELCOME, LIGHTS, HAZARDS, LIFE', 0)]

CSS = """
body{background:#15171a;color:#d8dade;font:15px/1.55 'Segoe UI',Arial,sans-serif;margin:0}
main{max-width:1060px;margin:0 auto;padding:28px 34px 80px}
h1{font-size:34px;margin:0 0 4px;letter-spacing:.04em}h2{margin-top:46px;border-bottom:2px solid #3c8cd8;padding-bottom:4px}
h3{margin-top:28px;color:#9fc8f0}a{color:#7fb8f0}code,pre{font-family:Consolas,monospace;background:#22262b;border-radius:4px}
code{padding:1px 5px}pre{padding:12px 14px;overflow-x:auto;line-height:1.4;border-left:3px solid #3c8cd8}
table{border-collapse:collapse;margin:10px 0 18px;width:100%}td,th{border:1px solid #33383f;padding:4px 8px;text-align:left;vertical-align:top}
th{background:#22262b}tr:nth-child(even) td{background:#1a1d21}.lede{color:#a9adb5;font-size:17px}
figure{margin:18px 0}figcaption{color:#9aa0a8;font-size:13px;margin-top:4px}img{max-width:100%;border-radius:4px}
.shots{display:grid;grid-template-columns:1fr 1fr;gap:14px}.note{background:#1f2a36;border-left:3px solid #3c8cd8;padding:8px 14px;margin:12px 0}
.warn{background:#362222;border-left:3px solid #d84030;padding:8px 14px;margin:12px 0}nav a{margin-right:14px}
svg text{font-family:Consolas,monospace}
"""


def esc(s):
    return html.escape(str(s))


# ---- diagrams -----------------------------------------------------------------------------------
def svg(w, h, body):
    return '<svg viewBox="0 0 %d %d" width="100%%" style="max-width:%dpx;background:#1b1e22;border-radius:6px">%s</svg>' % (w, h, w, body)


def text(x, y, s, size=13, fill='#d8dade', anchor='start', weight='normal'):
    return '<text x="%g" y="%g" font-size="%g" fill="%s" text-anchor="%s" font-weight="%s">%s</text>' % (x, y, size, fill, anchor, weight, esc(s))


def rect(x, y, w, h, fill, stroke='none', rx=3):
    return '<rect x="%g" y="%g" width="%g" height="%g" fill="%s" stroke="%s" rx="%g"/>' % (x, y, w, h, fill, stroke, rx)


def arrow(x1, y1, x2, y2, color='#e0b030', width=2):
    return ('<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s" stroke-width="%g" marker-end="url(#ah)"/>' % (x1, y1, x2, y2, color, width))


DEFS = '<defs><marker id="ah" markerWidth="10" markerHeight="8" refX="9" refY="4" orient="auto"><path d="M0,0 L10,4 L0,8 z" fill="#e0b030"/></marker></defs>'


def folder_diagram():
    b = DEFS
    rows = [(0, 'DepthWorks/', 'the folder you unzipped', '#3c8cd8'),
            (1, 'DepthWorks.exe', 'the game', '#d8dade'),
            (1, 'RawMetal-custom-maps.txt', 'written by the game: OK or ERROR per map file', '#e0b030'),
            (1, 'custom maps/', 'PUT MAP FILES HERE', '#50b060'),
            (2, 'Starter Map.rmc', 'the debug world: every part, lined up', '#d8dade'),
            (2, 'GUIDE.html', 'this guide', '#d8dade'),
            (2, 'guide/', 'pictures for this guide (ignored by the game)', '#8a9099'),
            (2, 'My Map.rmc', 'yours: any name ending in .rmc', '#50b060')]
    y = 34
    for depth, name, note, color in rows:
        x = 24 + depth * 34
        if depth: b += '<path d="M%g %g V%g H%g" stroke="#4a5058" fill="none"/>' % (x - 20, y - 22, y - 5, x - 6)
        icon = '#c8a050' if name.endswith('/') else '#7a8088'
        b += rect(x, y - 15, 16, 13, icon, rx=2) + text(x + 24, y - 4, name, 14, color, weight='bold') + text(430, y - 4, note, 13, '#9aa0a8')
        y += 30
    return svg(900, y, b)


def script_diagram():
    """An event left to right: when it is checked, what must hold, what it does."""
    b = DEFS
    cols = [(14, 'WHEN', '#3c8cd8', ['SCRIPT_TRIGGER where', 'box: you stand in it', 'map: anywhere on its map', 'campaign: on any map']),
            (250, 'IF', '#e0b030', ['SCRIPT_EVENT requireState', '+ SCRIPT_REQUIRE x 8', 'state = != < <= > >=', '@kills @remaining', '@health @map',
                                    'match all (AND) / any (OR)']),
            (486, 'THEN', '#50b060', ['SCRIPT_ACTION x 16', 'STATE   ADD  (counters)', 'ITEM    TAKE (keys)', 'MESSAGE "{state} left"',
                                      'OBJECTIVE CHECKPOINT', 'SHAKE COMPLETE']),
            (722, 'AGAIN?', '#d860a0', ['repeat 0: once ever', 'repeat 1: each time the', 'gates turn true again', '(re-arms when they fail)'])]
    for x, title, color, lines in cols:
        b += rect(x, 14, 220, 186, color + '26', color, 6) + text(x + 110, 38, title, 17, color, 'middle', 'bold')
        for k, line in enumerate(lines):
            b += text(x + 12, 64 + k * 22, line, 12.5)
        if x < 722: b += arrow(x + 221, 107, x + 235, 107)
    b += text(14, 226, 'Every frame, for every armed event: WHEN you are in its scope and IF its gates pass, THEN its actions run in order.', 13, '#9aa0a8')
    return svg(956, 240, b)


def anatomy_diagram():
    b = ''
    blocks = [('META_LEVEL_NAME: My Map ...', 'header: free text, ignored', '#30343a', 1),
              ('--- CUSTOM_CAMPAIGN_DATA_START ---', 'start marker (exact)', '#3c8cd8', 1),
              ('CAMPAIGN|My Map|0', 'name shown in CUSTOM MAPS, start map', '#50b060', 1),
              ('MAP|0|First room|0|0|12|12|0|0|3.5|...', 'map 0: origin, spawn, ceiling, open edges', '#c8a050', 1),
              ('LAYER|0|0|Floor|0|0', 'a floor level', '#c8a050', 1),
              ('ROW|0|0|0|########################', '24 rows of 24 tiles', '#c8a050', 3),
              ('STRUCT|... FIXTURE|... LIGHT|... ZONE|...', 'everything in map 0', '#9070d0', 3),
              ('MAP|1|Second room|24|0|...', 'map 1 sits beside it (origin 24,0)', '#c8a050', 1),
              ('...', '', '#30343a', 1),
              ('--- CUSTOM_CAMPAIGN_DATA_END ---', 'end marker (exact)', '#3c8cd8', 1)]
    y = 12
    for line, note, color, h in blocks:
        hh = 22 * h
        b += rect(14, y, 470, hh - 4, color + '40', color) + text(24, y + 15, line, 13)
        if note: b += text(500, y + 15, '< ' + note, 13, '#9aa0a8')
        y += hh
    return svg(900, y + 8, b)


def coordinate_diagram():
    s, ox, oy = 15, 60, 40
    b = DEFS
    for i in range(25):
        c = '#3a4048' if i % 6 else '#56606c'
        b += '<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s"/>' % (ox + i * s, oy, ox + i * s, oy + 24 * s, c)
        b += '<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s"/>' % (ox, oy + i * s, ox + 24 * s, oy + i * s, c)
    for i in (0, 6, 12, 18, 24):
        b += text(ox + i * s, oy - 6, i, 11, '#9aa0a8', 'middle') + text(ox - 8, oy + i * s + 4, i, 11, '#9aa0a8', 'end')
    b += arrow(ox, oy + 24 * s + 22, ox + 90, oy + 24 * s + 22) + text(ox + 96, oy + 24 * s + 26, '+x (east)', 13, '#e0b030')
    b += arrow(ox - 34, oy, ox - 34, oy + 90) + text(ox - 46, oy + 110, '+y', 13, '#e0b030')
    b += text(ox + 12 * s, oy - 22, 'north edge (y = 0)', 12, '#9aa0a8', 'middle')
    b += text(ox + 12 * s, oy + 24 * s + 48, 'south edge (y = 24)', 12, '#9aa0a8', 'middle')
    # a STRUCT box, a FIXTURE and a spawn
    b += rect(ox + 3 * s, oy + 4 * s, 5 * s, 2 * s, '#9070d080', '#9070d0', 0) + text(ox + 3 * s, oy + 4 * s - 4, 'STRUCT 3 4 8 6', 11, '#c0a8f0')
    b += '<circle cx="%g" cy="%g" r="6" fill="#50b060"/>' % (ox + 12 * s, oy + 18 * s) + text(ox + 12 * s + 10, oy + 18 * s + 4, 'spawn 12 18', 11, '#90d0a0')
    b += rect(ox + 16 * s, oy + 9 * s, 2 * s, 1.2 * s, '#c8a05080', '#c8a050', 0) + text(ox + 16 * s, oy + 9 * s - 4, 'FIXTURE at 17 9.6', 11, '#e0c080')
    # yaw compasses
    def compass(cx, cy, title, labels):
        out = text(cx, cy - 78, title, 13, '#d8dade', 'middle', 'bold') + '<circle cx="%g" cy="%g" r="52" fill="none" stroke="#4a5058"/>' % (cx, cy)
        for dx, dy, lab in labels:
            out += arrow(cx, cy, cx + dx * 46, cy + dy * 46) + text(cx + dx * 66, cy + dy * 66 + 4, lab, 12, '#e0b030', 'middle')
        return out
    b += compass(560, 140, 'spawn, LIGHT, ACTOR yaw', [(1, 0, '0'), (0, 1, '1.571'), (-1, 0, '3.142'), (0, -1, '-1.571')])
    b += compass(560, 330, 'SIGN, TERMINAL yaw', [(0, 1, '0'), (0, -1, '3.142'), (-1, 0, '-1.571'), (1, 0, '1.571')])
    b += text(450, 420, 'Yaw is in radians. Arrows show the way', 12, '#9aa0a8') + text(450, 436, 'the thing faces on this top-down plan.', 12, '#9aa0a8')
    return svg(720, oy + 24 * s + 64, b)


def tiles_diagram():
    rows = ['##########', '#........#', '#..C..B..#', '#........#', '#..T.....#', '##########']
    colors = {'#': '#6a6f78', '.': '#2c3036', 'C': '#c8a050', 'B': '#d84030', 'T': '#3c8cd8', '=': '#9070d0'}
    s, ox, oy = 30, 20, 20
    b = ''
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            b += rect(ox + x * s, oy + y * s, s - 2, s - 2, colors[ch], rx=2) + text(ox + x * s + s / 2 - 1, oy + y * s + 19, ch, 13, '#111' if ch != '.' else '#6a707a', 'middle', 'bold')
    legend = [('#', 'wall: solid from the floor to the ceiling'), ('.', 'floor (or nothing, on an upper layer)'),
              ('C', 'crate block, 0.6 m high'), ('B', 'explosive barrel, 1.1 m'), ('T', 'tank block, 2.6 m'),
              ('=', 'walkable deck: upper layers with thickness > 0')]
    for k, (ch, note) in enumerate(legend):
        y = oy + k * 30
        b += rect(360, y, 26, 26, colors[ch], rx=2) + text(373, y + 18, ch, 13, '#111', 'middle', 'bold') + text(396, y + 18, note, 13)
    b += text(ox, oy + 6 * s + 22, 'ROW|0|0|2|#..C..B..#  ... each real row is exactly 24 characters', 12, '#9aa0a8')
    return svg(900, oy + 6 * s + 34, b)


def heights_diagram():
    b = DEFS
    gx, gy, k = 70, 270, 52       # ground line, metres to pixels
    b += '<line x1="40" y1="%d" x2="860" y2="%d" stroke="#8a9099" stroke-width="2"/>' % (gy, gy)
    b += '<line x1="40" y1="%d" x2="860" y2="%d" stroke="#56606c" stroke-dasharray="6 4"/>' % (gy - 4.5 * k, gy - 4.5 * k)
    b += text(46, gy - 4.5 * k - 6, 'ceiling (the MAP ceiling field, e.g. 4.5)', 12, '#9aa0a8') + text(46, gy + 18, 'floor = layer elevation (0)', 12, '#9aa0a8')
    for m in range(5):
        b += text(30, gy - m * k + 4, '%d m' % m, 11, '#6a707a', 'end')
    # STRUCT
    b += rect(120, gy - 1.4 * k, 70, 1.4 * k, '#9070d080', '#9070d0', 0) + text(155, gy - 1.4 * k - 8, 'STRUCT', 12, '#c0a8f0', 'middle')
    b += text(200, gy - 1.4 * k + 4, 'top 1.4', 11, '#c0a8f0') + text(200, gy - 4, 'bottom 0', 11, '#c0a8f0')
    # plinth + FIXTURE
    b += rect(330, gy - .75 * k, 90, .75 * k, '#56606c', '#8a9099', 0) + rect(345, gy - 1.3 * k, 60, .55 * k, '#c8a05090', '#c8a050', 0)
    b += text(375, gy - 1.3 * k - 8, 'FIXTURE', 12, '#e0c080', 'middle') + text(430, gy - .75 * k + 4, 'base 0.75 (stands on the plinth)', 11, '#e0c080')
    b += text(430, gy - 1.3 * k + 10, 'height 0.55: top at 1.3', 11, '#e0c080')
    # light
    b += '<circle cx="680" cy="%g" r="9" fill="#e0b030"/>' % (gy - 4.3 * k) + text(664, gy - 4.3 * k + 4, 'LIGHT z 4.3 (just under the ceiling)', 11, '#e0b030', 'end')
    # sign
    b += rect(620, gy - 1.9 * k, 90, .5 * k, '#3c8cd8', '#7fb8f0', 2) + text(665, gy - 1.6 * k, 'SIGN z 1.65', 11, '#fff', 'middle')
    b += text(560, gy - 2.4 * k, 'z is the centre of the sign', 11, '#7fb8f0')
    b += text(46, gy + 40, 'Heights are metres above the BOTTOM layer of the map. One-layer maps: above the floor.', 13)
    return svg(900, gy + 54, b)


def record_diagram(record, labels):
    parts = record.split('|')
    b, x, y = '', 14, 30
    for k, p in enumerate(parts):
        w = 9 * max(len(p), 1) + 12
        color = ['#3c8cd8', '#50b060', '#c8a050', '#d84030', '#9070d0', '#e0b030'][k % 6]
        b += rect(x, y, w, 26, color + '50', color) + text(x + w / 2, y + 18, p, 13, '#fff', 'middle', 'bold')
        if k < len(labels) and labels[k]:
            ly = y + 46 + (k % 3) * 18
            b += '<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s"/>' % (x + w / 2, y + 26, x + w / 2, ly - 12, color)
            b += text(x + w / 2, ly, labels[k], 11, color, 'middle')
        x += w + 4
    return svg(max(900, x + 20), 130, b)


def grid_diagram(index):
    maps = index['maps']
    cols = max(m['col'] for m in maps) + 1; rows = max(m['row'] for m in maps) + 1
    s, ox, oy = 128, 20, 20
    b = ''
    colors = {'Welcome': '#3c8cd8', 'Lights': '#e0b030', 'Hazards': '#d84030', 'Life': '#50b060', 'Levels': '#40b8c0', 'Props': '#9070d0',
              'Ground paint': '#8aa040', 'Terrain': '#8aa040', 'Grove': '#8aa040', 'Seams': '#8aa040'}
    for i, m in enumerate(maps):
        x, y = ox + m['col'] * s, oy + m['row'] * 46
        base = m['name'].rstrip('0123456789 ')
        color = colors.get(base, '#56606c')
        b += rect(x, y, s - 4, 42, color + '60', color) + text(x + 6, y + 16, m['name'][:19], 11, '#fff', weight='bold')
        b += text(x + 6, y + 33, 'map %d  %d,%d' % (i, 24 * m['col'], 24 * m['row']), 10, '#c8ccd2')
    tx = ox + cols * s + 10
    b += '<circle cx="%g" cy="%g" r="5" fill="#50b060"/>' % (ox + 12 / 24 * (s - 4), oy + 6) + text(tx, oy + 26, '< you spawn in Welcome. Rooms run east.', 12, '#9aa0a8')
    b += text(tx, oy + 46 + 26, '< each set of models is one row; walk south', 12, '#9aa0a8')
    b += text(tx, oy + 92 + 26, '  through the corridor along their west side', 12, '#9aa0a8')
    last = max(m['row'] for m in maps if m['name'] in ('Ground paint', 'Seams')) if any(m['name'] == 'Seams' for m in maps) else None
    if last is not None:
        b += text(tx, oy + (last - 1) * 46 + 26, '< outdoors: ground paint, terrain, a grove', 12, '#9aa0a8')
        b += text(tx, oy + last * 46 + 26, '< seams: one of each kind, signed', 12, '#9aa0a8')
    return svg(max(900, tx + 330), oy + rows * 46 + 10, b)


def levels_diagram():
    """Side view of one map with two layers: what the Levels room builds."""
    b = DEFS
    gy, k, ox, kx = 300, 34, 60, 30          # ground line, metres to pixels (up), metres to pixels (across)
    X = lambda m: ox + m * kx
    Y = lambda z: gy - z * k
    b += rect(X(0), Y(7.5), 24 * kx, 7.5 * k, '#1f2329', '#3a4048', 0)
    b += '<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="#8a9099" stroke-width="3"/>' % (X(0), gy, X(24), gy)
    b += '<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="#56606c" stroke-dasharray="6 4"/>' % (X(0), Y(7.5), X(24), Y(7.5))
    b += text(X(.3), Y(7.5) - 6, 'MAP ceiling 7.5', 12, '#9aa0a8')
    b += rect(X(23), Y(7.5), kx, 7.5 * k, '#6a6f78', rx=0) + text(X(23.5), Y(3.9), '#', 14, '#111', 'middle', 'bold')
    for x1, x2 in ((12, 16), (18, 23)):
        b += rect(X(x1), Y(3.5), (x2 - x1) * kx, .25 * k, '#9070d0', '#c0a8f0', 0)
    b += text(X(16.5) + 15, Y(3.5) - 8, "hole: '.'", 11, '#c0a8f0', 'middle')
    b += text(X(19), Y(3.5) - 8, "'=' tiles", 12, '#c0a8f0', 'middle')
    for i in range(18):                     # the stair, rising east from x 6 to the deck edge at x 12
        z = 3.5 * (i + 1) / 18
        b += rect(X(6 + i / 3), Y(z), kx / 3 + .5, z * k, '#c8a05070', rx=0)
    b += text(X(7.5), Y(1.2), 'STAIR', 12, '#e0c080', 'middle', 'bold') + text(X(7.5), Y(.6), '0 to 3.5', 11, '#e0c080', 'middle')
    b += rect(X(19.3), Y(3.5 + .75), 1.4 * kx, .75 * k, '#c8a05090', '#c8a050', 0) + text(X(20), Y(4.6), 'base 3.5', 11, '#e0c080', 'middle')
    b += rect(X(19.3), Y(.75), 1.4 * kx, .75 * k, '#c8a05090', '#c8a050', 0) + text(X(20), Y(1.1), 'base 0', 11, '#e0c080', 'middle')
    b += text(X(24) + 12, Y(3.5) + 4, 'LAYER 1: elevation 3.5, thickness 0.25', 12, '#c0a8f0')
    b += text(X(24) + 12, Y(3.5) + 22, '(the walking surface is at 3.5;', 11, '#9aa0a8') + text(X(24) + 12, Y(3.5) + 36, ' the slab hangs 0.25 below it)', 11, '#9aa0a8')
    b += text(X(24) + 12, gy + 4, 'LAYER 0: elevation 0, thickness 0', 12, '#d8dade')
    b += text(X(24) + 12, Y(7.5) + 4, "'#' walls on layer 0 run up to the ceiling", 11, '#9aa0a8')
    b += text(X(0), gy + 26, 'x = 0 (west)', 11, '#6a707a') + text(X(24), gy + 26, 'x = 24 (east)', 11, '#6a707a', 'end')
    return svg(1100, gy + 40, b)


def stitch_diagram():
    """Four maps from above, joined edge to edge: what the open flags mean."""
    b = DEFS
    s, ox, oy = 9, 110, 50                 # pixels per metre
    maps = [(0, 0, 'map 0', '0 0', (0, 1, 0, 1)), (1, 0, 'map 1', '24 0', (0, 1, 1, 0)),
            (0, 1, 'map 2', '0 24', (1, 0, 0, 1)), (1, 1, 'map 3', '24 24', (1, 0, 1, 0))]
    for c, r, name, origin, (n, so, w, e) in maps:
        x, y = ox + c * 24 * s, oy + r * 24 * s
        b += rect(x, y, 24 * s, 24 * s, '#2c3036', '#56606c', 0)
        b += text(x + 12 * s, y + 12 * s - 6, name, 15, '#fff', 'middle', 'bold') + text(x + 12 * s, y + 12 * s + 14, 'origin ' + origin, 12, '#9aa0a8', 'middle')
        for flag, on, (tx, ty, anchor) in (('openN', n, (x + 12 * s, y + 16, 'middle')), ('openS', so, (x + 12 * s, y + 24 * s - 8, 'middle')),
                                           ('openW', w, (x + 6, y + 12 * s + 40, 'start')), ('openE', e, (x + 24 * s - 6, y + 12 * s + 40, 'end'))):
            b += text(tx, ty, '%s=%d' % (flag, on), 11, '#50b060' if on else '#d84030', anchor)
        for side, on in (('n', n), ('s', so), ('w', w), ('e', e)):
            if on: continue
            x1, y1, x2, y2 = {'n': (x, y, x + 24 * s, y), 's': (x, y + 24 * s, x + 24 * s, y + 24 * s),
                              'w': (x, y, x, y + 24 * s), 'e': (x + 24 * s, y, x + 24 * s, y + 24 * s)}[side]
            b += '<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="#d84030" stroke-width="4"/>' % (x1, y1, x2, y2)
    mid = ox + 24 * s
    b += '<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="#50b060" stroke-width="4" stroke-dasharray="10 6"/>' % (mid, oy, mid, oy + 48 * s)
    b += '<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="#50b060" stroke-width="4" stroke-dasharray="10 6"/>' % (ox, oy + 24 * s, ox + 48 * s, oy + 24 * s)
    b += text(mid, oy - 12, 'east-west seam (x = 24)', 13, '#50b060', 'middle', 'bold')
    b += text(ox - 8, oy + 24 * s + 4, 'north-south seam', 13, '#50b060', 'end', 'bold') + text(ox - 8, oy + 24 * s + 20, '(y = 24)', 12, '#50b060', 'end')
    b += arrow(ox - 60, oy + 40, ox - 60, oy + 110) + text(ox - 60, oy + 128, 'south', 11, '#e0b030', 'middle')
    tx = ox + 48 * s + 40
    for k, line in enumerate(['A seam is walkable when BOTH maps open it:', '  map 0 openE=1  and  map 1 openW=1', '  map 0 openS=1  and  map 2 openN=1',
                              '', 'The edge tiles on both sides must be floor (.).', 'A wall (#) in that row or column closes', 'that part of the seam, like a wall.',
                              '', 'Red edges are closed: openX=0, or no map', 'there. Indoors draw them as # walls;', 'outdoors the flag alone stops you.',
                              '', 'Origins are multiples of 24 so maps touch', 'exactly; overlapping maps fail to load.']):
        b += text(tx, oy + 14 + k * 20, line, 13, '#d8dade' if line and not line.startswith('  ') else '#9fc8f0')
    return svg(tx + 380, oy + 48 * s + 20, b)


def terrain_diagram():
    """Side profile of the outdoor ground: noise, a flat pad with its blend, and an indoor seam."""
    import math
    b = DEFS
    gy, k, ox, kx = 210, 22, 40, 11
    X = lambda m: ox + m * kx
    Y = lambda z: gy - z * k
    def height(m):
        noise = 1 + 2.5 * (.6 * math.sin(m / 9.5) + .4 * math.sin(m / 3.7 + 1))
        for a, c, h, blend in ((0, 6, 0, 8), (40, 52, 3.5, 5)):
            d = max(a - m, 0, m - c)
            t = max(0.0, 1 - d / blend)
            t = t * t * (3 - 2 * t)
            noise = noise * (1 - t) + h * t
        return noise
    pts = ' '.join('%g,%g' % (X(m / 2), Y(height(m / 2))) for m in range(0, 145))
    b += '<polygon points="%g,%g %s %g,%g" fill="#4a5a3a" stroke="#8aa040" stroke-width="2"/>' % (X(0), gy + 60, pts, X(72), gy + 60)
    for z, label in ((1, 'base 1'), (3.5, 'base + amplitude'), (-1.5, 'base - amplitude')):
        b += '<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="#56606c" stroke-dasharray="5 4"/>' % (X(0), Y(z), X(72), Y(z)) + text(X(72) + 6, Y(z) + 4, label, 11, '#9aa0a8')
    b += rect(X(-3.5), Y(3), 3.5 * kx, 3 * k, '#3a4048', '#8a9099', 0) + text(X(-1.75), Y(3) - 6, 'indoor map', 11, '#9aa0a8', 'middle')
    b += text(X(3), Y(0) - 8, 'TERRAIN_FLAT 0', 11, '#e0b030', 'middle') + text(X(3), Y(0) + 16, '(meets the floor)', 10, '#e0b030', 'middle')
    b += text(X(46), Y(3.5) - 26, 'TERRAIN_FLAT 3.5, blend 5', 11, '#e0b030', 'middle')
    b += '<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="#e0b030" stroke-width="3"/>' % (X(40), Y(3.5), X(52), Y(3.5))
    b += arrow(X(35), Y(5.3), X(38), Y(3.9)) + text(X(30), Y(5.6), 'blend: the slope back to the noise', 10, '#e0b030', 'middle')
    tree = X(62)
    hz = Y(height(62))
    b += '<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="#6a5030" stroke-width="5"/>' % (tree, hz, tree, hz - 46)
    b += '<circle cx="%g" cy="%g" r="18" fill="#3c6a30"/>' % (tree, hz - 54) + text(tree, hz - 80, 'FIXTURE base 0', 11, '#90d0a0', 'middle')
    b += text(tree, hz + 18, 'stands on the ground here', 10, '#90d0a0', 'middle')
    b += text(X(0), gy + 82, 'Outdoor maps: every height (base, z, bottom, top) is measured from the ground under the thing, not from 0.', 13)
    return svg(X(72) + 140, gy + 96, b)


# ---- record reference ---------------------------------------------------------------------------
RECORDS = [
    ('Campaign and maps', [
        ('CAMPAIGN|name|startMap', 'Once per file. The name appears in the CUSTOM MAPS menu; startMap is the map you spawn in.'),
        ('MAP|index|name|originX|originY|spawnX|spawnY|spawnZ|environment|ceiling|ambient|skybox|openN|openS|openW|openE|lift[|residency|floorMaterial|music|spawnYaw|spawnPitch|startUnarmed|hazeR|hazeG|hazeB|hazeDensity|wetness]',
         'One per map, numbered 0, 1, 2... with no gaps. origin places the 24 x 24 m map in the world: maps touch at multiples of 24. '
         'spawn is in map metres. environment 0 indoor, 1 outdoor. ceiling is the absolute ceiling height (indoors). ambient 0-1 is the base light (0.3 is a good start). '
         'skybox: industrial_night indoors; zone_overcast (or brutal_wasteland) outdoors, which also switches on the campaign TERRAIN. '
         'openN/S/W/E 1 lets you walk across that edge into the neighbouring map. lift 0. '
         'Optional tail: residency -1, floorMaterial 0 concrete / 1 carpet, music 0, spawnYaw radians, spawnPitch, startUnarmed 0/1. '
         'haze R G B (0-1) and density (0-0.1) give the map its own air: 0.01 is enough for lamps to hang cones of light in it. '
         'wetness 0-1 is how damp the floors are: a sheen and puddles that reflect the lamps (0.5 in the Lights hall); leave the haze fields empty to set only wetness.'),
        ('SURFACE|map|wall|floor|ceiling', 'Wall, floor and ceiling finish for the whole map; -1 keeps the default. See the surface table.'),
        ('LAYER|map|layer|name|elevation|thickness', 'A floor level. Layer 0 is the ground (elevation 0, thickness 0). Upper layers use thickness > 0 and = tiles for walkable deck. Upper # cells form walls from that layer to the map ceiling; upper . and _ cells stay empty.'),
        ('ROW|map|layer|row|24 characters', 'One line of the tile grid, row 0 (north) to 23 (south). Exactly 24 characters: see the tile diagram.')]),
    ('Outdoors (campaign-wide, world metres)', [
        ('TERRAIN|seed|base|amplitude|scale|roughness', 'The ground of every outdoor map. seed picks the pattern; base is the mean height (-10 to 16); amplitude how far hills rise and dip from it (0-12); '
         'scale the metres between hills (6-400); roughness 0 smooth to 0.9 craggy. One per file.'),
        ('TERRAIN_FLAT|x1|y1|x2|y2|height|blend', 'Levels the ground inside the box to height, sloping back to the noise over blend metres. Use for roads, building pads, and wherever an outdoor map meets an indoor one (height 0). World metres: map origin + map x, y.'),
        ('GROUND|x1|y1|x2|y2|kind|edge', 'Paints the ground: 0 short grass, 1 leafy grass, 2 clover, 3 wild, 4 dry dirt, 5 forest litter, 6 gravel, 7 mud, 8 meadow. edge 0-8 metres of soft border.'),
        ('HORIZON|depth|spacing|rise|model,model,...', 'Woods past every closed outdoor edge: a band depth metres deep, one model every spacing metres, the land rising rise metres away from you.'),
        ('STORM|calmMs|warningMs|stormMs|damage|ash', 'An ash storm cycle for the whole campaign: calm, a warning, then a storm doing damage per second to anyone outside a SHELTER. ash 0-1 is the haze between storms.'),
        ('SHELTER|map|x1|y1|x2|y2|bottom|top', 'Map metres. Under this box the storm does no harm. Heights are from the ground at its centre.')]),
    ('Building', [
        ('STRUCT|map|x1|y1|x2|y2|bottom|top|rail|material[|solid]', 'A box: walls, floors, platforms, plinths. x1<x2, y1<y2 in map metres; bottom/top are heights. rail 1 draws it as a thin railing. solid 0 lets you walk through it.'),
        ('PRIM|map|kind|x|y|z|w|d|h|facing|material[|solid]', 'A ready-made building part, centred at x,y and standing on z. w runs across it, d along its facing, h up; '
         'facing 0 +y (south), 1 +x (east), 2 -y (north), 3 -x (west) is the way it rises or opens. Give 0 for a size and it takes the default, sized for the 1.6 m player. '
         'Ramps, columns, arches, vaults, domes, struts and roundels are true curved or sloped shapes with invisible collision boxes inside; the rest are built from STRUCT boxes. '
         'Outdoors z is measured from the ground. Kinds and defaults are in the value tables; see them all in the Architecture rooms.'),
        ('STAIR|map|x1|y1|x2|y2|bottom|top|steps|alongY|ascending[|openUnderside|tread|rails]', 'A flight of steps inside the box. alongY 1 climbs along y, ascending 1 rises toward the larger coordinate. Optional open underside, tread thickness (0.12) and side rails.'),
        ('DOOR|map|left|right|y|z|entry|transfer|swinging|needClear|sign[|state|value|control|swing|maxOpen|open|opening|leaf|label|locked]', 'A door across x left..right at y (use a tile line + 0.5, between # tiles). swinging 0 slides, 1 swings. needClear 1 keeps it shut until the map\'s creatures are dead; state/value lock it until a state is set. transfer 1 makes it the exit to the next chapter, entry 1 the door you arrive through: leave both 0. sign 0. Optional: control 1 waits for the plant\'s control release; swing 1 or -1 is the side a swinging leaf opens to; maxOpen 0-1 how far it opens; open 0-1 how open it starts and opening 1 if it starts opening. leaf 0 the standard leaf, 1 a steel grille you can see and hear through (sliding doors), 2 a clean painted panel. label names it in the prompt (GATE gives E / OPEN GATE). locked is what the prompt says while it is locked (TIME LOCK / 20 S).'),
        ('PIPE|map|x1|y1|x2|y2|z|radius|endZ|material', 'A pipe from (x1,y1) at height z to (x2,y2) at endZ. Same x,y and different heights make a riser. material 0-6.'),
        ('WATER|map|x1|y1|x2|y2|bed|surface[|murk]', 'A volume of water from bed up to surface height. Build a basin around it with STRUCT. murk 0 is clear coolant; up to 1 makes it still, dark standing water (a flooded store, a pool nobody drains).'),
        ('BREAKABLE|map|x1|y1|x2|y2|bottom|top|material|health|holds|brokenState|triggerState', 'A box that shatters when shot (glass is material 17). brokenState is set when it breaks.')]),
    ('Things in the map', [
        ('FIXTURE|map|model|x|y|base|width|depth|height|yaw|solid[|core|visible]', 'A model from the model list, centred at x,y, its base this high, stretched to width x depth x height (use the listed size to keep it in proportion). solid 1 blocks you. At yaw 0 the imported models (63 and up) face south (+y) and yaw 3.142 turns them to face north; the engine\'s own models (0-62) face the other way. Check in game.'),
        ('CLUTTER|map|kind|x|y|z|yaw', 'A loose object that can be knocked about: kinds 0-6. z -999 drops it to the floor.'),
        ('CLUTTER|map|7|x|y|z|yaw|model|width|depth|height|pitch|roll|breakState', 'A loose model you can pick up and throw (see the Props room). breakState may be empty.'),
        ('PICKUP|map|kind|x|y', 'Health (0), shotgun ammo (1) or handgun ammo (2).'),
        ('CREATURE|map|kind|x|y|z[|scale|health|wakeState|hunter|prey]', 'A creature. scale 0.5-2, health 0 for the default. With a wakeState it sleeps (blind and deaf) until that state is set or it is hurt. With a hunter name it is a recurring animal: it cannot be killed, it runs when badly hurt and, out of sight and away, it is gone and the hunter state counts one more escape; every creature with the same hunter name is that animal, and when one wakes any other still about leaves. prey is an actor on this map (its ACTOR number) it comes out for first; a creature that bites (the Chimera) takes the head of a man its strike kills and stays down feeding on him a while.'),
        ('TERMINAL|map|x|y|z|control|title|line1|line2[|activateState|toggle|requireState|yaw]', 'A computer you read with E. control 1 sets activateState (toggle 1 flips it on and off) and can switch lights, doors and events.'),
        ('SIGN|map|x|y|z|width|height|yaw|title|subtitle|colour[|style|icon|ink|art]', 'A wall sign, its bottom edge at z, facing (sin yaw, cos yaw). colour is a 24-bit number (0x3c8cd8 = 3968216). Put it 0.05 m in front of a wall. style: 0 enamel plate, 1 hazard, 2 notice, 3 stencil painted on the wall, 4 route, 5 chalkboard, 6 picture. icon: 0 none, 1 warning, 2 electric, 3-6 arrow left/right/up/down, 7 no entry, 8 radiation, 9 fire, 10 exit. ink is the lettering colour (0 = the style\'s own). art names a SIGN_ART picture. The Campaign Builder\'s Sign designer writes all of these.'),
        ('AMBIENT|map|sound|x|y|z|gain|radius[|mode|every|spread|state|value]', 'A sound from the engine\'s sound bank, heard within radius metres of x,y (z -999 is 1.5 m over the floor there). sound is a bank name. Beds that loop: vent_airflow, air_conditioner, fan_large, light_buzz, fluorescent, static_electrical, radio_static, water_drip, gas_leak, drone, haunting, crying, mechanical, metal_tapping. Single sounds: metal_creak, metal_moan, metal_bang, metal_drag, metal_fall, glass_bang, distant_yell, distant_yell_2, house_creak, debris, monster_groan, monster_breath, monster_growl, sting, sting_piano, sting_scratch. The announcement voice: pa_access_denied, pa_access_granted, pa_doors_locked, pa_doors_unlocked, pa_containment_breach, pa_evacuate, pa_caution_hazard, pa_ventilation_offline, pa_unauthorized, pa_system_lockdown, pa_backup_generators, pa_hostile_detected. mode 0 loops it (beds only), 1 plays it now and then, every to every+spread seconds apart, 2 plays it once each time state takes value. With a state, modes 0 and 1 run only while state = value. Creatures and guards never hear these. The Campaign Builder\'s Sound tool places them.'),
        ('REVERB|map|wet|seconds', 'This map\'s room reverb: how much of it (0 to 1) and how long it rings (0.1 to 6 seconds). Without this record the game measures the space the player stands in, so a closet and a hall already sound different.'),
        ('SIGN_ART|name|width|height|pixels', 'A picture for signs: RGBA bytes in base64, at most 256 on a side and 65536 pixels. Signs name it in their art field. The Sign designer imports PNG and JPEG pictures and writes this for you.')]),
    ('Light and air', [
        ('LIGHT|map|x|y|z[|mount|yaw|intensity|range|kelvin|tint|state|value|invert|period|onTime|phase|flicker|fitting|w|d|h|gelR|gelG|gelB]',
         'A lamp. Only x, y, z are required. mount 0 ceiling, 1 wall (x,y on the wall face, yaw pointing out), 2 floor. intensity ~1, range in metres, kelvin 1000-12000 (3400 default), tint -1 magenta..1 green, '
         'state/value switch it with a state, period/onTime blink it, flicker 0-1. gel R G B (0-1 each) colours it: 1 0.1 0.06 is emergency red. '
         'A ceiling lamp throws its light down: full within about 30 degrees of straight down, fading to a fifth by 65, so it pools on the floor and scallops the walls; '
         'space a row about its height apart. A wall lamp lights the room in front of it; a floor lamp shines every way. The fitting glows in the lamp\'s own colour.'),
        ('DARK|map|x1|y1|x2|y2[|bottom|top|state|value|invert|jam]', 'True darkness inside the box; jam 1 (default) also kills the flashlight.'),
        ('ZONE|map|x1|y1|x2|y2|name[|bottom|top|title|hazeR|hazeG|hazeB|density]', 'A named place: the HUD shows name while you are inside. A title shows a big title card on entry. A haze gives the box its own fog colour.'),
        ('EMITTER|map|x|y|z|driftX|driftY|rise|rate|radius|growth|count', 'Smoke or steam rising from a point.')]),
    ('Danger', [
        ('HAZARD|map|kind|x1|y1|x2|y2|bottom|top|damage', 'Hurts whoever stands in the box, damage per second. Kinds in the hazard table.'),
        ('COMPACTOR|map|x1|y1|x2|y2|bed|raised|period', 'A crushing ceiling that cycles every period seconds.'),
        ('LEAD|map|ax|ay|az|bx|by|bz|length|radius[|material|live|...|loose]', 'A cable between two points; length longer than the gap makes it sag. material 8 is red; live 1 shocks. A loose cable (18th field 1) lies free at its far end.')]),
    ('Scripting', [
        ('OBJECTIVE_TEXT|map|id|text', 'Objective wording, used by SCRIPT_ACTION OBJECTIVE.'),
        ('SCRIPT_EVENT|map|id|x1|y1|x2|y2|bottom|top|requireState|requireValue|needClear', 'Fires once when you enter the box (and the gates pass). requireState may be empty (then requireValue is 1). needClear 1 also waits for the map\'s creatures to die.'),
        ('SCRIPT_REQUIRE|map|event|name|op|value', 'One more gate on an event (up to 8). op is = != < <= > >=. name is a state, or a live value: @kills and @remaining (creatures killed and hostiles left on the event\'s map), @health, @map (the map the player is on).'),
        ('SCRIPT_TRIGGER|map|event|where|match|repeat', 'How the event fires. where: box (its box, the default), map (anywhere on its map) or campaign (on any map). match: all gates must hold (default) or any one. '
         'repeat 1: fire again each time the gates turn true (each time you walk in), instead of once ever.'),
        ('SCRIPT_ACTION|map|event|STATE|name|value', 'What an event does, up to 16 per event, in order. Other kinds: OBJECTIVE id status(1 active, 2 done), ITEM id count, CHECKPOINT, COMPLETE (ends the campaign), SHAKE amount.'),
        ('SCRIPT_ACTION|map|event|ADD|name|amount', 'Adds to a state (negative takes away). Use it as a counter: crystals found, wardens killed, relays attuned.'),
        ('SCRIPT_ACTION|map|event|TAKE|item|count', 'Takes quest items back, the opposite of ITEM.'),
        ('SCRIPT_ACTION|map|event|MESSAGE|text|seconds', 'A caption for 0.5 to 15 seconds, up to 96 characters. {name} in the text shows that state\'s value: RELAYS {relays} / 2.'),
        ('TIMED_SEQUENCE|map|startState|doneState|x1|y1|x2|y2|bottom|top|duration|finish|soundInterval|soundCutoff|sound|gain|pitch[|loop]', 'A clock that runs once startState is set: it drives SEQUENCE_CUE lines and ACTOR movement.'),
        ('ACTOR|map|clockState|visual|height|loop|tool|index  +  ACTOR_KEY|map|index|timeMs|x|y|z|yaw|clip|phase', 'A person following keyframes on a clock. Two keys at the same spot (clip 0) make them stand idle, as in the Life room.')]),
]

def table(header, rows):
    out = '<table><tr>%s</tr>' % ''.join('<th>%s</th>' % esc(h) for h in header)
    for r in rows:
        out += '<tr>%s</tr>' % ''.join('<td>%s</td>' % (c if isinstance(c, str) and c.startswith('<') else esc(c)) for c in r)
    return out + '</table>'


def shots():
    os.makedirs(IMAGES, exist_ok=True)
    listing = os.path.join(ROOT, '.build', 'guide-shots.txt')
    os.makedirs(os.path.dirname(listing), exist_ok=True)
    number = {m['name']: m['index'] for m in starter.build().maps}
    with open(listing, 'w') as fh:
        for name, m, x, y, yaw, pitch, lift, _ in SHOTS:
            fh.write('%s %d %g %g %g %g %g\n' % (name, number[m], x, y, yaw, pitch, lift))
    subprocess.run([os.path.join(ROOT, 'RawMetal.exe'), '--zone-pack=' + PACK, '--zone-shots=' + listing], cwd=ROOT, check=True)
    from PIL import Image
    for name, *_ in SHOTS:
        ppm = os.path.join(ROOT, '.build', 'shot', name + '.ppm')
        Image.open(ppm).convert('RGB').save(os.path.join(IMAGES, name + '.jpg'), quality=86, optimize=True)
        os.remove(ppm)


def main():
    if '--shots' in sys.argv:
        shots()
    s = starter.build()
    index = {'maps': [{'name': m['name'], 'col': m['col'], 'row': m['row']} for m in s.maps], 'items': s.index}
    where = {}
    for it in s.index:
        where.setdefault(it['title'], it)
    mapname = {i: m['name'] for i, m in enumerate(s.maps)}
    scripts_map = next(i for i, m in enumerate(s.maps) if m['name'] == 'Scripts')
    models = starter.models()

    o = ['<!doctype html><html><head><meta charset="utf-8"><title>Custom maps: the guide</title><style>%s</style></head><body><main>' % CSS]
    o.append('<h1>Making custom maps</h1><p class="lede">A custom map is one <b>.rmc</b> file: a RawMetal Campaign. Put it in <code>custom maps</code> beside the game, '
             'start the game, and pick it under <b>CUSTOM MAPS</b> on the title screen. Inside, an .rmc is readable text you write by hand. This guide shows how it works, with '
             '<b>Starter Map.rmc</b> as the worked example: a walkable debug world with every part of a map lined up and labelled.</p>')
    o.append('<nav><a href="#start">Quick start</a><a href="#editor">Editor workflow</a><a href="#file">The file</a><a href="#space">Space and height</a><a href="#levels">Levels</a><a href="#stitch">Stitching maps</a><a href="#outdoors">Outdoors and terrain</a><a href="#scripting">Scripting</a><a href="#records">Records</a>'
             '<a href="#starter">Starter Map tour</a><a href="#values">Value tables</a><a href="#models">All models</a><a href="#errors">When it fails</a></nav>')

    o.append('<h2 id="start">1. Quick start</h2>')
    o.append('<figure>%s<figcaption>Where everything lives after you unzip the game.</figcaption></figure>' % folder_diagram())
    o.append('<ol><li>Copy <code>Starter Map.rmc</code> to <code>My Map.rmc</code> in the same folder.</li>'
             '<li>Right-click it, choose <b>Open with</b>, then <b>Notepad</b> (tick <i>Always use this app</i> so .rmc files open in Notepad from then on). '
             'Change the <code>CAMPAIGN|STARTER MAP|0</code> line to <code>CAMPAIGN|MY MAP|0</code> and save.</li>'
             '<li>Start the game: <b>CUSTOM MAPS</b> now lists MY MAP. Each .rmc file is its own campaign.</li>'
             '<li>Delete the parts you do not want, move the rest, and add your own. Restart the game, or reopen the CUSTOM MAPS menu, to reload.</li></ol>')
    o.append('<div class="note"><b>Starting from a blank Notepad file?</b> Notepad saves it as <code>My Map.txt</code>. Turn it into a campaign by renaming it to '
             '<code>My Map.rmc</code>. If you cannot see the <code>.txt</code> on the end, open File Explorer, choose <b>View</b>, then <b>Show</b>, and tick '
             '<b>File name extensions</b>. Windows asks whether you are sure you want to change the extension: choose <b>Yes</b>. '
             'Saves follow the map through the rename.</div>')
    o.append('<div class="note">The game rescans the folder every time the CUSTOM MAPS menu opens, so you can edit, save and reopen the menu without quitting. '
             'Any file that fails to load is skipped and listed in <code>RawMetal-custom-maps.txt</code> next to the game, with the line number of the problem.</div>')
    o.append('<p>A smallest working map, one closed room 6 x 6 m with a lamp:</p><pre>%s</pre>' % esc(
        '--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|TINY ROOM|0\n'
        'MAP|0|Tiny room|0|0|3.5|3.5|0|0|3|0.3|industrial_night|0|0|0|0|0\n'
        'LAYER|0|0|Floor|0|0\n' + '\n'.join('ROW|0|0|%d|%s' % (y, ('########' if y in (0, 7) else '#......#') + '#' * 16 if y < 8 else '#' * 24) for y in range(24)) +
        '\nLIGHT|0|4|4|2.85\n--- CUSTOM_CAMPAIGN_DATA_END ---'))

    o.append('<h2 id="file">2. The file</h2>')
    o.append('<figure>%s<figcaption>Anything before the start marker is ignored. Every line between the markers is one record: a tag, then fields split by <code>|</code>. Lines starting <code>#</code> are notes.</figcaption></figure>' % anatomy_diagram())
    o.append('<p>Save as plain text (UTF-8 or ASCII). Numbers use a dot for decimals. A field can be left empty (<code>||</code>) where the reference says it is optional.</p>')
    o.append('<h3>Reading a record</h3><figure>%s<figcaption>A FIXTURE record: in map 4, model 27 (a CRT monitor) at x 12.5, y 6, '
             'standing on a 0.75 m plinth, 0.42 x 0.40 x 0.38 m, turned to face south, solid.</figcaption></figure>' %
             record_diagram('FIXTURE|4|27|12.5|6|0.75|0.42|0.4|0.38|3.1416|1', ['record', 'map', 'model', 'x', 'y', 'base', 'width', 'depth', 'height', 'yaw', 'solid']))

    o.append('<h2 id="space">3. Space and height</h2>')
    o.append('<figure>%s<figcaption>One map from above: 24 x 24 metres, one tile per metre. x grows east, y grows south. '
             'Positions are map metres, so 12 12 is the middle of any map.</figcaption></figure>' % coordinate_diagram())
    o.append('<figure>%s<figcaption>The tile grid: the 24 ROW records of a layer. Tiles give you rooms quickly; STRUCT boxes give you anything finer.</figcaption></figure>' % tiles_diagram())
    o.append('<figure>%s<figcaption>Side view. Every height is in metres from the map\'s bottom layer.</figcaption></figure>' % heights_diagram())
    o.append('<h2 id="levels">4. Levels: stacking floors in one map</h2>')
    o.append('<p>A map can have several layers, one above another. Layer 0 is the ground floor. Each upper layer is its own LAYER record and '
             'its own 24 ROW records, where <code>=</code> is solid deck and <code>.</code> is open air. Climb between them with a STAIR whose '
             'top meets the deck edge. <b>Levels</b> in the Starter Map (east of Life) is this exact room.</p>')
    o.append('<figure>%s<figcaption>One map, two layers, from the side. Heights are always from the bottom layer, so a desk on the deck has base 3.5.</figcaption></figure>' % levels_diagram())
    o.append('<pre>%s</pre>' % esc('LAYER|4|0|Hall floor|0|0\nROW|4|0|0|########################      (24 rows: walls and floor)\n...\n'
                                   'LAYER|4|1|Upper deck|3.5|0.25\nROW|4|1|1|............===========.      (24 rows: = is deck, . is air)\n...\n'
                                   'STAIR|4|6|10|12|12|0|3.5|18|0|1|1|0.08|1     (x 6-12, rises east to 3.5)\n'
                                   'FIXTURE|4|24|20|14|3.5|1.4|0.7|0.75|0|1        (a desk ON the deck)'))
    o.append('<ul><li>Leave headroom: the slab hangs <i>thickness</i> below its elevation, so a deck at 3.5 with thickness 0.25 leaves 3.25 m underneath.</li>'
             '<li>Put the MAP ceiling above the highest deck plus head height (7.5 here), and light both floors: a LIGHT under the deck and one over it.</li>'
             '<li>A deck can carry on into the next map: give that map an upper layer at the same elevation with <code>=</code> tiles on the shared edge.</li>'
             '<li>Use ZONE bottom/top to label each floor separately: the HUD then names the floor you stand on.</li></ul>')

    o.append('<h2 id="stitch">5. Stitching maps together</h2>')
    o.append('<p>One map is 24 x 24 m. Bigger places are several maps laid edge to edge on a grid: '
             '<b>east-west</b> (side by side, the seam runs north-south) and <b>north-south</b> (one above the other on the plan, the seam runs east-west). '
             'A campaign can hold up to 180 maps, and you walk across a seam with no loading screen.</p>')
    o.append('<figure>%s<figcaption>Four maps, two of each kind of seam. Green: open seams. Red: closed edges.</figcaption></figure>' % stitch_diagram())
    o.append('<ol><li>Give every map an origin on the 24 m grid: <code>0 0</code>, <code>24 0</code>, <code>0 24</code>, <code>24 24</code>... '
             '(origin is MAP fields 4 and 5). Map numbers are separate from position: map 7 may sit anywhere.</li>'
             '<li>On each shared edge set the open flag on <b>both</b> maps: openE on the west map and openW on the east one; openS on the north map and openN on the south one.</li>'
             '<li>Keep the edge tiles floor on both sides. Row 23 of the north map meets row 0 of the south map; column 23 of the west map meets column 0 of the east map. '
             'A <code>#</code> in either closes that metre of the seam, so you can make doorways in a long seam.</li>'
             '<li>Match what crosses the seam: the same floor height (layer elevation), similar ceilings, and the same SURFACE if it should look like one room. '
             'Outdoors, terrain is one surface across the whole campaign, so it lines up by itself.</li>'
             '<li>Keep each thing inside its own map\'s 0-24 square: split a long wall into one STRUCT per map at the seam.</li></ol>')
    o.append('<p>The Starter Map is %d maps stitched like this; every open edge is a seam. <b>Seams</b>, under Ground paint, marks one of each with a dirt strip and a sign '
             'naming both maps\' flags, and a closed edge at its east end.</p>' % len(s.maps))
    o.append('<figure>%s</figure>' % grid_diagram(index))

    o.append('<h2 id="outdoors">6. Outdoors and terrain</h2>')
    o.append('<p>A map is outdoors when its MAP environment is <code>1</code>. With the skybox <code>zone_overcast</code> (grey day) or '
             '<code>brutal_wasteland</code> it also gets real ground: a smooth heightfield shaped by the campaign\'s TERRAIN record and covered in rock and soil. '
             'There is no ceiling, sun and sky light it, and you can mix outdoor and indoor maps in one campaign.</p>')
    o.append('<figure>%s<figcaption>The ground in profile: TERRAIN noise between base - amplitude and base + amplitude, levelled by TERRAIN_FLAT boxes.</figcaption></figure>' % terrain_diagram())
    o.append('<pre>%s</pre>' % esc('TERRAIN|2026|1|2.5|30|0.45                      rolling ground around 1 m\n'
                                   'TERRAIN_FLAT|0|264|72|266|0|8                   level ground where the indoor row opens\n'
                                   'TERRAIN_FLAT|37|275|44|281|3.5|5                a plateau\n'
                                   'GROUND|24|284|48|286|6|1                        a gravel road\n'
                                   'HORIZON|60|6|6|121,122,118,150,73              woods past the edges\n'
                                   'MAP|n|Starter Map / Ground paint|0|264|...|1|2.8|0.2|brutal_wasteland|1|1|0|1|...'))
    o.append('<ul><li><b>World metres.</b> TERRAIN_FLAT, GROUND and HORIZON are not tied to a map: their x, y are world positions (map origin plus map x, y). '
             'A pad at map 12 6 on the map with origin 24 264 is at 36 270.</li>'
             '<li><b>Heights follow the ground.</b> On an outdoor map every base, z, bottom and top is measured from the ground under it. A tree with base 0 '
             'stands on the hillside; a sign with z 1.6 is 1.6 m above the ground at its spot.</li>'
             '<li><b>Rows outdoors.</b> Leave the tiles <code>.</code>: the open flags are the boundary. <code>#</code> tiles still build 2.8 m walls if you want ruins.</li>'
             '<li><b>Indoor to outdoor.</b> Terrain is built from 1 m voxels and you walk half a metre above a TERRAIN_FLAT\'s height: a flat at 0 '
             'walks at +0.5, a flat at -1 at -0.5. The player only steps up 0.215 m without jumping, so ground can never meet a floor at 0 flush. '
             'Level the ground at -1 outside the door and build the difference as STRUCT steps: a landing at 0, then steps at -0.17 and -0.34. '
             'Every door of the Starter Map\'s facility is built this way.</li>'
             '<li><b>Which sky.</b> <code>zone_overcast</code> lays a low steam over ground below about 4 m, so low terrain reads milky white; '
             '<code>brutal_wasteland</code> has no steam and darker air, and the Starter Map\'s forest uses it with a blue-grey MAP haze.</li>'
             '<li><b>Buildings outdoors</b> are FIXTURE models (houses, sheds, the outdoor set) on a TERRAIN_FLAT pad, or STRUCT boxes. Add a SHELTER inside if the campaign has a STORM.</li></ul>')

    o.append('<h2 id="scripting">7. Scripting: making things happen</h2>')
    o.append('<p>Doors that unlock, lamps that switch, counters, messages, the end of the campaign: all of it is <b>states</b> and <b>events</b>. '
             'A state is a named whole number, 0 until something sets it (<code>starter_lever</code>, <code>relays</code>). Terminals, broken glass, '
             'timed sequences and events set states; doors, lights, terminals, creatures and events listen for them. An event is a SCRIPT_EVENT '
             'with the records below it that name it. <b>Scripts</b> in the Starter Map (east of Levels) has one working station for each idea here.</p>')
    o.append('<figure>%s<figcaption>One event, left to right. Only SCRIPT_EVENT is required: on its own it fires once, the first time you walk into its box.</figcaption></figure>' % script_diagram())
    o.append('<h3>Patterns</h3>')
    o.append(table(['You want', 'Write'], [
        ('A switch that opens a door', '<code>TERMINAL|...|1|LEVER|...|door_power|0</code> and <code>DOOR|...|door_power|1</code>. No event needed.'),
        ('A counter: collect three crystals', '<span>One box event per crystal with <code>ADD|crystals|1</code>, then an event gated <code>SCRIPT_REQUIRE|map|open|crystals|&gt;=|3</code>.</span>'),
        ('Two conditions at once (AND)', 'requireState on the SCRIPT_EVENT plus a SCRIPT_REQUIRE, or several SCRIPT_REQUIREs. All must hold.'),
        ('Either of two (OR)', '<code>SCRIPT_TRIGGER|map|ev|map|any|0</code> with one SCRIPT_REQUIRE per choice.'),
        ('Something every time you enter', '<code>SCRIPT_TRIGGER|map|ev|box|all|1</code>. Without repeat, an event fires once ever.'),
        ('After the last creature dies', '<code>SCRIPT_REQUIRE|map|ev|@remaining|=|0</code> with <code>where map</code>; add <code>@kills &gt;= 1</code> so an empty map does not count.'),
        ('A warning at low health', '<code>SCRIPT_REQUIRE|map|ev|@health|&lt;|30</code>, where map, repeat 1, a MESSAGE action.'),
        ('A switch on one map that changes another', '<span>Set a state; anything on any map can listen for it. For an event that must run wherever you are, use <code>where campaign</code>.</span>'),
        ('Show progress', '<code>SCRIPT_ACTION|map|ev|MESSAGE|RELAYS {relays} / 2|4</code>.')]))
    o.append('<pre>%s</pre>' % esc(
        '# The Starter Map vault: a lever AND three crystals, checked anywhere on map %d.\n'
        'SCRIPT_EVENT|{n}|starter_vault_open|0|0|24|24|-1|5|starter_lever|1|0\n'
        'SCRIPT_REQUIRE|{n}|starter_vault_open|starter_crystals|>=|3\n'
        'SCRIPT_TRIGGER|{n}|starter_vault_open|map|all|0\n'
        'SCRIPT_ACTION|{n}|starter_vault_open|STATE|starter_vault|1\n'
        'SCRIPT_ACTION|{n}|starter_vault_open|MESSAGE|VAULT OPEN: LEVER AND CRYSTALS >= 3 (ALL GATES)|4\n'
        'DOOR|{n}|17|20|7.5|0|0|0|0|0|0|starter_vault|1            (the door listens for the state)'
        .replace('{n}', str(scripts_map)) % scripts_map))
    o.append('<ul><li>Event names are unique across the whole file, and the SCRIPT_REQUIRE, SCRIPT_TRIGGER and SCRIPT_ACTION lines go after their SCRIPT_EVENT.</li>'
             '<li>A map-wide or campaign-wide event still needs a box in its SCRIPT_EVENT: use <code>0|0|24|24</code>.</li>'
             '<li>States are saved with the game, so counters and switches survive a reload.</li>'
             '<li>repeat 1 fires on the change, not every frame: a box event fires once per visit, a gate event once each time its gates go from false to true.</li></ul>')

    o.append('<h2 id="records">8. Records</h2><p>Fields in <code>[brackets]</code> are optional and can be left off the end. '
             'Every record\'s second field is the map number it belongs to.</p>')
    for title, recs in RECORDS:
        o.append('<h3>%s</h3>' % esc(title))
        o.append(table(['Record', 'What it does'], [('<code>%s</code>' % esc(r), d) for r, d in recs]))
    o.append('<p>The format also covers armed guards, dialogue and cargo lifts (the Ashfall Exclusion Zone and the shipped chapters '
             'are built from them). This guide covers the parts every map needs.</p>')

    o.append('<h2 id="starter">9. The Starter Map tour</h2><p>Pick <b>STARTER MAP</b> under CUSTOM MAPS. Everything stands on a plinth with a sign: '
             'the big text is its ID (the number you type in the record), the small text its name. Stand next to anything and the HUD, top left, '
             'names it in full. Nothing in it can hurt you unless you climb into a hazard pen, walk the gas strip in Scripts, or wake a creature (by hitting it, or with the Scripts terminal that wakes its Huntsman).</p><div class="shots">')
    for name, *_, caption in SHOTS:
        if os.path.exists(os.path.join(IMAGES, name + '.jpg')):
            o.append('<figure><img src="guide/%s.jpg" alt="%s"><figcaption>%s</figcaption></figure>' % (name, esc(name), esc(caption)))
    o.append('</div><p>To copy anything you see, search <code>Starter Map.rmc</code> for its sign text (for example <code>|#27|</code> or <code>HAZARD 4</code>): '
             'the records just above it build that exhibit.</p>')

    PRIM_KINDS = [
        ('box', '1 x 1 x 1', 'a plain box'), ('pillar', '0.5 x 0.5 x 3', 'a square pier'),
        ('stairs', '1.2 x (from h) x 1', 'a flight of 0.17 m risers on 0.28 m treads, rising toward facing'),
        ('ramp', '1.2 x (4 x h) x 0.6', 'a 1:4 slope, walked without steps'), ('column', '0.6 x 0.6 x 3', 'a round column'),
        ('arch', '2.4 x 0.6 x 3.4', 'a wall with a round-headed opening; refused if a standing player could not walk through'),
        ('vault', '4 x 4 x 3', 'a barrel-vaulted roof, open at both ends, 2 m+ walkable under'),
        ('dome', '4 x 4 x 2', 'a half ellipsoid'), ('doorframe', '1.2 x 0.3 x 2.2', 'jambs and lintel round a game-size door opening'),
        ('window', '3 x 0.3 x 1', 'a ribbon window with sill, head, mullions and glass; set z to about 0.9 so it frames the eye line'),
        ('fins', '3 x 0.4 x 3', 'vertical louvres with 0.38 m gaps: see through, not walk through'),
        ('canopy', '3 x 1.5 x 0.25', 'a cantilevered slab with a lip; set z to 2.6 or more'),
        ('railing', '2 x 0.06 x 1.1', 'a guard rail'), ('corbel', '0.6 x 0.6 x 0.6', 'stepped brackets'),
        ('plinth', '2 x 2 x 0.45', 'a podium at seat height, jumped onto'),
        ('slit', '1.2 x 0.4 x 3', 'a wall with a 0.25 m arrow slit'), ('clerestory', '3 x 0.4 x 3', 'a wall with a slot above head height'),
        ('embrasure', '1.6 x 0.8 x 3', 'a deep wall with an 0.8 m opening at eye height'),
        ('roundel', '2 x 0.4 x 3', 'a wall with a round opening centred near eye height'),
        ('niche', '1 x 0.5 x 1.6', 'an alcove in a wall face'), ('hood', '1 x 0.5 x 1.6', 'an alcove with a projecting hood'),
        ('batter', '3 x 1 x 2.5', 'a wall leaning back, too steep to climb'), ('waffle', '4 x 4 x 0.6', 'a coffered slab with ribs under it; z is its underside'),
        ('pilotis', '0.6 x 0.6 x 3.5', 'a column with a flared capital'), ('megastep', '3 x (from h) x 1.35', '0.45 m risers you jump, or sit on'),
        ('lightwell', '4 x 4 x 1.1', 'a parapet round a shaft'), ('bridge', '3 x 8 x 3', 'an enclosed span you walk through, with an eye-level slot'),
        ('core', '4 x 4 x 8', 'a stair or lift tower with a 1.2 x 2.2 m door in its face'), ('strut', '0.35 x 3 x 3', 'a diagonal brace'),
        ('girder', '0.5 x 4 x 0.6', 'a steel I-beam'), ('quadpier', '1 x 1 x 3', 'four slender piers you can walk between'),
        ('band', '3 x 0.08 x 0.25', 'a string course on a facade'), ('spout', '0.25 x 0.8 x 0.2', 'a rain scupper'),
        ('chase', '0.8 x 0.2 x 3', 'a recessed service channel'), ('lattice', '2 x 0.2 x 2.4', 'a cobogo screen with 0.24 m holes')]
    o.append('<h2 id="values">10. Value tables</h2>')
    o.append('<h3>STRUCT materials</h3>' + table(['material', 'look'], sorted(starter.MATERIALS.items())))
    o.append('<h3>The player\'s scale</h3><p>Build to these and everything works: the PRIM defaults already do.</p>' + table(['measure', 'metres', 'what it means'], [
        ('standing height / eyes', '1.6 / 1.42', 'openings to walk through need 1.75 m clear; put windows and slots around 1.42'),
        ('crouched / prone height', '1.05 / 0.58', 'the lowest crawlspace is 0.6 m'),
        ('width (radius 0.2)', '0.4', 'gaps under 0.4 m stop her but can be seen and shot through'),
        ('automatic step', '0.215', 'risers and ledges up to this are walked; stairs use 0.17'),
        ('jump / crouch-jump', '1.04 / 1.1', 'guard rails and parapets of 1.1 m need a crouch-jump'),
        ('doors', '1.2 x 2.2', 'the doorframe and core door size')]))
    o.append('<h3>PRIM kinds</h3>' + table(['kind', 'default w x d x h', 'what it builds'], PRIM_KINDS))
    o.append('<h3>SURFACE sets</h3>' + table(['value', 'finish'], [(-1, 'default')] + list(enumerate(starter.SURFACES))))
    o.append('<h3>Creatures, pickups, hazards, clutter</h3>')
    o.append(table(['value', 'CREATURE kind', 'PICKUP kind', 'HAZARD kind', 'CLUTTER kind'],
                   [(k, starter.CREATURES[k] if k < 8 else '', starter.PICKUPS[k] if k < 3 else '', starter.HAZARDS[k] if k < 8 else '',
                     (starter.CLUTTER_KINDS + ['a model: see CLUTTER 7'])[k] if k < 8 else '') for k in range(8)]))
    o.append('<h3>ACTOR visuals</h3>' + table(['visual', 'body'], list(enumerate(starter.ACTORS))))

    o.append('<h2 id="models">11. All %d models</h2><p>Use the ID as the FIXTURE model. Width x depth x height is the model\'s own size in metres: '
             'stretch all three by the same factor to keep it in proportion. "Where" is its plinth in the Starter Map (map name, x, y).</p>' % len(models))
    o.append('<div class="warn">Models 13 (electrical cabinet) and 47 (open container) still draw almost black in custom maps. For a container use 48 (closed).</div>')
    rows = []
    for slot, name, w, d, h, _ in models:
        it = where.get('#%d' % slot)
        loc = '%s, %g %g' % (mapname[it['map']], it['x'], it['y']) if it else ''
        rows.append((slot, name, '%.2f x %.2f x %.2f' % (w, d, h), loc))
    o.append(table(['ID', 'name', 'size (m)', 'where'], rows))

    o.append('<h2 id="editor">Editor: selection, floors and export</h2>')
    o.append('<p>The editor uses the game engine and saves native <code>.rmc</code> files. Use the matching game and editor build, '
             'then save your campaign into the game&#39;s <code>custom maps</code> folder and choose it under <b>CUSTOM MAPS</b>.</p>')
    o.append('<h3>Pick and reshape</h3><ul>'
             '<li><b>Shift-click</b> adds or removes objects from the selector. Drag a selection box in the plan to select several; Shift adds that box to the current selection. '
             'Move, Turn, Copy and Delete operate on the selection. An ordinary click picks one object.</li>'
             '<li>The plan and 3D view outline the object under the pointer. Selected objects have visible handles. Drag a footprint corner to resize it; a pipe or cable has endpoint handles.</li>'
             '<li>In 3D, select a wall or ceiling and drag its highlighted upper edge upward to create headroom. A map wall follows the native MAP ceiling. '
             'For a separate thin roof slab, drag the underside edge: the slab lifts with its thickness intact. Escape cancels the drag; one Undo restores the whole edit.</li>'
             '<li>Wall and floor finishes use metre-based texture repeats. Resizing changes how many repeats fit on a face rather than stretching one image over it. '
             'Model and material IDs remain native.</li></ul>')
    o.append('<h3>Floors / layers dock</h3><p>The adapting right dock has <b>Inspector</b> and <b>Floors / layers</b> tabs. '
             'The layer stack shows floor names, elevations, visibility and locks, with a small plan preview. '
             'Other-floor outlines let you align a new floor with the building below it.</p>')
    o.append(table(['Command', 'Result'], [
        ('New Layer... (Ctrl+Shift+N)', 'Name a new floor and choose its elevation and thickness. New upper floors reserve 2.4 m of headroom by raising the MAP ceiling when needed.'),
        ('New from Visible', 'Create an editable composite group of copies of the visible floors. Relative floor heights, stairs, masks and native materials remain intact.'),
        ('New Layer Group', 'Organize selected floors together.'),
        ('Duplicate Layers (Ctrl+Shift+D)', 'Copy selected floors and their native objects together, preserving cross-floor relationships.'),
        ('Merge Down', 'Move the active floor content onto the floor below and combine their tile layouts.'),
        ('Delete Layers', 'Remove selected floor content and retain a valid, contiguous layer list. The map keeps at least one floor.'),
        ('Stack', 'Change the order in the dock. Elevation remains a separate physical property.'),
        ('Mask', 'Use a plan selection as an editing-view mask, invert it, or remove it.'),
        ('Transparency', 'Adjust visibility while editing in 2D and 3D.'),
        ('Transform', 'Resize the floor footprint, flip the layout, or edit elevation and thickness.')]))
    o.append('<p><b>Visibility, opacity and masks are editing aids.</b> They are saved with the project as editor comments. '
             'They do not silently delete geometry from the exported playable map. Play uses the complete authored map and its real collision. '
             'To make a permanent hole, erase the floor tiles or edit its native geometry.</p>')
    o.append('<p>The standing player is <b>1.6 m</b> tall. Check the dock&#39;s headroom readout before playing an upper floor: '
             'a deck at 3.5 m under a ceiling at 4.5 m has only 1 m of room. Raise that ceiling, or lift the underside of an explicit roof slab. '
             'Place the player start on the intended floor.</p>')
    o.append('<p>On an upper layer, <code>.</code> is empty space and <code>=</code> is a walkable deck with positive thickness. '
             'The upper-floor Floor tool offers <b>Floor</b> or <b>Open space</b>; choose any floor material, including concrete. Open space cuts the floor geometry for ramp openings. The Paint tool paints local rectangles or stripes; Fill paints the connected room. Material-painted floors export as native STRUCT geometry, with metre-scale UVs. Upper <code>#</code> walls reach the authored MAP ceiling and inherit the map wall finish. '
             'Keep the game engine updated for this upper-wall behavior.</p>')
    o.append('<h3>Project history and recovery</h3><p>Each project has its own Undo and Redo history. New, Open, Starter and campaign imports reset the previous project&#39;s editing history and selection. Escape or returning a drag to its start restores the clean state. Recovery waits for a gesture to finish; failed saves keep the previous file and recovery copy, and recovery-write errors appear in the status bar. Duplicated and merged floors retain automatic dressing ownership and room settings. Copy cables with their owners to keep the new set connected, or copy a cable alone to make an independent copy using the engine-resolved endpoints.</p>')
    o.append('<h3>Corridors</h3><p>Choose <b>Corridor</b> and drag from clear floor in one room to clear floor in another. The tool finds a two-metre-wide route, including clearance for its walls, around authored furniture and uninvolved rooms. Reversing the drag gives the same result. Upstairs it builds native floor geometry and preserves existing floor materials and ramp openings. Native wall openings affect the active storey; wall sections above and below are retained. Automatic lamps fill coverage gaps, and managed furniture keeps the saved passage clear. Already-connected rooms, blocked routes and insufficient headroom make no changes. One Undo restores the complete operation.</p>')
    o.append('<h3>Adding adjoining maps</h3><p>Click <b>+ Map</b> and choose North/top, South/bottom, West/left or East/right. Occupied edges are disabled. The new map shares the ground-floor elevation, a short three-tile opening joins the edges, and the editor switches to its ground floor. Uncheck <b>Open a passage between maps</b> in that menu to keep the original walls and boundary settings intact. One Undo restores the whole join. The native renderer draws the active map and nearby maps within 48 m, while distant maps stay out of the draw list.</p>')
    o.append('<h3>Managed dressing and manual content</h3><p>Autodressing starts on and can be disabled. It uses room context, usable space, access and furniture relationships. '
             'It repairs conflicting generated placements while keeping valid ones stable. Unmarked older maps are user-authored. '
             'Manually editing a generated item makes it yours. New walls take priority and remove the furniture or items they physically intersect; Undo restores both.</p>')

    o.append('<h2 id="errors">12. When it fails</h2>')
    o.append(table(['You see', 'Why, and the fix'], [
        ('The map is not in CUSTOM MAPS', 'Open RawMetal-custom-maps.txt beside the game. Each file is OK or ERROR with the line number and record. The file must end in .rmc and sit directly in custom maps (not a subfolder). A name like My Map.rmc.txt means Notepad added .txt: rename it to end in .rmc only.'),
        ('ERROR custom map rows must be exactly 24 characters', 'Count the tiles: every ROW has 24 characters, no spaces.'),
        ('ERROR ... missing CUSTOM_CAMPAIGN_DATA markers', 'Both marker lines must be present exactly as shown, three dashes each side.'),
        ('ERROR custom campaign maps require contiguous MAP records', 'Number maps 0, 1, 2... with no gap, each with one MAP record.'),
        ('ERROR custom map chunk footprints overlap', 'Two maps share an origin area: origins must be 24 m apart.'),
        ('ERROR SCRIPT_REQUIRE (or SCRIPT_TRIGGER, script action) references an undeclared event', 'Put it after the SCRIPT_EVENT it names, with the same map number and the name spelled the same.'),
        ('A scripted event never fires', 'Check each gate in the HUD with a MESSAGE showing {state}; a box event needs you inside its box and between bottom and top; @remaining counts sleeping creatures too.'),
        ('You fall through the floor or start in a wall', 'The spawn tile must be floor (.) and inside 0-24.'),
        ('A model is invisible or inside out', 'Check width/depth/height are positive and its yaw; try yaw 0 and 3.142.'),
        ('It is too dark', 'Raise MAP ambient (0.3-0.4) or add LIGHT records just below the ceiling.')]))
    o.append('<p style="color:#6a707a;margin-top:40px">Generated by tools/campaign/make_custom_map_guide.py.</p></main></body></html>')
    with open(os.path.join(MAPS, 'GUIDE.html'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(o))
    print('GUIDE.html written (%d models, %d shots)' % (len(models), sum(os.path.exists(os.path.join(IMAGES, n[0] + '.jpg')) for n in SHOTS)))


if __name__ == '__main__':
    main()
