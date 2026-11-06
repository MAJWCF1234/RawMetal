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
PACK = os.path.join(MAPS, 'Starter Map.txt')

# name, map, x, y, yaw degrees, pitch, lift: map-local views of the Starter Map.
SHOTS = [
    ('welcome', 0, 12, 2.2, 90, -.05, .3, 'Spawn: the welcome room. Material pillars, stairs, doors, water and pickups.'),
    ('lights', 1, 12, 15, -90, 0, 0, 'Lights: one booth per kind of lamp. Walk in to see each one alone.'),
    ('hazards', 2, 1.5, 12, 0, -.12, 1.0, 'Hazards: the eight hazard kinds fenced off, with a compactor, smoke, glass and cables.'),
    ('life', 3, 12, 8.6, 90, -.1, 0, 'Life: every creature asleep in a pen (they wake if hurt) and every actor body behind.'),
    ('actors', 3, 12, 15.2, 90, 0, 0, 'Actor bodies stand in a row, each labelled with its ACTOR visual number.'),
    ('props', 4, 12, 1.2, 90, -.25, 1.2, 'Props: every loose object you can pick up and throw, on plinths.'),
    ('medical', 12, 12, 1.2, 90, -.2, 1.0, 'A model row (Medical). The HUD top-left names the model you stand at.'),
    ('industrial', 8, 12, 1.2, 90, -.1, 2.0, 'Industrial: whole buildings, shown at reduced scale (the sign says by how much).'),
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
            (2, 'Starter Map.txt', 'the debug world: every part, lined up', '#d8dade'),
            (2, 'GUIDE.html', 'this guide', '#d8dade'),
            (2, 'guide/', 'pictures for this guide (ignored by the game)', '#8a9099'),
            (2, 'My Map.txt', 'yours: any name ending in .txt', '#50b060')]
    y = 34
    for depth, name, note, color in rows:
        x = 24 + depth * 34
        if depth: b += '<path d="M%g %g V%g H%g" stroke="#4a5058" fill="none"/>' % (x - 20, y - 22, y - 5, x - 6)
        icon = '#c8a050' if name.endswith('/') else '#7a8088'
        b += rect(x, y - 15, 16, 13, icon, rx=2) + text(x + 24, y - 4, name, 14, color, weight='bold') + text(430, y - 4, note, 13, '#9aa0a8')
        y += 30
    return svg(900, y, b)


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
    colors = {'Welcome': '#3c8cd8', 'Lights': '#e0b030', 'Hazards': '#d84030', 'Life': '#50b060', 'Props': '#9070d0'}
    for i, m in enumerate(maps):
        x, y = ox + m['col'] * s, oy + m['row'] * 46
        base = m['name'].rstrip('0123456789 ')
        color = colors.get(base, '#56606c')
        b += rect(x, y, s - 4, 42, color + '60', color) + text(x + 6, y + 16, m['name'][:19], 11, '#fff', weight='bold')
        b += text(x + 6, y + 33, 'map %d  %d,%d' % (i, 24 * m['col'], 24 * m['row']), 10, '#c8ccd2')
    b += '<circle cx="%g" cy="%g" r="5" fill="#50b060"/>' % (ox + 12 / 24 * (s - 4), oy + 6) + text(ox + 4 * s + 10, oy + 26, '< you spawn in Welcome. Rooms run east.', 12, '#9aa0a8')
    b += text(ox + 4 * s + 10, oy + 46 + 26, '< each set of models is one row; walk south', 12, '#9aa0a8')
    b += text(ox + 4 * s + 10, oy + 92 + 26, '  through the corridor along their west side', 12, '#9aa0a8')
    return svg(max(900, ox + cols * s + 340), oy + rows * 46 + 10, b)


# ---- record reference ---------------------------------------------------------------------------
RECORDS = [
    ('Campaign and maps', [
        ('CAMPAIGN|name|startMap', 'Once per file. The name appears in the CUSTOM MAPS menu; startMap is the map you spawn in.'),
        ('MAP|index|name|originX|originY|spawnX|spawnY|spawnZ|environment|ceiling|ambient|skybox|openN|openS|openW|openE|lift[|residency|floorMaterial|music|spawnYaw|spawnPitch|startUnarmed]',
         'One per map, numbered 0, 1, 2... with no gaps. origin places the 24 x 24 m map in the world: maps touch at multiples of 24. '
         'spawn is in map metres. environment 0 indoor, 1 outdoor. ceiling is the absolute ceiling height (indoors). ambient 0-1 is the base light (0.3 is a good start). '
         'skybox: industrial_night indoors. openN/S/W/E 1 lets you walk across that edge into the neighbouring map. lift 0. '
         'Optional tail: residency -1, floorMaterial 0 concrete / 1 carpet, music 0, spawnYaw radians, spawnPitch, startUnarmed 0/1.'),
        ('SURFACE|map|wall|floor|ceiling', 'Wall, floor and ceiling finish for the whole map; -1 keeps the default. See the surface table.'),
        ('LAYER|map|layer|name|elevation|thickness', 'A floor level. Layer 0 is the ground (elevation 0, thickness 0). Upper layers use thickness > 0 and = tiles for walkable deck.'),
        ('ROW|map|layer|row|24 characters', 'One line of the tile grid, row 0 (north) to 23 (south). Exactly 24 characters: see the tile diagram.')]),
    ('Building', [
        ('STRUCT|map|x1|y1|x2|y2|bottom|top|rail|material[|solid]', 'A box: walls, floors, platforms, plinths. x1<x2, y1<y2 in map metres; bottom/top are heights. rail 1 draws it as a thin railing. solid 0 lets you walk through it.'),
        ('STAIR|map|x1|y1|x2|y2|bottom|top|steps|alongY|ascending[|openUnderside|tread|rails]', 'A flight of steps inside the box. alongY 1 climbs along y, ascending 1 rises toward the larger coordinate. Optional open underside, tread thickness (0.12) and side rails.'),
        ('DOOR|map|left|right|y|z|entry|transfer|swinging|needClear|sign[|state|value]', 'A door across x left..right at y (use a tile line + 0.5, between # tiles). swinging 0 slides, 1 swings. needClear 1 keeps it shut until the map\'s creatures are dead; state/value lock it until a state is set. transfer 1 makes it the exit to the next chapter, entry 1 the door you arrive through: leave both 0. sign 0.'),
        ('PIPE|map|x1|y1|x2|y2|z|radius|endZ|material', 'A pipe from (x1,y1) at height z to (x2,y2) at endZ. Same x,y and different heights make a riser. material 0-6.'),
        ('WATER|map|x1|y1|x2|y2|bed|surface', 'A volume of water from bed up to surface height. Build a basin around it with STRUCT.'),
        ('BREAKABLE|map|x1|y1|x2|y2|bottom|top|material|health|holds|brokenState|triggerState', 'A box that shatters when shot (glass is material 17). brokenState is set when it breaks.')]),
    ('Things in the map', [
        ('FIXTURE|map|model|x|y|base|width|depth|height|yaw|solid[|core|visible]', 'A model from the model list, centred at x,y, its base this high, stretched to width x depth x height (use the listed size to keep it in proportion). solid 1 blocks you. At yaw 0 the imported models (63 and up) face south (+y) and yaw 3.142 turns them to face north; the engine\'s own models (0-62) face the other way. Check in game.'),
        ('CLUTTER|map|kind|x|y|z|yaw', 'A loose object that can be knocked about: kinds 0-6. z -999 drops it to the floor.'),
        ('CLUTTER|map|7|x|y|z|yaw|model|width|depth|height|pitch|roll|breakState', 'A loose model you can pick up and throw (see the Props room). breakState may be empty.'),
        ('PICKUP|map|kind|x|y', 'Health (0), shotgun ammo (1) or handgun ammo (2).'),
        ('CREATURE|map|kind|x|y|z[|scale|health|wakeState]', 'A creature. scale 0.5-2, health 0 for the default. With a wakeState it sleeps (blind and deaf) until that state is set or it is hurt.'),
        ('TERMINAL|map|x|y|z|control|title|line1|line2[|activateState|toggle|requireState|yaw]', 'A computer you read with E. control 1 sets activateState (toggle 1 flips it on and off) and can switch lights, doors and events.'),
        ('SIGN|map|x|y|z|width|height|yaw|title|subtitle|colour', 'A wall sign. z is its centre. colour is a 24-bit number (0x3c8cd8 = 3968216). Put it 0.05 m in front of a wall.')]),
    ('Light and air', [
        ('LIGHT|map|x|y|z[|mount|yaw|intensity|range|kelvin|tint|state|value|invert|period|onTime|phase|flicker|fitting|w|d|h|gelR|gelG|gelB]',
         'A lamp. Only x, y, z are required. mount 0 ceiling, 1 wall (x,y on the wall face, yaw pointing out), 2 floor. intensity ~1, range in metres, kelvin 1000-12000 (3400 default), tint -1 magenta..1 green, '
         'state/value switch it with a state, period/onTime blink it, flicker 0-1. gel R G B (0-1 each) colours it: 1 0.1 0.06 is emergency red.'),
        ('DARK|map|x1|y1|x2|y2[|bottom|top|state|value|invert|jam]', 'True darkness inside the box; jam 1 (default) also kills the flashlight.'),
        ('ZONE|map|x1|y1|x2|y2|name[|bottom|top|title|hazeR|hazeG|hazeB|density]', 'A named place: the HUD shows name while you are inside. A title shows a big title card on entry. A haze gives the box its own fog colour.'),
        ('EMITTER|map|x|y|z|driftX|driftY|rise|rate|radius|growth|count', 'Smoke or steam rising from a point.')]),
    ('Danger', [
        ('HAZARD|map|kind|x1|y1|x2|y2|bottom|top|damage', 'Hurts whoever stands in the box, damage per second. Kinds in the hazard table.'),
        ('COMPACTOR|map|x1|y1|x2|y2|bed|raised|period', 'A crushing ceiling that cycles every period seconds.'),
        ('LEAD|map|ax|ay|az|bx|by|bz|length|radius[|material|live|...|loose]', 'A cable between two points; length longer than the gap makes it sag. material 8 is red; live 1 shocks. A loose cable (18th field 1) lies free at its far end.')]),
    ('Scripting (the basics)', [
        ('OBJECTIVE_TEXT|map|id|text', 'Objective wording, used by SCRIPT_ACTION OBJECTIVE.'),
        ('SCRIPT_EVENT|map|id|x1|y1|x2|y2|bottom|top|requireState|requireValue|needClear', 'Fires once when you enter the box (and the gates pass). requireState may be empty.'),
        ('SCRIPT_ACTION|map|event|STATE|name|value', 'What an event does. Other kinds: OBJECTIVE id status(1 active, 2 done), ITEM id count, CHECKPOINT, COMPLETE (ends the campaign), SHAKE amount.'),
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
    with open(listing, 'w') as fh:
        for name, m, x, y, yaw, pitch, lift, _ in SHOTS:
            fh.write('%s %d %g %g %g %g %g\n' % (name, m, x, y, yaw, pitch, lift))
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
    models = starter.models()

    o = ['<!doctype html><html><head><meta charset="utf-8"><title>Custom maps: the guide</title><style>%s</style></head><body><main>' % CSS]
    o.append('<h1>Making custom maps</h1><p class="lede">A custom map is one plain text file. Put it in <code>custom maps</code> beside the game, '
             'start the game, and pick it under <b>CUSTOM MAPS</b> on the title screen. This guide shows how the file works, with '
             '<b>Starter Map.txt</b> as the worked example: a walkable debug world with every part of a map lined up and labelled.</p>')
    o.append('<nav><a href="#start">Quick start</a><a href="#file">The file</a><a href="#space">Space and height</a><a href="#records">Records</a>'
             '<a href="#starter">Starter Map tour</a><a href="#values">Value tables</a><a href="#models">All models</a><a href="#errors">When it fails</a></nav>')

    o.append('<h2 id="start">1. Quick start</h2>')
    o.append('<figure>%s<figcaption>Where everything lives after you unzip the game.</figcaption></figure>' % folder_diagram())
    o.append('<ol><li>Copy <code>Starter Map.txt</code> to <code>My Map.txt</code> in the same folder.</li>'
             '<li>Open it in Notepad. Change the <code>CAMPAIGN|STARTER MAP|0</code> line to <code>CAMPAIGN|MY MAP|0</code>.</li>'
             '<li>Start the game: <b>CUSTOM MAPS</b> now lists MY MAP. Each map file is its own campaign.</li>'
             '<li>Delete the parts you do not want, move the rest, and add your own. Restart the game, or reopen the CUSTOM MAPS menu, to reload.</li></ol>')
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
    o.append('<h3>Joining maps</h3><p>A campaign can hold up to 180 maps. Give each map an origin 24 m from its neighbour '
             '(0 0, 24 0, 0 24...) and set the shared edge open on both maps (openE on the west map, openW on the east one). '
             'Leave that edge column or row of tiles as floor. The Starter Map is 27 maps joined like this:</p>')
    o.append('<figure>%s</figure>' % grid_diagram(index))

    o.append('<h2 id="records">4. Records</h2><p>Fields in <code>[brackets]</code> are optional and can be left off the end. '
             'Every record\'s second field is the map number it belongs to.</p>')
    for title, recs in RECORDS:
        o.append('<h3>%s</h3>' % esc(title))
        o.append(table(['Record', 'What it does'], [('<code>%s</code>' % esc(r), d) for r, d in recs]))
    o.append('<p>The format also covers outdoor terrain, ash storms, armed guards, dialogue and cargo lifts (the Ashfall Exclusion Zone '
             'is built from them). This guide covers the parts every map needs.</p>')

    o.append('<h2 id="starter">5. The Starter Map tour</h2><p>Pick <b>STARTER MAP</b> under CUSTOM MAPS. Everything stands on a plinth with a sign: '
             'the big text is its ID (the number you type in the record), the small text its name. Stand next to anything and the HUD, top left, '
             'names it in full. Nothing in it can hurt you unless you climb into a hazard pen or wake a creature by hitting it.</p><div class="shots">')
    for name, *_, caption in SHOTS:
        if os.path.exists(os.path.join(IMAGES, name + '.jpg')):
            o.append('<figure><img src="guide/%s.jpg" alt="%s"><figcaption>%s</figcaption></figure>' % (name, esc(name), esc(caption)))
    o.append('</div><p>To copy anything you see, search <code>Starter Map.txt</code> for its sign text (for example <code>|#27|</code> or <code>HAZARD 4</code>): '
             'the records just above it build that exhibit.</p>')

    o.append('<h2 id="values">6. Value tables</h2>')
    o.append('<h3>STRUCT materials</h3>' + table(['material', 'look'], sorted(starter.MATERIALS.items())))
    o.append('<h3>SURFACE sets</h3>' + table(['value', 'finish'], [(-1, 'default')] + list(enumerate(starter.SURFACES))))
    o.append('<h3>Creatures, pickups, hazards, clutter</h3>')
    o.append(table(['value', 'CREATURE kind', 'PICKUP kind', 'HAZARD kind', 'CLUTTER kind'],
                   [(k, starter.CREATURES[k] if k < 8 else '', starter.PICKUPS[k] if k < 3 else '', starter.HAZARDS[k] if k < 8 else '',
                     (starter.CLUTTER_KINDS + ['a model: see CLUTTER 7'])[k] if k < 8 else '') for k in range(8)]))
    o.append('<h3>ACTOR visuals</h3>' + table(['visual', 'body'], list(enumerate(starter.ACTORS))))

    o.append('<h2 id="models">7. All %d models</h2><p>Use the ID as the FIXTURE model. Width x depth x height is the model\'s own size in metres: '
             'stretch all three by the same factor to keep it in proportion. "Where" is its plinth in the Starter Map (map name, x, y).</p>' % len(models))
    o.append('<div class="warn">Models 0, 1, 2, 13, 47 and 50 (early engine pieces) draw black: they exist for old maps. Pick another model for new work.</div>')
    rows = []
    for slot, name, w, d, h, _ in models:
        it = where.get('#%d' % slot)
        loc = '%s, %g %g' % (mapname[it['map']], it['x'], it['y']) if it else ''
        rows.append((slot, name, '%.2f x %.2f x %.2f' % (w, d, h), loc))
    o.append(table(['ID', 'name', 'size (m)', 'where'], rows))

    o.append('<h2 id="errors">8. When it fails</h2>')
    o.append(table(['You see', 'Why, and the fix'], [
        ('The map is not in CUSTOM MAPS', 'Open RawMetal-custom-maps.txt beside the game. Each file is OK or ERROR with the line number and record. The file must end in .txt and sit directly in custom maps (not a subfolder).'),
        ('ERROR custom map rows must be exactly 24 characters', 'Count the tiles: every ROW has 24 characters, no spaces.'),
        ('ERROR ... missing CUSTOM_CAMPAIGN_DATA markers', 'Both marker lines must be present exactly as shown, three dashes each side.'),
        ('ERROR custom campaign maps require contiguous MAP records', 'Number maps 0, 1, 2... with no gap, each with one MAP record.'),
        ('ERROR custom map chunk footprints overlap', 'Two maps share an origin area: origins must be 24 m apart.'),
        ('You fall through the floor or start in a wall', 'The spawn tile must be floor (.) and inside 0-24.'),
        ('A model is invisible or inside out', 'Check width/depth/height are positive and its yaw; try yaw 0 and 3.142.'),
        ('It is too dark', 'Raise MAP ambient (0.3-0.4) or add LIGHT records just below the ceiling.')]))
    o.append('<p style="color:#6a707a;margin-top:40px">Generated by tools/campaign/make_custom_map_guide.py.</p></main></body></html>')
    with open(os.path.join(MAPS, 'GUIDE.html'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(o))
    print('GUIDE.html written (%d models, %d shots)' % (len(models), sum(os.path.exists(os.path.join(IMAGES, n[0] + '.jpg')) for n in SHOTS)))


if __name__ == '__main__':
    main()
