"""Generate the Starter Map: every part of a custom map lined up, like a debug world.

  python tools/campaign/make_starter_map.py [OUT_TXT] [--index OUT_JSON]

Each part stands on its own plinth with a sign (ID and name) and a HUD label when you
walk up to it. Maps are 24 m cells on a grid, open wherever they touch. Row 0 holds
the rooms, east from the spawn; every later row is one set of models, read eastward,
with a clear corridor down the west side joining the rows:

  WELCOME     map tiles, structure materials, pipes, stairs, doors, water, pickups,
              terminals and signs
  LIGHTS      colour temperatures, tints, gels, flicker, timed and wall lamps, darkness
  HAZARDS     the eight hazard kinds fenced off, a compactor, smoke, glass, cables
  LIFE        every creature (asleep until hurt) and every actor body, standing still
  LEVELS      a second layer: a deck over the hall, a stair up, a hole to look down
  PROPS       the loose clutter kinds and every curated prop you can pick up and throw
  MODELS      every fixture model by set; buildings over 7.5 m shown scaled down
  OUTDOORS    under the model rows: ground paints, terrain, a grove, then the SEAMS
              between maps, north-south and east-west, signed with their open flags

Writes "custom maps/Starter Map.txt" by default. The file is also the worked
example for custom maps/GUIDE.html (tools/campaign/make_custom_map_guide.py).
"""
import json, math, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PI, HALF = math.pi, math.pi / 2
NEVER = 'starter_never'          # a state nothing sets: keeps creatures asleep

# Sizes for the engine's own models (slots 0-62 carry no catalogue size): typical
# placements in the shipped maps, or a measured guess where none exist.
LEGACY_SIZE = {
    0: (2.0, .2, 3.0), 1: (2.0, .2, 3.0), 2: (.6, .6, 3.0), 3: (1.0, .3, .6), 4: (1.0, 1.0, .15), 5: (3.0, .4, 3.0),
    6: (1.87, .6, .99), 7: (1.65, .5, 1.8), 8: (.67, .2, .91), 9: (3.0, .2, 1.5), 10: (3.0, .2, 1.5), 11: (.6, .5, .5),
    12: (2.0, 1.4, 1.6), 13: (.91, .5, 2.2), 14: (1.58, 1.62, 2.2), 15: (1.2, 1.15, .16), 16: (2.05, 4.9, 2.15),
    20: (1.2, .6, 1.2), 22: (.78, .49, 1.3), 23: (1.2, 2.6, 2.2), 24: (1.4, .7, .75), 25: (1.6, 1.6, .75),
    31: (.9, .5, .75), 32: (.5, .6, 1.3), 35: (.35, .35, 1.1), 37: (.9, .3, 1.8), 39: (1.9, 1.4, 2.7), 40: (.4, .4, .7),
    41: (2.4, 1.0, 1.1), 42: (.5, .5, .6), 43: (.3, .3, 4.0), 44: (.4, .1, 4.7), 45: (.32, .12, .32), 46: (3.0, 3.0, 2.5),
    47: (2.44, 6.1, 2.6), 48: (2.44, 10.8, 2.5), 49: (.55, .55, .8), 50: (1.0, .8, 1.9), 51: (1.86, .44, .63),
    52: (1.4, .55, .7), 53: (3.77, 1.72, 2.27), 55: (2.2, 2.8, 2.9), 56: (3.83, 22.4, 5.33), 57: (1.6, .3, 1.1),
    58: (.97, .41, 2.24), 60: (1.29, .08, 1.58), 61: (2.27, .54, .95), 62: (1.5, .68, 2.38)}

SECTIONS = [  # (title, test on the model name); the first that matches wins
    ('ENGINE AND OFFICE', lambda n, s: s < 63 or n.startswith('office_')),
    ('INDUSTRIAL', lambda n, s: n.startswith('ind_')),
    ('MEDICAL', lambda n, s: n.startswith(('med_', 'clinic_'))),
    ('LABS', lambda n, s: n.startswith(('lab_', 'anl_'))),
    ('POWER AND WATER', lambda n, s: n.startswith('pwr_')),
    ('MINES', lambda n, s: n.startswith('mine_')),
    ('CAVERNS', lambda n, s: n.startswith(('cave_', 'rock_'))),
    ('SCRAP CITY', lambda n, s: n.startswith('scrap_')),
    ('OUTDOORS', lambda n, s: True)]

CLUTTER_KINDS = ['paper', 'cardboard', 'bottle', 'electronics A', 'electronics B', 'plastic', 'wood']
CREATURES = ['Huntsman', 'Wasp', 'Brute', 'Warden', 'Mutant', 'Snatcher', 'Chimera', 'Transformed']
ACTORS = ['Worker', 'Wasp', 'Huntsman', 'Cargo', 'Cleanup', 'Cleanup Engineer', 'Cleanup Sharpshooter', 'Cleanup Heavy',
          'Cleanup Medic', 'Cleanup Rusher', 'Scientist', 'Senior Scientist', 'Female Scientist', 'Female Doctor',
          'Lab Tech', 'Investigator']
HAZARDS = ['Electricity', 'Steam', 'Crusher', 'Toxic', 'Fire', 'Falling debris', 'Pressure', 'Anomaly']
PICKUPS = ['Health', 'Shotgun ammo', 'Handgun ammo']
MATERIALS = {0: 'default wall', 1: 'water (hidden)', 2: 'steel', 3: 'wall finish', 4: 'rust', 5: 'concrete',
             6: 'hidden', 7: 'grating', 8: 'dark steel', 9: 'hidden', 10: 'brick', 11: 'tread plate', 12: 'timber',
             13: 'hazard stripe', 14: 'yellow paint', 15: 'bunker concrete', 16: 'blood', 17: 'glass', 18: 'rubber',
             19: 'copper', 20: 'pale steel', 21: 'office wall', 22: 'drop ceiling', 23: 'carpet', 24: 'office tile',
             25: 'glow', 26: 'rock', 27: 'clinic tile', 28: 'lab panel', 29: 'corrugated', 30: 'invisible proxy',
             31: 'brick', 32: 'mine timber', 33: 'ancient stone', 34: 'growth', 35: 'crystal'}
SURFACES = ['concrete', 'pressure works', 'brick', 'clinic tile', 'lab panel', 'rock', 'corrugated', 'office plaster',
            'mine timber', 'ancient stone', 'clinic teal', 'clinic green', 'clinic maroon']
START_X = 2.2                     # museum rows start east of the section corridor
ACCENT = {'WELCOME': 0x3c8cd8, 'LIGHTS': 0xe0b030, 'HAZARDS': 0xd84030, 'LIFE': 0x50b060, 'PROPS': 0x9070d0}


def f(v):
    if isinstance(v, str): return v
    if isinstance(v, bool): return '1' if v else '0'
    if isinstance(v, int): return str(v)
    s = f'{v:.4f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


class Starter:
    def __init__(self):
        self.maps = []          # one dict per map: records, rows, walls, meta
        self.index = []         # what stands where, for the guide
        self.world = []         # campaign-wide records (terrain, ground paint, horizon), world metres

    # --- maps -------------------------------------------------------------------------------
    def new_map(self, name, col, row, ceiling=4.0, surface=None, ambient=.34):
        """Maps sit on a grid of 24 m cells; an edge is open wherever a neighbour exists."""
        m = {'name': name, 'records': [], 'ceiling': ceiling, 'surface': surface, 'ambient': ambient,
             'walls': set(), 'lights': True, 'index': len(self.maps), 'col': col, 'row': row,
             'outdoor': False, 'layers': []}
        self.maps.append(m)
        return m

    def outdoor_map(self, name, col, row):
        """Environment 1 under the zone sky: the campaign's TERRAIN shapes the ground and every
        height on the map (props, signs, posts, creatures) is measured from that ground."""
        m = self.new_map(name, col, row, ceiling=2.8, ambient=.3)
        m.update(outdoor=True, lights=False)
        return m

    def rec(self, m, *fields):
        m['records'].append('|'.join(f(x) for x in fields))

    def zone(self, m, x1, y1, x2, y2, name, title='', bottom='', top=''):
        x1, y1, x2, y2 = max(0, x1), max(0, y1), min(24, x2), min(24, y2)
        self.rec(m, 'ZONE', m['index'], x1, y1, x2, y2, name[:40], bottom, top, title[:40])

    def sign(self, m, x, y, z, w, h, title, sub, accent, yaw=PI):
        self.rec(m, 'SIGN', m['index'], x, y, z, w, h, yaw, title[:28], sub[:34], accent)

    def struct(self, m, x1, y1, x2, y2, bottom, top, material, rail=0):
        self.rec(m, 'STRUCT', m['index'], x1, y1, x2, y2, bottom, top, rail, material)

    def post_sign(self, m, cx, front, title, sub, accent, width=.9):
        """A label stand at the front of a pad: a short steel post, the sign on its face."""
        self.struct(m, cx - width / 2, front - .06, cx + width / 2, front, 0, .55, 20)
        self.sign(m, cx, front - .07, .34, width - .06, .3, title, sub, accent)

    def fence(self, m, x1, y1, x2, y2):
        for a in ((x1, y1, x2, y1 + .06), (x1, y2 - .06, x2, y2), (x1, y1, x1 + .06, y2), (x2 - .06, y1, x2, y2)):
            self.struct(m, *a, 0, 1.1, 2, rail=1)

    # --- museum rows --------------------------------------------------------------------------
    def museum(self, title, items, accent, surface_cycle, row):
        """Pack items into rows of plinths across as many maps as it takes, eastward along
        one grid row. Each item is a dict with w, d, h, a label and an emit(m, cx, cy, base)
        that writes its own records. x 0-2 stays clear: the corridor between sections."""
        maps = []
        def begin():
            n = len(maps)
            tall = max((it['h'] for it in items), default=2)
            m = self.new_map('%s%s' % (title.title(), ' %d' % (n + 1) if n else ''), n, row,
                             ceiling=min(14.0, max(4.5, tall + 1.2)),
                             surface=surface_cycle[(len(self.maps)) % len(surface_cycle)])
            if n == 0: self.zone(m, 0, 0, 24, 1.2, '', title)
            maps.append(m)
            return m
        m = begin(); x = START_X; y0 = 2.6; band = []
        def flush():
            nonlocal y0, band
            if not band: return
            depth = max(b[1]['d'] for b in band) + .3
            for cx, it, cw in band:
                cy = y0 + depth / 2
                small = it['h'] < .6 and it['w'] < 1.3 and it['d'] < 1.3
                top = .75 if small else .04
                pw, pd = it['w'] + .2, it['d'] + .2
                self.struct(m, cx - pw / 2, cy - pd / 2, cx + pw / 2, cy + pd / 2, 0, top, 20 if small else 13)
                it['emit'](m, cx, cy, top)
                front = cy - pd / 2
                if small:
                    self.sign(m, cx, front - .01, .42, min(.9, cw - .1), .28, it['title'], it['sub'], accent)
                else:
                    self.post_sign(m, cx, front - .15, it['title'], it['sub'], accent, min(.9, cw - .1))
                self.zone(m, cx - cw / 2, y0 - 2.0, cx + cw / 2, y0 + depth, it['zone'])
                self.index.append({'map': m['index'], 'section': title, 'x': round(cx, 2), 'y': round(cy, 2),
                                   'title': it['title'], 'name': it['sub'], 'w': it['w'], 'd': it['d'], 'h': it['h']})
            y0 += depth + 2.3
            band = []
        for it in items:
            cw = max(it['w'], .85) + .55
            if x + cw > 22.8:
                flush(); x = START_X
            depth = it['d'] + .3
            if y0 + max([depth] + [b[1]['d'] + .3 for b in band]) > 22.6:
                flush(); x = START_X
                if y0 + depth > 22.6:
                    m = begin(); y0 = 2.6
            band.append((x + cw / 2, it, cw)); x += cw
        flush()
        return maps

    # --- output -------------------------------------------------------------------------------
    def write(self, path):
        out = ['META_LEVEL_ID: 0', 'META_LEVEL_NAME: Starter Map', 'META_DEFAULT_TARGET: CUSTOM',
               'META_CAMPAIGN_NAME: STARTER MAP', '',
               '# The Starter Map: every part of a custom map lined up, like a debug world.',
               '# Generated by tools/campaign/make_starter_map.py. Read custom maps/GUIDE.html first;',
               '# every line below is one record of the format it explains. Lines starting # are notes.',
               '', '--- CUSTOM_CAMPAIGN_DATA_START ---', 'CAMPAIGN|STARTER MAP|0']
        if self.world:
            out += ['', '# ==== Campaign-wide: terrain, ground paint, horizon (world metres; outdoor maps only) ===='] + self.world
        cells = {(m['col'], m['row']) for m in self.maps}
        for i, m in enumerate(self.maps):
            out += ['', '# ==== Map %d: %s ====' % (i, m['name'])]
            spawn = m.get('spawn', (12, 1.6, HALF))
            c, r = m['col'], m['row']
            # The y=0 edge is "north", y=24 "south".
            north, south, west, east = ((c, r - 1) in cells, (c, r + 1) in cells, (c - 1, r) in cells, (c + 1, r) in cells)
            # MAP|index|name|originX|originY|spawnX|spawnY|spawnZ|environment|ceiling|ambient|skybox|
            #     openN|openS|openW|openE|lift|residencyGroup|floorMaterial|musicCue|spawnYaw|spawnPitch|startUnarmed
            out.append('|'.join(f(v) for v in ['MAP', i, 'Starter Map / ' + m['name'], 24 * c, 24 * r, spawn[0], spawn[1], 0,
                                                int(m['outdoor']), m['ceiling'], m['ambient'],
                                                'zone_overcast' if m['outdoor'] else 'industrial_night',
                                                int(north), int(south), int(west), int(east), 0, -1, 0, 0, spawn[2], 0, 0]))
            if m['surface'] is not None:
                s = SURFACES.index(m['surface'])
                out.append('SURFACE|%d|%d|%d|-1' % (i, s, s if s not in (3, 4, 10, 11, 12) else 0))
            out.append('LAYER|%d|0|%s|0|0' % (i, 'Ground' if m['outdoor'] else 'Hall floor'))
            # Outdoors a closed edge needs no wall: the MAP open flags alone stop you there.
            edge = (lambda x, y: False) if m['outdoor'] else (
                lambda x, y: (y == 0 and not north) or (y == 23 and not south) or (x == 0 and not west) or (x == 23 and not east))
            for y in range(24):
                row = ''.join('#' if edge(x, y) or (x, y) in m['walls'] else m.get('tiles', {}).get((x, y), '.') for x in range(24))
                out.append('ROW|%d|0|%d|%s' % (i, y, row))
            for k, (name, elevation, thickness, deck) in enumerate(m['layers'], 1):
                out.append('LAYER|%d|%d|%s|%s|%s' % (i, k, name, f(elevation), f(thickness)))
                for y in range(24):
                    out.append('ROW|%d|%d|%d|%s' % (i, k, y, ''.join('=' if (x, y) in deck else '.' for x in range(24))))
            if m['lights']:
                z = m['ceiling'] - .15
                for lx in (4, 12, 20):
                    for ly in (4, 12, 20):
                        out.append('LIGHT|%d|%s|%s|%s|0|0|1.1|%s' % (i, lx, ly, f(z), f(min(24, m['ceiling'] + 9))))
            out += m['records']
        out.append('--- CUSTOM_CAMPAIGN_DATA_END ---')
        with open(path, 'w', encoding='ascii', newline='\r\n') as fh:
            fh.write('\n'.join(out) + '\n')
        return len(out)


def models():
    t = open(os.path.join(ROOT, 'src', 'world', 'ModelIds.h')).read()
    names = [(int(i), n) for n, i in re.findall(r'inline constexpr int (\w+)=(\d+);', t)]
    sizes = [tuple(map(float, re.findall(r'[-\d.]+', r)))
             for r in re.findall(r'\{([^}]*)\}', re.search(r'float size\[\]\[3\]=\{(.*?)\};', t, re.S).group(1))]
    fronts = json.load(open(os.path.join(ROOT, 'tools', 'dev', 'model_fronts.json')))
    result = []
    for slot, name in sorted(names):
        w, d, h = sizes[slot] if slot < len(sizes) and sizes[slot] != (0, 0, 0) else LEGACY_SIZE.get(slot, (1, 1, 1))
        # Unlisted engine models (0-62) face -z: yaw 0 shows their front, as the shipped maps place them.
        front = fronts.get(name, {}).get('front', '+z') if isinstance(fronts.get(name), dict) else '+z' if slot >= 63 else '-z'
        result.append((slot, name, w, d, h, {'+z': PI, '-z': 0, '+x': HALF, '-x': -HALF}.get(front, PI)))
    return result


def build():
    s = Starter()
    blue = ACCENT['WELCOME']

    # ---- WELCOME -----------------------------------------------------------------------------
    m = s.new_map('Welcome', 0, 0, ceiling=4.5)
    m['spawn'] = (12, 2.2, HALF)
    # The far wall keeps one gap (x 9-12) into the model sections behind it.
    m['walls'] |= {(x, 23) for x in range(24) if not 9 <= x <= 11}
    s.zone(m, 0, 0, 24, 3.5, 'WELCOME', 'STARTER MAP')
    s.sign(m, 17.5, 22.95, 3.0, 6.0, 1.1, 'STARTER MAP', '< ROOMS    MODELS: THROUGH THE GAP >', blue)
    s.sign(m, 10.5, 22.95, 3.4, 2.4, .4, 'MODELS THIS WAY', 'one row per set, west corridor', blue)
    s.rec(m, 'TERMINAL', 0, 9.5, 2.6, 0, 0, 'STARTER MAP / READ ME',
          'EVERY PART OF A CUSTOM MAP STANDS HERE WITH ITS ID.',
          'ROOMS: WALK EAST. MODELS: THROUGH THE GAP. GUIDE.html', '', 0, '', HALF)
    # Map tiles: characters in ROW records that build blocks by themselves.
    for x, ch, name in ((2, 'C', 'crate tile 0.6 m'), (4, 'B', 'barrel tile (explodes)'), (6, 'T', 'tank tile 2.6 m')):
        m.setdefault('tiles', {})[(x, 6)] = ch
        s.post_sign(m, x + .5, 5.75, 'ROW "%s"' % ch, name, blue, .95)
        s.zone(m, x - .3, 3.8, x + 1.3, 7.2, 'ROW TILE %s / %s' % (ch, name))
        s.index.append({'map': 0, 'section': 'WELCOME', 'x': x + .5, 'y': 6.5, 'title': 'ROW "%s"' % ch, 'name': name})
    # STRUCT materials: one pillar each, two rows.
    for k in range(36):
        x = 1.4 + (k % 18) * 1.18; y = 9.2 if k < 18 else 11.8
        if k != 30: s.struct(m, x - .3, y - .3, x + .3, y + .3, 0, 1.4, k)
        else: s.struct(m, x - .3, y - .3, x + .3, y + .3, 0, .02, 13)
        s.sign(m, x, y - .31, .25, .56, .3, 'MAT %d' % k, MATERIALS[k], blue)
        s.zone(m, x - .58, y - 1.2, x + .58, y + .4, 'STRUCT material %d / %s' % (k, MATERIALS[k]))
    # Pipes: the seven finishes, standing as risers.
    for k in range(7):
        x = 14.5 + k * 1.1
        s.rec(m, 'PIPE', 0, x, 5.6, x, 5.6, 0, .09, 2.6, k)
        s.post_sign(m, x, 5.25, 'PIPE %d' % k, ['service', 'rusted', 'steel', 'yellow', 'red', 'pale', 'dark'][k], blue, .95)
        s.zone(m, x - .55, 3.8, x + .55, 6.2, 'PIPE material %d' % k)
    # Stairs: solid, open underside, open with rails; each up to a deck.
    for k, (x, label, extra) in enumerate(((1.5, 'STAIR solid', []), (4.5, 'STAIR open', [1, .08, 0]), (7.5, 'STAIR open+rails', [1, .08, 1]))):
        s.rec(m, 'STAIR', 0, x, 14.5, x + 2, 18, 0, 1.6, 9, 1, 1, *extra)
        s.struct(m, x, 18, x + 2, 20, 1.5, 1.6, 11)
        s.post_sign(m, x + 1, 14.2, label, 'STAIR record', blue, 1.6)
        s.zone(m, x, 13.2, x + 2, 20, label)
    # Water: a basin you can wade in.
    for a in ((13, 13.5, 18, 13.6), (13, 17.4, 18, 17.5), (13, 13.5, 13.1, 17.5), (17.9, 13.5, 18, 17.5)):
        s.struct(m, *a, 0, .5, 5)
    s.rec(m, 'WATER', 0, 13.1, 13.6, 17.9, 17.4, 0, .42)
    s.post_sign(m, 15.5, 13.3, 'WATER', 'wade in', blue, 1.4)
    s.zone(m, 13, 12.4, 18, 17.5, 'WATER volume')
    # Pickups.
    for k, name in enumerate(PICKUPS):
        x = 19.5 + k * 1.3
        s.rec(m, 'PICKUP', 0, k, x, 15.5)
        s.post_sign(m, x, 14.6, 'PICKUP %d' % k, name, blue, 1.1)
        s.zone(m, x - .6, 13.4, x + .6, 16.4, 'PICKUP kind %d / %s' % (k, name))
    # Doors: a short block wall with a sliding and a swinging door into a closet.
    for x in range(13, 24):
        if x not in (15, 16, 19, 20): m['walls'].add((x, 20))
    m['walls'] |= {(12, 21), (12, 22), (23, 21), (23, 22)}
    s.rec(m, 'DOOR', 0, 15, 17, 20.5, 0, 0, 0, 0, 0, 0)
    s.rec(m, 'DOOR', 0, 19, 21, 20.5, 0, 0, 0, 1, 0, 0)
    s.sign(m, 16, 19.95, 2.75, 1.6, .32, 'DOOR sliding', 'DOOR record', blue)
    s.sign(m, 20, 19.95, 2.75, 1.6, .32, 'DOOR swinging', 'swinging flag 1', blue)
    s.zone(m, 14.5, 18.4, 17.5, 20, 'DOOR / sliding')
    s.zone(m, 18.5, 18.4, 21.5, 20, 'DOOR / swinging')
    # A control terminal that switches a red lamp.
    s.rec(m, 'TERMINAL', 0, 13.6, 18.8, 0, 1, 'LAMP SWITCH', 'A CONTROL TERMINAL SETS A STATE.', 'THE RED LAMP LISTENS FOR IT.', 'starter_lamp', 1, '', -HALF)
    s.rec(m, 'LIGHT', 0, 14.0, 18.8, 4.3, 0, 0, 1.6, 5, 3400, 0, 'starter_lamp', 1, 0, '', '', '', '', '', '', '', '', 1, .1, .06)
    s.zone(m, 12.4, 17.6, 14.4, 20, 'TERMINAL control + switched LIGHT')
    s.sign(m, 13.5, 19.95, 1.4, .9, .3, 'TERMINAL', 'E: red lamp', blue)

    # ---- LIGHTS --------------------------------------------------------------------------------
    m = s.new_map('Lights', 1, 0, ceiling=3.6, ambient=.12)
    m['lights'] = False
    s.zone(m, 0, 0, 2.5, 24, '', 'LIGHTS')
    gold = ACCENT['LIGHTS']
    for y in (16, 17, 18, 19, 20, 21, 22, 1, 2, 3, 4, 5, 6, 7):
        for x in (1, 4, 7, 10, 13, 16, 19, 22):
            m['walls'].add((x, y))
    north = [('2000 K', 'sodium', dict(kelvin=2000)), ('2800 K', 'incandescent', dict(kelvin=2800)), ('4000 K', 'warm tube', dict(kelvin=4000)),
             ('5600 K', 'daylight', dict(kelvin=5600)), ('8000 K', 'cold', dict(kelvin=8000)), ('TINT +0.6', 'fluorescent green', dict(tint=.6)),
             ('TINT -0.6', 'magenta', dict(tint=-.6))]
    south = [('GEL RED', 'gel 1 0.1 0.06', dict(gel=(1, .1, .06))), ('GEL GREEN', 'gel 0.1 1 0.15', dict(gel=(.1, 1, .15))),
             ('GEL BLUE', 'gel 0.15 0.35 1', dict(gel=(.15, .35, 1))), ('FLICKER 0.6', 'failing tube', dict(flicker=.6)),
             ('TIMED', 'period 2 s, on 1 s', dict(period=2, onTime=1)), ('WALL LAMP', 'mount 1, caged', dict(mount=1)),
             ('DARK ZONE', 'no light; torch jams', dict(dark=True))]
    for row, items in ((1, north), (0, south)):
        for k, (title, sub, o) in enumerate(items):
            cx = 2.5 + k * 3 + .5
            y1, y2 = (16, 23) if row else (1, 8)
            ly = 19.5 if row else 4.5
            if o.get('dark'):
                s.rec(m, 'DARK', 1, cx - 1, y1, cx + 1, y2)
            elif o.get('mount') == 1:
                s.rec(m, 'LIGHT', 1, cx, (23.0 if row else 1.0), 1.9, 1, (-HALF if row else HALF), 1.4, 5)
            else:
                g = o.get('gel')
                tail = [o.get('kelvin', 3400), o.get('tint', 0), '', '', '', o.get('period', ''), o.get('onTime', ''), '', o.get('flicker', '')]
                if g: tail += ['', '', '', '', *g]
                s.rec(m, 'LIGHT', 1, cx, ly, 3.4, 0, 0, 1.5, 5, *tail)
            s.sign(m, cx, (22.95 if row else 1.05), 1.0, 1.7, .4, title, sub, gold, PI if row else 0)
            s.zone(m, cx - 1, y1, cx + 1, y2, 'LIGHT / %s / %s' % (title, sub))
            s.index.append({'map': 1, 'section': 'LIGHTS', 'x': cx, 'y': ly, 'title': title, 'name': sub})
    s.rec(m, 'LIGHT', 1, 12, 12, 3.45, 0, 0, .7, 9)

    # ---- HAZARDS -------------------------------------------------------------------------------
    m = s.new_map('Hazards', 2, 0, ceiling=4.5)
    s.zone(m, 0, 0, 2.5, 24, '', 'HAZARDS')
    red = ACCENT['HAZARDS']
    for k, name in enumerate(HAZARDS):
        cx = 3.2 + (k % 4) * 5.2; cy = 5.5 if k < 4 else 18.0
        s.fence(m, cx - 1.7, cy - 1.7, cx + 1.7, cy + 1.7)
        s.rec(m, 'HAZARD', 2, k, cx - 1.4, cy - 1.4, cx + 1.4, cy + 1.4, 0, 2.2, 2)
        # Both rows face the middle aisle: the south row's signs on its north fence, and back.
        if k < 4: s.sign(m, cx, cy + 1.71, .7, 1.6, .36, 'HAZARD %d' % k, name, red, 0)
        else: s.sign(m, cx, cy - 1.71, .7, 1.6, .36, 'HAZARD %d' % k, name, red, PI)
        s.zone(m, cx - 1.9, cy - 1.9 if k < 4 else cy - 3.2, cx + 1.9, cy + 3.2 if k < 4 else cy + 1.9, 'HAZARD kind %d / %s' % (k, name))
        s.index.append({'map': 2, 'section': 'HAZARDS', 'x': cx, 'y': cy, 'title': 'HAZARD %d' % k, 'name': name})
    s.rec(m, 'EMITTER', 2, 16.2, 11.8, .3, .2, .1, 2.4, .3, .1, .5, 7)
    s.post_sign(m, 16.2, 10.9, 'EMITTER', 'smoke plume', red, 1.2)
    s.zone(m, 15.2, 9.6, 17.2, 12.8, 'EMITTER / smoke')
    s.fence(m, 18.4, 9.6, 22.6, 13.6)
    s.rec(m, 'COMPACTOR', 2, 18.7, 9.9, 22.3, 13.3, 0, 2.6, 5)
    s.sign(m, 20.5, 9.59, .7, 1.6, .36, 'COMPACTOR', 'crushes on a cycle', red)
    s.zone(m, 18.4, 8.4, 22.6, 13.6, 'COMPACTOR / crushes every 5 s')
    s.rec(m, 'BREAKABLE', 2, 7.0, 11.75, 9.4, 11.8, 0, 2.2, 17, 20, 0, 'starter_glass_broken', NEVER)
    s.post_sign(m, 8.2, 11.1, 'BREAKABLE', 'glass, material 17', red, 1.3)
    s.zone(m, 7, 9.9, 9.4, 12.2, 'BREAKABLE / glass pane')
    for x in (11.0, 14.0):
        s.struct(m, x - .05, 11.7, x + .05, 11.8, 0, 1.6, 2)
    s.rec(m, 'LEAD', 2, 11.05, 11.75, 1.3, 13.95, 11.75, 1.3, 3.4, .008, 8, 1)
    s.rec(m, 'LEAD', 2, 11.05, 11.75, .8, 11.6, 12.4, 0, 1.3, .007, '', '', '', '', '', '', '', 1)
    s.post_sign(m, 12.5, 11.0, 'LEAD', 'live cable, loose cable', red, 1.6)
    s.zone(m, 10.6, 9.9, 14.4, 12.6, 'LEAD / live red cable, loose black')

    # ---- LIFE ----------------------------------------------------------------------------------
    m = s.new_map('Life', 3, 0, ceiling=5.0)
    s.zone(m, 0, 0, 2.5, 24, '', 'LIFE')
    green = ACCENT['LIFE']
    for k, name in enumerate(CREATURES):
        cx = 3.3 + (k % 4) * 5.3; cy = 5.2 if k < 4 else 12.0
        s.fence(m, cx - 2.1, cy - 2.1, cx + 2.1, cy + 2.1)
        s.rec(m, 'CREATURE', 3, k, cx, cy, 0, '', '', NEVER)
        s.sign(m, cx, cy - 2.16, .7, 1.8, .36, 'CREATURE %d' % k, name + ' / asleep', green)
        s.zone(m, cx - 2.3, cy - 3.6, cx + 2.3, cy + 2.1, 'CREATURE kind %d / %s (asleep)' % (k, name))
        s.index.append({'map': 3, 'section': 'LIFE', 'x': cx, 'y': cy, 'title': 'CREATURE %d' % k, 'name': name})
    s.rec(m, 'TIMED_SEQUENCE', 3, 'starter_clock', 'starter_clock_done', 0, 0, 24, 24, -100, 100, 60000, 60000, 0, 0, 0, 0, 1, 1)
    for k, name in enumerate(ACTORS):
        cx = 1.6 + k * 1.37; cy = 19.6
        s.rec(m, 'ACTOR', 3, 'starter_clock', k, 1.75 if k not in (1, 2) else 1.0, 0, 0, k)
        s.rec(m, 'ACTOR_KEY', 3, k, 0, cx, cy, 0, -HALF, 0, 0)
        s.rec(m, 'ACTOR_KEY', 3, k, 60000, cx, cy, 0, -HALF, 0, 1)
        s.sign(m, cx, 18.3, .42, 1.25, .3, 'ACTOR %d' % k, name, green)
        s.struct(m, cx - .63, 18.31, cx + .63, 18.37, 0, .6, 20)
        s.zone(m, cx - .68, 16.4, cx + .68, 21, 'ACTOR visual %d / %s' % (k, name))
        s.index.append({'map': 3, 'section': 'LIFE', 'x': cx, 'y': cy, 'title': 'ACTOR %d' % k, 'name': name})

    # ---- LEVELS: one map stacked vertically. Layer 0 is the hall floor; layer 1 is a steel
    # deck at 3.5 m drawn with '=' tiles, reached by a stair, with a hole you can look down.
    m = s.new_map('Levels', 4, 0, ceiling=7.5)
    m['lights'] = False
    teal = 0x40b8c0
    deck = {(x, y) for x in range(12, 23) for y in range(1, 23)} - {(x, y) for x in (16, 17) for y in (8, 9)}
    m['layers'].append(('Upper deck', 3.5, .25, deck))
    s.zone(m, 0, 0, 2.5, 24, '', 'LEVELS')
    s.rec(m, 'STAIR', 4, 6, 10, 12, 12, 0, 3.5, 18, 0, 1, 1, .08, 1)
    s.post_sign(m, 7, 9.7, 'STAIR to layer 1', 'rises east to 3.5 m', teal, 1.6)
    s.zone(m, 5, 9, 12, 12.6, 'STAIR 0 to 3.5 m, joins the deck at x 12', bottom=-.5, top=3.4)
    s.sign(m, 22.95, 6, 1.7, 2.4, .5, 'LAYER 0 / elevation 0', 'the ground floor, under the deck', teal, -HALF)
    s.sign(m, 22.95, 18, 5.3, 2.4, .5, 'LAYER 1 / elevation 3.5', "'=' tiles: a deck 0.25 m thick", teal, -HALF)
    s.zone(m, 12, 0, 24, 24, 'LAYER 0 / under the deck (headroom 3.25)', bottom=-.5, top=3.2)
    s.zone(m, 12, 0, 24, 24, "LAYER 1 / the deck, '=' tiles at 3.5 m", bottom=3.3, top=7.5)
    s.zone(m, 15.5, 7.5, 18.5, 10.5, "a hole: '.' tiles in the deck row", bottom=3.3, top=7.5)
    s.struct(m, 15.5, 6.9, 17.5, 6.96, 3.5, 4.05, 20)
    s.sign(m, 16.5, 6.89, 3.84, 1.94, .3, "HOLE: '.' IN LAYER 1", 'look down to layer 0', teal)
    # Fixtures and props count from the bottom layer: base 3.5 stands on the deck.
    for x, y, base, sl, w, d, h, label in ((20, 14, 3.5, 24, 1.4, .7, .75, 'desk on the deck, base 3.5'),
                                           (20, 14, 0, 24, 1.4, .7, .75, 'desk under it, base 0'),
                                           (14.5, 20, 3.5, 6, 1.87, .6, .99, 'generator, base 3.5'),
                                           (14.5, 4, 0, 12, 2.0, 1.4, 1.6, 'compressor, base 0')):
        s.rec(m, 'FIXTURE', 4, sl, x, y, base, w, d, h, 0, 1)
        s.zone(m, x - 1.4, y - 1.4, x + 1.4, y + 1.4, '#%d %s' % (sl, label), bottom=base - .2, top=base + 3.2)
    for x, y in ((14, 4), (20, 4), (14, 12), (20, 12), (14, 20), (20, 20)):
        s.rec(m, 'LIGHT', 4, x, y, 3.05, 0, 0, 1.0, 6)     # under the deck
        s.rec(m, 'LIGHT', 4, x, y, 7.3, 0, 0, 1.1, 8)      # over it
    for y in (4, 12, 20):
        s.rec(m, 'LIGHT', 4, 5, y, 7.3, 0, 0, 1.2, 12)      # the open, double-height west half
    s.index.append({'map': 4, 'section': 'LEVELS', 'x': 17, 'y': 12, 'title': 'LAYER 1', 'name': 'deck at 3.5 m'})

    # ---- PROPS (carryable) and MODELS -----------------------------------------------------------
    physics = json.load(open(os.path.join(ROOT, 'tools', 'campaign', 'prop_physics.json')))
    allm = models()
    slot = {n: i for i, n, *_ in allm}
    catalogue = {n: (w, d, h) for _, n, w, d, h, _ in allm}
    props = []
    for k, name in enumerate(CLUTTER_KINDS):
        props.append({'w': .4, 'd': .4, 'h': .3, 'title': 'CLUTTER %d' % k, 'sub': name, 'zone': 'CLUTTER kind %d / %s' % (k, name),
                      'emit': (lambda k: lambda m, cx, cy, base: s.rec(m, 'CLUTTER', m['index'], k, cx, cy, base, 0))(k)})
    for name, p in physics.items():
        if name not in slot: continue
        w, d, h = p.get('size', catalogue[name])
        props.append({'w': max(w, .2), 'd': max(d, .2), 'h': h, 'title': 'PROP %d' % slot[name], 'sub': name,
                      'zone': 'CLUTTER 7 / #%d %s / %g kg' % (slot[name], name, p.get('mass', 0)),
                      'emit': (lambda sl, w, d, h: lambda m, cx, cy, base: s.rec(m, 'CLUTTER', m['index'], 7, cx, cy, base, 0, sl, w, d, h, 0, 0, ''))(slot[name], w, d, h)})
    s.museum('PROPS', props, ACCENT['PROPS'], [None], 1)

    groups = {t: [] for t, _ in SECTIONS}
    for sl, name, w, d, h, yaw in allm:
        groups[next(t for t, test in SECTIONS if test(name, sl))].append((sl, name, w, d, h, yaw))
    palette = [0xc8a050, 0x60a0d0, 0xd06060, 0x60c0a0, 0xd0b040, 0xa08060, 0x8080c0, 0xc07040, 0x70b050]
    for gi, (title, _) in enumerate(SECTIONS):
        items = []
        for sl, name, w, d, h, yaw in groups[title]:
            scale = min(1.0, 7.5 / max(w, d), 12.4 / h)
            sw, sd, sh = w * scale, d * scale, h * scale
            if yaw in (HALF, -HALF): fw, fd = sd, sw
            else: fw, fd = sw, sd
            items.append({'w': fw, 'd': fd, 'h': sh, 'title': '#%d' % sl, 'sub': name + (' x%.2f' % scale if scale < 1 else ''),
                          'zone': ('#%d %s %gx%gx%g' % (sl, name, round(w, 1), round(d, 1), round(h, 1)))[:40],
                          'emit': (lambda sl, sw, sd, sh, yaw: lambda m, cx, cy, base:
                                   s.rec(m, 'FIXTURE', m['index'], sl, cx, cy, base, sw, sd, sh, yaw, int(sh > .05)))(sl, sw, sd, sh, yaw)})
        s.museum(title, items, palette[gi], SURFACES, 2 + gi)
    outdoors(s, slot, catalogue)
    return s


GROUNDS = ['short grass', 'leafy grass', 'clover', 'wild', 'dry dirt', 'forest litter', 'gravel', 'mud', 'meadow']


def outdoors(s, slot, catalogue):
    """Outdoor maps under the last model row, and the seams between maps made visible.
    Terrain records are campaign-wide and in world metres (map origin + local x, y)."""
    row = 1 + len(SECTIONS) + 1                       # the first row under the model rows
    oy = 24 * row                                     # its world y
    olive = 0x8aa040
    # The indoor row above keeps its south wall, with a doorway where its west corridor runs:
    # '#' tiles in an open edge close that part of the seam.
    for m in s.maps:
        if m['row'] == row - 1 and m['col'] <= 2:
            m['walls'] |= {(x, 23) for x in range(3, 24)}
    s.world += ['# TERRAIN|seed|base|amplitude|scale|roughness: rolling ground, 1 m +- 2.5 m, hills ~30 m apart.',
                'TERRAIN|2026|1|2.5|30|0.45',
                '# TERRAIN_FLAT|x1|y1|x2|y2|height|blend: level at 0 where the indoor rows open onto the outdoors.',
                'TERRAIN_FLAT|0|%d|72|%d|0|8' % (oy, oy + 2),
                '# HORIZON|depth|spacing|rise|models: trees past every closed outdoor edge.',
                'HORIZON|60|6|6|%d,%d,%d,%d,%d' % (slot['pine_a'], slot['pine_b'], slot['oak_a'], slot['aspen_a'], slot['dead_tree_a'])]

    # Ground paint: the nine GROUND kinds, 5 m squares on a level pad.
    m = s.outdoor_map('Ground paint', 0, row)
    s.zone(m, 0, 0, 24, 2, '', 'OUTDOORS')
    s.world.append('TERRAIN_FLAT|1|%d|23|%d|0|5' % (oy + 3, oy + 23))
    for k, name in enumerate(GROUNDS):
        x1, y1 = 2 + (k % 3) * 7, 4 + (k // 3) * 6.5
        s.world.append('GROUND|%g|%g|%g|%g|%d|1' % (x1, oy + y1, x1 + 5, oy + y1 + 5, k))
        s.post_sign(m, x1 + 2.5, y1 - .2, 'GROUND %d' % k, name, olive, 1.2)
        s.zone(m, x1 - .5, y1 - 1.3, x1 + 5.5, y1 + 5.2, 'GROUND kind %d / %s' % (k, name))
        s.index.append({'map': m['index'], 'section': 'OUTDOORS', 'x': x1 + 2.5, 'y': y1 + 2.5, 'title': 'GROUND %d' % k, 'name': name})

    # Terrain: the noise as it comes, a raised plateau and a level road cut through it.
    m = s.outdoor_map('Terrain', 1, row)
    ox = 24
    s.world += ['TERRAIN_FLAT|%d|%d|%d|%d|3.5|5' % (ox + 13, oy + 11, ox + 20, oy + 17),
                'TERRAIN_FLAT|%d|%d|%d|%d|1|3' % (ox, oy + 20, ox + 24, oy + 22),
                'GROUND|%d|%d|%d|%d|6|1' % (ox, oy + 20, ox + 24, oy + 22),
                'GROUND|%d|%d|%d|%d|8|3' % (ox + 13, oy + 11, ox + 20, oy + 17)]
    s.post_sign(m, 6, 6, 'TERRAIN', 'seed, base, amplitude, scale', olive, 1.6)
    s.zone(m, 0, 2, 12, 12, 'TERRAIN / noise: base 1, amplitude 2.5')
    s.post_sign(m, 16.5, 12.3, 'TERRAIN_FLAT 3.5', 'a plateau, blend 5', olive, 1.6)
    s.zone(m, 12, 10, 21, 18, 'TERRAIN_FLAT / plateau at 3.5 m')
    s.post_sign(m, 4, 19.6, 'TERRAIN_FLAT 1', 'a road, blend 3, gravel', olive, 1.6)
    s.zone(m, 0, 19, 24, 23, 'TERRAIN_FLAT + GROUND 6 / a level road')
    s.index.append({'map': m['index'], 'section': 'OUTDOORS', 'x': 16.5, 'y': 14, 'title': 'TERRAIN', 'name': 'plateau, road, noise'})

    # Grove: outdoor models placed as they would be in a real map, not on plinths.
    m = s.outdoor_map('Grove', 2, row)
    ox = 48
    s.world.append('GROUND|%d|%d|%d|%d|5|4' % (ox + 2, oy + 4, ox + 22, oy + 22))
    rng = __import__('random').Random(7)
    trees = ['pine_a', 'pine_b', 'oak_a', 'oak_b', 'oak_c', 'aspen_a', 'aspen_b', 'dead_tree_a', 'dead_tree_b']
    under = ['bush_a', 'bush_b', 'bush_c', 'fern_a', 'fern_big', 'grass_patch', 'tall_grass_m', 'stump', 'log']
    def plant(name, x, y, scale, solid):
        w, d, h = catalogue[name]
        s.rec(m, 'FIXTURE', m['index'], slot[name], x, y, 0, w * scale, d * scale, h * scale, rng.uniform(-PI, PI), solid)
    for k in range(16):
        plant(trees[k % len(trees)], 4 + (k % 4) * 5 + rng.uniform(-1.2, 1.2), 6 + (k // 4) * 4.5 + rng.uniform(-1, 1),
              rng.uniform(.8, 1.15), 1)
    for k in range(30):
        plant(under[k % len(under)], rng.uniform(3, 21), rng.uniform(5, 22), rng.uniform(.8, 1.2), 0)
    s.post_sign(m, 12, 2.6, 'GROVE', 'trees: FIXTURE, base 0 on terrain', olive, 1.8)
    s.zone(m, 0, 2, 24, 24, 'GROVE / FIXTURE base 0 sits on the ground')

    # Seams: a north-south seam (Ground paint above, Seams below), an east-west seam
    # (between the two seam maps) and a closed edge; the seams are painted dirt, the closed edge mud.
    a = s.outdoor_map('Seams', 0, row + 1)
    b = s.outdoor_map('Seams 2', 1, row + 1)
    sy = oy + 24
    s.world += ['TERRAIN_FLAT|0|%d|48|%d|0|6' % (sy + 1, sy + 21),
                'GROUND|0|%g|24|%g|4|0' % (sy - 1, sy + 1),
                'GROUND|23|%d|25|%d|4|0' % (sy, sy + 24),
                'GROUND|46|%d|48|%d|7|0' % (sy, sy + 24)]
    north = next(m['index'] for m in s.maps if (m['col'], m['row']) == (0, row))
    s.post_sign(a, 12, 2.2, 'SEAM NORTH-SOUTH', 'map %d openS=1 / map %d openN=1' % (north, a['index']), olive, 2.2)
    s.zone(a, 0, 0, 24, 3, 'SEAM N-S / map %d south edge = map %d north' % (north, a['index']))
    s.post_sign(a, 21.2, 12, 'SEAM EAST-WEST', 'map %d openE=1 / map %d openW=1' % (a['index'], b['index']), olive, 2.0)
    s.zone(a, 20, 9, 24, 15, 'SEAM E-W / map %d east edge' % a['index'])
    s.zone(b, 0, 9, 4, 15, 'SEAM E-W / map %d west edge' % b['index'])
    s.post_sign(b, 20.5, 12, 'CLOSED EDGE', 'openE=0: no neighbour, no way on', olive, 2.0)
    s.zone(b, 19, 9, 24, 15, 'CLOSED EDGE / openE=0 stops you here')
    for m in (a, b):
        s.index.append({'map': m['index'], 'section': 'SEAMS', 'x': 12, 'y': 12, 'title': m['name'].upper(), 'name': 'map seams'})


def main():
    argv = sys.argv[1:]
    index = None
    if '--index' in argv:
        at = argv.index('--index'); index = argv[at + 1]; del argv[at:at + 2]
    out = argv[0] if argv else os.path.join(ROOT, 'custom maps', 'Starter Map.txt')
    s = build()
    lines = s.write(out)
    if index:
        json.dump({'maps': [{'name': m['name'], 'col': m['col'], 'row': m['row']} for m in s.maps], 'items': s.index},
                  open(index, 'w'), indent=1)
    print('%s: %d maps, %d lines, %d labelled parts' % (out, len(s.maps), lines, len(s.index)))


if __name__ == '__main__':
    main()
