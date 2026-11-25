"""Level-design kits: ready-made pieces an artist drops into a map in the Campaign Builder.

Every kit is a PREFAB (map format 2, docs/Systems/map-format.md): records in metres from the kit's
middle, with '@' in the map field. The Builder places one as a PREFAB_PLACE (one Ctrl+Z, R turns it a
quarter, "Take apart" makes its pieces loose to edit), and the Starter Map's Kits Gallery sets every one
down (make_starter_map.py kits_gallery).

  python tools/campaign/kits.py        writes tools/Level Editor/editor/NativeKits.inc

The kits are built from library models (src/assets/library/catalog.json, by name), STRUCT boxes,
PRIM shapes, lamps and the odd emitter. The front of a kit is its south side (+y): the doorway of a
room, the open mouth of a corridor end. Imported models face south at yaw 0.
"""
import json, math, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CATALOG = os.path.join(ROOT, 'src', 'assets', 'library', 'catalog.json')
OUT = os.path.join(ROOT, 'tools', 'Level Editor', 'editor', 'NativeKits.inc')
PI, HALF = math.pi, math.pi / 2
T, H = .2, 3.0                     # wall thickness and height
# STRUCT materials (GUIDE.html "STRUCT materials").
CONCRETE, STEEL, FINISH, DARK_STEEL, BRICK, TREAD, TIMBER, HAZARD = 5, 2, 3, 8, 10, 11, 12, 13
OFFICE_WALL, CARPET, OFFICE_TILE, CLINIC_TILE, LAB_PANEL, TEAL_DADO = 21, 23, 24, 27, 28, 36
GRATING, BUNKER, FLOOR_TILE, PLANK, CAST, WEATHERED = 7, 15, 50, 51, 42, 43
# Models whose front is not their south side at yaw 0 (a turn added to every placement;
# tools/dev/model_fronts.json): the engine's office chair faces north.
FRONT = {'office_chair': PI}
# The engine's own models (slots 0-62) carry no catalogue size: their meshes' own metres, measured
# from the pack files they were made from (src/assets/office/OFFICE-SOURCE.md, assets/facility).
ENGINE = {'desk': (2.205, 1.092, .891), 'office_chair': (.94, .862, 1.096), 'crt_monitor': (.602, .533, .567),
          'keyboard': (.683, .225, .041), 'desk_phone': (.333, .333, .089), 'file_cabinet_tall': (.485, .525, 1.409),
          'water_cooler': (.378, .449, 1.396), 'pallet': (1.44, 1.31, .2), 'drum': (.749, .749, 1.08),
          'test_machine': (2.66, 3.4, 3.52)}


def num(v):
    s = ('%.3f' % v).rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


class Kit:
    def __init__(self, key, title, w, d, catalog, note=''):
        self.key, self.title, self.w, self.d, self.note = key, title, w, d, note
        self.cat = catalog
        self.lines = []           # records with '$name' standing for a model's slot

    # -- pieces -------------------------------------------------------------------------------
    def size(self, name):
        if name in ENGINE:
            return ENGINE[name]
        c = self.cat[name]
        return c['width'], c['depth'], c['height']

    def fx(self, name, x, y, z=0.0, yaw=0.0, k=1.0, solid=1):
        w, d, h = self.size(name)
        yaw = math.remainder(yaw + FRONT.get(name, 0.0), 2 * PI)
        self.lines.append('FIXTURE|@|$%s|%s|%s|%s|%s|%s|%s|%s|%d' % (name, num(x), num(y), num(z), num(max(w * k, .01)),
                                                                  num(max(d * k, .01)), num(max(h * k, .01)), num(yaw), solid))

    def wall_fx(self, name, side, along, z=0.0, k=1.0, solid=1, gap=.02, inset=None):
        """A model backed onto the inside of a wall: side N/S/W/E, `along` the wall from the middle."""
        w, d, h = self.size(name)
        off = (inset if inset is not None else T) + d * k / 2 + gap
        if side == 'N': self.fx(name, along, -self.d / 2 + off, z, 0.0, k, solid)
        if side == 'S': self.fx(name, along, self.d / 2 - off, z, PI, k, solid)
        if side == 'W': self.fx(name, -self.w / 2 + off, along, z, -HALF, k, solid)
        if side == 'E': self.fx(name, self.w / 2 - off, along, z, HALF, k, solid)

    def box(self, x1, y1, x2, y2, bottom, top, material, rail=0):
        self.lines.append('STRUCT|@|%s|%s|%s|%s|%s|%s|%d|%d' % (num(min(x1, x2)), num(min(y1, y2)), num(max(x1, x2)),
                                                              num(max(y1, y2)), num(bottom), num(top), rail, material))

    def prim(self, kind, x, y, z, w, d, h, facing, material):
        self.lines.append('PRIM|@|%s|%s|%s|%s|%s|%s|%s|%d|%d' % (kind, num(x), num(y), num(z), num(w), num(d), num(h), facing, material))

    def lamp(self, x, y, z, intensity=1.0, rng=6.0, kelvin=3400, mount=0, yaw=0.0):
        self.lines.append('LIGHT|@|%s|%s|%s|%d|%s|%s|%s|%d' % (num(x), num(y), num(z), mount, num(yaw), num(intensity), num(rng), kelvin))

    def fx_sized(self, name, x, y, z, w, d, h, yaw=0.0, solid=1):
        self.lines.append('FIXTURE|@|$%s|%s|%s|%s|%s|%s|%s|%s|%d' % (name, num(x), num(y), num(z), num(w), num(d), num(h), num(yaw), solid))

    def door(self, left, right, y, state='', value=0, follow=False, label='', leaf=0, swinging=False):
        """A lifting DOOR along X in a wall from y to y+1 (fields 2-25; Door.h). follow: open exactly while its state
        holds value, after every other follower on that state has shut (World.h Door::follow)."""
        f = ['DOOR', '@', num(left), num(right), num(y), '0', '0', '0', '1' if swinging else '0', '0', '-1', state, str(value) if state else '',
             '', '', '', '', '', str(leaf), label, '', '', '', '', '', '1' if follow else '']
        while f[-1] == '':
            f.pop()
        self.lines.append('|'.join(f))

    def terminal(self, x, y, z, title, line1, line2, state, toggle=True, yaw=0.0):
        self.lines.append('TERMINAL|@|%s|%s|%s|1|%s|%s|%s|%s|%d||%s' % (num(x), num(y), num(z), title, line1, line2, state, int(toggle), num(yaw)))

    def pipe(self, x1, y1, x2, y2, z, radius, end_z, material):
        self.lines.append('PIPE|@|%s|%s|%s|%s|%s|%s|%s|%d' % (num(x1), num(y1), num(x2), num(y2), num(z), num(radius), num(end_z), material))

    def loose(self, name, x, y, z, yaw=0.0, pitch=0.0, roll=0.0, k=1.0):
        """A loose prop (CLUTTER kind 7, the library model): it can lie tilted and be knocked about."""
        w, d, h = self.size(name)
        self.lines.append('CLUTTER|@|7|%s|%s|%s|%s|$%s|%s|%s|%s|%s|%s|' % (num(x), num(y), num(z), num(yaw), name, num(w * k),
                                                                       num(max(d * k, .01)), num(h * k), num(pitch), num(roll)))

    def emitter(self, x, y, z, fields):
        self.lines.append('EMITTER|@|%s|%s|%s|%s' % (num(x), num(y), num(z), '|'.join(num(v) if isinstance(v, float) else str(v) for v in fields)))

    # -- shells -------------------------------------------------------------------------------
    def floor(self, material, x1=None, y1=None, x2=None, y2=None):
        x1 = -self.w / 2 if x1 is None else x1
        y1 = -self.d / 2 if y1 is None else y1
        x2 = self.w / 2 if x2 is None else x2
        y2 = self.d / 2 if y2 is None else y2
        self.box(x1, y1, x2, y2, 0, .04, material)

    def walls(self, material, door=1.2, sides='NSWE', door_side='S', h=H):
        """The kit's outer walls, a doorway `door` wide in the middle of door_side (0: none)."""
        W, D = self.w / 2, self.d / 2
        # A side wall stops at the north and south walls, or runs to the corner where there is none.
        top, bottom = (-D + T if 'N' in sides else -D), (D - T if 'S' in sides else D)
        spans = {'N': (-W, -D, W, -D + T), 'S': (-W, D - T, W, D), 'W': (-W, top, -W + T, bottom), 'E': (W - T, top, W, bottom)}
        for side in sides:
            x1, y1, x2, y2 = spans[side]
            if side == door_side and door:
                if side in 'NS':
                    self.box(x1, y1, -door / 2, y2, 0, h, material)
                    self.box(door / 2, y1, x2, y2, 0, h, material)
                    self.box(-door / 2, y1, door / 2, y2, 2.2, h, material)
                else:
                    self.box(x1, y1, x2, -door / 2, 0, h, material)
                    self.box(x1, door / 2, x2, y2, 0, h, material)
                    self.box(x1, -door / 2, x2, door / 2, 2.2, h, material)
            else:
                self.box(x1, y1, x2, y2, 0, h, material)

    def ceiling_lamp(self, model, x, y, kelvin=4000, rng=6.0, yaw=0.0, intensity=1.0, top=H, flicker=None):
        """A ceiling lamp that IS its fitting: one LIGHT carrying the model as its fitting (fields 18-21), so the
        engine draws the fitting flush under whatever is above it and its glow dims, flickers and switches with
        the light (its state, period, flicker), never a separate model that stays lit while the room goes dark."""
        w, d, h = self.size(model)
        # Blank between kelvin and flicker: tint, state, value, invert, period, on time and phase (fields 10-16).
        self.lines.append('LIGHT|@|%s|%s|%s|0|%s|%s|%s|%d||||||||%s|$%s|%s|%s|%s' % (
            num(x), num(y), num(top - h - .1), num(yaw), num(intensity), num(rng), kelvin,
            '' if flicker is None else num(flicker), model, num(w), num(d), num(h)))


def build(catalog):
    kits = []

    def kit(key, title, w, d, note=''):
        k = Kit(key, title, w, d, catalog, note)
        kits.append(k)
        return k

    # ---------------------------------------------------------------- rooms (6 x 5 m unless noted)
    k = kit('kit_office', 'Office', 6, 5, 'two desks, files, a cooler')
    k.floor(CARPET); k.walls(OFFICE_WALL)
    for x in (-1.45, 1.45):
        k.fx('desk', x, -.55)
        k.fx('office_chair', x, .35, yaw=PI)
        k.fx('crt_monitor', x + .25, -.75, .89, 0)   # screens face whoever sits at the desk (south)
        k.fx('keyboard', x + .25, -.3, .89, 0)
    k.fx('desk_phone', -2.2, -.6, .89, 0)
    k.fx('fur_books', 2.1, -.85, .89)
    k.wall_fx('file_cabinet_tall', 'N', -2.4)
    k.wall_fx('file_cabinet_tall', 'N', -1.85)
    k.wall_fx('water_cooler', 'E', 1.6)
    k.wall_fx('office_bookcase', 'W', .9, k=.8)
    k.fx('dress_plant', 2.45, 1.95, k=.8)
    for x in (-1.45, 1.45):
        k.ceiling_lamp('lamp_ceiling_tube', x, .2, 4200, yaw=0)

    k = kit('kit_ward', 'Ward room', 6, 5, 'two beds, monitors, supplies')
    k.floor(FLOOR_TILE); k.walls(TEAL_DADO)
    for x in (-1.5, 1.5):
        k.fx('med_ward_bed', x, -1.25, yaw=HALF)
        k.fx('fur_bedside_table', x + 1.0, -1.95)
        k.fx('med_vitals_monitor', x + 1.0, -1.95, .64, 0)
    k.wall_fx('med_supply_cabinet', 'W', 1.2)
    k.fx('med_wheelchair', 1.9, 1.5, yaw=-HALF * .5)
    k.fx('med_first_aid_kit', -2.55, 1.2, 1.7)
    for x in (-1.5, 1.5):
        k.ceiling_lamp('clinic_ceiling_lamp_1', x, .3, 4800)

    k = kit('kit_lab', 'Lab', 6, 5, 'benches, a microscope, specimen store')
    k.floor(OFFICE_TILE); k.walls(LAB_PANEL)
    k.wall_fx('lab_clean_bench', 'N', -1.6)
    k.wall_fx('lab_clean_bench', 'N', -.35)
    k.fx('lab_microscope', -1.6, -1.95, .81, 0)
    k.fx('lab_scope_monitor', -.35, -1.95, .81, 0)
    k.wall_fx('lab_specimen_cabinet', 'E', -.9)
    k.wall_fx('lab_sample_fridge', 'N', 1.45)
    k.wall_fx('lab_monitoring_screen', 'W', -.6, 1.4, solid=0)
    k.fx('test_machine', -1.2, 1.0, k=.5)
    k.fx('med_syringe', -.25, -2.0, .81)
    k.fx('office_chair', -1.0, -1.3, yaw=PI)
    for x in (-1.4, 1.4):
        k.ceiling_lamp('lamp_ceiling_tube', x, 0, 5200)

    k = kit('kit_bathroom', 'Bathroom', 5, 4, 'two toilets, two sinks, a mirror')
    k.floor(FLOOR_TILE); k.walls(CLINIC_TILE)
    for x in (-1.5, -.3):
        k.wall_fx('home_toilet', 'N', x)
    k.box(-.9, -1.8, -.86, -.4, 0, 1.9, STEEL)                    # a stall partition between the toilets
    k.fx('home_toilet_roll', -2.25, -1.55, .7, -HALF)
    for x in (1.0, 1.8):
        k.wall_fx('home_sink', 'E', x - 1.4, .45)
    k.wall_fx('med_washroom_mirror', 'E', 0.0, 1.25, solid=0, k=.9)
    k.wall_fx('home_hand_dryer', 'W', 1.0, 1.1, solid=0)
    k.wall_fx('home_soap_dispenser', 'E', -1.1, 1.0, solid=0)
    k.ceiling_lamp('med_fluoro_light', 0, 0, 5600, yaw=HALF)

    k = kit('kit_storage', 'Storage room', 5, 4, 'shelves, crates, barrels')
    k.floor(CONCRETE); k.walls(CONCRETE)
    k.wall_fx('store_shelf_metal', 'N', -1.2)
    k.wall_fx('store_shelf_metal', 'N', .5)
    k.wall_fx('store_shelf_tall', 'E', -.6)
    k.fx('store_wooden_crate', -1.6, .9)
    k.fx('store_wooden_crate', -1.6, .9, .36, .2)
    k.fx('drum', 1.7, 1.2)
    k.fx('store_box_stack', -.5, .9, yaw=.15)
    k.fx('dress_bucket', 1.0, 1.4)
    k.fx('store_toolbox', .4, -.95, .9)
    k.wall_fx('lamp_bulkhead', 'N', 1.9, 2.4, solid=0)
    k.lamp(0, 0, 2.7, .9, 5.5, 3000)

    k = kit('kit_control_room', 'Control room', 6, 5, 'consoles, cabinets, two chairs')
    k.floor(TREAD); k.walls(FINISH)
    k.wall_fx('mach_control_panel', 'N', -1.6)
    k.wall_fx('mach_control_panel', 'N', -.8)
    k.wall_fx('lab_control_board', 'N', .5, .9)
    k.fx('desk', .5, -1.35, k=.9)
    k.wall_fx('pwr_switchgear_cabinet', 'E', .4, k=.9)
    k.wall_fx('lab_monitoring_screen', 'N', 1.8, 1.6, solid=0)
    k.wall_fx('mach_wall_box', 'W', -.4, .9, solid=0)
    k.fx('office_chair', -1.2, -.6, yaw=PI)
    k.fx('office_chair', .5, -.45, yaw=PI)
    k.fx('crt_monitor', .1, -1.5, .8, 0, .8)
    for x in (-1.4, 1.4):
        k.ceiling_lamp('lamp_ceiling_tube', x, .4, 6000, intensity=.85)

    k = kit('kit_server_room', 'Server room', 5, 4, 'racks, a terminal, cold light')
    k.floor(TREAD); k.walls(DARK_STEEL)
    for x in (-1.6, -.9, -.2, .5):
        k.wall_fx('scrap_server_cabinet', 'N', x)
    k.wall_fx('pwr_control_cabinet', 'E', .6)
    k.wall_fx('bld_cable_run', 'N', -.5, 2.3, solid=0, k=1.0, inset=T + .45)
    k.fx('lab_data_terminal', -1.0, 1.2, yaw=PI)
    k.wall_fx('mach_wall_box', 'W', .8, .9, solid=0)
    k.ceiling_lamp('lamp_ceiling_tube', 0, 0, 7500, intensity=.8)

    k = kit('kit_kitchen', 'Kitchen', 6, 5, 'range, fryer, fridge, prep table')
    k.floor(FLOOR_TILE); k.walls(FINISH)
    k.wall_fx('home_range', 'N', -1.5)
    k.wall_fx('home_cooker_hood', 'N', -1.5, 1.9, solid=0)
    k.wall_fx('home_fryer', 'N', .3)
    k.wall_fx('home_fridge', 'E', -1.4)
    k.fx('home_prep_table', 0, .5)
    k.fx('home_microwave', .4, .5, .92)
    k.wall_fx('store_shelf_metal', 'W', .4, k=.9)
    k.fx('home_trash_can', 2.3, 1.7)
    for x in (-1.4, 1.4):
        k.ceiling_lamp('lamp_ceiling_tube', x, 0, 4400)

    # ---------------------------------------------------------------- corridors (3 m wide, 3 m grid)
    def corridor(key, title, w, d, walls, posts=(), note=''):
        k = kit(key, title, w, d, note)
        k.floor(CAST)
        k.walls(CONCRETE, door=0, sides=walls)
        for x, y in posts:
            k.box(x - T / 2, y - T / 2, x + T / 2, y + T / 2, 0, H, CONCRETE)
        return k

    k = corridor('kit_corridor', 'Corridor', 3, 6, 'WE', note='a 6 m run, two lamps')
    for y in (-1.5, 1.5):
        k.ceiling_lamp('lamp_ceiling_tube', 0, y, 4000, yaw=HALF, rng=5.0)
    k.wall_fx('radiator_panel', 'W', .5, .1)
    k.fx('bld_pipe_run', .96, 0, 2.2, solid=0)                    # a pipe along the east wall, under the ceiling
    k = corridor('kit_corridor_corner', 'Corridor corner', 3, 3, 'NW', posts=((1.4, 1.4),), note='turns from south to east')
    k.ceiling_lamp('lamp_ceiling_tube', 0, 0, 4000, rng=5.0)
    k = corridor('kit_corridor_tee', 'Corridor T', 3, 3, 'N', posts=((-1.4, 1.4), (1.4, 1.4)), note='open south, east and west')
    k.ceiling_lamp('lamp_ceiling_tube', 0, 0, 4000, rng=5.0, yaw=HALF)
    k = corridor('kit_corridor_cross', 'Corridor crossing', 3, 3, '', posts=((-1.4, -1.4), (1.4, -1.4), (-1.4, 1.4), (1.4, 1.4)),
                 note='open every way')
    k.ceiling_lamp('lamp_ceiling_tube', 0, 0, 4000, rng=5.0)
    k.fx('med_floor_drain', 0, 0, .04, solid=0)

    # ---------------------------------------------------------------- building pieces
    k = kit('kit_stairwell', 'Stairwell', 4, 6, 'a flight up to a landing 3 m high')
    k.floor(CONCRETE); k.walls(CONCRETE, door=0, sides='NWE', h=6.0)
    k.prim('stairs', -.9, .5, 0, 1.6, 0, 3.0, 2, CONCRETE)      # climbs north: 18 risers over 5 m, from the open end to the landing
    k.box(-1.8, -2.8, 1.8, -1.95, 2.9, 3.0, CONCRETE)              # the landing
    k.box(0, -1.98, 1.8, -1.95, 3.0, 4.0, STEEL, rail=1)           # a rail along the landing's open edge
    # Each lamp model's light is bare, at its own face: a wall lamp (mount 1) draws a backing, diffuser and guard
    # of its own, and set on these it drew a lamp inside the arm lamp and one hung in the air 35 cm off the wall
    # before the bulkhead (map audit, LAMP IN LAMP).
    k.wall_fx('lamp_wall', 'N', 1.0, 4.4, solid=0)
    k.lamp(1.0, -2.15, 4.5, .9, 6.0, 3600, mount=2)                 # under the arm lamp's shade, at its outer end
    k.wall_fx('lamp_bulkhead', 'E', 1.5, 2.2, solid=0)
    k.lamp(1.62, 1.5, 2.29, .8, 5.0, 3600, mount=2)                 # just off the bulkhead's lens

    k = kit('kit_lift_lobby', 'Lift lobby', 6, 5, 'lift doors, call button, a bench')
    k.floor(OFFICE_TILE)
    k.fx('bld_lift_doors', 0, -2.36, yaw=0)
    k.box(-3, -2.5, -2.04, -2.3, 0, H, OFFICE_WALL)
    k.box(2.04, -2.5, 3, -2.3, 0, H, OFFICE_WALL)
    k.walls(OFFICE_WALL, door=1.6, sides='WES')
    k.fx('bld_lift_button', 1.2, -2.18, 1.0, solid=0)
    k.fx('out_bench', -2.4, .6)
    k.fx('dress_plant', 2.4, -1.7, k=.8)
    k.fx('dress_carpet', 0, .3, .04, 0, .7, solid=0)
    for x in (-1.2, 1.2):
        k.ceiling_lamp('lamp_ceiling_bar', x, 0, 4200, yaw=HALF)

    k = kit('kit_doorway', 'Doorway', 4, 1, 'a wall with a framed door and a lamp over it')
    k.fx('bld_doorway_frame', 0, 0)
    k.lamp(0, .3, 2.5, .8, 4.0, 3400, mount=1, yaw=HALF)

    k = kit('kit_doorway_wide', 'Wide doorway', 4, 1, 'a wall with a wide opening and a lamp over it')
    k.fx('bld_doorway_wide', 0, 0)
    k.lamp(0, .4, 2.8, .8, 4.0, 3400, mount=1, yaw=HALF)

    k = kit('kit_doorway_double', 'Double doorway', 4, 1, 'a wall with two openings round a centre post, and a lamp over each')
    k.fx('bld_doorway_double', 0, 0)
    for x in (-1, 1):
        k.lamp(x, .3, 2.5, .8, 4.0, 3400, mount=1, yaw=HALF)

    k = kit('kit_catwalk', 'Catwalk run', 8, 2, 'an 8 m grating walk 2.5 m up, posts and a ladder')
    k.box(-4, -.7, 4, .7, 2.4, 2.5, GRATING)
    k.box(-4, -.75, 4, -.7, 2.5, 3.5, STEEL, rail=1)
    k.box(-4, .7, 4, .75, 2.5, 3.5, STEEL, rail=1)
    for x in (-3.6, 0, 3.6):
        for y in (-.75, .75):
            k.box(x - .06, y - .06, x + .06, y + .06, 0, 2.4, STEEL)
    k.fx('bld_ladder', 4.15, 0, 0, -HALF, k=.4)
    for x in (-2, 2):
        k.fx('bld_lamp_cage', x, 0, 2.15, solid=0, k=1.4)
        k.lamp(x, 0, 2.3, .9, 6.0, 3200, mount=2)                    # the bulb in its cage (a ceiling lamp drew its own housing in it)

    k = kit('kit_loading_dock', 'Loading dock', 8, 6, 'a raised dock, steps, pallets and a forklift')
    k.box(-4, -3, 4, -.5, 0, 1.2, CONCRETE)                         # the dock
    k.box(-4, -.55, 4, -.5, .9, 1.2, HAZARD)                        # its striped edge
    k.prim('stairs', 3.0, .5, 0, 1.4, 0, 1.2, 2, CONCRETE)          # steps up onto it at the east end
    k.fx('store_cement_pallet', -2.8, -2.2, 1.2)
    k.fx('pallet', -1.3, -2.3, 1.2)
    k.fx('store_box_stack', -1.3, -2.3, 1.4)
    k.fx('ind_crate', .3, -1.9, 1.2)
    k.fx('drum', 1.5, -2.3, 1.2)
    k.fx('mine_depot_forklift', -.6, 1.4, 0, HALF)
    for x in (-3.4, -1.4):
        k.fx('urb_bollard', x, .1)
    k.lamp(-2, -2.4, 2.8, 1.0, 7.0, 3000, mount=1, yaw=HALF)
    k.lamp(2, -2.4, 2.8, 1.0, 7.0, 3000, mount=1, yaw=HALF)
    k.box(-4, -3.2, 4, -3.0, 0, 3.4, BRICK)                         # the back wall the lamps hang on

    # ---------------------------------------------------------------- outdoor
    k = kit('kit_yard_fence', 'Yard fence', 12, 1, 'three panels of fence, posts, a gate')
    for x in (-4.2, -.6):
        k.fx('bld_metal_fence', x, 0)
    for x in (-6.0, -2.4, 1.2):
        k.fx('out_fence_post', x, 0)
    k.fx('out_fence_gate', 1.9, 0)
    k.fx('out_fence_post', 2.6, 0)
    k.fx('bld_metal_fence', 4.4, 0)
    k.fx('out_fence_post', 6.0, 0)

    k = kit('kit_ruin_corner', 'Ruin corner', 6, 6, 'broken walls, a fallen pillar, rubble, ivy')
    k.fx('bld_wall_a', -.9, -2.8)
    k.fx('bld_wall_a', -2.8, -.6, yaw=HALF)
    k.fx('bld_pillar_broken', -2.8, -2.8)
    k.fx('bld_pillar_broken', 1.5, -2.8, k=.8)
    k.fx('bld_brick_rubble', .6, -.8)
    k.fx('out_debris_bricks', -1.2, 1.2, yaw=.6)
    k.fx('bld_gravel_pile', 1.6, 1.5, yaw=.4)
    k.fx('ivy_wall_a', -1.2, -2.58, .3, solid=0)
    k.fx('nat_stone_round', 2.2, -1.2, k=2.5)
    k.fx('out_concrete_block', .5, 1.9, yaw=1.8)

    k = kit('kit_campsite', 'Campsite', 6, 6, 'a tent, a fire, logs to sit on')
    k.fx('out_tent', -1.4, -1.4, yaw=.5)
    k.fx('out_campfire', .8, .6)
    k.emitter(.8, .6, 0, (0.0, 0.0, .6, 1.0, .3, .6, 20, 8, .9))
    k.lamp(.8, .6, .6, 1.2, 7.0, 1900, mount=2)
    k.fx('log', .8, 2.3, k=.6)
    k.fx('log', 2.5, .4, yaw=HALF, k=.6)
    k.fx('scrap_bedroll', -1.0, 1.4, yaw=-.4)
    k.fx('out_sawbuck', 2.2, -1.8, yaw=.3)
    k.fx('scrap_lantern', -.2, -1.1)
    k.fx('mine_petrol_can', -2.3, .2)
    k.fx('dress_bucket', -2.2, .8)

    # ---------------------------------------------------------------- structures (2026-11-23, the user's packs)
    k = kit('kit_escalator', 'Escalator', 2.2, 9, 'a stopped escalator: steps, glass sides, handrails, landing plates')
    k.box(-1.1, 3.6, 1.1, 4.5, 0, .06, STEEL)                    # the bottom landing plate (south, the front)
    k.prim('stairs', 0, -.1, 0, 1.4, 7.4, 4.42, 2, TREAD)          # 26 steps rising north
    k.box(-1.1, -4.5, 1.1, -3.8, 4.36, 4.42, STEEL)               # the top landing plate
    for side in (-1, 1):
        x = side * .8
        for i in range(8):                                        # each side steps up the flight:
            y0 = 3.6 - i * .925
            z = 4.42 * ((i + .5) / 8)
            k.box(x - .05, y0 - .925, x + .05, y0, 0, z + .9, DARK_STEEL)  # a steel balustrade a step at a time
        k.pipe(x, 4.2, x, -4.2, 1.0, .035, 4.42 + 1.0, DARK_STEEL)  # the handrail, sloping with the flight

    k = kit('kit_airlock', 'Airlock', 4, 6.4, 'two lifting hatches on one cycle: the terminal inside shuts one, then opens the other')
    k.floor(GRATING, -2, -3.2, 2, 3.2)
    for side in ('W', 'E'):
        x1 = -2 if side == 'W' else 1.8
        k.box(x1, -2.7, x1 + .2, 2.7, 0, 3.2, DARK_STEEL)
    k.box(-2, -2.7, 2, 2.7, 3.0, 3.2, DARK_STEEL)                  # the ceiling: sealed
    for yc, yaw, value, label in ((-3.2, 0.0, 1, 'INNER HATCH'), (2.2, PI, 0, 'OUTER HATCH')):
        # The plate is cut to a blast-door frame's outline (tools/library/make_airlock_bulkhead.py), 1 m deep like any
        # wall a door stands in; unseen walls give it its collision round the opening, and the hatch is a real lifting
        # DOOR that follows the cycle state (outer while it is 0, inner while it is 1).
        k.fx_sized('airlock_bulkhead', 0, yc + .5, 0, 4, 1, 3.2, yaw, solid=0)
        for x1, x2, z0 in ((-2, -1.3, 0), (1.3, 2, 0), (-1.3, 1.3, 2.48)):
            k.box(x1, yc, x2, yc + 1, z0, 3.2, 30)                    # 30: collision only, never drawn
        k.door(-1.3, 1.3, yc, 'airlock_cycle', value, follow=True, label=label)
    k.terminal(1.62, 0, 1.2, 'AIRLOCK', 'ONE HATCH AT A TIME', 'E: CYCLE THE LOCK', 'airlock_cycle', True, -HALF)
    k.lamp(0, 0, 2.8, .9, 5.0, 1700)                              # a red-amber cycle lamp

    k = kit('kit_shower', 'Shower stall', 2.2, 1.6, 'a tiled stall, a drain, a riser and a head')
    k.fx('bath_floor_tile', 0, 0, .005, 0, solid=0)
    k.fx('bath_stall_open', 0, 0, 0, PI)                          # the pack's stall opens north at yaw 0
    k.fx('bath_drain', 0, .1, .012, solid=0)
    k.fx('bath_pipe', -.5, -.68, 1.2, 0, k=4.0)                     # the riser up the back wall
    k.pipe(-.5, -.7, -.5, -.35, 2.05, .02, 2.05, STEEL)              # the arm out to the head
    k.prim('box', -.5, -.32, 1.98, .16, .16, .05, 0, STEEL)           # the head
    k.lamp(0, 0, 2.3, .6, 3.5, 5200)

    k = kit('kit_rebar', 'Loose rebar', 3, 3, 'rebar mats and a cracked block, loose to kick about')
    k.loose('rebar_bars_a', -.2, -.4, .03, .3, pitch=HALF)
    k.loose('rebar_bars_b', .4, .5, .08, -.5, pitch=HALF)
    k.fx('concrete_block_cracked', 1.0, -.8, 0, .4)
    k.fx('out_debris_bricks', -1.0, .9, 0, 1.2)

    k = kit('kit_cracked_slab', 'Cracked slab', 6, 2, 'broken concrete panels, bare rebar, rubble')
    k.fx('slab_full', -1.5, 0)
    k.fx('slab_broken', 1.5, 0)
    k.fx('rebar_bars_b', 1.5, -.04, .7, k=.6, solid=0)              # the bars left standing out of the break
    k.fx('slab_chipped', 1.7, .7, 0, .25)
    k.fx('concrete_block_cracked', -1.8, .7, 0, HALF)
    k.fx('bld_brick_rubble', .4, .7, 0, .9)

    k = kit('kit_heavy_door', 'Heavy door', 4.4, 1.2, 'a bulkhead door in a steel wall, worked from the terminal beside it')
    for x1, x2 in ((-2.2, -1.1), (1.1, 2.2)):
        k.box(x1, -.5, x2, .5, 0, 3.2, DARK_STEEL)
    k.box(-1.1, -.5, 1.1, .5, 2.6, 3.2, DARK_STEEL)                    # the head over the leaf
    k.door(-1.1, 1.1, -.5, 'heavy_door', 1, follow=True, label='HEAVY DOOR')
    k.terminal(1.45, .5, 1.25, 'HEAVY DOOR', 'THE LEAF RISES', 'E: OPEN / SHUT', 'heavy_door', True, .001)   # on the south face
    k.terminal(-1.45, -.5, 1.25, 'HEAVY DOOR', 'THE LEAF RISES', 'E: OPEN / SHUT', 'heavy_door', True, PI)   # and the north

    k = kit('kit_mesh_door', 'Mesh door', 3.4, 1.0, 'a steel grille gate that rises, in a frame of piers')
    for x1, x2 in ((-1.7, -1.2), (1.2, 1.7)):
        k.box(x1, -.45, x2, .45, 0, 2.9, STEEL)
    k.box(-1.2, -.45, 1.2, .45, 2.55, 2.9, STEEL)
    k.door(-1.2, 1.2, -.5, 'mesh_door', 1, follow=True, label='GATE', leaf=1)
    k.terminal(1.45, .5, 1.25, 'GATE', 'A STEEL GRILLE', 'E: RAISE / LOWER', 'mesh_door', True, .001)

    k = kit('kit_ticket_gate', 'Ticket gate', 3.0, 1.0, 'two pedestals and a flap; the ticket machine lets you through')
    for x1, x2 in ((-1.5, -.55), (.55, 1.5)):
        k.box(x1, -.45, x2, .45, 0, 1.05, DARK_STEEL)                   # the pedestals
        k.box(x1 + .05, -.4, x2 - .05, .4, 1.05, 1.1, STEEL)
    k.door(-.55, .55, -.5, 'ticket_paid', 1, follow=True, label='TICKET GATE', leaf=2, swinging=True)
    k.terminal(-1.05, .5, 1.1, 'TICKET MACHINE', 'FARE: ONE TOKEN', 'E: PAY', 'ticket_paid', True, .001)

    k = kit('kit_fluorescent_bank', 'Fluorescent bank', 7.4, .6, 'a 7 m run of twin-tube fittings on one channel, hung at ceiling height')
    k.box(-3.7, -.06, 3.7, .06, H - .1, H - .04, DARK_STEEL)          # the carrier channel under the ceiling
    for i in range(6):                                                # each fitting its own light: one can die alone
        k.ceiling_lamp('lamp_suspended_twin_tube', -3.0 + i * 1.2, 0, 4600, rng=5.0, intensity=.8, top=H - .1,
                       flicker=.35 if i == 4 else None)                # the fifth tube is going

    k = kit('kit_curtain_wall', 'Curtain wall', 6, .4, 'two storeys of glass in a steel grid')
    for x in (-1.5, 1.5):                                          # the glazing: the engine's window shape, glass and all
        for z in (.05, 3.05):
            k.prim('window', x, 0, z, 2.9, .2, 2.9, 0, DARK_STEEL)
    for x in (-3, 0, 3):                                           # mullions
        k.box(x - .05, -.12, x + .05, .12, 0, 6, DARK_STEEL)
    for z in (0, 3, 6):                                            # transoms at each floor
        k.box(-3.05, -.12, 3.05, .12, z - .06 if z else 0, z + .06 if z < 6 else 6.06, DARK_STEEL)

    k = kit('kit_megafacade', 'Megastructure facade', 6, 1, 'three storeys of window modules between concrete piers')
    for x in (-1.5, 1.5):
        k.fx('facade_concrete', x, 0, 0, HALF)
        for z, name in ((1, 'facade_stucco_window'), (4, 'facade_brick_window'), (7, 'facade_stucco_window')):
            k.fx(name, x, 0, z, HALF)
    for x in (-3, 0, 3):                                            # the piers that carry it
        k.box(x - .25, -.45, x + .25, .3, 0, 10.4, CONCRETE)
    for z in (4, 7, 10):                                            # ledge bands between storeys
        k.box(-3.2, -.3, 3.2, .2, z - .12, z + .06, CONCRETE)
    return kits


def prefab_lines(kit, slot):
    """The kit's PREFAB records with each '$name' replaced by that model's slot."""
    out = []
    for line in kit.lines:
        parts = line.split('|')
        out.append('|'.join(str(slot[p[1:]]) if p.startswith('$') else p for p in parts))
    return out


def load_catalog():
    return json.load(open(CATALOG))


def cpp_string(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def write_inc(kits):
    rows = []
    for k in kits:
        body = '\n     '.join(cpp_string(line)[:-1] + '\\n"' for line in k.lines)
        rows.append('    {%s, L%s, L%s, %.2ff, %.2ff,\n     %s},' % (cpp_string(k.key), cpp_string(k.title), cpp_string(k.note), k.w, k.d, body))
    text = ('// Generated by tools/campaign/kits.py: do not edit; change the kits there and run it again.\n'
            '// Each kit is a PREFAB body (records in metres from its middle, map field "@"), "$name" standing for\n'
            "// the library model's slot (resolved from ModelIds.h by NativeKitMethods.inc).\n"
            'struct KitDef {\n  const char *key;\n  const wchar_t *title, *note;\n  float w, d;\n  const char *body;\n};\n'
            'static constexpr KitDef kitDefs[] = {\n' + '\n'.join(rows) + '\n};\n'
            'static constexpr int KitCount = int(std::size(kitDefs));\n')
    with open(OUT, 'w', newline='\n', encoding='utf-8') as f:
        f.write(text)


def main():
    catalog = load_catalog()
    kits = build(catalog)
    missing = sorted({l.split('|')[2][1:] for k in kits for l in k.lines if l.split('|')[2].startswith('$') and l.split('|')[2][1:] not in catalog and l.split('|')[2][1:] not in ENGINE})
    if missing:
        raise SystemExit('kits name models the catalog lacks: ' + ', '.join(missing))
    write_inc(kits)
    print('%s: %d kits, %d records' % (OUT, len(kits), sum(len(k.lines) for k in kits)))


if __name__ == '__main__':
    main()
