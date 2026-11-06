"""Generate 'The Leyline Sanctuary.txt': modern fantasy ruins in the woods. A 12-map custom campaign.

  python tools/campaign/make_leyline_sanctuary.py

The grid (4 x 3 maps of 24 m; F = forest/outdoor, M = under the mountain/indoor):

        col 0              col 1                col 2                 col 3
 row 0  F Trailhead Camp - F Whispering Circle - F Watchtower Ridge    M The Leyline Heart
              |                  |                                         | (Sanctum Seal)
 row 1  F Sunken Dig ----- F Overgrown Nave ==> M Hall of Pillars --- M Conduit Observatory
              |                  |                    |                    |
 row 2  F Bramble Hollow - F Wayside Shrine ==> M Flooded Crypt ---- M Crystal Chasm

 ==> : a door into the mountain. The temple portal opens once both leyline relays are
       attuned; the barrow door opens with Corporal Voss's clearance.

Quest: North relay (Circle) -> South relay (Dig) -> portal -> crypt conduit (Flooded
Crypt) -> Sanctum Seal (Observatory) -> stabilize the Leyline Heart. Optional: Voss's
clearance on Watchtower Ridge; the Greenbriar relic in the crypt, returned to Dr. Vance.
"""
import math
import os
import random

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PI, HALF = math.pi, math.pi / 2
CYAN, AMBER, GREEN, GOLD = (0.15, 0.9, 1.0), (1.0, 0.55, 0.15), (0.25, 1.0, 0.55), (1.0, 0.75, 0.3)
ACCENT = 0x3cc8d8

# Model ids (src/world/ModelIds.h) and their own sizes (w, d, h).
M = {
    'generator': (6, 1.87, .6, .99), 'compressor': (12, 2.0, 1.4, 1.6), 'tank_system': (14, 1.58, 1.62, 2.2),
    'service_van': (16, 2.05, 4.9, 2.15), 'wooden_crate': (17, .6, .6, .6), 'desk': (24, 1.4, .7, .75),
    'crt_monitor': (27, .42, .4, .38), 'bookcase': (37, .9, .3, 1.8), 'drum': (49, .55, .55, .8),
    'bench': (51, 1.86, .44, .63), 'car_a': (69, 2.56, 6.15, 2.17), 'cable_reel': (68, 1.0, 2.0, 2.0),
    'cottage': (72, 15.9, 14.7, 5.43), 'dead_tree_a': (73, 2.97, 3.34, 7.91), 'dead_tree_b': (74, 1.75, 1.96, 4.47),
    'fence_wood': (76, 2.0, .06, 1.27), 'fern_a': (77, .97, 1.04, 1.15), 'fern_big': (78, 2.0, 2.18, 2.62),
    'ind_generator': (95, 3.65, 1.3, 1.75), 'ind_sandbags': (103, 2.5, 1.6, 2.54), 'lamppost': (112, 1.05, 3.47, 10.52),
    'log': (113, 3.3, 1.93, .56), 'mushrooms': (117, .41, .41, .43), 'oak_a': (118, 3.72, 3.63, 6.91),
    'oak_b': (119, 3.2, 3.29, 6.06), 'oak_c': (120, 4.27, 4.43, 8.29), 'pine_a': (121, 3.66, 3.36, 10.11),
    'pine_b': (122, 3.66, 3.36, 10.11), 'rock_big': (124, 3.62, 3.3, 2.01), 'rock_mid': (125, 2.52, 2.67, 2.12),
    'rock_small': (126, 1.52, 1.36, .81), 'sapling': (127, 1.97, 2.5, 4.3), 'stump': (128, .88, .93, .52),
    'creeper': (141, 1.89, .41, 3.68), 'bush_a': (142, .7, .7, 1.0), 'bush_c': (144, 1.02, 1.02, 1.0),
    'grass_patch': (146, 2.1, 2.1, .5), 'leafy_a': (148, 3.36, 3.36, 4.04), 'aspen_a': (150, 3.91, 4.47, 9.96),
    'aspen_b': (151, 3.84, 3.55, 5.7), 'fallen_pine': (152, .71, 4.98, 1.61), 'rock_mossy': (153, 1.58, 1.16, .48),
    'boulder': (154, 3.6, 2.67, 2.1), 'boulder_round': (155, 3.15, 2.17, 2.98), 'calcified_tree': (156, 2.97, 3.34, 7.91),
    'crystal_bed': (158, 1.22, 1.22, .21), 'fungus_bulbs': (160, 1.22, .95, 1.79), 'fungus_funnel': (161, 1.98, 1.84, 1.75),
    'fungus_giant': (162, 3.65, 3.65, 3.34), 'fungus_shelf': (163, 3.24, 2.32, .68), 'fungus_spire': (164, 1.96, 1.97, 3.4),
    'arch': (166, 3.0, .6, 1.8), 'dais': (167, 5.2, 6.0, 1.2), 'gateway': (168, 3.0, 1.11, 3.0),
    'pillar': (169, .85, .85, 4.53), 'pillar_broken': (170, .85, .85, 1.46), 'pillar_fallen': (171, 2.13, .6, .52),
    'relic': (172, .5, .4, .4), 'rubble': (173, 1.71, 1.71, .3), 'wall_ruin': (174, 2.85, .75, 3.0),
    'ossuary_wall': (175, 3.0, .75, 3.0), 'rock_block': (176, 3.14, 2.02, 2.88), 'rock_column': (177, 2.1, 2.67, 3.6),
    'rock_slab': (178, 3.47, 2.36, 1.82), 'stalactite': (179, 1.97, 2.74, 3.72), 'stalagmite': (181, 1.97, 2.74, 3.72),
    'stalagmite_cluster': (182, 3.5, 4.5, 2.0), 'web': (183, 1.36, .02, .8),
    'analyzer': (184, 1.83, 2.22, 3.11), 'anomaly_relic': (185, .5, .4, .4), 'cell_gate': (187, 3.5, .19, 2.72),
    'clean_bench': (188, 1.18, .56, .81), 'control_board': (189, 1.32, .19, .6), 'data_terminal': (190, .89, .81, .48),
    'desk_lamp': (191, .18, .4, .52), 'exec_desk': (193, 2.21, 1.1, .89), 'instrument_terminal': (196, .92, .47, .68),
    'mainframe': (198, 2.2, 2.28, 3.18), 'microscope': (199, .18, .22, .29), 'notice_board': (200, 1.2, .07, .9),
    'sample_fridge': (205, .7, .85, 1.0), 'scope_monitor': (206, .47, .34, .45), 'specimen_cabinet': (207, 1.34, .74, 2.19),
    'specimen_crate': (208, 1.12, 1.12, 1.12), 'test_chamber': (210, 1.14, .68, .64),
    'portable_generator': (257, .87, 1.04, .82), 'pump': (248, 3.06, .94, 1.75),
    'cable_bundle': (277, 4.0, .14, .22), 'diesel_genset': (282, 2.29, 1.07, 1.22), 'generator_unit': (287, 1.13, 2.61, 1.67),
    'locker_bank': (290, .87, .85, 2.42), 'aerial': (302, .91, 1.4, 1.95), 'bedroll': (304, .92, 1.96, .18),
    'brazier': (305, .65, .75, .52), 'lantern': (314, .26, .31, .77), 'leanto': (315, 3.57, 2.12, 3.06),
    'sat_dish': (320, 1.03, 1.1, 1.73), 'shack': (322, 3.57, 6.25, 3.06), 'candle': (324, .3, .3, .1),
    'switch_cabinet_1': (362, 1.2, .66, 2.91), 'switch_cabinet_2': (363, 1.5, .82, 2.7),
}
HUNTSMAN, WASP, BRUTE, WARDEN, MUTANT, SNATCHER, CHIMERA, TRANSFORMED = range(8)
HEALTH, SHELLS, NINE = 0, 1, 2
FEMALE_SCIENTIST, CLEANUP, SHARPSHOOTER, SENIOR_SCIENTIST = 12, 4, 6, 11


def f(v):
    if isinstance(v, str): return v
    if isinstance(v, bool): return '1' if v else '0'
    if isinstance(v, int): return str(v)
    s = f'{v:.4f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


class Map:
    def __init__(self, index, name, col, row, outdoor, ceiling=2.8, ambient=.3, spawn=(12, 12, 0), haze=None, surface=None):
        self.index, self.name, self.col, self.row, self.outdoor = index, name, col, row, outdoor
        self.ceiling, self.ambient, self.spawn, self.haze, self.surface = ceiling, ambient, spawn, haze, surface
        self.walls, self.gaps, self.layers, self.records, self.actors = set(), {}, [], [], 0
        self.open = {'n': 0, 's': 0, 'w': 0, 'e': 0}

    def rec(self, *fields):
        self.records.append('|'.join(f(x) for x in fields))

    def note(self, text):
        self.records.append('# ' + text)

    # --- things -------------------------------------------------------------------------------
    def fix(self, name, x, y, base=0, scale=1.0, yaw=0.0, solid=None, size=None):
        sl, w, d, h = M[name]
        if size: w, d, h = size
        if solid is None: solid = h * scale > .9
        self.rec('FIXTURE', self.index, sl, x, y, base, w * scale, d * scale, h * scale, yaw, int(solid))

    def box(self, x1, y1, x2, y2, bottom, top, material, rail=0):
        self.rec('STRUCT', self.index, x1, y1, x2, y2, bottom, top, rail, material)

    def light(self, x, y, z, intensity=1.2, radius=9, kelvin=3400, gel=None, state='', value='', invert='', flicker=0):
        tail = [kelvin, 0, state, value, invert, '', '', '', flicker]
        if gel: tail += ['', '', '', '', *gel]
        self.rec('LIGHT', self.index, x, y, z, 0, 0, intensity, radius, *tail)

    def sign(self, x, y, z, w, h, yaw, title, sub):
        self.rec('SIGN', self.index, x, y, z, w, h, yaw, title[:28], sub[:34], ACCENT)

    def zone(self, x1, y1, x2, y2, name, title='', bottom='', top=''):
        self.rec('ZONE', self.index, x1, y1, x2, y2, name[:40], bottom, top, title[:40])

    def terminal(self, x, y, z, title, line1, line2, activate='', require='', yaw=0.0):
        self.rec('TERMINAL', self.index, x, y, z, int(bool(activate)), title, line1[:56], line2[:56], activate, 0, require, yaw)

    def lead(self, a, b, sag=1.15, live=0):
        span = math.dist(a, b)
        assert span <= 11.8, (self.name, a, b, span)     # the engine allows 12 m of cable per lead
        length = min(span * sag, 11.95)
        self.rec('LEAD', self.index, *a, *b, max(length, .2), .014, 8, live)

    def event(self, name, box, require='', value=1, clear=0, actions=(), gates=(), where='box', match='all', repeat=0):
        """gates: (name, op, value) SCRIPT_REQUIRE tests; where/match/repeat: SCRIPT_TRIGGER (1.2.2)."""
        x1, y1, x2, y2, lo, hi = box
        self.rec('SCRIPT_EVENT', self.index, name, x1, y1, x2, y2, lo, hi, require, value, clear)
        for g in gates:
            self.rec('SCRIPT_REQUIRE', self.index, name, *g)
        if (where, match, repeat) != ('box', 'all', 0):
            self.rec('SCRIPT_TRIGGER', self.index, name, where, match, repeat)
        for a in actions:
            self.rec('SCRIPT_ACTION', self.index, name, *a)

    def creature(self, kind, x, y, z=0, scale='', health=''):
        self.rec('CREATURE', self.index, kind, x, y, z, scale, health)

    def pickup(self, kind, x, y):
        self.rec('PICKUP', self.index, kind, x, y)

    def prop(self, model, x, y, z, yaw=0.0):
        sl, w, d, h = M[model] if isinstance(model, str) else model
        self.rec('CLUTTER', self.index, 7, x, y, z, yaw, sl, w, d, h, 0, 0, '')

    def actor(self, visual, x, y, yaw, height=1.75):
        """A standing person on this map's own clock. Returns the actor index."""
        if self.actors == 0:
            self.rec('TIMED_SEQUENCE', self.index, 'ley_clock_%d' % self.index, 'ley_clock_%d_done' % self.index,
                     0, 0, 24, 24, -100, 100, 60000, 60000, 0, 0, 0, 0, 1, 1)
        i = self.actors
        self.actors += 1
        self.rec('ACTOR', self.index, 'ley_clock_%d' % self.index, visual, height, 0, 0, i)
        self.pending_keys = getattr(self, 'pending_keys', []) + [(i, x, y, yaw)]
        return i

    def keys(self):
        for i, x, y, yaw in getattr(self, 'pending_keys', []):
            self.rec('ACTOR_KEY', self.index, i, 0, x, y, 0, yaw, 0, 0)
            self.rec('ACTOR_KEY', self.index, i, 60000, x, y, 0, yaw, 0, 1)
        self.pending_keys = []

    def ranger(self, visual, x, y, yaw, patrol, weapon=1, talk=''):
        i = self.actor(visual, x, y, yaw, 1.78)
        self.rec('ACTOR_AI', self.index, i, 3, '', 1.45, 2, 6)
        self.rec('ACTOR_FACTION', self.index, i, 2, 'ley_rangers')
        self.rec('ACTOR_GUARD', self.index, i, 18, .35, 700, 6000, 'ley_rangers_alert')
        for px, py in patrol:
            self.rec('ACTOR_PATROL', self.index, i, px, py, 0, 6000)
        self.rec('ACTOR_WEAPON', self.index, i, weapon, 650, 1000, 16 if weapon == 1 else 18, 7 if weapon == 1 else 6)
        self.rec('ACTOR_TACTICS', self.index, i, 0, 1)
        self.rec('ACTOR_ROLE', self.index, i, 1)
        self.rec('ACTOR_HEALTH', self.index, i, 120, 'ley_r_dmg_%d_%d' % (self.index, i), 'ley_r_dead_%d_%d' % (self.index, i))
        self.rec('ACTOR_VOICE', self.index, i, 1)
        self.rec('ACTOR_STANCE', self.index, i, 'ley_ranger_stance')
        if talk: self.rec('ACTOR_TALK', self.index, i, talk)
        return i

    def civilian(self, visual, x, y, yaw, talk):
        i = self.actor(visual, x, y, yaw)
        self.rec('ACTOR_AI', self.index, i, 1, '', 1.2, 2, 6)
        self.rec('ACTOR_FACTION', self.index, i, 2, '')
        self.rec('ACTOR_TALK', self.index, i, talk)
        return i

    def dialogue(self, node, speaker, line, replies):
        """replies: (text, next, require, requireValue, hide, sets)."""
        self.rec('DIALOGUE', self.index, node, speaker, line, node)
        for text, nxt, req, val, hide, sets in replies:
            self.rec('DIALOGUE_REPLY', self.index, node, text, nxt, req, val, hide, sets, '')

    # --- layout -------------------------------------------------------------------------------
    def border(self):
        """Indoor maps: walls round the edge, except each open edge's doorway (tiles 10-13)."""
        for k in range(24):
            for side, tile in (('n', (k, 0)), ('s', (k, 23)), ('w', (0, k)), ('e', (23, k))):
                if not (self.open[side] and 10 <= k <= 13):
                    self.walls.add(tile)

    def west_gate(self, state, sign_title, sign_sub):
        """A vestibule behind the west doorway, its door in the north wall (DOOR spans x only)."""
        self.walls |= {(6, y) for y in range(9, 15)} | {(x, 14) for x in range(1, 7)} | {(1, 9)} | {(6, 9)}
        self.rec('DOOR', self.index, 2, 6, 9.5, 0, 0, 0, 0, 0, 0, state, 1)
        self.sign(5.95, 12.0, 1.9, 1.9, .4, -HALF, sign_title, sign_sub)
        self.light(3.5, 12, 2.5, 1.0, 6, 2600, flicker=.15)

    def north_gate(self, state):
        """An inner wall at row 2 with a door over the north doorway (tiles 10-13)."""
        self.walls |= {(x, 2) for x in range(1, 23) if not 10 <= x <= 13}
        self.rec('DOOR', self.index, 10, 14, 2.5, 0, 0, 0, 0, 0, 0, state, 1)


def build():
    maps = []
    names = [('Trailhead Camp', 1), ('Whispering Circle', 1), ('Watchtower Ridge', 1), ('The Leyline Heart', 0),
             ('Sunken Dig Site', 1), ('Overgrown Nave', 1), ('Hall of Pillars', 0), ('Conduit Observatory', 0),
             ('Bramble Hollow', 1), ('Wayside Shrine', 1), ('Flooded Crypt', 0), ('Crystal Chasm', 0)]
    for i, (name, outdoor) in enumerate(names):
        maps.append(Map(i, name, i % 4, i // 4, bool(outdoor)))
    links = [(0, 1), (1, 2), (0, 4), (1, 5), (4, 5), (4, 8), (5, 9), (8, 9), (5, 6), (9, 10), (6, 7), (6, 10),
             (10, 11), (11, 7), (7, 3)]
    for a, b in links:
        A, B = maps[a], maps[b]
        if A.row == B.row:
            west, east = (A, B) if A.col < B.col else (B, A)
            west.open['e'] = east.open['w'] = 1
        else:
            north, south = (A, B) if A.row < B.row else (B, A)
            north.open['s'] = south.open['n'] = 1
    for m in maps:
        if not m.outdoor: m.border()

    world = ['TERRAIN|4091|1.2|2.4|30|0.42',
             'HORIZON|90|6|9|121,122,118,120,150,73,151']
    flat = lambda x1, y1, x2, y2, h, blend: world.append('TERRAIN_FLAT|%s|%s|%s|%s|%s|%s' % tuple(f(v) for v in (x1, y1, x2, y2, h, blend)))
    ground = lambda x1, y1, x2, y2, kind, edge=1.5: world.append('GROUND|%s|%s|%s|%s|%s|%s' % tuple(f(v) for v in (x1, y1, x2, y2, kind, edge)))
    ground(0, 0, 48, 72, 5, 2)                  # forest litter everywhere, paths and clearings on top
    ground(48, 0, 72, 24, 3, 2)                 # wild grass up on the ridge

    # Shared objectives (any map may own them).
    m0 = maps[0]
    for oid, text in (('obj_wards', 'Attune both leyline relays: the Whispering Circle and the Sunken Dig'),
                      ('obj_portal', 'Enter the mountain through the Overgrown Nave portal'),
                      ('obj_conduit', 'Restore the crypt conduit in the Flooded Crypt'),
                      ('obj_seal', 'Release the Sanctum Seal in the Conduit Observatory'),
                      ('obj_heart', 'Stabilize the Leyline Heart'),
                      ('obj_voss', 'Optional: find Corporal Voss on Watchtower Ridge'),
                      ('obj_relic', 'Optional: recover the Greenbriar relic from the crypt')):
        m0.rec('OBJECTIVE_TEXT', 0, oid, text)

    rng = random.Random(1907)

    def scatter(m, items, area, count, avoid=(), solid_trees=True):
        x1, y1, x2, y2 = area
        placed = 0
        for _ in range(count * 30):
            if placed >= count: break
            x, y = rng.uniform(x1, x2), rng.uniform(y1, y2)
            if any(ax1 <= x <= ax2 and ay1 <= y <= ay2 for ax1, ay1, ax2, ay2 in avoid): continue
            name = rng.choice(items)
            big = M[name][3] > 3
            m.fix(name, x, y, 0, rng.uniform(.85, 1.15), rng.uniform(-PI, PI), solid=big and solid_trees)
            placed += 1

    TREES = ['pine_a', 'pine_b', 'oak_a', 'oak_c', 'aspen_a', 'aspen_b', 'oak_b']
    UNDER = ['fern_a', 'fern_big', 'bush_a', 'bush_c', 'grass_patch', 'rock_mossy', 'stump', 'mushrooms']

    # ===================================================================== MAP 0: TRAILHEAD CAMP
    m = maps[0]
    m.spawn, m.ambient, m.haze = (6.5, 6.5, 0.78), .34, (.55, .66, .64, .016)
    flat(2, 2, 19, 18, 1.0, 5)
    ground(2, 2, 18, 15, 6)                     # gravel camp pad
    ground(10, 14, 14, 24, 4)                   # trail south
    ground(17, 9, 24, 14, 4)                    # trail east
    m.event('ley_start', (3, 3, 11, 11, -20, 20), actions=[
        ('ITEM', 'flashlight', 1), ('ITEM', 'handgun', 2), ('OBJECTIVE', 'obj_wards', 1), ('OBJECTIVE', 'obj_voss', 1),
        ('STATE', 'ley_ranger_stance', 3)])
    # Both relays, in either order, open the portal: a counter checked on every map.
    m.event('ley_wards_open', (0, 0, 24, 24, -20, 30), gates=[('relays', '>=', 2)], where='campaign', actions=[
        ('OBJECTIVE', 'obj_wards', 2), ('OBJECTIVE', 'obj_portal', 1), ('STATE', 'portal_open', 1),
        ('MESSAGE', 'PORTAL WARD OPEN / THE OVERGROWN NAVE', 5), ('CHECKPOINT',)])
    m.zone(1, 1, 19, 18, 'PROJECT GREENBRIAR BASECAMP', 'THE LEYLINE SANCTUARY')
    m.zone(19, 8, 24, 16, 'EAST TRAIL / WHISPERING CIRCLE')
    m.zone(8, 19, 16, 24, 'SOUTH TRAIL / SUNKEN DIG SITE')
    m.fix('service_van', 5.2, 11.0, 0, 1, .15)
    m.fix('car_a', 4.6, 3.9, 0, 1, 1.4)
    m.fix('leanto', 14.0, 5.2, 0, 1, PI)
    m.fix('sat_dish', 17.4, 4.4, 0, 1, -.6)
    m.fix('ind_generator', 10.2, 4.0, 0, 1, 0)
    m.fix('cable_reel', 12.8, 8.8, 0, 1, .5)
    m.fix('exec_desk', 14.0, 5.4, 0, 1, PI)
    m.fix('data_terminal', 13.5, 5.4, .89, 1, PI, solid=False)
    m.fix('scope_monitor', 14.6, 5.5, .89, 1, PI, solid=False)
    m.prop((34, .09, .09, .10), 14.2, 5.15, .89, .4)        # field mug
    m.prop((349, .31, .15, .07), 13.0, 5.2, .89, 1.2)       # dosimeter
    m.prop((350, .11, .38, .05), 15.0, 5.1, .89, -.3)       # walkie
    for x, y, yaw in ((11.2, 7.6, .2), (16.5, 7.8, -.4), (17.4, 8.9, .9)):
        m.fix('specimen_crate', x, y, 0, 1, yaw)
    m.fix('bedroll', 15.8, 10.2, 0, 1, .3, solid=False)
    m.fix('bedroll', 17.2, 11.3, 0, 1, 1.0, solid=False)
    m.fix('brazier', 11.8, 11.8, 0, 1, 0)
    m.rec('EMITTER', 0, 11.8, 11.8, .45, .05, .02, 1.6, .35, .12, .35, 6)
    m.fix('gateway', 21.6, 11.8, 0, 1, HALF)
    m.fix('pillar', 21.2, 9.3, 0, 1)
    m.fix('pillar_broken', 21.3, 14.4, 0, 1, .4)
    m.fix('gateway', 12.0, 21.6, 0, 1, 0)
    m.fix('pillar', 9.3, 21.3, 0, 1)
    m.fix('crystal_bed', 20.8, 9.9, 0, 1, .3)
    m.fix('crystal_bed', 9.9, 20.9, 0, 1, 1.1)
    m.lead((10.2, 4.6, 1.3), (12.8, 8.8, 1.6))
    m.lead((12.8, 8.8, 1.6), (21.0, 9.3, 3.0))
    m.lead((12.8, 8.8, 1.6), (9.4, 18.5, 2.4))
    m.light(14.0, 6.0, 2.7, 1.6, 10, 3200)
    m.light(11.8, 11.8, 1.2, 1.4, 8, 2100, flicker=.25)
    m.light(21.4, 11.8, 2.4, 1.5, 9, 6500, CYAN)
    m.light(12.0, 21.4, 2.4, 1.5, 9, 6500, CYAN)
    m.terminal(13.5, 6.0, 0, 'EXPEDITION LOG // DR. VANCE', 'THE STONE SANCTUARY UNDER THE MOUNTAIN IS WAKING UP.',
               'TWO RELAYS HOLD THE PORTAL: THE CIRCLE, THEN THE DIG.')
    m.box(19.6, 14.1, 20.8, 14.2, 0, 1.2, 20)
    m.sign(20.2, 14.04, .85, 1.1, .35, PI, 'EAST: STONE CIRCLE', 'north ward relay')
    m.box(14.1, 19.6, 14.2, 20.8, 0, 1.2, 20)
    m.sign(14.04, 20.2, .85, 1.1, .35, -HALF, 'SOUTH: SUNKEN DIG', 'south trench relay')
    m.pickup(HEALTH, 15.8, 6.4); m.pickup(SHELLS, 16.4, 6.4); m.pickup(NINE, 15.2, 6.4)
    m.civilian(FEMALE_SCIENTIST, 13.2, 11.2, -2.4, 'vance_hello')
    m.ranger(CLEANUP, 7.5, 15.5, -1.2, [(7.5, 15.5), (17.0, 15.0), (16.5, 3.5)], 1)
    m.keys()
    m.dialogue('vance_hello', 'DR. ARDEN VANCE',
               'You came up the forestry road. Good. We opened the vault under the mountain and the old wards slammed shut behind our dig team.',
               [('How do I get inside the mountain?', 'vance_portal', '', 1, '', ''),
                ('What is this place?', 'vance_lore', '', 1, '', ''),
                ('Anything else I should look for?', 'vance_relic', '', 1, 'vance_relic_ask', ''),
                ('I found your relic in the crypt.', 'vance_thanks', 'relic_found', 1, 'relic_returned', ''),
                ('Later.', '', '', 1, '', '')])
    m.dialogue('vance_portal', 'DR. ARDEN VANCE',
               'Two field relays feed the portal ward: the stone circle east of here and the one down in the sunken dig. Attune both, any order. The Overgrown Nave opens after that.',
               [('Understood.', 'vance_hello', '', 1, '', '')])
    m.dialogue('vance_lore', 'DR. ARDEN VANCE',
               'An Old City sanctuary, built over a living leyline. Our generators hum in tune with the crystal. Corporal Voss holds the ridge to the east; he knows the barrow way in.',
               [('Back.', 'vance_hello', '', 1, '', '')])
    m.dialogue('vance_relic', 'DR. ARDEN VANCE',
               'Lena took the Greenbriar relic down into the flooded crypt before the wards closed. If she did not make it, at least bring the relic back.',
               [('I will look for it.', 'vance_hello', '', 1, '', 'vance_relic_ask=1')])
    m.dialogue('vance_thanks', 'DR. ARDEN VANCE',
               'Lena\'s field tag is still on it. Thank you. Take the spare magazines from the van; you will need them more than we will.',
               [('Thank you.', '', '', 1, '', 'relic_returned=1')])
    m.event('ley_relic_asked', (0, 0, 24, 24, -20, 20), 'vance_relic_ask', 1, actions=[('OBJECTIVE', 'obj_relic', 1)])
    m.event('ley_relic_reward', (0, 0, 24, 24, -20, 20), 'relic_returned', 1, actions=[('OBJECTIVE', 'obj_relic', 2), ('ITEM', 'handgun', 3)])
    scatter(m, TREES, (0, 0, 24, 24), 14, avoid=[(1.5, 1.5, 19.5, 18.5), (8, 17, 16, 24), (17, 8, 24, 16)])
    scatter(m, UNDER, (0, 0, 24, 24), 18, avoid=[(2, 2, 19, 18), (9, 18, 15, 24), (18, 9, 24, 15)])

    # ===================================================================== MAP 1: WHISPERING CIRCLE
    m = maps[1]
    m.spawn, m.haze = (3, 12, 0), (.48, .66, .66, .02)
    ox, oy = 24, 0
    flat(ox + 6, oy + 5, ox + 21, oy + 19, 2.2, 6)
    flat(ox, oy + 10, ox + 7, oy + 14, 1.2, 4)
    ground(ox, oy + 10, ox + 8, oy + 14, 4)
    ground(ox + 7, oy + 6, ox + 20, oy + 18, 2, 2)      # clover on the henge
    m.zone(6, 5, 21, 19, 'THE WHISPERING CIRCLE', 'THE WHISPERING CIRCLE')
    cx, cy = 13.5, 12.0
    m.fix('dais', cx, cy, 0, 1)
    m.fix('relic', cx, cy, 1.2, 1.5, solid=False)
    m.box(cx - .35, cy - .35, cx + .35, cy + .35, 1.2, 4.2, 35)
    m.fix('crystal_bed', cx - 1.5, cy - 1.3, 1.2, 1, .4)
    m.fix('crystal_bed', cx + 1.5, cy + 1.2, 1.2, 1, 1.9)
    for k in range(8):
        a = k * PI / 4 + PI / 8
        px, py = cx + math.cos(a) * 6, cy + math.sin(a) * 5.6
        m.fix('pillar_broken' if k in (2, 5) else 'pillar', px, py, 0, 1.0 if k not in (0, 4) else 1.25, a)
    m.fix('pillar_fallen', cx + 3.5, cy + 6.8, 0, 1, .3)
    m.fix('analyzer', 8.0, 9.6, 0, 1, HALF)
    m.fix('portable_generator', 7.6, 14.4, 0, 1, .3)
    m.lead((8.0, 10.2, 2.4), (cx - .4, cy, 3.0))
    m.lead((7.6, 14.4, .6), (8.0, 10.2, 1.0))
    m.terminal(9.6, 12.0, 0, 'NORTH WARD RELAY // HENGE', 'STANDING STONES CHARGED. RESONANCE LOCKED AT 412 HZ.',
               'ENGAGE TO ATTUNE THE NORTH LEYLINE CONDUIT.', 'ward_north', '', -HALF)
    m.light(cx, cy, 4.6, 1.8, 12, 2400, AMBER, 'ward_north', 1, 1, .2)
    m.light(cx, cy, 4.6, 2.6, 16, 7500, CYAN, 'ward_north', 1, 0)
    m.rec('EMITTER', 1, cx, cy, 1.3, .02, -.02, 2.8, .45, .18, .4, 8)
    m.event('ley_north_done', (0, 0, 24, 24, -20, 30), 'ward_north', 1, where='map', actions=[
        ('SHAKE', .4), ('ADD', 'relays', 1), ('MESSAGE', 'LEYLINE RELAYS {relays} / 2', 4)])
    m.pickup(SHELLS, 8.8, 13.6); m.pickup(HEALTH, 9.4, 14.2)
    m.creature(HUNTSMAN, 18.0, 7.5); m.creature(HUNTSMAN, 19.0, 16.5); m.creature(WASP, 15.5, 4.5)
    for x, y, name in ((17.0, 4.8, 'fungus_spire'), (19.6, 12.0, 'fungus_bulbs'), (5.2, 19.5, 'calcified_tree')):
        m.fix(name, x, y, 0, 1, rng.uniform(-PI, PI))
    scatter(m, TREES, (0, 0, 24, 24), 16, avoid=[(5, 4, 22, 20), (0, 9, 8, 15)])
    scatter(m, UNDER + ['mushrooms', 'mushrooms'], (0, 0, 24, 24), 20, avoid=[(6, 5, 21, 19)])

    # ===================================================================== MAP 2: WATCHTOWER RIDGE
    m = maps[2]
    m.spawn, m.haze = (2, 12, 0), (.52, .62, .66, .02)
    ox, oy = 48, 0
    flat(ox, oy + 9, ox + 6, oy + 15, 1.6, 4)
    flat(ox + 6, oy + 4, ox + 20, oy + 19, 3.0, 5)          # the ridge top
    flat(ox, oy + 21.5, ox + 24, oy + 24, 6.0, 3)           # the mountain shoulder hides the vaults below
    flat(ox + 21.5, oy, ox + 24, oy + 24, 6.0, 3)
    ground(ox, oy + 10, ox + 8, oy + 14, 4)
    ground(ox + 7, oy + 5, ox + 19, oy + 18, 6, 2)          # trampled gravel round the tower
    m.zone(6, 4, 20, 19, 'WATCHTOWER RIDGE / RANGER OUTPOST', 'WATCHTOWER RIDGE')
    # The ruined watchtower: four ancient-stone walls, a doorway west, a stair up to its roof.
    tx1, ty1, tx2, ty2 = 11.0, 8.0, 17.0, 14.0
    m.box(tx1, ty1, tx2, ty1 + .4, 0, 3.4, 33)
    m.box(tx1, ty2 - .4, tx2, ty2, 0, 3.4, 33)
    m.box(tx2 - .4, ty1, tx2, ty2, 0, 3.4, 33)
    m.box(tx1, ty1, tx1 + .4, ty1 + 2.2, 0, 3.4, 33)
    m.box(tx1, ty1 + 3.8, tx1 + .4, ty2, 0, 3.4, 33)
    m.box(tx1, ty1 + 2.2, tx1 + .4, ty1 + 3.8, 2.4, 3.4, 33)   # lintel over the doorway
    m.box(tx1 + .4, ty1 + .4, tx2 - .4, ty1 + 3.6, 3.0, 3.25, 33)   # roof slab, north half
    m.box(tx1 + .4, ty1 + 3.6, tx1 + 1.2, ty2 - .4, 3.0, 3.25, 33)  # roof, west strip
    m.box(tx2 - 1.4, ty1 + 3.6, tx2 - .4, ty2 - .4, 3.0, 3.25, 33)  # roof, east strip
    for x1, y1, x2, y2 in ((tx1, ty1, tx2, ty1 + .25), (tx1, ty2 - .25, tx2, ty2), (tx2 - .25, ty1, tx2, ty2), (tx1, ty1, tx1 + .25, ty2)):
        m.box(x1, y1, x2, y2, 3.25, 4.1, 33)                       # battlements
    m.rec('STAIR', 2, tx1 + 1.2, ty1 + 3.6, tx2 - 1.4, ty2 - .4, 0, 3.0, 14, 0, 1, 0, .12, 0)
    m.rec('SHELTER', 2, tx1, ty1, tx2, ty2, 0, 3.0)
    m.fix('aerial', tx1 + 1.6, ty1 + 1.6, 3.25, 1, .4)
    m.fix('lantern', tx2 - 1.0, ty1 + 1.0, 3.25, 1, 0, solid=False)
    m.light(14.0, 11.0, 2.7, 1.2, 8, 2800)
    m.light(tx2 - 1.0, ty1 + 1.0, 4.0, 1.4, 10, 2600, flicker=.15)
    m.fix('ind_sandbags', 9.0, 6.0, 0, 1, .2)
    m.fix('ind_sandbags', 9.0, 17.0, 0, 1, -.2)
    m.fix('locker_bank', 12.0, 8.9, 0, 1, 0)
    m.fix('specimen_crate', 16.0, 10.6, 0, 1, .1)
    m.fix('lamppost', 19.0, 16.0, 0, .55, 0)
    m.light(19.0, 16.0, 5.4, 1.4, 12, 4200)
    m.pickup(SHELLS, 13.0, 10.0); m.pickup(SHELLS, 13.6, 10.0); m.pickup(HEALTH, 14.2, 10.0); m.pickup(NINE, 14.8, 10.0)
    m.terminal(15.6, 9.2, 0, 'OUTPOST LOG // CPL. K. VOSS', 'DIG TEAM WENT IN THROUGH THE BARROW BY THE SHRINE.',
               'MY CODE STILL OPENS THAT DOOR. TALK TO ME FIRST.', yaw=PI)
    voss = m.ranger(SHARPSHOOTER, 9.6, 11.5, 0, [(9.6, 11.5), (9.4, 13.0)], 1, 'voss_hello')
    m.ranger(CLEANUP, 18.5, 7.0, PI, [(18.5, 7.0), (18.5, 17.0)], 1)
    m.ranger(CLEANUP, 6.5, 16.5, -HALF, [(6.5, 16.5), (14.0, 17.5)], 2)
    m.keys()
    m.dialogue('voss_hello', 'CPL. KAELEN VOSS',
               'Easy. Vance sent you? Then you are the only one going in. Things come down off that mountain at night, and my people stay on the wall.',
               [('How do I get under the mountain?', 'voss_barrow', '', 1, '', ''),
                ('What came out of the vault?', 'voss_things', '', 1, '', ''),
                ('Later.', '', '', 1, '', '')])
    m.dialogue('voss_barrow', 'CPL. KAELEN VOSS',
               'South of here, past the nave, there is a wayside shrine. The barrow behind it runs straight into the flooded crypt. My code is on this card.',
               [('I will take it.', '', '', 1, 'voss_clearance', 'voss_clearance=1')])
    m.dialogue('voss_things', 'CPL. KAELEN VOSS',
               'Wardens. Tall, quiet, wrong. And something big in the heart chamber that the crystal will not let die. Shoot for the joints.',
               [('Back.', 'voss_hello', '', 1, '', '')])
    m.event('ley_voss_card', (0, 0, 24, 24, -20, 30), 'voss_clearance', 1, actions=[('OBJECTIVE', 'obj_voss', 2), ('ITEM', 'handgun', 2)])
    m.creature(WASP, 4.0, 4.0); m.creature(HUNTSMAN, 4.5, 20.0)
    scatter(m, ['pine_a', 'pine_b', 'dead_tree_a', 'aspen_b'], (0, 0, 24, 24), 14, avoid=[(5, 3, 21, 20), (0, 9, 7, 15)])
    scatter(m, ['rock_big', 'rock_mid', 'rock_small', 'boulder', 'bush_c', 'grass_patch'], (0, 0, 24, 24), 16,
            avoid=[(5, 3, 21, 20), (0, 9, 7, 15)])

    # ===================================================================== MAP 4: SUNKEN DIG SITE
    m = maps[4]
    m.spawn, m.haze = (12, 3, HALF), (.5, .64, .62, .02)
    ox, oy = 0, 24
    flat(ox + 9, oy, ox + 15, oy + 4, .8, 4)
    flat(ox + 3, oy + 4, ox + 21, oy + 21, .2, 5)
    flat(ox + 20, oy + 9, ox + 24, oy + 15, .4, 4)
    flat(ox + 9, oy + 20, ox + 15, oy + 24, .4, 4)
    ground(ox + 4, oy + 5, ox + 20, oy + 20, 7, 2)          # mud in the trench
    ground(ox + 10, oy, ox + 14, oy + 6, 4)
    m.zone(3, 3, 21, 21, 'SUNKEN EXCAVATION TRENCH', 'SUNKEN DIG SITE')
    m.fix('wall_ruin', 7.5, 7.0, 0, 1)
    m.fix('wall_ruin', 16.5, 7.0, 0, 1)
    m.fix('arch', 12.0, 7.0, 0, 1)
    m.fix('ossuary_wall', 4.6, 14.5, 0, 1, HALF)
    m.fix('rubble', 9.2, 9.5, 0, 1, .7)
    m.fix('pillar_fallen', 18.2, 13.0, 0, 1, 1.2)
    for a in ((10, 10, 15, 10.25), (10, 14.75, 15, 15), (10, 10, 10.25, 15), (14.75, 10, 15, 15)):
        m.box(*a, 0, .55, 33)
    m.rec('WATER', 4, 10.25, 10.25, 14.75, 14.75, -.2, .42)
    m.fix('crystal_bed', 12.5, 12.5, .05, 1.3, .8, solid=False)
    m.light(12.5, 12.5, 1.2, 1.9, 10, 7000, (.15, .9, .85))
    m.fix('shack', 6.8, 18.0, 0, 1, HALF)
    m.rec('SHELTER', 4, 3.6, 15, 10, 21, 0, 3)
    m.fix('diesel_genset', 4.5, 11.6, 0, 1, 0)
    m.fix('pump', 17.5, 17.5, 0, 1, .3)
    m.fix('instrument_terminal', 11.2, 17.6, 0, 1, PI)
    m.fix('specimen_crate', 15.8, 18.4, 0, 1, .4)
    m.fix('clean_bench', 13.5, 4.6, 0, 1, 0)
    m.fix('microscope', 13.3, 4.6, .81, 1, 0, solid=False)
    m.prop((230, .15, .15, .24), 13.8, 4.6, .81)            # specimen jar
    m.terminal(12.5, 18.2, 0, 'SOUTH TRENCH RELAY // DIG SITE', 'HYDRO-CRYSTAL CONDUIT STABLE. PORTAL WARD WAITING.',
               'ONE OF TWO RELAYS HOLDING THE PORTAL WARD.', 'ward_south', '', PI)
    m.light(12.5, 18.2, 2.4, 1.8, 10, 6800, GREEN, 'ward_south', 1)
    m.light(12.5, 18.2, 2.4, 1.0, 8, 2400, AMBER, 'ward_south', 1, 1, .3)
    m.lead((4.5, 11.6, 1.0), (12.4, 17.8, 1.0))
    m.lead((4.5, 11.6, 1.0), (6.8, 17.0, 2.9))
    m.light(6.8, 18.0, 2.7, 1.2, 8, 3000)
    m.event('ley_south_done', (0, 0, 24, 24, -20, 30), 'ward_south', 1, where='map', actions=[
        ('SHAKE', .6), ('ADD', 'relays', 1), ('MESSAGE', 'LEYLINE RELAYS {relays} / 2', 4)])
    m.pickup(HEALTH, 10.2, 17.6); m.pickup(SHELLS, 14.2, 17.6); m.pickup(NINE, 15.0, 17.6)
    m.creature(HUNTSMAN, 17.5, 11.0); m.creature(MUTANT, 18.5, 4.0); m.creature(WASP, 5.0, 5.0)
    scatter(m, TREES, (0, 0, 24, 24), 14, avoid=[(2.5, 2.5, 21.5, 21.5), (8, 0, 16, 4), (20, 8, 24, 16), (8, 20, 16, 24)])
    scatter(m, UNDER, (0, 0, 24, 24), 14, avoid=[(3, 3, 21, 21)])

    # ===================================================================== MAP 5: OVERGROWN NAVE
    m = maps[5]
    m.spawn, m.haze = (3, 12, 0), (.46, .62, .6, .022)
    ox, oy = 24, 24
    flat(ox, oy + 8, ox + 24, oy + 16, 0, 5)                # the processional, level with the hall floor
    flat(ox + 10, oy, ox + 14, oy + 8, .6, 4)
    flat(ox + 10, oy + 16, ox + 14, oy + 24, .3, 4)
    ground(ox, oy + 9, ox + 22, oy + 15, 6, 1)
    m.zone(2, 7, 22, 17, 'THE OVERGROWN NAVE', 'THE OVERGROWN NAVE')
    # The mountain face: a cliff of ancient stone with the portal cut into it.
    m.walls |= {(23, y) for y in range(24) if not 10 <= y <= 13}
    m.box(22.3, 0, 24, 10, 0, 7.4, 26)
    m.box(22.3, 14, 24, 24, 0, 7.4, 26)
    m.box(22.3, 10, 24, 14, 3.6, 7.4, 33)
    m.fix('gateway', 22.0, 12.0, 0, 1.2, HALF)
    for x, y, name, s in ((21.2, 3.0, 'rock_block', 1.3), (21.0, 7.6, 'boulder_round', 1.2), (21.2, 16.8, 'rock_column', 1.4),
                          (21.0, 21.0, 'boulder', 1.3), (19.8, 1.5, 'rock_mid', 1.1)):
        m.fix(name, x, y, 0, s, rng.uniform(-PI, PI))
    m.sign(22.25, 8.9, 2.8, 2.0, .45, -HALF, 'HALL OF PILLARS', 'the portal ward holds it shut')
    for y, name, s in ((2.0, 'rock_block', 1.3), (6.5, 'boulder_round', 1.3), (16.0, 'rock_column', 1.2), (20.5, 'boulder', 1.4)):
        m.fix(name, 23.0, y, 6.6, s, rng.uniform(-PI, PI))
    for y in (4.0, 8.0, 15.6, 19.8):
        m.fix('creeper', 22.2, y, 3.6, 1.0, -HALF, solid=False)
    m.fix('pine_a', 21.6, 0.8, 0, 1.1, .3, solid=True)
    m.fix('pine_b', 21.4, 23.0, 0, 1.0, 2.1, solid=True)
    for x in (5.5, 9.5, 13.5, 17.5):
        for y, broken in ((8.3, x == 13.5), (15.7, x == 5.5)):
            m.fix('pillar_broken' if broken else 'pillar', x, y, 0, 1.15)
    m.fix('pillar_fallen', 13.2, 10.0, 0, 1.2, .4)
    m.fix('crystal_bed', 9.5, 9.3, 0, 1)
    m.fix('crystal_bed', 17.5, 14.7, 0, 1, 1.2)
    m.fix('fungus_giant', 19.0, 5.0, 0, 1)
    m.fix('fungus_spire', 4.5, 19.5, 0, 1.2)
    m.fix('calcified_tree', 8.5, 4.8, 0, 1, .6)
    m.fix('calcified_tree', 15.0, 19.4, 0, 1, -.8)
    for x in (7.5, 11.5, 15.5, 19.5):
        m.fix('creeper', x, 8.3, 0, .9, 0, solid=False)
    m.fix('generator_unit', 18.4, 14.2, 0, 1)
    m.lead((18.4, 14.2, 1.2), (23.6, 13.4, .3))
    m.light(9.5, 12.0, 2.6, 1.5, 10, 6800, CYAN)
    m.light(21.2, 12.0, 3.0, 1.8, 10, 3200)
    m.light(21.2, 12.0, 3.0, 1.6, 10, 6500, GREEN, 'portal_open', 1)
    m.pickup(HEALTH, 18.6, 11.0); m.pickup(SHELLS, 19.2, 11.0)
    m.creature(BRUTE, 13.5, 12.0); m.creature(WASP, 19.0, 9.6); m.creature(HUNTSMAN, 6.0, 20.0)
    scatter(m, TREES, (0, 0, 21, 24), 12, avoid=[(0, 7, 23, 17), (9, 0, 15, 8), (9, 16, 15, 24)])
    scatter(m, UNDER + ['fungus_bulbs'], (0, 0, 21, 24), 16, avoid=[(0, 8, 23, 16)])

    # ===================================================================== MAP 8: BRAMBLE HOLLOW
    m = maps[8]
    m.spawn, m.haze = (12, 3, HALF), (.42, .58, .52, .026)
    ox, oy = 0, 48
    flat(ox + 3, oy + 10, ox + 21, oy + 13.5, -1.0, 2.5)    # the creek bed
    flat(ox + 3, oy + 15, ox + 13, oy + 22, .6, 4)          # the woodcutter's clearing
    flat(ox + 9, oy, ox + 15, oy + 6, .4, 4)
    ground(ox + 3, oy + 10, ox + 21, oy + 13.5, 7, 1)
    ground(ox + 4, oy + 16, ox + 12, oy + 21, 8, 2)
    m.zone(2, 2, 22, 22, 'BRAMBLE HOLLOW', 'BRAMBLE HOLLOW')
    m.rec('WATER', 8, 3.5, 10.4, 20.5, 13.1, -1.4, -.55)
    m.fix('fallen_pine', 12.0, 11.8, -.1, 1.2, 0)            # the log bridge
    m.fix('log', 7.0, 11.6, -.6, 1, .3, solid=False)
    m.fix('cottage', 8.0, 18.5, 0, .5, .2)                   # the woodcutter's house, half size
    m.fix('wooden_crate', 14.0, 17.5, 0, 1, .3); m.fix('wooden_crate', 14.5, 18.2, 0, 1, .9)
    m.fix('drum', 15.2, 16.8, 0, 1)
    m.fix('lantern', 13.6, 16.6, .6, 1, solid=False)
    m.light(13.6, 16.6, 1.2, 1.2, 7, 2300, flicker=.2)
    for k in range(9):                                       # a fairy ring of glowing mushrooms
        a = k * 2 * PI / 9
        m.fix('mushrooms', 18 + math.cos(a) * 2.2, 18.5 + math.sin(a) * 2.2, 0, 1.6, a, solid=False)
    m.fix('crystal_bed', 18, 18.5, 0, 1.1)
    m.light(18, 18.5, 1.0, 1.4, 7, 6500, GREEN)
    m.pickup(HEALTH, 18, 18.5); m.pickup(SHELLS, 14.6, 17.4); m.pickup(NINE, 13.4, 18.0)
    m.terminal(14.0, 16.2, 0, 'SUPPLY CACHE // GREENBRIAR', 'DROPPED BY THE SURVEY TEAM BEFORE THE WARDS CLOSED.',
               'THE RING OF LIGHTS IN THE GRASS IS NOT OURS.', yaw=PI)
    m.creature(SNATCHER, 6.0, 6.0); m.creature(HUNTSMAN, 19.0, 6.5); m.creature(MUTANT, 4.0, 20.5)
    scatter(m, ['oak_a', 'oak_b', 'oak_c', 'leafy_a', 'aspen_b', 'dead_tree_b'], (0, 0, 24, 24), 16,
            avoid=[(2.5, 9.5, 21.5, 14), (2.5, 14.5, 22, 22), (8, 0, 16, 6), (20, 8, 24, 16)])
    scatter(m, UNDER + ['fern_big', 'fern_big'], (0, 0, 24, 24), 20, avoid=[(3, 10, 21, 13.6), (3, 15, 22, 22)])

    # ===================================================================== MAP 9: WAYSIDE SHRINE
    m = maps[9]
    m.spawn, m.haze = (3, 12, 0), (.44, .58, .58, .024)
    ox, oy = 24, 48
    flat(ox, oy + 8, ox + 24, oy + 16, 0, 5)
    flat(ox + 10, oy, ox + 14, oy + 8, .3, 4)
    ground(ox + 4, oy + 4, ox + 12, oy + 10, 6, 1)
    ground(ox, oy + 10, ox + 22, oy + 14, 4)
    m.zone(3, 3, 13, 11, 'THE WAYSIDE SHRINE', 'WAYSIDE SHRINE')
    m.zone(14, 8, 24, 16, 'THE BARROW DOOR')
    m.fix('gateway', 8.0, 5.0, 0, 1)
    m.fix('arch', 8.0, 8.4, 0, 1)
    m.fix('dais', 8.0, 6.6, 0, .55)
    m.fix('relic', 8.0, 6.6, .66, 1, solid=False)
    for x, y in ((6.6, 6.0), (9.4, 6.0), (6.8, 7.4), (9.2, 7.4)):
        m.fix('candle', x, y, .66, 1, solid=False)
    m.fix('lantern', 5.4, 7.6, 0, 1, solid=False); m.fix('lantern', 10.6, 7.6, 0, 1, solid=False)
    m.light(8.0, 6.6, 1.6, 1.6, 9, 2100, flicker=.3)
    m.rec('SHELTER', 9, 6, 4, 10, 9, 0, 3)
    m.fix('bench', 8.0, 9.6, 0, 1, PI)
    m.terminal(5.6, 9.0, 0, 'A NOTE PINNED TO THE SHRINE', 'LENA: WENT IN THROUGH THE BARROW WITH THE RELIC.',
               'IF THE DOOR IS SHUT, ASK VOSS UP ON THE RIDGE FOR THE CODE.', yaw=0)
    # The barrow: a mound of rock with an ancient door cut through it into the crypt.
    m.walls |= {(23, y) for y in range(24) if not 10 <= y <= 13}
    m.box(22.3, 0, 24, 10, 0, 6.4, 26)
    m.box(22.3, 14, 24, 24, 0, 6.4, 26)
    m.box(22.3, 10, 24, 14, 3.2, 6.4, 33)
    m.fix('ossuary_wall', 21.8, 8.8, 0, 1, HALF)
    m.fix('ossuary_wall', 21.8, 15.2, 0, 1, HALF)
    for x, y, name in ((20.6, 4.0, 'boulder'), (20.8, 19.8, 'rock_block'), (19.5, 22.0, 'rock_mid')):
        m.fix(name, x, y, 0, 1.3, rng.uniform(-PI, PI))
    m.sign(22.25, 9.6, 2.6, 1.8, .4, -HALF, 'THE BARROW', 'into the flooded crypt')
    for y, name, s in ((1.5, 'boulder_round', 1.2), (5.5, 'rock_mid', 1.4), (17.5, 'boulder', 1.1), (21.5, 'rock_block', 1.0)):
        m.fix(name, 23.0, y, 5.6, s, rng.uniform(-PI, PI))          # a broken rocky crest, not a flat wall top
    for y in (3.0, 7.6, 16.4, 20.6):
        m.fix('creeper', 22.2, y, 2.6, 1.0, -HALF, solid=False)
    m.light(21.0, 12.0, 2.6, 1.4, 9, 6800, CYAN)
    m.pickup(SHELLS, 9.0, 4.2); m.pickup(HEALTH, 7.0, 4.2)
    m.creature(TRANSFORMED, 18.0, 12.0); m.creature(HUNTSMAN, 16.0, 19.0)
    scatter(m, TREES + ['dead_tree_a'], (0, 0, 24, 24), 14, avoid=[(3, 3, 13, 11), (0, 8, 24, 16), (9, 0, 15, 8)])
    scatter(m, UNDER, (0, 0, 24, 24), 16, avoid=[(3, 3, 13, 11), (0, 9, 24, 15)])

    # ===================================================================== MAP 6: HALL OF PILLARS
    m = maps[6]
    m.ceiling, m.ambient, m.surface = 6.8, .2, (9, 9, 9)
    m.spawn, m.haze = (3.5, 12, 0), (.18, .36, .42, .015)
    m.west_gate('portal_open', 'PORTAL WARD', 'opens when both relays sing')
    deck = ({(x, y) for x in range(15, 23) for y in range(3, 23)} | {(x, y) for x in range(4, 16) for y in range(18, 23)})
    m.layers.append(('Upper rune gallery', 3.2, .25, deck))
    m.zone(1, 1, 15, 18, 'HALL OF PILLARS / LOWER CRYPT', 'HALL OF PILLARS', -.5, 3.0)
    m.zone(4, 3, 23, 23, 'UPPER RUNE GALLERY', '', 3.0, 6.8)
    m.event('ley_enter_hall', (1, 1, 23, 9, -1, 7), actions=[
        ('OBJECTIVE', 'obj_portal', 2), ('OBJECTIVE', 'obj_conduit', 1), ('CHECKPOINT',)])
    m.rec('STAIR', 6, 7, 13, 9, 18, 0, 3.2, 16, 1, 1, 0, .12, 1)
    for x, y in ((5.0, 6.5), (13.5, 7.5), (13.5, 15.5), (5.0, 18.6), (11.0, 18.6)):
        m.fix('pillar', x, y, 0, 1, size=(.95, .95, 6.6))
    m.fix('dais', 11.0, 10.8, 0, .6)
    m.box(10.6, 10.4, 11.4, 11.2, .72, 3.0, 35)
    m.fix('crystal_bed', 11.0, 10.8, .72, 1, solid=False)
    m.fix('ossuary_wall', 1.6, 4.5, 0, 1, HALF)
    m.fix('ossuary_wall', 1.6, 20.0, 0, 1, HALF)
    m.fix('rubble', 3.5, 16.0, 0, 1)
    m.fix('mainframe', 20.5, 12.0, 3.2, 1, -HALF)
    m.fix('exec_desk', 18.0, 7.5, 3.2, 1, -HALF)
    m.fix('data_terminal', 18.0, 7.5, 4.09, 1, -HALF, solid=False)
    m.terminal(18.6, 9.0, 3.2, 'GALLERY LOG // DIG TEAM', 'CONDUIT THROUGH THE CRYPT KEEPS THE INNER SEAL LIVE.',
               'RESTORE IT, THEN RELEASE THE SEAL IN THE OBSERVATORY.', yaw=-HALF)
    m.lead((20.5, 12.0, 6.2), (20.5, 22.0, 6.2))
    m.lead((11.0, 10.8, 3.0), (11.0, 20.0, 6.4))
    m.light(11.0, 10.8, 3.6, 2.0, 13, 7200, CYAN)
    for x, y in ((4, 9), (4, 16), (9, 4)):
        m.light(x, y, 2.9, 1.0, 8, 3000)
    for x, y in ((18.5, 6), (18.5, 16), (8, 20.5)):
        m.light(x, y, 6.5, 1.2, 10, 3600)
    m.light(18.5, 20.5, 2.9, .9, 7, 2800)
    m.pickup(SHELLS, 18.0, 10.5); m.pickup(NINE, 18.0, 11.5); m.pickup(HEALTH, 3.0, 4.0)
    m.creature(WARDEN, 9.0, 6.0); m.creature(MUTANT, 18.5, 16.0, 3.2); m.creature(WASP, 12.0, 20.5)

    # ===================================================================== MAP 10: FLOODED CRYPT
    m = maps[10]
    m.ceiling, m.ambient, m.surface = 5.6, .16, (9, 9, 9)
    m.spawn, m.haze = (3.5, 12, 0), (.12, .3, .3, .022)
    m.west_gate('voss_clearance', 'BARROW DOOR', 'ranger code lock / cpl. voss')
    ledge = {(x, y) for x in range(15, 23) for y in range(1, 7)}
    m.layers.append(('Conduit ledge', 2.8, .25, ledge))
    m.zone(1, 1, 23, 23, 'THE FLOODED CRYPT', 'THE FLOODED CRYPT', -.5, 2.6)
    m.zone(15, 1, 23, 7, 'CONDUIT LEDGE', '', 2.6, 5.6)
    m.event('ley_enter_crypt', (1, 1, 23, 23, -1, 7), actions=[('OBJECTIVE', 'obj_portal', 2), ('OBJECTIVE', 'obj_conduit', 1)])
    m.rec('WATER', 10, 7, 3, 23, 23, -.4, .32)
    m.rec('WATER', 10, 1, 15, 7, 23, -.4, .32)
    m.rec('STAIR', 10, 19, 7, 21, 12, 0, 2.8, 14, 1, 0, 0, .12, 1)
    for x in (4.5, 9.0, 13.5, 18.0):
        for y in (8.5, 19.5):
            if not (x in (4.5, 18.0) and y == 8.5):     # keep the barrow door and the ledge stair clear
                m.fix('ossuary_wall', x, y, 0, 1, 0)
    for x, y in ((7.0, 14.0), (11.0, 14.0), (15.0, 14.0)):
        m.fix('rock_slab', x, y, 0, .6, HALF)                  # sarcophagi
    m.fix('web', 1.4, 21.5, 2.0, 1.5, HALF, solid=False)
    # The relic alcove, south-west, dark until your torch finds it.
    m.box(1, 20.5, 4.5, 20.8, 0, 3.4, 33)
    m.fix('dais', 2.6, 22.0, 0, .3)
    m.fix('anomaly_relic', 2.6, 22.0, .36, 1.2, solid=False)
    m.rec('DARK', 10, 1, 20.8, 4.6, 23, -1, 4, '', '', '', 0)
    m.light(2.6, 22.0, .9, .5, 2.5, 6500, CYAN)
    m.event('ley_relic', (1, 21, 4.5, 23, -1, 3), actions=[('STATE', 'relic_found', 1), ('OBJECTIVE', 'obj_relic', 1), ('SHAKE', .3)])
    m.terminal(4.0, 21.4, 0, 'FIELD TAG // DR. LENA MORROW', 'IT HUMS WHEN THE CONDUIT RUNS. TAKE IT BACK TO ARDEN.',
               'I AM GOING DEEPER. DO NOT FOLLOW ME INTO THE CHASM.', yaw=HALF)
    # The conduit switch on the ledge, cables down into the water.
    m.fix('switch_cabinet_1', 19.0, 2.0, 2.8, 1, 0)
    m.terminal(17.0, 2.4, 2.8, 'CRYPT CONDUIT // MANUAL BREAKER', 'CRYSTAL CONDUIT DE-ENERGISED. INNER SEAL OFFLINE.',
               'CLOSE THE BREAKER TO FEED THE OBSERVATORY.', 'crypt_conduit', '', 0)
    m.lead((19.0, 2.0, 5.4), (19.0, 12.0, .3))
    m.lead((19.0, 12.0, .3), (23.0, 12.0, .3))
    m.light(19.0, 3.0, 5.2, 1.4, 10, 6500, CYAN, 'crypt_conduit', 1)
    m.light(19.0, 3.0, 5.2, .8, 8, 2200, AMBER, 'crypt_conduit', 1, 1, .4)
    m.event('ley_conduit_done', (0, 0, 24, 24, -1, 7), 'crypt_conduit', 1, actions=[
        ('OBJECTIVE', 'obj_conduit', 2), ('OBJECTIVE', 'obj_seal', 1), ('SHAKE', .6), ('CHECKPOINT',)])
    for x, y in ((6, 4), (12, 4), (6, 11), (12, 17), (18, 17)):
        m.light(x, y, 5.2, .9, 8, 2600, flicker=.12)
    m.light(11.0, 14.0, 1.0, 1.0, 9, 7000, (.15, .7, .8))
    m.pickup(HEALTH, 17.5, 4.0); m.pickup(SHELLS, 18.5, 4.0); m.pickup(NINE, 9.0, 11.5)
    m.creature(MUTANT, 9.0, 16.5); m.creature(MUTANT, 16.0, 11.0); m.creature(SNATCHER, 20.0, 20.0)

    # ===================================================================== MAP 11: CRYSTAL CHASM
    m = maps[11]
    m.ceiling, m.ambient, m.surface = 8.0, .14, (5, 5, 5)
    m.spawn, m.haze = (3, 12, 0), (.1, .26, .36, .02)
    # A steel catwalk spans the fissure east-west; the cave floor goes round it north and south.
    walk = {(x, y) for x in range(7, 17) for y in range(8, 10)}
    m.layers.append(('Steel catwalk', 3.2, .18, walk))
    m.zone(1, 1, 23, 23, 'THE CRYSTAL CHASM', 'THE CRYSTAL CHASM', -.5, 3.0)
    m.zone(7, 8, 17, 10, 'SURVEY CATWALK', '', 3.0, 8.0)
    m.rec('STAIR', 11, 3, 8, 7, 10, 0, 3.2, 16, 0, 1, 0, .1, 1)
    m.rec('STAIR', 11, 17, 8, 21, 10, 0, 3.2, 16, 0, 0, 0, .1, 1)
    # The fissure: anomaly light under the catwalk. Wade through it on the floor and it burns.
    m.rec('HAZARD', 11, 7, 8.5, 5, 15.5, 13, 0, 1.6, 14)
    for x, y in ((9, 6), (10.5, 11.5), (13.5, 6.5), (8.5, 12.5), (14.5, 11.8)):
        m.box(x - .3, y - .3, x + .3, y + .3, 0, rng.uniform(1.0, 2.6), 35)
        m.fix('crystal_bed', x + .4, y - .3, 0, 1.2, rng.uniform(-PI, PI), solid=False)
    m.light(11, 9, 1.0, 2.4, 12, 7500, CYAN)
    m.light(11, 9, .5, 1.4, 8, 6500, (.6, .2, 1.0), flicker=.3)
    m.rec('EMITTER', 11, 11, 9, .2, .02, .03, 1.2, .5, .3, .5, 10)
    for x, y, name, s in ((3, 3, 'stalagmite_cluster', 1), (21, 21, 'stalagmite_cluster', 1.1), (21.5, 4, 'stalagmite', 1),
                          (3, 20, 'rock_column', 1.2), (20, 15.5, 'boulder_round', 1), (7, 20.5, 'fungus_giant', 1)):
        m.fix(name, x, y, 0, s, rng.uniform(-PI, PI))
    for x, y in ((6, 4), (14, 18), (19, 13)):
        m.fix('stalactite', x, y, 4.2, 1, rng.uniform(-PI, PI), solid=False)
    m.fix('instrument_terminal', 12.0, 8.3, 3.2, 1, 0)
    m.terminal(13.0, 9.6, 3.2, 'SURVEY CATWALK // MORROW', 'THE FISSURE SINGS AT NIGHT. IT KNOWS WE ARE HERE.',
               'OBSERVATORY IS NORTH. DO NOT WALK IN THE LIGHT.', yaw=PI)
    for x, y in ((8, 9), (15, 9), (12, 2), (12, 20)):
        m.light(x, y, 5.6 if y != 9 else 6.0, .9, 8, 3800)
    m.pickup(HEALTH, 14.5, 9.0); m.pickup(SHELLS, 20.0, 18.0); m.pickup(NINE, 3.0, 16.0); m.pickup(SHELLS, 9.0, 9.0)
    m.creature(WASP, 12.0, 3.0); m.creature(WASP, 8.0, 18.0); m.creature(WARDEN, 20.0, 12.5)

    # ===================================================================== MAP 7: CONDUIT OBSERVATORY
    m = maps[7]
    m.ceiling, m.ambient, m.surface = 6.4, .24, (4, 9, 4)
    m.spawn, m.haze = (3.5, 12, 0), (.16, .3, .36, .012)
    m.north_gate('sanctum_seal')
    deck = {(x, y) for x in range(15, 23) for y in range(4, 20)}
    m.layers.append(('Observation deck', 3.2, .25, deck))
    m.zone(1, 3, 23, 23, 'CONDUIT OBSERVATORY', 'CONDUIT OBSERVATORY', -.5, 3.0)
    m.zone(15, 4, 23, 20, 'OBSERVATION DECK', '', 3.0, 6.4)
    m.rec('STAIR', 7, 19, 20, 21, 23, 0, 3.2, 14, 1, 0, 0, .12, 1)     # south-east corner, clear of the chasm doorway
    # Dr. Solis's glassed office, west side: glass panes you can shoot out.
    m.box(1, 15, 6.6, 15.15, 0, .9, 20)
    m.rec('BREAKABLE', 7, 1, 15, 6.6, 15.12, .9, 2.6, 17, 25, 0, 'ley_glass_a', '')
    m.box(6.5, 15, 6.65, 18.5, 0, 2.6, 20)
    m.box(6.5, 20.2, 6.65, 23, 0, 2.6, 20)
    m.fix('exec_desk', 3.6, 18.0, 0, 1, 0)
    m.fix('crt_monitor', 3.4, 18.0, .89, 1, 0, solid=False)
    m.fix('specimen_cabinet', 1.8, 21.8, 0, 1, HALF)
    m.fix('bookcase', 5.4, 22.6, 0, 1, PI)
    m.civilian(SENIOR_SCIENTIST, 4.4, 19.6, -1.9, 'solis_hello')
    m.keys()
    m.dialogue('solis_hello', 'DR. MIRA SOLIS',
               'You got through the crypt? Then the conduit is yours to wake, or it already is. The seal console is by the north door. Once it opens, the heart will know.',
               [('What is in the heart chamber?', 'solis_heart', '', 1, '', ''),
                ('Is Lena Morrow alive?', 'solis_lena', '', 1, '', ''),
                ('Stay here.', '', '', 1, '', '')])
    m.dialogue('solis_heart', 'DR. MIRA SOLIS',
               'A monolith of living crystal in a spring older than the ruins. Something guards it. Kill it, climb to the altar and lock the harmonic. The mountain goes quiet.',
               [('Back.', 'solis_hello', '', 1, '', '')])
    m.dialogue('solis_lena', 'DR. MIRA SOLIS',
               'She went down into the chasm alone. I heard her on the radio for an hour. Then only the singing.',
               [('Back.', 'solis_hello', '', 1, '', '')])
    # The machinery.
    m.fix('mainframe', 20.5, 6.0, 3.2, .95, -HALF)
    m.fix('analyzer', 20.5, 11.5, 3.2, .95, -HALF)
    m.fix('control_board', 16.0, 8.0, 3.2, 1, HALF)
    m.fix('test_chamber', 9.5, 6.0, 0, 1.6)
    m.fix('switch_cabinet_2', 1.9, 6.5, 0, 1, HALF)
    m.fix('switch_cabinet_1', 1.9, 9.0, 0, 1, HALF)
    m.fix('sample_fridge', 17.5, 21.5, 0, 1, PI)
    m.fix('clean_bench', 15.6, 21.6, 0, 1, PI)
    m.fix('scope_monitor', 15.6, 21.6, .81, 1, PI, solid=False)
    m.terminal(8.2, 3.6, 0, 'SANCTUM SEAL CONSOLE', 'INNER SEAL LOCKED. FED BY THE CRYPT CONDUIT.',
               'RELEASE TO OPEN THE HEART CHAMBER.', 'sanctum_seal', 'crypt_conduit', 0)
    m.sign(16.0, 3.05, 1.8, 2.2, .45, 0, 'THE LEYLINE HEART', 'sanctum seal / north door')
    m.lead((1.9, 6.5, 2.6), (9.5, 6.0, 1.0))
    m.lead((9.5, 6.0, 1.0), (12.0, 3.0, 2.6), live=1)
    m.lead((20.5, 6.0, 6.3), (20.5, 16.0, 6.3))
    m.light(12.0, 4.5, 3.0, 1.4, 9, 6500, CYAN, 'sanctum_seal', 1)
    m.light(12.0, 4.5, 3.0, 1.2, 9, 2000, (1, .12, .08), 'sanctum_seal', 1, 1, .2)
    for x, y in ((5, 6), (10, 12), (4, 18.5), (12, 19)):
        m.light(x, y, 3.0, 1.2, 9, 4200)
    for x, y in ((19, 8), (19, 16)):
        m.light(x, y, 6.1, 1.2, 9, 4600)
    m.light(18, 12, 2.9, .9, 7, 3800)
    m.event('ley_seal_open', (0, 0, 24, 24, -1, 7), 'sanctum_seal', 1, actions=[
        ('OBJECTIVE', 'obj_seal', 2), ('OBJECTIVE', 'obj_heart', 1), ('SHAKE', 1.0), ('CHECKPOINT',)])
    m.pickup(SHELLS, 9.5, 18.0); m.pickup(SHELLS, 10.2, 18.0); m.pickup(HEALTH, 9.0, 16.5)
    m.creature(BRUTE, 12.0, 12.0); m.creature(TRANSFORMED, 18.5, 14.0, 3.2)

    # ===================================================================== MAP 3: THE LEYLINE HEART
    m = maps[3]
    m.ceiling, m.ambient, m.surface = 9.0, .2, (9, 9, 5)
    m.spawn, m.haze = (12, 21.5, -HALF), (.12, .4, .46, .018)
    altar = {(x, y) for x in range(6, 18) for y in range(2, 7)}
    m.layers.append(('High altar', 2.8, .3, altar))
    m.zone(1, 1, 23, 23, 'THE LEYLINE HEART', 'THE LEYLINE HEART', -.5, 9)
    m.rec('STAIR', 3, 3, 3, 6, 6, 0, 2.8, 15, 0, 1, 0, .12, 1)
    m.rec('STAIR', 3, 18, 3, 21, 6, 0, 2.8, 15, 0, 0, 0, .12, 1)
    for a in ((7, 9.5, 17, 9.8), (7, 16.2, 17, 16.5), (7, 9.5, 7.3, 16.5), (16.7, 9.5, 17, 16.5)):
        m.box(*a, 0, .5, 33)
    m.rec('WATER', 3, 7.3, 9.8, 16.7, 16.2, -.3, .42)
    m.fix('dais', 12.0, 13.0, 0, .85)
    m.box(11.2, 12.2, 12.8, 13.8, 1.0, 7.6, 35)
    m.box(10.5, 12.6, 11.2, 13.4, 1.0, 4.6, 35)
    m.box(12.8, 12.6, 13.5, 13.4, 1.0, 5.2, 35)
    m.box(11.6, 11.5, 12.4, 12.2, 1.0, 3.6, 35)
    m.fix('fungus_spire', 9.2, 10.8, 0, 1)
    m.rec('EMITTER', 3, 12, 13, 1.2, .03, .03, 4.5, .5, .25, .5, 12)
    for x, y in ((4.5, 10), (19.5, 10), (4.5, 18), (19.5, 18), (4.5, 14), (19.5, 14)):
        m.fix('pillar', x, y, 0, 1, size=(1.0, 1.0, 9.0))
    m.fix('switch_cabinet_1', 3.0, 21.0, 0, 1, HALF)
    m.fix('switch_cabinet_2', 21.0, 21.0, 0, 1, -HALF)
    m.lead((3.0, 21.0, 2.6), (10.8, 13.0, 3.6), live=1)
    m.lead((21.0, 21.0, 2.6), (13.2, 13.0, 3.6), live=1)
    m.fix('gateway', 12.0, 2.8, 2.8, 1.15)
    m.box(11.6, 4.1, 12.4, 4.9, 2.8, 3.6, 33)
    m.fix('anomaly_relic', 12.0, 4.5, 3.6, 1.2, solid=False)
    m.terminal(12.0, 5.4, 2.8, 'LEYLINE STABILIZER // HEART', 'CORE RESONANCE CRITICAL. THE GUARDIAN FEEDS ON IT.',
               'LOCK THE HARMONIC ONCE THE CHAMBER IS QUIET.', 'heart_stabilized', '', 0)
    m.event('ley_heart_quiet', (0, 0, 24, 24, -1, 10), gates=[('@remaining', '=', 0)], where='map', actions=[
        ('MESSAGE', 'THE CHAMBER IS QUIET / LOCK THE HARMONIC ON THE ALTAR', 6)])
    m.event('ley_win', (0, 0, 24, 24, -1, 10), 'heart_stabilized', 1, clear=1, actions=[
        ('OBJECTIVE', 'obj_heart', 2), ('SHAKE', 1.5), ('COMPLETE',)])
    m.light(12, 13, 8.0, 2.8, 18, 7500, CYAN, 'heart_stabilized', 1, 1)
    m.light(12, 13, 8.0, 2.4, 18, 5600, GOLD, 'heart_stabilized', 1)
    m.light(12, 13, 2.2, 2.2, 12, 6500, (.2, 1.0, .65))
    m.light(12, 4.5, 6.2, 2.0, 12, 3400, GOLD)
    for x, y in ((5, 20), (19, 20)):
        m.light(x, y, 5.5, 1.2, 10, 3400)
    m.pickup(HEALTH, 4.5, 20.5); m.pickup(SHELLS, 5.5, 20.5); m.pickup(SHELLS, 18.5, 20.5); m.pickup(NINE, 19.5, 20.5)
    m.creature(CHIMERA, 12.0, 8.0, scale=1.25, health=900)
    m.creature(WARDEN, 6.5, 16.0)
    m.creature(WARDEN, 17.5, 16.0)

    return maps, world


def write(maps, world):
    out = ['META_LEVEL_ID: 0', 'META_LEVEL_NAME: The Leyline Sanctuary', 'META_DEFAULT_TARGET: CUSTOM',
           'META_CAMPAIGN_NAME: THE LEYLINE SANCTUARY', '',
           '# THE LEYLINE SANCTUARY: modern fantasy ruins in the woods. 12 stitched maps:',
           '# a forest of ruins outside, an old stone sanctuary under the mountain inside.',
           '# Generated by tools/campaign/make_leyline_sanctuary.py.', '',
           '--- CUSTOM_CAMPAIGN_DATA_START ---', 'CAMPAIGN|THE LEYLINE SANCTUARY|0', '',
           '# ==== Campaign-wide: terrain, levelled pads, ground paint, horizon woods (world metres) ===='] + world
    for m in maps:
        out += ['', '# ==== Map %d: %s (%d, %d) %s ====' % (m.index, m.name, 24 * m.col, 24 * m.row, 'outdoor' if m.outdoor else 'indoor')]
        sx, sy, yaw = m.spawn
        fields = ['MAP', m.index, 'Leyline / ' + m.name, 24 * m.col, 24 * m.row, sx, sy, 0, int(m.outdoor), m.ceiling, m.ambient,
                  'zone_overcast' if m.outdoor else 'industrial_night', m.open['n'], m.open['s'], m.open['w'], m.open['e'],
                  0, -1, 0, 0, yaw, 0, 0]
        if m.haze:
            r, g, b, density = m.haze
            fields += [r, g, b, density * (.55 if m.outdoor else 1)]   # forest mist thin enough to see the next ruin
        out.append('|'.join(f(v) for v in fields))
        if m.surface: out.append('SURFACE|%d|%d|%d|%d' % (m.index, *m.surface))
        out.append('LAYER|%d|0|%s|0|0' % (m.index, 'Ground' if m.outdoor else 'Floor'))
        for y in range(24):
            out.append('ROW|%d|0|%d|%s' % (m.index, y, ''.join('#' if (x, y) in m.walls else '.' for x in range(24))))
        for k, (name, elevation, thickness, deck) in enumerate(m.layers, 1):
            out.append('LAYER|%d|%d|%s|%s|%s' % (m.index, k, name, f(elevation), f(thickness)))
            for y in range(24):
                out.append('ROW|%d|%d|%d|%s' % (m.index, k, y, ''.join('=' if (x, y) in deck else '.' for x in range(24))))
        out += m.records
    out.append('--- CUSTOM_CAMPAIGN_DATA_END ---')
    return '\n'.join(out) + '\n'


def main():
    maps, world = build()
    text = write(maps, world)
    for path in (os.path.join(ROOT, 'custom maps', 'The Leyline Sanctuary.txt'),
                 r'C:\Users\majwc\Downloads\DepthWorks-v1.2.1\custom maps\The Leyline Sanctuary.txt'):
        if os.path.isdir(os.path.dirname(path)):
            with open(path, 'w', encoding='ascii', newline='\r\n') as fh:
                fh.write(text)
            print('Wrote', path, '(%d lines, %d KB)' % (len(text.splitlines()), len(text) // 1024))


if __name__ == '__main__':
    main()
