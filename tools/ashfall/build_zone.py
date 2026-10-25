"""Generate the Ashfall Exclusion Zone custom campaign.

  python tools/ashfall/build_zone.py            # writes both campaign copies
  python tools/ashfall/build_zone.py --stats    # also prints per-map counts

The zone is a grid of 24 m maps (ZONE_W x ZONE_H) stitched into one outdoor
world. Everything here is authored in WORLD metres; the writer splits each
piece into the map whose square contains it and converts to map-local
coordinates. Heights of fixtures, creatures, lights, hazards and emitters
are relative to the ground (the engine settles them on the terrain); water
and TERRAIN_FLAT heights are absolute.

To extend the zone, add a point of interest in build(): give it a pad,
buildings and props (fixtures by library name, see
src/assets/library/catalog.json), lights, monsters, NPCs and a terminal.
See .claude/skills/ashfall-zone/SKILL.md.
"""
import argparse, json, math, os, random

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CATALOG = json.load(open(os.path.join(ROOT, 'src', 'assets', 'library', 'catalog.json')))
OUTPUTS = [os.path.join(ROOT, 'src', 'assets', 'campaigns', 'RawMetal-Ashfall-Exclusion-Zone.txt'),
           os.path.join(ROOT, 'custom maps', 'RawMetal-Ashfall-Exclusion-Zone.txt')]

ZONE_W, ZONE_H, CHUNK = 5, 4, 24
TERRAIN = dict(seed=7331, base=4.0, amplitude=3.2, scale=34.0, roughness=0.45)
PI = math.pi

# ---------------------------------------------------------------------------
# Terrain: an exact port of World.cpp authoredTerrainHeight / authoredTerrainTop.
M32 = 0xffffffff


def lattice(seed, x, y):
    h = ((seed * 0x9E3779B1) & M32) ^ (((x & M32) * 0x85EBCA77) & M32) ^ (((y & M32) * 0xC2B2AE3D) & M32)
    h ^= h >> 15; h = (h * 0x2C1B3C6D) & M32; h ^= h >> 12; h = (h * 0x297A2D39) & M32; h ^= h >> 15
    return (h & 0xffffff) / float(0xffffff) * 2 - 1


def value_noise(seed, x, y):
    ix, iy = math.floor(x), math.floor(y); fx, fy = x - ix, y - iy
    fade = lambda t: t * t * t * (t * (t * 6 - 15) + 10)
    u, v = fade(fx), fade(fy)
    a, b = lattice(seed, ix, iy), lattice(seed, ix + 1, iy)
    c, d = lattice(seed, ix, iy + 1), lattice(seed, ix + 1, iy + 1)
    return (a * (1 - u) + b * u) * (1 - v) + (c * (1 - u) + d * u) * v


class Terrain:
    def __init__(self, **k):
        self.__dict__.update(k); self.flats = []; self.ground = []

    def raw(self, x, y):
        s, amp, freq, norm = 0.0, 1.0, 1 / max(4.0, self.scale), 0.0
        for o in range(4):
            s += value_noise((self.seed + o * 101) & M32, x * freq, y * freq) * amp
            norm += amp; amp *= self.roughness; freq *= 2.03
        return self.base + self.amplitude * s / norm

    def height(self, x, y):
        h = self.raw(x, y)
        for (x1, y1, x2, y2, fh, blend) in self.flats:
            dx = max(x1 - x, 0, x - x2); dy = max(y1 - y, 0, y - y2); d = math.hypot(dx, dy)
            t = min(max(1 - d / blend, 0), 1) if blend > 0 else (1 if d <= 0 else 0)
            t = t * t * (3 - 2 * t); h += (fh - h) * t
        return h

    def ground(self, x, y):
        """Approximate walking surface: the voxel top plus half a voxel."""
        h = self.height(math.floor(x) + .5, math.floor(y) + .5)
        return math.floor(h + .5) + .5

    def flat(self, x1, y1, x2, y2, height=None, blend=5.0):
        if height is None:
            height = round(sum(self.height(x1 + (x2 - x1) * i / 4, y1 + (y2 - y1) * j / 4) for i in range(5) for j in range(5)) / 25)
        self.flats.append((x1, y1, x2, y2, float(height), blend)); return height

    def paint(self, x1, y1, x2, y2, kind, edge=1.5):
        """GROUND cover: 0 short grass, 1 leafy grass, 2 clover, 3 wild, 4 dry dirt,
        5 forest litter, 6 gravel, 7 mud, 8 meadow (the default noise mix)."""
        self.ground.append((x1, y1, x2, y2, kind, edge))


# ---------------------------------------------------------------------------
class Zone:
    def __init__(self):
        self.t = Terrain(**TERRAIN)
        self.maps = {(cx, cy): dict(fixtures=[], structs=[], lights=[], creatures=[], pickups=[], clutter=[], terminals=[],
                                    signs=[], hazards=[], emitters=[], water=[], zones=[], actors=[], events=[], objectives=[],
                                    sequences=[], hints=[], nav=[])
                     for cy in range(ZONE_H) for cx in range(ZONE_W)}
        self.keep_clear = []  # (x, y, radius): no vegetation
        self.objective_texts = []

    @staticmethod
    def chunk(x, y):
        return (min(ZONE_W - 1, max(0, int(x // CHUNK))), min(ZONE_H - 1, max(0, int(y // CHUNK))))

    def local(self, x, y):
        cx, cy = self.chunk(x, y)
        return self.maps[(cx, cy)], x - cx * CHUNK, y - cy * CHUNK

    def clear(self, x, y, r):
        self.keep_clear.append((x, y, r))

    # --- placement helpers ------------------------------------------------------------
    def model(self, name, x, y, yaw=0.0, scale=1.0, solid=True, core=1.0, base=0.0, sx=None, sy=None, sz=None):
        c = CATALOG[name]; m, lx, ly = self.local(x, y)
        w, d, h = c['width'] * (sx or scale), c['depth'] * (sy or scale), c['height'] * (sz or scale)
        m['fixtures'].append((c['slot'], lx, ly, base, w, d, h, yaw, 1 if solid else 0, core))

    def struct(self, x1, y1, x2, y2, bottom, top, rail=0, material=15):
        cx, cy = self.chunk((x1 + x2) / 2, (y1 + y2) / 2); ox, oy = cx * CHUNK, cy * CHUNK
        self.maps[(cx, cy)]['structs'].append((x1 - ox, y1 - oy, x2 - ox, y2 - oy, bottom, top, rail, material))

    def light(self, x, y, z, intensity=1.5, rng=9.0, kelvin=3000, tint=0.0):
        m, lx, ly = self.local(x, y); m['lights'].append((lx, ly, z, 0, 0, intensity, rng, kelvin, tint))

    def creature(self, kind, x, y):
        m, lx, ly = self.local(x, y); m['creatures'].append((kind, lx, ly, 0))

    def pickup(self, kind, x, y):
        m, lx, ly = self.local(x, y); m['pickups'].append((kind, lx, ly))

    def junk(self, kind, x, y, yaw=0.0):
        m, lx, ly = self.local(x, y); m['clutter'].append((kind, lx, ly, -999, yaw))

    def terminal(self, x, y, title, a, b, state='', require='', yaw=0.0, z=0.0):
        m, lx, ly = self.local(x, y); m['terminals'].append((lx, ly, z, title, a, b, state, require, yaw))

    def sign(self, x, y, z, w, h, yaw, title, sub, rgb=14726704):
        m, lx, ly = self.local(x, y); m['signs'].append((lx, ly, z, w, h, yaw, title, sub, rgb))

    def hazard(self, kind, x, y, r, damage, top=2.2):
        m, lx, ly = self.local(x, y); m['hazards'].append((kind, lx - r, ly - r, lx + r, ly + r, -.2, top, damage))

    def nav_hint(self, x, y, cover=True):
        """NAV_HINT: a Cover (or Path) node guards prefer; nudged off solid props."""
        x, y = self.clear_spot(x, y, 'nav hint')
        m, lx, ly = self.local(x, y); m['nav'].append((lx, ly, 1 if cover else 0))

    def emitter(self, x, y, z=0.3, drift=(.2, .1), rise=2.6, rate=.35, radius=.12, growth=.5, count=8):
        m, lx, ly = self.local(x, y); m['emitters'].append((lx, ly, z, drift[0], drift[1], rise, rate, radius, growth, count))

    def zone_name(self, x1, y1, x2, y2, name):
        for (cx, cy), m in self.maps.items():
            ox, oy = cx * CHUNK, cy * CHUNK
            a, b, c, d = max(x1, ox), max(y1, oy), min(x2, ox + CHUNK), min(y2, oy + CHUNK)
            if a < c and b < d:
                m['zones'].append((a - ox, b - oy, c - ox, d - oy, name))

    def blocked(self, x, y, r=.55):
        """True when a body of radius r at (x, y) touches a solid fixture or structure."""
        m, lx, ly = self.local(x, y)
        for (slot, fx, fy, base, w, d, h, yaw, solid, core) in m['fixtures']:
            if not solid:
                continue
            c, s = math.cos(yaw), math.sin(yaw)
            dx, dy = lx - fx, ly - fy
            # Fixture frame: model X runs along (c, -s), model Z along (s, c).
            u, v = dx * c - dy * s, dx * s + dy * c
            if abs(u) < w * core / 2 + r and abs(v) < d * core / 2 + r:
                return True
        for (x1, y1, x2, y2, *_rest) in m['structs']:
            if x1 - r < lx < x2 + r and y1 - r < ly < y2 + r:
                return True
        return False

    def clear_spot(self, x, y, what):
        """The nearest unblocked point to (x, y), staying in the same map."""
        if not self.blocked(x, y):
            return x, y
        for ring in range(1, 16):
            for k in range(12):
                a = k * PI / 6
                px, py = x + math.cos(a) * ring * .3, y + math.sin(a) * ring * .3
                if self.chunk(px, py) == self.chunk(x, y) and not self.blocked(px, py):
                    print('nudged', what, (x, y), '->', (round(px, 2), round(py, 2)))
                    return px, py
        raise SystemExit('no clear ground for %s at %s' % (what, (x, y)))

    def actor(self, x, y, visual, yaw, mode=0, faction=5, squad='', patrol=(), weapon=None, role=None, health=None, name='', voice=0):
        x, y = self.clear_spot(x, y, name or 'actor')
        patrol = [self.clear_spot(px, py, 'patrol') + (pause,) for (px, py, pause) in patrol]
        if patrol:
            x, y = patrol[0][0], patrol[0][1]
        for (px, py, _) in [(x, y, 0)] + list(patrol):
            self.clear(px, py, .8)
        m, lx, ly = self.local(x, y)
        for (px, py, _) in patrol:
            assert self.chunk(px, py) == self.chunk(x, y), ('patrol leaves its map', name or (x, y), (px, py))
        m['actors'].append(dict(x=lx, y=ly, visual=visual, yaw=yaw, mode=mode, faction=faction, squad=squad,
                                patrol=[(px - (x - lx), py - (y - ly), pause) for (px, py, pause) in patrol],
                                weapon=weapon, role=role, health=health, name=name, voice=voice))

    def event(self, x1, y1, x2, y2, name, actions, require='', value=1):
        m, lx, ly = self.local((x1 + x2) / 2, (y1 + y2) / 2)
        ox, oy = (x1 + x2) / 2 - lx, (y1 + y2) / 2 - ly
        m['events'].append((name, max(0, x1 - ox), max(0, y1 - oy), min(24, x2 - ox), min(24, y2 - oy), require, value, actions))

    def objective(self, map_xy, ident, text):
        m, _, _ = self.local(*map_xy); m['objectives'].append((ident, text))


# ---------------------------------------------------------------------------
def build(z):
    t = z.t
    W, H = ZONE_W * CHUNK, ZONE_H * CHUNK
    # --- The cordon: hills climb to every edge, a concrete fence on the crest. -------
    for (x1, y1, x2, y2) in [(0, 0, W, 2), (0, H - 2, W, H), (0, 0, 2, H), (W - 2, 0, W, H)]:
        t.flat(x1, y1, x2, y2, 11, 14)
    # Textured concrete wall panels, 4 m each, along the crest.
    for i in range(int(W // 4)):
        x = i * 4 + 2
        z.model('ind_wall', x, .45, yaw=0, solid=True, sx=.995)
        z.model('ind_wall', x, H - .45, yaw=PI, solid=True, sx=.995)
    for i in range(int(H // 4)):
        y = i * 4 + 2
        z.model('ind_wall', .45, y, yaw=PI / 2, solid=True, sx=.995)
        z.model('ind_wall', W - .45, y, yaw=-PI / 2, solid=True, sx=.995)
    # --- Roads -----------------------------------------------------------------------------------
    roads = [(6, 79, 42, 84), (40, 50, 45, 84), (40, 50, 76, 55), (71, 18, 76, 55), (44, 12, 76, 17)]
    # Ground cover under the places, before the roads so the tracks cross them.
    for (x1, y1, x2, y2, kind, edge) in [
            (2, 2, 42, 40, 5, 4), (4, 36, 30, 56, 3, 3), (8, 40, 26, 52, 7, 3),  # forest litter, swamp edge, mud
            (4, 77, 24, 94, 3, 2), (16, 56, 37, 76, 6, 1.5), (48, 44, 68, 61, 6, 1.2),  # camp, fuel station, checkpoint
            (78, 52, 97, 73, 4, 2.5), (103, 34, 115, 46, 3, 2), (80, 4, 117, 30, 6, 2),  # scrapyard, tower, industrial
            (46, 2, 66, 12, 4, 2), (50, 22, 66, 38, 4, 4), (80, 76, 117, 93, 3, 3)]:  # mine, anomalies, village
        t.paint(x1, y1, x2, y2, kind, edge)
    for r in roads:
        t.paint(r[0] + .6, r[1] + .6, r[2] - .6, r[3] - .6, 4, 1.2)
    for r in roads:
        t.flat(*r, None, 6)
        for i in range(0, int(max(r[2] - r[0], r[3] - r[1])), 2):
            pass
    for r in roads:
        x1, y1, x2, y2 = r
        z.clear((x1 + x2) / 2, (y1 + y2) / 2, 0)
    road_cells = roads

    def on_road(x, y, pad=1.5):
        return any(x1 - pad <= x <= x2 + pad and y1 - pad <= y <= y2 + pad for (x1, y1, x2, y2) in road_cells)
    z.on_road = on_road
    # Roadside poles every 16 m, some down.
    rnd = random.Random(11)
    for (x1, y1, x2, y2) in roads:
        horizontal = x2 - x1 > y2 - y1
        n = int((x2 - x1 if horizontal else y2 - y1) // 16)
        for i in range(1, n + 1):
            px = x1 + i * 16 if horizontal else x2 + 1.6
            py = y1 - 1.6 if horizontal else y1 + i * 16
            if px > x2 or py > y2 or px < 3 or py < 3 or px > W - 3 or py > H - 3:
                continue
            if rnd.random() < .25:
                z.model('pole_light', px, py, yaw=PI / 2 if horizontal else 0, solid=True, sz=.25, core=1)
            else:
                z.model('pole_light', px, py, yaw=PI / 2 if horizontal else 0, solid=True, core=.3)
    # ================================================================================
    # 1. ROOKIE CAMP (start, south-west): stalkers round a fire in a fenced yard.
    cx0, cy0 = 13, 86
    t.flat(4, 77, 24, 94, None, 6)
    z.clear(cx0, cy0, 8)
    z.model('cottage', 8.2, 89, yaw=0, scale=.55)
    z.model('bus_wreck', 18, 92.3, yaw=0)
    z.hazard(4, cx0 + 1.5, cy0 - 1, .5, 6, top=1.2)  # the camp fire
    z.emitter(cx0 + 1.5, cy0 - 1, .4, (.25, .15), 3.2, .3, .14, .7, 9)
    z.light(cx0 + 1.5, cy0 - 1, .9, 3.2, 12, 1900, .25)
    for a in range(5):
        ang = a / 5 * 2 * PI
        z.model('log', cx0 + 1.5 + math.cos(ang) * 2.2, cy0 - 1 + math.sin(ang) * 2.2, yaw=ang + PI / 2, scale=.55, solid=False)
    # Fence the yard but leave the road open where it leaves the camp eastward.
    for i, (sx, sy) in enumerate([(4.5, 79), (8, 79), (11.5, 79), (21, 79), (23, 86.5), (23, 88.5)]):
        z.model('fence_wood', sx, sy, yaw=0 if sy == 79 else PI / 2, solid=True)
    z.model('ind_sandbags', 15, 79.5, sz=.45)
    z.model('ind_crate', 21.8, 89.3); z.model('ind_crate', 21.8, 90.2, yaw=.3); z.model('ind_barrel', 20.6, 89.6)
    z.model('dumpster', 5.6, 80.6, yaw=0)
    z.terminal(19.5, 84.8, 'CAMP / ELDER', 'WELCOME TO THE ZONE, ROOKIE. THE COURIER NEVER CAME BACK.',
               'FIND HIS PDA AT THE OLD FUEL STATION NORTH OF THE BUS.', 'zone_briefed')
    z.sign(10.5, 79.2, 1.9, 2.6, .55, 0, 'ROOKIE CAMP', 'NO SHOOTING INSIDE THE FENCE')
    for (px, py, yaw) in [(11.5, 83.5, 0.3), (15.5, 83.8, 2.5), (16, 88.6, 3.6), (13.4, 87.6, 5.2)]:
        z.actor(px, py, 0, yaw)  # stalkers at the fire
    z.actor(20, 87, 0, 1.6, mode=2, faction=5, voice=10)  # the camp guide: an escort who follows on request
    z.pickup(0, 7.5, 83.2); z.pickup(1, 8.6, 83.4)
    z.zone_name(4, 77, 24, 94, 'ROOKIE CAMP')
    # ================================================================================
    # 2. FUEL STATION (north of the camp road).
    gx, gy = 26, 64
    t.flat(16, 56, 37, 76, None, 6)
    z.clear(gx + 2, gy + 4, 11)
    z.model('gas_store', 22, 61, yaw=PI, scale=.8)
    # The pack's canopy is the roof slab alone: lift it onto four steel columns.
    z.model('gas_canopy', 30, 70, scale=.75, solid=False, base=4.0)
    for (px, py) in [(23.6, 65.4), (36.4, 65.4), (23.6, 74.6), (36.4, 74.6)]:
        z.struct(px - .18, py - .18, px + .18, py + .18, 0, 4.05, rail=0, material=12)
    for (cx_, cy_) in [(27.5, 70), (32.5, 70)]:
        z.model('fuel_pump', cx_, cy_, yaw=PI / 2)
        z.struct(cx_ - .1, cy_ - .1, cx_ + .1, cy_ + .1, 0, 3.9, rail=0, material=12)
    z.model('gas_sign', 35.5, 59.5, yaw=PI)
    z.model('car_a', 31.8, 64, yaw=1.9, scale=.75)
    z.model('car_c', 20, 72.5, yaw=.2, scale=.75)
    z.model('dumpster', 17.5, 58.5); z.model('tire', 18.6, 60.2, yaw=1.1, solid=False); z.model('tire', 19.1, 59.8, yaw=.2, solid=False)
    z.terminal(28.5, 66.5, 'COURIER / PDA', 'RELAY CODE 4471. THE MILITARY SIT ON THE ROAD EAST.',
               'THE RADIO TOWER ON THE EAST RIDGE CAN STILL TRANSMIT.', 'zone_pda', 'zone_briefed')
    z.creature(4, 23, 67)  # a mutant has the station
    z.creature(0, 35, 74)
    z.pickup(1, 21.5, 66); z.pickup(0, 33, 61)
    for i in range(6):
        z.junk(i % 4, 24 + i * .7, 66 + (i % 2) * .6, i * .7)
    z.zone_name(16, 56, 37, 76, 'FUEL STATION')
    # ================================================================================
    # 3. MILITARY CHECKPOINT on the east road.
    kx, ky = 58, 52.5
    t.flat(48, 44, 68, 61, None, 5)
    z.clear(kx, ky, 9)
    z.model('ind_hut', 64, 59.5, yaw=PI)
    for (bx, by, yaw) in [(53, 50.6, 0), (55.5, 50.6, 0), (60.5, 54.6, 0), (63, 54.6, 0)]:
        z.model('ind_roadblock', bx, by, yaw=yaw)
    for (bx, by, yaw) in [(52, 57.5, 0), (59.5, 58, 0), (50.5, 49.5, PI / 2)]:
        z.model('ind_sandbags', bx, by, yaw=yaw, sz=.5)
    z.model('ind_barrier_wall', 66.5, 50, yaw=0, sy=.6)
    z.model('ind_barrier_wall', 49.6, 56, yaw=0, sy=.6)
    for (lx_, ly_) in [(51, 49), (65, 56)]:
        z.model('lamppost', lx_, ly_, yaw=PI / 2, core=.25)
        z.light(lx_, ly_, 6.5, 4.5, 16, 5200, .05)
    z.model('ind_generator', 67.5, 58.5, yaw=PI / 2)
    z.sign(58, 50.2, 2.6, 3, .6, 0, 'CHECKPOINT 4', 'TURN BACK / LETHAL FORCE AUTHORISED', 13385256)
    patrols = [[(53, 52.5, 3000), (63, 52.5, 3000)], [(57, 56.5, 2500), (51, 56.5, 2500)], [(62, 57, 4000), (66, 56, 3000)], [(58, 49, 5000)]]
    for i, p in enumerate(patrols):
        z.actor(p[0][0], p[0][1], [4, 5, 6, 8][i], 0, mode=3, faction=4, squad='zone_military', patrol=p,
                weapon=(1 if i % 2 == 0 else 2, 650, 1000, 16, 7), role=[1, 2, 3, 5][i], health=110, voice=8)
    for (hx, hy) in [(54.2, 51.7), (61.8, 55.7), (52, 56.3), (59.5, 56.8), (65.6, 50.6), (50.4, 54.2)]:
        z.nav_hint(hx, hy)
    for (hx, hy) in [(52, 52.5), (60, 52.5)]:
        z.nav_hint(hx, hy, cover=False)
    z.terminal(64.5, 57.2, 'CHECKPOINT / LOG', 'ORDERS: NOBODY CROSSES TO THE TOWER. THE RELAY IS LIVE.',
               'CODE 4471 OPENS THE TRANSMITTER. DO NOT LET IT REACH THE STALKERS.', '', '', PI)
    z.pickup(1, 62.5, 60.5); z.pickup(0, 63.5, 60.6)
    z.zone_name(48, 44, 68, 61, 'MILITARY CHECKPOINT')
    # ================================================================================
    # 4. SCRAPYARD (east, off the road).
    sx_, sy_ = 86, 62
    t.flat(78, 52, 97, 73, None, 6)
    z.clear(sx_, sy_, 4)
    z.model('bus_wreck', 82, 58, yaw=1.2)
    for (cxx, cyy, yaw, m) in [(90, 56, .4, 'car_b'), (92, 60, 2.1, 'car_a'), (88, 67, 1.0, 'car_c'), (94, 68, 3.0, 'car_b'), (80, 68, .1, 'car_a')]:
        z.model(m, cxx, cyy, yaw=yaw, scale=.75)
    z.model('ind_cargo_open', 85, 71, yaw=0)
    z.model('ind_cargo_full', 95.5, 63, yaw=PI / 2)
    z.model('dumpster', 79.5, 55); z.model('brick_pile', 90.5, 64); z.model('cable_reel', 84, 63.5, yaw=.6)
    for i in range(10):
        z.model('tire', 86 + (i % 4) * .9, 58.5 + (i // 4) * .9, yaw=i * .9, solid=False)
    z.model('ind_pallets', 93, 54.5, yaw=.4); z.model('ind_barrel', 83, 66); z.model('ind_barrel', 83.6, 66.4)
    for i in range(10):
        z.junk(i % 6, 84 + (i % 5) * 1.3, 61 + (i // 5) * 1.8, i * .5)
    patrols = [[(86, 62, 3000), (91, 58, 3000)], [(82, 64, 2500), (88, 70, 3000)], [(93, 65, 4000)]]
    for i, p in enumerate(patrols):
        z.actor(p[0][0], p[0][1], [9, 7, 4][i], 0, mode=3, faction=4, squad='zone_bandits', patrol=p,
                weapon=(1, 700, 1100, 14, 6), role=[6, 4, 1][i], health=90, voice=9)
    for (hx, hy) in [(89, 57.6), (91.6, 61.6), (87, 65.8), (95.2, 66.4), (81.5, 66.6)]:
        z.nav_hint(hx, hy)
    z.creature(2, 95, 71)
    z.pickup(1, 89, 61.5); z.pickup(1, 81, 63); z.pickup(0, 93, 70)
    z.zone_name(78, 52, 97, 73, 'SCRAPYARD')
    # ================================================================================
    # 5. RADIO TOWER on the east ridge (the objective).
    rx, ry = 109, 40
    t.flat(103, 34, 115, 46, None, 4)
    z.clear(rx, ry, 6)
    for (dx, dy) in [(-1.2, -1.2), (1.2, -1.2), (-1.2, 1.2), (1.2, 1.2)]:
        z.struct(rx + dx - .1, ry + dy - .1, rx + dx + .1, ry + dy + .1, 0, 22, 0, 18)
    for h in range(2, 22, 3):
        for (x1, y1, x2, y2) in [(-1.2, -1.2, 1.2, -1.1), (-1.2, 1.1, 1.2, 1.2), (-1.2, -1.2, -1.1, 1.2), (1.1, -1.2, 1.2, 1.2)]:
            z.struct(rx + x1, ry + y1, rx + x2, ry + y2, h, h + .12, 0, 18)
    z.struct(rx - 1.6, ry - 1.6, rx + 1.6, ry + 1.6, 22, 22.2, 0, 12)
    z.light(rx, ry, 22.6, 6, 24, 1500, .9)
    z.model('ind_hut', rx - 5, ry + 2, yaw=PI / 2)
    z.model('transformer', rx + 4, ry + 3.5, yaw=PI / 2)
    z.model('ind_generator', rx - 4.5, ry - 3.5)
    z.terminal(rx - 2.6, ry + .5, 'RADIO TOWER / TRANSMITTER', 'ENTER RELAY CODE 4471. TRANSMITTING ON ALL BANDS.',
               'MESSAGE SENT: THE ZONE IS GROWING. GET EVERYONE OUT.', 'zone_transmit', 'zone_pda', PI / 2)
    z.creature(3, rx + 6, ry - 5)  # the Warden guards the ridge
    z.pickup(0, rx - 4, ry + 4.5)
    z.zone_name(103, 34, 115, 46, 'RADIO TOWER')
    # ================================================================================
    # 6. INDUSTRIAL ZONE (north-east).
    t.flat(80, 4, 117, 30, None, 6)
    z.clear(98, 17, 14)
    z.model('ind_warehouse', 96, 11, yaw=0, scale=.7)
    z.model('ind_works', 108, 24, yaw=PI / 2, scale=.65)
    for (cxx, cyy) in [(86, 22), (90.5, 22)]:
        z.model('ind_cooling', cxx, cyy, scale=.8)
    z.model('ind_reservoir', 111, 8, scale=.7)
    z.model('ind_silo', 85, 9); z.model('ind_silo', 87, 9)
    z.model('ind_pipe_arch', 98, 22, yaw=PI / 2)
    z.model('ind_cargo_full', 100, 27, yaw=0); z.model('ind_generator', 92, 27.5)
    z.model('transformer', 82.5, 15, yaw=PI / 2)
    for i in range(6):
        z.model('ind_wall', 82 + i * 4, 4.2, yaw=0)
    z.creature(2, 95, 18); z.creature(2, 104, 13); z.creature(0, 88, 26); z.creature(0, 110, 18)
    z.pickup(1, 97, 17); z.pickup(0, 102, 20)
    z.terminal(93, 18, 'PLANT 2 / SHIFT LOG', 'COOLING LOOP FAILED AT 02:40. EVERYONE WENT TO THE MINE.',
               'SOMETHING CAME UP OUT OF THE RESERVOIR.')
    z.zone_name(80, 4, 117, 30, 'INDUSTRIAL ZONE')
    # ================================================================================
    # 7. MINE (north).
    t.flat(46, 2, 66, 12, None, 5)
    z.clear(56, 7, 6)
    z.model('mine_headframe', 52, 6.5)
    z.model('mine_entrance', 59, 5.5, yaw=PI / 2)
    z.model('mine_lift', 52, 9.8)
    z.model('ind_crate', 55, 9.5); z.model('ind_crate', 55.8, 9.7, yaw=.4); z.model('ind_barrel', 63.5, 9)
    z.light(56, 9, 3.2, 3.5, 12, 2600, .15)
    z.model('pole_light', 49, 9.5, core=.3)
    z.creature(4, 61.5, 9.5); z.creature(0, 50, 11)
    z.terminal(56, 10.5, 'MINE / LIFT LOG', 'SHAFT 2 FLOODED. LIFT LOCKED AT SURFACE.', 'NINE MINERS UNACCOUNTED FOR.')
    z.pickup(0, 54, 10.5)
    z.zone_name(46, 2, 66, 12, 'OLD MINE')
    # ================================================================================
    # 8. FOREST LAIR (north-west) and the hunter's camp.
    z.clear(18, 24, 5)
    z.model('dead_tree_a', 18, 22, yaw=.3); z.model('log', 16, 25, yaw=1.0); z.model('log', 20.5, 26, yaw=2.2)
    for i in range(6):
        z.model('mushrooms', 17 + (i % 3) * 1.1, 23.5 + (i // 3) * 1.3, yaw=i, solid=False)
    z.creature(4, 18, 24); z.creature(0, 14, 20); z.creature(0, 22, 21)
    z.pickup(0, 19.5, 24.5)
    t.flat(27, 10, 35, 17, None, 4)
    z.clear(31, 13, 5)
    z.model('ind_shed', 31, 12.5, yaw=.2, scale=.75)
    z.hazard(4, 28.5, 15.5, .35, 6, top=1)
    z.emitter(28.5, 15.5, .3, (.2, .1), 2.4, .3, .1, .5, 7)
    z.light(28.5, 15.5, .9, 2.6, 9, 1900, .25)
    z.terminal(33.5, 15.5, "HUNTER'S CAMP / NOTE", 'BLOOD TRAIL INTO THE PINES. I AM NOT FOLLOWING IT.',
               'THE FOREST GOES QUIET BEFORE THE BIG ONE COMES OUT.')
    z.pickup(1, 30, 15.8)
    z.zone_name(4, 4, 40, 40, 'PINE FOREST')
    # ================================================================================
    # 9. SWAMP (west).
    px_, py_ = 16, 46
    pond = t.flat(9.5, 40.5, 22.5, 51.5, round(TERRAIN['base']) - 2, 7)
    z.water = getattr(z, 'water', []) + [(9.5, 40.5, 22.5, 51.5, pond + .5 - .1, pond + .5 + .55)]
    z.clear(px_, py_, 9)
    for (dx, dy) in [(-9, -6), (9, 5), (-7, 7), (10, -6)]:
        z.model('dead_tree_b', px_ + dx, py_ + dy, yaw=dx * .3, scale=1.2, core=.12)
    for i in range(22):
        a = i / 22 * 2 * PI; r = 8.5 + (i % 3) * .7
        z.model('grass_b' if i % 2 else 'wheat', px_ + math.cos(a) * r, py_ + math.sin(a) * r * .75, yaw=i, solid=False, scale=1.2)
    z.creature(1, px_ - 3, py_); z.creature(1, px_ + 4, py_ + 2); z.creature(1, px_, py_ - 3)
    z.zone_name(4, 36, 30, 56, 'SWAMP')
    # ================================================================================
    # 10. ANOMALY FIELD (centre).
    ax, ay = 58, 30
    z.clear(ax, ay, 9)
    rnd = random.Random(42)
    for i in range(7):
        a = i / 7 * 2 * PI + rnd.random() * .5; r = 2.5 + rnd.random() * 5
        hx, hy = ax + math.cos(a) * r, ay + math.sin(a) * r
        z.hazard(7, hx, hy, 1.1 + rnd.random() * .8, 14)
        z.emitter(hx, hy, .2, (0, 0), 1.6, .25, .2, .9, 6)
    for (dx, dy) in [(0, 0), (5, -4), (-4, 5)]:
        z.pickup(0, ax + dx, ay + dy)  # artifacts
    for (dx, dy) in [(-8, -6), (7, 6), (-6, 8)]:
        z.model('dead_tree_a', ax + dx, ay + dy, yaw=dx, scale=.9, core=.1)
    z.terminal(ax - 9.5, ay + .5, 'SCIENTIST / FIELD NOTES', 'GRAVITY ANOMALIES. THE AIR SHIMMERS ABOVE EACH ONE.',
               'ARTIFACTS SIT AT THE CENTRE. WALK BETWEEN THE VORTICES.')
    z.zone_name(48, 20, 68, 40, 'ANOMALY FIELD')
    # ================================================================================
    # 11. VILLAGE RUINS (south-east): two homes standing, one burned out.
    t.flat(80, 76, 117, 93, None, 6)
    for (hx, hy, yaw) in [(88, 84, 0), (108, 81, PI)]:
        z.clear(hx, hy, 7); z.model('cottage', hx, hy, yaw=yaw, scale=.7)
    bx, by = 99, 88
    z.clear(bx, by, 6)
    for (x1, y1, x2, y2, h) in [(bx - 4, by - 3, bx + 4, by - 2.7, 2.2), (bx - 4, by - 3, bx - 3.7, by + 3, 1.4), (bx + 3.7, by - 3, bx + 4, by + 1, 2.6), (bx - 1, by + 2.7, bx + 4, by + 3, .9)]:
        z.struct(x1, y1, x2, y2, 0, h, 0, 15)
    z.model('brick_pile', bx, by); z.model('brick_pile', bx + 2, by + 1.5, yaw=1); z.model('dead_tree_b', bx + 6, by - 3)
    z.hazard(4, bx - 1.5, by + .5, .4, 5, top=1); z.emitter(bx - 1.5, by + .5, .3, (.2, .1), 3, .28, .12, .6, 8)
    z.light(bx - 1.5, by + .5, .9, 2.4, 9, 1900, .25)
    for i in range(8):
        z.model('fence_wood', 84 + i * 2, 77.5, yaw=0)
    for i in range(14):
        z.model('wheat', 82 + (i % 7) * 2.2, 89 + (i // 7) * 2.4, yaw=i, solid=False, scale=1.1)
    z.creature(4, 104, 89.5); z.creature(0, 92, 91)
    z.actor(96.5, 80, 0, 2.0, mode=1, faction=5)
    z.terminal(94.6, 79, 'VILLAGE / DIARY', 'THE SOLDIERS BURNED THE MIDDLE HOUSE. NOBODY WAS INSIDE, THEY SAID.',
               'WE HEAR SCRATCHING UNDER THE FLOOR AT NIGHT.')
    z.pickup(0, 108, 88); z.pickup(1, 94.8, 82.5)
    z.zone_name(80, 76, 117, 93, 'VILLAGE RUINS')
    # Car wrecks along the roads.
    for (cxx, cyy, yaw, m) in [(37, 86.5, .3, 'car_b'), (42.5, 66, 1.4, 'car_c'), (73.5, 33, 1.7, 'car_a'), (66, 14.5, .1, 'car_b')]:
        z.model(m, cxx, cyy, yaw=yaw, scale=.75)
    z.model('billboard', 46.5, 74, yaw=PI / 2)
    z.model('bus_shelter', 38, 86.5, yaw=PI)
    # --- Quest chain --------------------------------------------------------------------
    z.event(4, 77, 24, 94, 'zone_arrival', [('OBJECTIVE', 'zone_obj_elder', 1)])
    z.event(4, 77, 24, 94, 'zone_briefed_step', [('OBJECTIVE', 'zone_obj_elder', 2), ('OBJECTIVE', 'zone_obj_pda', 1)], 'zone_briefed')
    z.event(16, 56, 37, 76, 'zone_pda_step', [('OBJECTIVE', 'zone_obj_pda', 2), ('OBJECTIVE', 'zone_obj_tower', 1)], 'zone_pda')
    z.event(103, 34, 115, 46, 'zone_transmit_step', [('OBJECTIVE', 'zone_obj_tower', 2), ('OBJECTIVE', 'zone_obj_out', 1), ('STATE', 'zone_complete', 1)], 'zone_transmit')
    z.objective((10, 85), 'zone_obj_elder', 'Talk to the camp elder by the fire')
    z.objective((10, 85), 'zone_obj_pda', "Find the courier's PDA at the fuel station")
    z.objective((10, 85), 'zone_obj_tower', 'Reach the radio tower on the east ridge and transmit')
    z.objective((10, 85), 'zone_obj_out', 'Transmission sent. Survive the Zone')
    scatter(z)


# ---------------------------------------------------------------------------
def biome(x, y):
    """0 open steppe, 1 woodland edge, 2 pine forest."""
    if x < 40 and y < 40:
        return 2
    if (x > 100 and y > 45 and y < 75) or (y < 40 and x < 75):
        return 1
    return 0


def scatter(z):
    rnd = random.Random(2026)
    t = z.t
    taken = []

    def free(x, y, r):
        if x < 3 or y < 3 or x > ZONE_W * CHUNK - 3 or y > ZONE_H * CHUNK - 3:
            return False
        if z.on_road(x, y):
            return False
        for (cx, cy, cr) in z.keep_clear:
            if math.hypot(x - cx, y - cy) < cr + r:
                return False
        for (fx1, fy1, fx2, fy2, _, _) in t.flats:
            if fx1 + 2 < x < fx2 - 2 and fy1 + 2 < y < fy2 - 2 and not (fx1 < 3 or fy1 < 3 or fx2 > ZONE_W * CHUNK - 3 or fy2 > ZONE_H * CHUNK - 3):
                return False
        for (tx, ty, tr) in taken:
            if math.hypot(x - tx, y - ty) < r + tr:
                return False
        return True
    def pick(table):
        r = rnd.random() * sum(w for _, w in table)
        for name, w in table:
            r -= w
            if r <= 0:
                return name
        return table[-1][0]
    TREES = {2: [('pine_a', 4), ('pine_b', 4), ('aspen_b', 1), ('dead_tree_a', .6)],
             1: [('leafy_a', 3), ('leafy_b', 3), ('oak_a', 2), ('oak_b', 2), ('oak_c', 1), ('aspen_a', 1.5), ('aspen_b', 1.5), ('pine_b', 1), ('dead_tree_b', .6)],
             0: [('leafy_a', 2), ('leafy_b', 2), ('aspen_b', 1), ('oak_c', 1), ('dead_tree_b', 1.5), ('dead_tree_a', 1), ('sapling', 1)]}
    # Patches: each cluster is one plant mix, a few metres across.
    PATCHES = {2: [([('bracken', 4), ('fern_a', 3), ('fern_big', 1), ('grass_l', 2)], 3),
                   ([('grass_l', 3), ('grass_bush', 2), ('mushrooms', .5), ('bracken', 1)], 2),
                   ([('bush_a', 2), ('bush_c', 2), ('fern_a', 2)], 1)],
               1: [([('tall_grass_l', 3), ('tall_grass_m', 3), ('grass_l', 2), ('aster', 1)], 3),
                   ([('bush_a', 2), ('bush_b', 2), ('bush_c', 2), ('bush_d', 1), ('sumac', 2)], 2),
                   ([('goldenrod', 3), ('tall_grass_m', 2), ('dogbane', 1)], 1.5),
                   ([('clover_patch', 2), ('grass_patch', 2), ('grass_bush', 2)], 1)],
               0: [([('tall_grass_l', 4), ('tall_grass_m', 4), ('grass_l', 2), ('grass_a', 1), ('grass_b', 1)], 4),
                   ([('goldenrod', 4), ('aster', 2), ('tall_grass_m', 2), ('dogbane', 1)], 2),
                   ([('sumac', 3), ('bush_b', 2), ('bush_d', 2), ('tall_grass_l', 2)], 1.2),
                   ([('clover_patch', 2), ('grass_patch', 3), ('grass_bush', 2), ('wheat', 1)], 1.2)]}
    for (cx, cy) in z.maps:
        ox, oy = cx * CHUNK, cy * CHUNK
        b = biome(ox + 12, oy + 12)
        trees = {2: 30, 1: 14, 0: 4}[b]
        for _ in range(trees * 4):
            if trees <= 0:
                break
            x, y = ox + rnd.uniform(.5, 23.5), oy + rnd.uniform(.5, 23.5)
            if not free(x, y, 1.6):
                continue
            z.model(pick(TREES[b]), x, y, yaw=rnd.uniform(0, 2 * PI), scale=rnd.uniform(.75, 1.15), core=.1)
            taken.append((x, y, 1.6)); trees -= 1
        # Plant patches: dense clusters of one mix, scattered tufts between.
        for _ in range({2: 9, 1: 11, 0: 13}[b]):
            px, py = ox + rnd.uniform(1, 23), oy + rnd.uniform(1, 23)
            mix = pick([(m, w) for m, w in PATCHES[b]])
            radius = rnd.uniform(1.6, 3.4)
            for _ in range(int(radius * radius * 2.2)):
                ang, d = rnd.uniform(0, 2 * PI), radius * math.sqrt(rnd.random())
                x, y = px + math.cos(ang) * d, py + math.sin(ang) * d
                if not free(x, y, .15):
                    continue
                z.model(pick(mix), x, y, yaw=rnd.uniform(0, 2 * PI), scale=rnd.uniform(.75, 1.3), solid=False)
        for _ in range(30):
            x, y = ox + rnd.uniform(.3, 23.7), oy + rnd.uniform(.3, 23.7)
            if free(x, y, .15):
                z.model(pick([('grass_a', 2), ('grass_b', 2), ('grass_c', 2), ('grass_l', 2), ('tall_grass_m', 1)]), x, y,
                        yaw=rnd.uniform(0, 2 * PI), scale=rnd.uniform(.8, 1.3), solid=False)
        # Rocks, logs and fallen trees.
        for _ in range(3 if b else 2):
            x, y = ox + rnd.uniform(2, 22), oy + rnd.uniform(2, 22)
            if free(x, y, 2):
                name = pick([('rock_mossy', 2), ('rock_big', 1), ('rock_mid', 1), ('log', 1), ('stump', 1)] + ([('fallen_pine', 1.5)] if b == 2 else []))
                z.model(name, x, y, yaw=rnd.uniform(0, 2 * PI), scale=rnd.uniform(.7, 1.0),
                        solid=name not in ('rock_small',))
                taken.append((x, y, 2))


# ---------------------------------------------------------------------------
def fmt(v):
    if isinstance(v, float):
        s = ('%.3f' % v).rstrip('0').rstrip('.')
        return s if s not in ('-0', '') else '0'
    return str(v)


def enc(s):
    return s.replace('%', '%25').replace('|', '%7C')


def write(z, stats=False):
    t = z.t
    lines = ['META_LEVEL_ID: 1', 'META_LEVEL_NAME: ASHFALL EXCLUSION ZONE', 'META_DEFAULT_TARGET: CUSTOM',
             'META_CAMPAIGN_NAME: ASHFALL EXCLUSION ZONE', 'META_EDITOR_PROJECT: ASHFALL EXCLUSION ZONE',
             'META_EDITOR_PLAN_AREA: Ashfall / Zone', '',
             '# Generated by tools/ashfall/build_zone.py. Edit the generator, not this file.', '',
             '--- CUSTOM_CAMPAIGN_DATA_START ---']
    order = [(cx, cy) for cy in range(ZONE_H) for cx in range(ZONE_W)]
    index = {k: i for i, k in enumerate(order)}
    start = index[Zone.chunk(13, 86)]
    lines.append('CAMPAIGN|ASHFALL EXCLUSION ZONE|%d' % start)
    lines.append('TERRAIN|%d|%s|%s|%s|%s' % (t.seed, fmt(t.base), fmt(t.amplitude), fmt(t.scale), fmt(t.roughness)))
    for f in t.flats:
        lines.append('TERRAIN_FLAT|' + '|'.join(fmt(float(v)) for v in f))
    for (x1, y1, x2, y2, kind, edge) in t.ground:
        lines.append('GROUND|%s|%s|%s|%s|%d|%s' % (fmt(float(x1)), fmt(float(y1)), fmt(float(x2)), fmt(float(y2)), kind, fmt(float(edge))))
    names = {}
    for (x1, y1, x2, y2, name) in [(4, 77, 24, 94, 'Rookie Camp')]:
        pass
    for k in order:
        i = index[k]; cx, cy = k; m = z.maps[k]
        spawn = (9.0 if k == Zone.chunk(13, 86) else 12.0, 9.0 if k == Zone.chunk(13, 86) else 12.0)
        if k == Zone.chunk(13, 86):
            spawn = (17.5 - cx * CHUNK, 82.2 - cy * CHUNK)
        lines.append('MAP|%d|Zone %d-%d|%d|%d|%s|%s|0|1|2.8|0.3|zone_overcast|%d|%d|%d|%d|0|-1|0|0|%s' % (
            i, cx, cy, cx * CHUNK, cy * CHUNK, fmt(spawn[0]), fmt(spawn[1]),
            1 if cy > 0 else 0, 1 if cy < ZONE_H - 1 else 0, 1 if cx > 0 else 0, 1 if cx < ZONE_W - 1 else 0,
            fmt(-PI / 2 if k == Zone.chunk(13, 86) else 0.0)))
        lines.append('LAYER|%d|0|Zone ground|0|0' % i)
        for r in range(24):
            lines.append('ROW|%d|0|%d|%s' % (i, r, '.' * 24))
    for k in order:
        i = index[k]; m = z.maps[k]
        ox, oy = k[0] * CHUNK, k[1] * CHUNK
        for f in m['fixtures']:
            slot, lx, ly, base, w, d, h, yaw, solid, core = f
            lines.append('FIXTURE|%d|%d|%s|%s|%s|%s|%s|%s|%s|%d|%s' % (i, slot, fmt(lx), fmt(ly), fmt(float(base)), fmt(w), fmt(d), fmt(h), fmt(yaw), solid, fmt(float(core))))
        for s in m['structs']:
            x1, y1, x2, y2, b, tp, rail, mat = s
            lines.append('STRUCT|%d|%s|%s|%s|%s|%s|%s|%d|%d' % (i, fmt(x1), fmt(y1), fmt(x2), fmt(y2), fmt(float(b)), fmt(float(tp)), rail, mat))
        for (lx, ly, zz, mount, yaw, inten, rng, kel, tint) in m['lights']:
            lines.append('LIGHT|%d|%s|%s|%s|%d|%s|%s|%s|%d|%s' % (i, fmt(lx), fmt(ly), fmt(float(zz)), mount, fmt(float(yaw)), fmt(float(inten)), fmt(float(rng)), kel, fmt(float(tint))))
        for (kind, lx, ly, zz) in m['creatures']:
            lines.append('CREATURE|%d|%d|%s|%s|0' % (i, kind, fmt(lx), fmt(ly)))
        for (kind, lx, ly) in m['pickups']:
            lines.append('PICKUP|%d|%d|%s|%s' % (i, kind, fmt(lx), fmt(ly)))
        for (kind, lx, ly, zz, yaw) in m['clutter']:
            lines.append('CLUTTER|%d|%d|%s|%s|-999|%s' % (i, kind, fmt(lx), fmt(ly), fmt(float(yaw))))
        for (lx, ly, zz, title, a, b, state, req, yaw) in m['terminals']:
            lines.append('TERMINAL|%d|%s|%s|%s|0|%s|%s|%s|%s|0|%s|%s' % (i, fmt(lx), fmt(ly), fmt(float(zz)), enc(title), enc(a), enc(b), state, req, fmt(float(yaw))))
        for (lx, ly, zz, w, h, yaw, title, sub, rgb) in m['signs']:
            lines.append('SIGN|%d|%s|%s|%s|%s|%s|%s|%s|%s|%d' % (i, fmt(lx), fmt(ly), fmt(float(zz)), fmt(float(w)), fmt(float(h)), fmt(float(yaw)), enc(title), enc(sub), rgb))
        for (kind, x1, y1, x2, y2, b, tp, dmg) in m['hazards']:
            lines.append('HAZARD|%d|%d|%s|%s|%s|%s|%s|%s|%s' % (i, kind, fmt(x1), fmt(y1), fmt(x2), fmt(y2), fmt(b), fmt(float(tp)), fmt(float(dmg))))
        for (hx, hy, kind) in m['nav']:
            lines.append('NAV_HINT|%d|%s|%s|0|%d' % (i, fmt(float(hx)), fmt(float(hy)), kind))
        for e in m['emitters']:
            lines.append('EMITTER|%d|' % i + '|'.join(fmt(float(v)) if not isinstance(v, int) else str(v) for v in e))
        for (x1, y1, x2, y2, name) in m['zones']:
            lines.append('ZONE|%d|%s|%s|%s|%s|%s' % (i, fmt(float(x1)), fmt(float(y1)), fmt(float(x2)), fmt(float(y2)), enc(name.upper())))
        # NPCs: one looping clock per map drives every actor's keys.
        if m['actors']:
            clock = 'zone_clock_%d' % i
            lines.append('TIMED_SEQUENCE|%d|%s|%s_done|0|0|24|24|-100|100|60000|60000|0|0|0|0|1|1' % (i, clock, clock))
            for a_i, a in enumerate(m['actors']):
                lines.append('ACTOR|%d|%s|%d|1.6|0|0|%d' % (i, clock, a['visual'], a_i))
                if a['mode']:
                    lines.append('ACTOR_AI|%d|%d|%d||%s|2|6' % (i, a_i, a['mode'], fmt(1.45 if a['mode'] == 3 else 1.2)))
                    lines.append('ACTOR_FACTION|%d|%d|%d|%s' % (i, a_i, a['faction'], a['squad'] if a['mode'] == 3 else ''))
                if a['mode'] == 3:
                    lines.append('ACTOR_GUARD|%d|%d|18|0.35|700|6000|%s_alert' % (i, a_i, a['squad']))
                    for (px, py, pause) in a['patrol']:
                        lines.append('ACTOR_PATROL|%d|%d|%s|%s|0|%d' % (i, a_i, fmt(float(px)), fmt(float(py)), pause))
                    w = a['weapon']
                    lines.append('ACTOR_WEAPON|%d|%d|%d|%d|%d|%d|%d' % (i, a_i, *w))
                    lines.append('ACTOR_TACTICS|%d|%d|0|1' % (i, a_i))
                    lines.append('ACTOR_ROLE|%d|%d|%d' % (i, a_i, a['role']))
                    lines.append('ACTOR_HEALTH|%d|%d|%d|%s_dmg_%d_%d|%s_dead_%d_%d' % (i, a_i, a['health'], a['squad'], i, a_i, a['squad'], i, a_i))
                if a['voice']:
                    lines.append('ACTOR_VOICE|%d|%d|%d' % (i, a_i, a['voice']))
                lines.append('ACTOR_KEY|%d|%d|0|%s|%s|0|%s|0|0' % (i, a_i, fmt(a['x']), fmt(a['y']), fmt(float(a['yaw']))))
                lines.append('ACTOR_KEY|%d|%d|60000|%s|%s|0|%s|0|1' % (i, a_i, fmt(a['x']), fmt(a['y']), fmt(float(a['yaw']))))
        for (name, x1, y1, x2, y2, req, val, actions) in m['events']:
            lines.append('SCRIPT_EVENT|%d|%s|%s|%s|%s|%s|-100|100|%s|%d|0' % (i, name, fmt(float(x1)), fmt(float(y1)), fmt(float(x2)), fmt(float(y2)), req, val))
            for (kind, ident, value) in actions:
                lines.append('SCRIPT_ACTION|%d|%s|%s|%s|%d' % (i, name, kind, ident, value))
        for (ident, text) in m['objectives']:
            lines.append('OBJECTIVE_TEXT|%d|%s|%s' % (i, ident, enc(text)))
    # Water (absolute heights), assigned to the map that holds each pond.
    for (x1, y1, x2, y2, bed, surface) in getattr(z, 'water', []):
        k = Zone.chunk((x1 + x2) / 2, (y1 + y2) / 2); i = index[k]; ox, oy = k[0] * CHUNK, k[1] * CHUNK
        lines.append('WATER|%d|%s|%s|%s|%s|%s|%s' % (i, fmt(float(max(0, x1 - ox))), fmt(float(max(0, y1 - oy))), fmt(float(min(24, x2 - ox))), fmt(float(min(24, y2 - oy))), fmt(float(bed)), fmt(float(surface))))
    lines.append('--- CUSTOM_CAMPAIGN_DATA_END ---')
    text = '\n'.join(lines) + '\n'
    for p in OUTPUTS:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, 'w', newline='\n', encoding='utf-8') as f:
            f.write(text)
    if stats:
        for k in order:
            m = z.maps[k]
            print('map %2d %s fixtures %4d structs %3d creatures %d actors %d' % (index[k], k, len(m['fixtures']), len(m['structs']), len(m['creatures']), len(m['actors'])))
        print('flats', len(t.flats), 'bytes', len(text))


if __name__ == '__main__':
    ap = argparse.ArgumentParser(); ap.add_argument('--stats', action='store_true'); a = ap.parse_args()
    z = Zone(); build(z); write(z, a.stats)
