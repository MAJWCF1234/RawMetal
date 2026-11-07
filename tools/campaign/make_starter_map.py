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
  SCRIPTS     east of Levels: counters, AND/ANY gates, repeat, live values (@kills,
              @remaining, @health) and a campaign-wide event, each one working
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
OX, OY = 1, 1                    # the building's first cell; the forest ring takes column and row 0
FACADE = 14.0                    # height of the building's outer wall, above the tallest hall inside
TOWER = 26.0                     # the stair towers at its outside corners
FOG = (.3, .35, .39, .024)      # outdoor air: blue-grey, thick enough to lose the far wing
SIDES = {'N': (0, -1), 'S': (0, 1), 'W': (-1, 0), 'E': (1, 0)}
OPPOSITE = {'N': 'S', 'S': 'N', 'W': 'E', 'E': 'W'}

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
        self.doors = {}         # (map index, side) -> [(from, to)] metres along that edge left open to outside
        self.thresholds = []    # TERRAIN_FLATs under the door landings, written last so nothing levels them again
        self.raised = []        # (map, world x, world y, height, FIXTURE fields): plants on ledges, based once the ground is known
        self.absolute = []      # (map, x1, y1, x2, y2, bottom, top, material, rail): outdoor STRUCTs at absolute heights
        self.sloped = 0         # absolute boxes over ground that falls further than they are tall
        self.closed = set()     # pairs of neighbouring cells whose shared edge stays shut
        self.start = 0          # the map you spawn in

    # --- maps -------------------------------------------------------------------------------
    def new_map(self, name, col, row, ceiling=4.0, surface=None, ambient=.34, shift=True):
        """Maps sit on a grid of 24 m cells; an edge is open wherever a neighbour exists.
        col and row count from the building's corner; the forest ring sits outside it."""
        if shift: col, row = col + OX, row + OY
        m = {'name': name, 'records': [], 'ceiling': ceiling, 'surface': surface, 'ambient': ambient,
             'walls': set(), 'lights': True, 'index': len(self.maps), 'col': col, 'row': row,
             'outdoor': False, 'layers': []}
        self.maps.append(m)
        return m

    def outdoor_map(self, name, col, row, shift=True):
        """Environment 1 under the zone sky: the campaign's TERRAIN shapes the ground and every
        height on the map (props, signs, posts, creatures) is measured from that ground."""
        m = self.new_map(name, col, row, ceiling=2.8, ambient=.2, shift=shift)
        m.update(outdoor=True, lights=False, haze=FOG)
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

    def astruct(self, m, x1, y1, x2, y2, bottom, top, material, rail=0):
        """A STRUCT at absolute heights, level with the indoor floors. Outdoors the engine adds
        the lowest ground under a box to its bottom and the highest to its top, so these wait
        for the terrain (settle) and are written with those taken off again."""
        if not m['outdoor']: return self.struct(m, x1, y1, x2, y2, bottom, top, material, rail)
        self.absolute.append((m, x1, y1, x2, y2, bottom, top, material, rail))

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
    def settle(self):
        """An outdoor FIXTURE's base is measured from the terrain under it, so a plant on a
        ledge z metres up stands at z less the ground there (Ashfall's port of the terrain)."""
        sys.path.insert(0, os.path.join(ROOT, 'tools', 'ashfall'))
        from build_zone import Terrain
        t = None
        for line in self.world + self.thresholds:
            f = line.split('|')
            if f[0] == 'TERRAIN':
                t = Terrain(seed=int(f[1]), base=float(f[2]), amplitude=float(f[3]), scale=float(f[4]), roughness=float(f[5]))
            elif f[0] == 'TERRAIN_FLAT' and t:
                t.flats.append(tuple(float(v) for v in f[1:7]))
        for m, x, y, z, fields in self.raised:
            fields[5] = z - (t.ground(x, y) if t else 0)
            self.rec(m, *fields)
        self.raised = []
        for m, x1, y1, x2, y2, bottom, top, material, rail in self.absolute:
            ox, oy = 24 * m['col'], 24 * m['row']                       # World::settleExclusionFacilityOnTerrain
            nx, ny = max(1, math.ceil((x2 - x1) * 2)), max(1, math.ceil((y2 - y1) * 2))
            under = [t.ground(ox + x1 + (x2 - x1) * i / nx, oy + y1 + (y2 - y1) * j / ny) if t else 0
                     for i in range(nx + 1) for j in range(ny + 1)]
            low, high = bottom - min(under), top - max(under)
            if high < low: self.sloped += 1                             # ground falls more than the box is tall:
            self.struct(m, x1, y1, x2, y2, min(low, high), high, material, rail)   # it reaches down to meet it
        self.absolute = []

    def write(self, path):
        self.settle()
        out = ['META_LEVEL_ID: 0', 'META_LEVEL_NAME: Starter Map', 'META_DEFAULT_TARGET: CUSTOM',
               'META_CAMPAIGN_NAME: STARTER MAP', '',
               '# The Starter Map: every part of a custom map lined up, like a debug world.',
               '# Generated by tools/campaign/make_starter_map.py. Read custom maps/GUIDE.html first;',
               '# every line below is one record of the format it explains. Lines starting # are notes.',
               '', '--- CUSTOM_CAMPAIGN_DATA_START ---', 'CAMPAIGN|STARTER MAP|%d' % self.start]
        if self.world:
            out += ['', '# ==== Campaign-wide: terrain, ground paint, horizon (world metres; outdoor maps only) ===='] + self.world
            if self.thresholds:
                out += ['# Terrain walks half a metre above its TERRAIN_FLAT height, so ground levelled at -1 meets',
                        '# a STRUCT landing at 0 (the indoor floor) with one .25 m step down.'] + self.thresholds
        cells = {(m['col'], m['row']) for m in self.maps}
        for i, m in enumerate(self.maps):
            out += ['', '# ==== Map %d: %s ====' % (i, m['name'])]
            spawn = m.get('spawn', (12, 1.6, HALF))
            c, r = m['col'], m['row']
            # The y=0 edge is "north", y=24 "south".
            def open_to(n):
                return n in cells and frozenset(((c, r), n)) not in self.closed
            north, south, west, east = open_to((c, r - 1)), open_to((c, r + 1)), open_to((c - 1, r)), open_to((c + 1, r))
            # MAP|index|name|originX|originY|spawnX|spawnY|spawnZ|environment|ceiling|ambient|skybox|
            #     openN|openS|openW|openE|lift|residencyGroup|floorMaterial|musicCue|spawnYaw|spawnPitch|startUnarmed
            out.append('|'.join(f(v) for v in ['MAP', i, 'Starter Map / ' + m['name'], 24 * c, 24 * r, spawn[0], spawn[1], 0,
                                                int(m['outdoor']), m['ceiling'], m['ambient'],
                                                'brutal_wasteland' if m['outdoor'] else 'industrial_night',
                                                int(north), int(south), int(west), int(east), 0, -1, 0, 0, spawn[2], 0, 0,
                                                *m.get('haze', ())]))
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
    s.zone(m, 0, 0, 24, 3.5, 'WELCOME', 'WELCOME')
    s.doors[(m['index'], 'N')] = [(10, 14)]          # the front door, out to the forest
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
    scripts(s)                  # built after the rest so every earlier map keeps its number
    building(s, slot, catalogue)
    return s


def scripts(s):
    """SCRIPTS, east of Levels: one working station per scripting feature (1.2.2).
    Every station is a SCRIPT_EVENT you can trigger, with a sign naming its records."""
    m = s.new_map('Scripts', 5, 0, ceiling=4.5, surface='lab panel')
    i = m['index']
    pink = 0xd860a0
    levels = next(x for x in s.maps if x['name'] == 'Levels')
    levels['walls'] |= {(23, y) for y in range(24) if not 10 <= y <= 13}   # keep the Levels wall signs on a wall
    m['walls'] |= {(0, y) for y in range(24) if not 10 <= y <= 13} | {(x, 23) for x in range(24)}
    s.zone(m, 0, 9, 2.5, 15, '', 'SCRIPTS')

    def event(name, box, require='', value=1, actions=(), gates=(), where='box', match='all', repeat=0, owner=None):
        o = owner or m
        x1, y1, x2, y2 = box
        s.rec(o, 'SCRIPT_EVENT', o['index'], name, x1, y1, x2, y2, -1, 5, require, value, 0)
        for g in gates:
            s.rec(o, 'SCRIPT_REQUIRE', o['index'], name, *g)
        if (where, match, repeat) != ('box', 'all', 0):
            s.rec(o, 'SCRIPT_TRIGGER', o['index'], name, where, match, repeat)
        for a in actions:
            s.rec(o, 'SCRIPT_ACTION', o['index'], name, *a)

    def label(cx, y, title, sub, facing_south, width=2.4):
        """A post sign facing the middle aisle (y 9-14)."""
        if facing_south:
            s.struct(m, cx - width / 2, y, cx + width / 2, y + .06, 0, .55, 20)
            s.sign(m, cx, y + .07, .34, width - .06, .3, title, sub, pink, 0)
        else:
            s.post_sign(m, cx, y, title, sub, pink, width)

    def station(x1, y1, x2, y2, zone, title, sub):
        s.zone(m, x1, y1, x2, y2, zone)
        s.index.append({'map': i, 'section': 'SCRIPTS', 'x': round((x1 + x2) / 2, 2), 'y': round((y1 + y2) / 2, 2), 'title': title, 'name': sub})

    s.rec(m, 'TERMINAL', i, 3.4, 9.0, 0, 0, 'SCRIPTS / READ ME', 'EACH STATION IS A SCRIPT_EVENT. TRIGGER IT AND WATCH.',
          'THE SIGN NAMES ITS RECORDS. FILE: SEARCH starter_', '', 0, '', 0)

    # Counter: three pads, each ADDs one and shows the running count.
    for k, x in enumerate((5, 8, 11)):
        s.struct(m, x - .6, 3.4, x + .6, 4.6, 0, .05, 35)
        event('starter_pad_%d' % k, (x - .6, 3.4, x + .6, 4.6), actions=[
            ('ADD', 'starter_crystals', 1), ('MESSAGE', 'ADD: CRYSTALS {starter_crystals} / 3', 3)])
    s.rec(m, 'LIGHT', i, 8, 4, 4.3, 0, 0, 1.6, 6, 6500, 0, 'starter_crystals', 3, 0, '', '', '', '', '', '', '', '', .2, .9, 1)
    label(8, 7.4, 'ADD + MESSAGE', 'a counter: step on all 3 pads', True, 3.0)
    station(3.5, 2.5, 12.5, 8, 'ADD + MESSAGE / a counter, step on each pad', 'ADD + MESSAGE', 'counter pads')

    # AND: the vault opens on the lever AND three crystals (SCRIPT_REQUIRE >=).
    m['walls'] |= {(14, y) for y in range(1, 8)} | {(x, 7) for x in range(14, 23) if not 17 <= x <= 19}
    s.rec(m, 'DOOR', i, 17, 20, 7.5, 0, 0, 0, 0, 0, 0, 'starter_vault', 1)
    s.rec(m, 'TERMINAL', i, 15.5, 8.6, 0, 1, 'VAULT LEVER', 'PULL THE LEVER. THE VAULT ALSO WANTS',
          'THREE CRYSTALS: SCRIPT_REQUIRE starter_crystals >= 3.', 'starter_lever', 0, '', PI)
    event('starter_vault_open', (0, 0, 24, 24), 'starter_lever', 1, gates=[('starter_crystals', '>=', 3)], where='map', actions=[
        ('STATE', 'starter_vault', 1), ('MESSAGE', 'VAULT OPEN: LEVER AND CRYSTALS >= 3 (ALL GATES)', 4)])
    s.sign(m, 21.45, 8.05, 1.9, 2.3, .45, 'SCRIPT_REQUIRE', 'lever = 1 AND crystals >= 3', pink, 0)
    s.rec(m, 'PICKUP', i, 0, 17.5, 3.5)
    s.rec(m, 'PICKUP', i, 1, 19.5, 3.5)
    s.rec(m, 'LIGHT', i, 18.5, 4, 4.3, 0, 0, 1.4, 6, 3400, 0, 'starter_vault', 1)
    station(14, 1, 23, 9.5, 'SCRIPT_REQUIRE / lever AND crystals >= 3', 'SCRIPT_REQUIRE', 'vault: all gates')

    # ANY: either switch lights the lamp; both off puts it out again (two repeating events).
    for x, name, colour in ((4, 'starter_red', 'RED'), (8, 'starter_blue', 'BLUE')):
        s.rec(m, 'TERMINAL', i, x, 15.6, 0, 1, '%s SWITCH' % colour, 'FLIPS %s ON AND OFF.' % name.upper(),
              'EITHER SWITCH LIGHTS THE LAMP: MATCH ANY.', name, 1, '', 0)
    s.rec(m, 'LIGHT', i, 6, 16.5, 4.3, 0, 0, 1.6, 6, 6500, 0, 'starter_either', 1, 0, '', '', '', '', '', '', '', '', .1, 1, .15)
    event('starter_any_on', (0, 0, 24, 24), gates=[('starter_red', '=', 1), ('starter_blue', '=', 1)], where='map', match='any', repeat=1,
          actions=[('STATE', 'starter_either', 1), ('MESSAGE', 'MATCH ANY: ONE SWITCH IS ENOUGH', 3)])
    event('starter_any_off', (0, 0, 24, 24), gates=[('starter_red', '=', 0), ('starter_blue', '=', 0)], where='map', repeat=1,
          actions=[('STATE', 'starter_either', 0)])
    label(6, 14.6, 'MATCH ANY', 'either switch lights the lamp', False, 3.0)
    station(3, 14.5, 9.5, 18.5, 'SCRIPT_TRIGGER any / either switch', 'MATCH ANY', 'two switches, one lamp')

    # Repeat: a doormat that counts every visit, not every frame.
    s.struct(m, 11, 15.5, 13, 17.5, 0, .03, 13)
    event('starter_mat', (11, 15.5, 13, 17.5), repeat=1, actions=[
        ('ADD', 'starter_visits', 1), ('MESSAGE', 'REPEAT 1: VISIT {starter_visits}', 2)])
    label(12, 14.6, 'REPEAT', 'counts each visit', False, 2.0)
    station(10.5, 14.5, 13.5, 18.5, 'SCRIPT_TRIGGER repeat 1 / the doormat', 'REPEAT', 'doormat visit counter')

    # Live values: a sleeping Huntsman; once it is dead, @kills and @remaining say so.
    s.fence(m, 16, 15, 22.5, 21.5)
    s.rec(m, 'CREATURE', i, 0, 19.2, 18.8, 0, '', '', 'starter_wake')
    s.rec(m, 'TERMINAL', i, 17.2, 14.2, 0, 1, 'WAKE THE HUNTSMAN', 'IT SLEEPS UNTIL starter_wake IS SET.',
          'KILL IT: @kills >= 1 AND @remaining = 0 FIRE.', 'starter_wake', 0, '', 0)
    s.rec(m, 'LIGHT', i, 19.2, 18.2, 4.3, 0, 0, 1.5, 7, 6500, 0, 'starter_pen_clear', 1, 0, '', '', '', '', '', '', '', '', .1, 1, .15)
    event('starter_pen', (0, 0, 24, 24), gates=[('@kills', '>=', 1), ('@remaining', '=', 0)], where='map', actions=[
        ('STATE', 'starter_pen_clear', 1), ('MESSAGE', 'LIVE VALUES: @kills >= 1, @remaining = 0', 4)])
    label(20.5, 14.4, '@kills  @remaining', 'live values: clear the pen', False, 2.6)
    station(15.5, 14.2, 23, 22, '@kills / @remaining / the Huntsman pen', '@kills @remaining', 'creature pen')

    # @health: a strip of gas; dropping under half health warns you, every time.
    s.rec(m, 'HAZARD', i, 3, 3.5, 20.2, 9.5, 22.6, 0, 2.2, 12)
    for x in (3.5, 9.5):
        s.struct(m, x - .05, 20.2, x + .05, 22.6, 0, 1.1, 2, rail=1)
    s.rec(m, 'PICKUP', i, 0, 11.5, 21)
    s.rec(m, 'PICKUP', i, 0, 12.5, 21)
    event('starter_hurt', (0, 0, 24, 24), gates=[('@health', '<', 50)], where='map', repeat=1, actions=[
        ('MESSAGE', '@health < 50: GET OUT OF THE GAS', 3)])
    label(6.5, 19.6, '@health', 'gas strip: warns under 50', False, 2.6)
    station(3, 19, 13, 23, '@health / a gas strip that warns you', '@health', 'gas strip')

    # Campaign scope: the event lives on map 0 (Welcome) but hears this switch from here.
    s.rec(m, 'TERMINAL', i, 21.5, 11.8, 0, 1, 'CALL MAP 0', 'SETS starter_call. THE EVENT THAT ANSWERS',
          'IS ON MAP 0: SCRIPT_TRIGGER campaign.', 'starter_call', 0, '', -HALF)
    event('starter_call_heard', (0, 0, 24, 24), 'starter_call', 1, where='campaign', owner=s.maps[0], actions=[
        ('MESSAGE', 'MAP 0 HEARD YOU: SCRIPT_TRIGGER CAMPAIGN', 4)])
    s.sign(m, 22.95, 11.8, 2.2, 2.4, .45, 'CAMPAIGN SCOPE', 'an event on map 0 answers here', pink, -HALF)
    station(19.5, 10, 23, 13.8, 'SCRIPT_TRIGGER campaign / call map 0', 'CAMPAIGN SCOPE', 'cross-map event')


GROUNDS = ['short grass', 'leafy grass', 'clover', 'wild', 'dry dirt', 'forest litter', 'gravel', 'mud', 'meadow']


def outdoors(s, slot, catalogue):
    """Outdoor maps under the last model row, and the seams between maps made visible.
    Terrain records are campaign-wide and in world metres (map origin + local x, y)."""
    row = 1 + len(SECTIONS) + 1                       # the first row under the model rows
    oy = 24 * (row + OY)                              # its world y
    x0 = 24 * OX                                      # world x of the building's west edge
    olive = 0x8aa040
    # The indoor row above opens onto these maps only where its west corridor runs.
    for m in s.maps:
        if m['row'] == row + OY - 1 and m['col'] <= 2 + OX:
            s.doors[(m['index'], 'S')] = [(0, 3)]
    s.world += ['# TERRAIN|seed|base|amplitude|scale|roughness: rolling ground, 1 m +- 2.5 m, hills ~30 m apart.',
                'TERRAIN|2026|1|2.5|30|0.45',
                '# TERRAIN_FLAT|x1|y1|x2|y2|height|blend: level at 0 where the indoor rows open onto the outdoors.',
                'TERRAIN_FLAT|%d|%d|%d|%d|0|8' % (x0, oy, x0 + 72, oy + 2),
                '# HORIZON|depth|spacing|rise|models: woods past every closed outdoor edge.',
                'HORIZON|110|5|9|%d,%d,%d,%d,%d,%d' % (slot['pine_a'], slot['pine_b'], slot['pine_a'], slot['oak_a'], slot['aspen_a'], slot['oak_c'])]

    # Ground paint: the nine GROUND kinds, 5 m squares on a level pad.
    m = s.outdoor_map('Ground paint', 0, row)
    s.zone(m, 0, 0, 24, 2, '', 'OUTDOORS')
    s.world.append('TERRAIN_FLAT|%d|%d|%d|%d|0|5' % (x0 + 1, oy + 3, x0 + 23, oy + 23))
    for k, name in enumerate(GROUNDS):
        x1, y1 = 2 + (k % 3) * 7, 4 + (k // 3) * 6.5
        s.world.append('GROUND|%g|%g|%g|%g|%d|1' % (x0 + x1, oy + y1, x0 + x1 + 5, oy + y1 + 5, k))
        s.post_sign(m, x1 + 2.5, y1 - .2, 'GROUND %d' % k, name, olive, 1.2)
        s.zone(m, x1 - .5, y1 - 1.3, x1 + 5.5, y1 + 5.2, 'GROUND kind %d / %s' % (k, name))
        s.index.append({'map': m['index'], 'section': 'OUTDOORS', 'x': x1 + 2.5, 'y': y1 + 2.5, 'title': 'GROUND %d' % k, 'name': name})

    # Terrain: the noise as it comes, a raised plateau and a level road cut through it.
    m = s.outdoor_map('Terrain', 1, row)
    ox = x0 + 24
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
    ox = x0 + 48
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
    s.world += ['TERRAIN_FLAT|%d|%d|%d|%d|0|6' % (x0, sy + 1, x0 + 48, sy + 21),
                'GROUND|%d|%g|%d|%g|4|0' % (x0, sy - 1, x0 + 24, sy + 1),
                'GROUND|%d|%d|%d|%d|4|0' % (x0 + 23, sy, x0 + 25, sy + 24),
                'GROUND|%d|%d|%d|%d|7|0' % (x0 + 46, sy, x0 + 48, sy + 24)]
    s.closed.add(frozenset(((b['col'], b['row']), (b['col'] + 1, b['row']))))   # the exhibit's closed edge stays closed
    north = next(m['index'] for m in s.maps if (m['col'], m['row']) == (OX, row + OY))
    s.post_sign(a, 12, 2.2, 'SEAM NORTH-SOUTH', 'map %d openS=1 / map %d openN=1' % (north, a['index']), olive, 2.2)
    s.zone(a, 0, 0, 24, 3, 'SEAM N-S / map %d south edge = map %d north' % (north, a['index']))
    s.post_sign(a, 21.2, 12, 'SEAM EAST-WEST', 'map %d openE=1 / map %d openW=1' % (a['index'], b['index']), olive, 2.0)
    s.zone(a, 20, 9, 24, 15, 'SEAM E-W / map %d east edge' % a['index'])
    s.zone(b, 0, 9, 4, 15, 'SEAM E-W / map %d west edge' % b['index'])
    s.post_sign(b, 20.5, 12, 'CLOSED EDGE', 'openE=0: no neighbour, no way on', olive, 2.0)
    s.zone(b, 19, 9, 24, 15, 'CLOSED EDGE / openE=0 stops you here')
    for m in (a, b):
        s.index.append({'map': m['index'], 'section': 'SEAMS', 'x': 12, 'y': 12, 'title': m['name'].upper(), 'name': 'map seams'})


TREES = [('pine_a', 8, 1.15, 1.75), ('pine_b', 8, 1.15, 1.75), ('aspen_a', 2, 1.0, 1.45), ('oak_a', 1, .9, 1.2),
         ('dead_tree_a', 1, .9, 1.3), ('dead_tree_b', 1, .9, 1.3)]
UNDER = [('fern_a', 6), ('fern_big', 5), ('bush_a', 2), ('bush_b', 2), ('bush_c', 2), ('grass_a', 1), ('tall_grass_l', 1),
         ('rock_mossy', 2), ('mushrooms', 1), ('stump', 1)]
WALL, STAIN, DARK = 2, 8, 8      # smooth grey concrete (steel reads as cast concrete), dark streaks, ribbon windows
FLOORS = (4.4, 8.0, 11.6)        # the outer wall's floor ledges; a ribbon window under each


def building(s, slot, catalogue):
    """Every indoor map is one building: the edges that face outside are walled (with the
    doors in s.doors), the outdoor maps around it carry its outer wall as STRUCT boxes, and
    every free cell in a ring around it all is forest. The front door faces the spawn."""
    cells = {(m['col'], m['row']): m for m in s.maps}
    inside = {c for c, m in cells.items() if not m['outdoor']}
    cols = [c for c, _ in cells]; rows = [r for _, r in cells]
    welcome = s.maps[0]
    olive, blue = 0x8aa040, ACCENT['WELCOME']

    # The forest: every free cell in the bounding box, grown by one cell all round.
    forest = []
    for r in range(min(rows) - 1, max(rows) + 2):
        for c in range(min(cols) - 1, max(cols) + 2):
            if (c, r) in cells: continue
            entrance = (c, r) == (welcome['col'], welcome['row'] - 1)
            m = s.outdoor_map('Entrance' if entrance else 'Forest', c, r, shift=False)
            cells[(c, r)] = m
            forest.append(m)
            if entrance: s.start = m['index']

    # Walls on every indoor edge that faces outside, except the doors.
    for (c, r) in inside:
        m = cells[(c, r)]
        for side, (dx, dy) in SIDES.items():
            if (c + dx, r + dy) in inside: continue
            gaps = s.doors.get((m['index'], side), [])
            for t in range(24):
                if any(a <= t + .5 <= b for a, b in gaps): continue
                m['walls'].add({'N': (t, 0), 'S': (t, 23), 'W': (0, t), 'E': (23, t)}[side])

    rng = __import__('random').Random(2026)
    for m in [x for x in s.maps if x['outdoor']]:
        c, r = m['col'], m['row']
        wx, wy = 24 * c, 24 * r
        is_forest = m in forest
        keep_clear = []                          # (x1, y1, x2, y2): no trees here
        sides = [side for side, (dx, dy) in SIDES.items() if (c + dx, r + dy) in inside]

        def box(side, a, b, d0, d1, z0, z1, mat):
            """A box d0-d1 metres out from the building along this map's edge, a-b along it."""
            a, b = max(0, a), min(24, b)
            if b - a < .01: return
            if z0 == 0: z0 = -1.2                                              # under the lowered ground at doors
            if side == 'N': s.astruct(m, a, d0, b, d1, z0, z1, mat)
            elif side == 'S': s.astruct(m, a, 24 - d1, b, 24 - d0, z0, z1, mat)
            elif side == 'W': s.astruct(m, d0, a, d1, b, z0, z1, mat)
            else: s.astruct(m, 24 - d1, a, 24 - d0, b, z0, z1, mat)

        def strip(side, d0, d1):
            """The world-metre box d0-d1 out from this map's edge on that side."""
            return {'N': (wx, wy + d0, wx + 24, wy + d1), 'S': (wx, wy + 24 - d1, wx + 24, wy + 24 - d0),
                    'W': (wx + d0, wy, wx + d1, wy + 24), 'E': (wx + 24 - d1, wy, wx + 24 - d0, wy + 24)}[side]

        for side in sides:
            dx, dy = SIDES[side]
            gaps = s.doors.get((cells[(c + dx, r + dy)]['index'], OPPOSITE[side]), [])
            solid, at = [], 0
            for a, b in sorted(gaps):
                solid.append((at, a)); at = b
            solid.append((at, 24))
            for a, b in solid:
                box(side, a, b, 0, .3, 0, FACADE, WALL)
                box(side, a, b, 0, .6, 0, rng.uniform(.5, 1.3), STAIN)         # damp up the foot of the wall
                for z in FLOORS[:1]:                                           # ground-floor ribbon, not over doors
                    box(side, a + .4, b - .4, .3, .33, z - 1.9, z - .45, DARK)
            for a, b in gaps:
                box(side, a, b, 0, .3, 4.4, FACADE, WALL)                     # over the door
                box(side, a - 1.2, b + 1.2, 0, 4.0, 4.0, 4.4, WALL)           # a deep concrete canopy
                box(side, a, b, -.4, .4, -1.2, .01, WALL)                      # a sill across the seam, so the
                box(side, a, a + .1, -.4, .4, 0, 4.4, WALL)                    # doorway is one primitive from
                box(side, b - .1, b, -.4, .4, 0, 4.4, WALL)                    # room to landing, without a rip
                box(side, a, b, -.4, .4, 4.3, 4.4, WALL)
                box(side, a - .6, b + .6, 0, 2.2, -1.2, 0, WALL)               # a landing level with the floor
                box(side, a - .6, b + .6, 2.2, 2.8, -1.2, -.17, WALL)          # two steps down: the player only
                box(side, a - .6, b + .6, 2.8, 3.4, -1.2, -.34, WALL)          # walks up .215 m without a jump
                a1, b1, d1 = max(0, a - 2), min(24, b + 2), 5
                s.thresholds.append('TERRAIN_FLAT|%g|%g|%g|%g|-1|4' % {
                    'N': (wx + a1, wy, wx + b1, wy + d1), 'S': (wx + a1, wy + 24 - d1, wx + b1, wy + 24),
                    'W': (wx, wy + a1, wx + d1, wy + b1), 'E': (wx + 24 - d1, wy + a1, wx + 24, wy + b1)}[side])
            def overgrow(base, depth, count):
                """Plants rooted on a ledge: the building is going back to the forest."""
                for _ in range(count):
                    t, name = rng.uniform(.6, 23.4), rng.choice(('grass_a', 'grass_b', 'fern_a', 'grass_l', 'bush_b'))
                    x, y = {'N': (t, depth), 'S': (t, 24 - depth), 'W': (depth, t), 'E': (24 - depth, t)}[side]
                    k = rng.uniform(.7, 1.2); w, d, h = catalogue[name]
                    s.raised.append((m, wx + x, wy + y, base, ['FIXTURE', m['index'], slot[name], x, y, None,
                                                               w * k, d * k, h * k, rng.uniform(-PI, PI), 0]))
            for z in FLOORS:
                if z > FLOORS[0]: box(side, 0, 24, .3, .33, z - 1.9, z - .45, DARK)
                box(side, 0, 24, 0, 1.1, z - .35, z, WALL)                    # a floor ledge
                overgrow(z, .7, rng.randint(2, 5))
            box(side, 0, 24, 0, .45, FACADE, FACADE + 1.0, WALL)              # the parapet
            overgrow(FACADE + 1.0, .25, rng.randint(2, 4))
            for p in (0, 24):                                                  # a fin at each seam
                box(side, p - .5, p + .5, 0, 1.3, 0, FACADE + 1.0, WALL)
            for _ in range(rng.randint(3, 6)):                                 # streaks where water runs
                a = rng.uniform(1, 22); top = rng.choice(FLOORS + (FACADE,))
                if any(a < gb + .5 and a + 2 > ga - .5 for ga, gb in gaps): continue
                box(side, a, a + rng.uniform(.4, 1.8), .33, .35, rng.uniform(0, top - 3), top - .35, STAIN)
            if is_forest:
                t = rng.uniform(0, 1.2)
                while t < 23.4:                                                # ferns crowd the foot of the wall
                    if not any(ga - 1 < t < gb + 1 for ga, gb in gaps):
                        d = rng.uniform(.9, 2.1); name = rng.choice(('fern_big', 'fern_a', 'fern_a', 'bush_b'))
                        x, y = {'N': (t, d), 'S': (t, 24 - d), 'W': (d, t), 'E': (24 - d, t)}[side]
                        k = rng.uniform(.9, 1.4); w, dd, h = catalogue[name]
                        s.rec(m, 'FIXTURE', m['index'], slot[name], x, y, 0, w * k, dd * k, h * k, rng.uniform(-PI, PI), 0)
                    t += rng.uniform(1.1, 2.0)
                x1, y1, x2, y2 = strip(side, 0, 9)
                s.world.append('TERRAIN_FLAT|%g|%g|%g|%g|-1|10' % (x1, y1, x2, y2))   # walks at -.5, below the floor
                s.world.append('GROUND|%g|%g|%g|%g|6|1' % strip(side, 2.2, 5.8))
                keep_clear.append(strip(side, 0, 8))
                fx1, fy1, fx2, fy2 = strip(side, 0, 3)
                s.zone(m, fx1 - wx, fy1 - wy, fx2 - wx, fy2 - wy, "FACADE / STRUCT boxes on the forest's map")

        # Outside corners of the building: a stair tower standing above the roof, with the
        # site's emblem high on its face, the path bending round it.
        for (cx, cy), (dx, dy) in (((0, 0), (-1, -1)), ((24, 0), (1, -1)), ((0, 24), (-1, 1)), ((24, 24), (1, 1))):
            if (c + dx, r + dy) in inside and (c + dx, r) not in inside and (c, r + dy) not in inside:
                tx1, tx2 = (0, 6) if cx == 0 else (18, 24)
                ty1, ty2 = (0, 6) if cy == 0 else (18, 24)
                sx, sy = (1, 1) if cx == 0 else (-1, 1)
                sy = 1 if cy == 0 else -1
                fx, fy = (tx2 if sx > 0 else tx1), (ty2 if sy > 0 else ty1)
                s.astruct(m, tx1, ty1, tx2, ty2, -1.2, TOWER, WALL)
                s.astruct(m, tx1, ty1, tx2, ty2, TOWER, TOWER + .1, STAIN)
                s.astruct(m, tx1 - .05, ty1 - .05, tx2 + .05, ty2 + .05, -1.2, rng.uniform(.8, 1.6), STAIN)
                mx, my = (ty1 + ty2) / 2, (tx1 + tx2) / 2        # a tall stair slit up each open face
                s.astruct(m, min(fx, fx + .03 * sx), mx - .5, max(fx, fx + .03 * sx), mx + .5, 2.5, TOWER - 7, DARK)
                s.astruct(m, my - .5, min(fy, fy + .03 * sy), my + .5, max(fy, fy + .03 * sy), 2.5, TOWER - 7, DARK)
                s.sign(m, (tx1 + tx2) / 2, fy + .05 * sy, TOWER - 3.6, 5.2, 1.7, 'SITE-01', 'STARTER MAP FACILITY', 0xb8c0bc,
                       0 if sy > 0 else PI)
                a = rng.uniform(tx1 + .5, tx2 - 2)
                s.astruct(m, a, min(fy, fy + .035 * sy), a + rng.uniform(.6, 1.5), max(fy, fy + .035 * sy), 2, TOWER - 6, STAIN)
                if is_forest:
                    sq = lambda d: (wx + (0 if cx == 0 else 24 - d), wy + (0 if cy == 0 else 24 - d),
                                    wx + (d if cx == 0 else 24), wy + (d if cy == 0 else 24))
                    s.world.append('TERRAIN_FLAT|%g|%g|%g|%g|-1|10' % sq(14))
                    s.world.append('GROUND|%g|%g|%g|%g|6|1' % sq(10))
                    keep_clear.append(sq(13))

        # A bridge where the forest runs between two wings, as high as the second floor.
        if is_forest and {'N', 'S'} <= set(sides) or is_forest and {'W', 'E'} <= set(sides):
            along_y = {'N', 'S'} <= set(sides)
            def span(a, b, z0, z1, mat):
                if along_y: s.astruct(m, a, 0, b, 24, z0, z1, mat)
                else: s.astruct(m, 0, a, 24, b, z0, z1, mat)
            span(9.5, 14.5, 7.6, 10.6, WALL)
            span(9.5, 14.5, 10.6, 10.68, STAIN)
            span(9.47, 9.5, 8.3, 9.9, DARK)
            span(14.5, 14.53, 8.3, 9.9, DARK)
            s.zone(m, *((9.5, 0, 14.5, 24) if along_y else (0, 9.5, 24, 14.5)), 'BRIDGE / one STRUCT 24 m long, 7.6 m up')

        if not is_forest: continue
        # Forest floor: litter everywhere, a patch of something greener.
        s.world.insert(0, 'GROUND|%d|%d|%d|%d|5|2' % (wx, wy, wx + 24, wy + 24))
        px, py, pw = rng.uniform(2, 14), rng.uniform(2, 14), rng.uniform(5, 9)
        s.world.insert(1, 'GROUND|%g|%g|%g|%g|%d|4' % (wx + px, wy + py, wx + px + pw, wy + py + pw, rng.choice((1, 3, 2, 8))))
        if m['name'] == 'Entrance':
            m['spawn'] = (12, 7, HALF)
            s.world.append('GROUND|%d|%d|%d|%d|6|1' % (wx + 10, wy + 4, wx + 14, wy + 24))
            s.world.append('TERRAIN_FLAT|%d|%d|%d|%d|-1|8' % (wx + 6, wy + 2, wx + 18, wy + 24))
            keep_clear.append((wx + 5, wy + 1, wx + 19, wy + 24))
            s.zone(m, 0, 0, 24, 24, 'ENTRANCE / the front door is ahead', 'STARTER MAP')
            s.sign(m, 12, 22.84, 7.0, 8.0, 1.5, 'STARTER MAP', 'every part of a custom map, inside', 0xb8c0bc)
            s.post_sign(m, 16.2, 9.5, 'OUTSIDE', 'FIXTURE trees, STRUCT walls', olive, 1.8)
            s.index.append({'map': m['index'], 'section': 'FOREST', 'x': 12, 'y': 12, 'title': 'ENTRANCE', 'name': 'spawn, front door'})
        else:
            s.zone(m, 0, 0, 24, 24, 'FOREST / FIXTURE trees, GROUND 5 litter')

        def free(x, y, gap, placed):
            gx, gy = wx + x, wy + y
            if any(a <= gx <= b2 and b <= gy <= d for a, b, b2, d in keep_clear): return False
            return all((x - px) ** 2 + (y - py) ** 2 >= (gap + pg) ** 2 for px, py, pg in placed)

        def pick(table):
            total = sum(t[1] for t in table); k = rng.uniform(0, total)
            for t in table:
                k -= t[1]
                if k <= 0: return t
            return table[-1]

        placed = []
        for _ in range(140):                     # trees: trunks block, canopies do not
            if len(placed) >= 24: break
            x, y = rng.uniform(1.8, 22.2), rng.uniform(1.8, 22.2)
            if not free(x, y, 1.6, placed): continue
            name, _, lo, hi = pick(TREES)
            k = rng.uniform(lo, hi); w, d, h = catalogue[name]
            s.rec(m, 'FIXTURE', m['index'], slot[name], x, y, 0, w * k, d * k, h * k, rng.uniform(-PI, PI), 1, .1)
            placed.append((x, y, 1.6))
        for _ in range(120):                     # undergrowth: walk-through
            if len(placed) >= 24 + 30: break
            x, y = rng.uniform(.8, 23.2), rng.uniform(.8, 23.2)
            if not free(x, y, .6, placed): continue
            name, _ = pick(UNDER)
            k = rng.uniform(.8, 1.3); w, d, h = catalogue[name]
            s.rec(m, 'FIXTURE', m['index'], slot[name], x, y, 0, w * k, d * k, h * k, rng.uniform(-PI, PI), 0)
            placed.append((x, y, .6))
        for name in rng.sample(['rock_big', 'rock_mid', 'rock_small', 'log', 'fallen_pine', 'rock_mossy'], 2):
            x, y = rng.uniform(3, 21), rng.uniform(3, 21)
            if not free(x, y, 1.5, placed): continue
            w, d, h = catalogue[name]
            s.rec(m, 'FIXTURE', m['index'], slot[name], x, y, 0, w, d, h, rng.uniform(-PI, PI), 1, .8)
            placed.append((x, y, 1.5))


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
