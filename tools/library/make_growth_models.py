"""Write the growth models the Campaign Builder's Growth brush paints, from the purchased LRT texture
pack (O:/retro official/Textures/LRT - Texture Pack - RT28.8 - 128x/tile).

  python tools/library/make_growth_models.py
  python tools/library/register.py growth_vine_sheet growth_moss_wall growth_moss_floor growth_hanging_vine
  python tools/campaign/model_ids.py

The geometry is ours and simple; the textures are copied as they are (never altered) into
src/assets/library/, which is gitignored like every purchased asset:
  growth_vine_sheet   flat vine sheets laid on a wall (RT_vine): ivy that hugs the surface
  growth_moss_wall    a ragged patch of moss standing on a wall (RT_moss_block)
  growth_moss_floor   the same patch lying on a floor or a ledge
  growth_hanging_vine a vine hanging from a ceiling or an edge: two crossed strips, the stem
                      (RT_cave_vines_plant) and its tip (RT_cave_vines); it sways (plant physbones)
OBJ axes are the importer's: x across, y up, z out of the wall; the base is at y 0.
"""
import json, math, os, random, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LIB = os.path.join(ROOT, 'src', 'assets', 'library')
PACK = r'O:/retro official/Textures/LRT - Texture Pack - RT28.8 - 128x/tile'


class Mesh:
    def __init__(self, name):
        self.name, self.v, self.vt, self.faces = name, [], [], {}

    def tri(self, mat, a, b, c, both=True):
        """Three (x, y, z, u, v) corners; both: the back face too."""
        ids = []
        for x, y, z, u, v in (a, b, c):
            self.v.append((x, y, z)); self.vt.append((u, v)); ids.append(len(self.v))
        self.faces.setdefault(mat, []).append(ids)
        if both:
            self.faces[mat].append([ids[0], ids[2], ids[1]])

    def quad(self, mat, a, b, c, d):
        self.tri(mat, a, b, c); self.tri(mat, a, c, d)

    def write(self, textures):
        xs, ys, zs = [p[0] for p in self.v], [p[1] for p in self.v], [p[2] for p in self.v]
        cx, cz = (min(xs) + max(xs)) / 2, (min(zs) + max(zs)) / 2
        lines = ['# %s: geometry by tools/library/make_growth_models.py' % self.name, 'mtllib %s.mtl' % self.name, 'o ' + self.name]
        lines += ['v %.5f %.5f %.5f' % (x - cx, y - min(ys), z - cz) for x, y, z in self.v]
        lines += ['vt %.5f %.5f' % uv for uv in self.vt]
        tris = 0
        for mat, faces in self.faces.items():
            lines.append('usemtl %s_%s' % (self.name, mat))
            for f in faces:
                lines.append('f ' + ' '.join('%d/%d' % (i, i) for i in f)); tris += 1
        open(os.path.join(LIB, self.name + '.obj'), 'w', newline='\n').write('\n'.join(lines) + '\n')
        mats = {}
        for mat, source in textures.items():
            out = '%s_%s.png' % (self.name, mat)
            shutil.copyfile(os.path.join(PACK, source), os.path.join(LIB, out))
            mats['%s_%s' % (self.name, mat)] = out
        meta = {'name': self.name, 'source': PACK + '/' + ', '.join(textures.values()), 'width': round(max(xs) - min(xs), 3),
                'depth': round(max(max(zs) - min(zs), .01), 3), 'height': round(max(max(ys) - min(ys), .01), 3), 'triangles': tris, 'materials': mats}
        json.dump(meta, open(os.path.join(LIB, self.name + '.json'), 'w'), indent=1)
        print(self.name, meta['width'], meta['depth'], meta['height'], tris, 'triangles')


def patch(rng, n, rx, ry):
    """A ragged outline round the origin: n points, radius wandering 60-100 %."""
    pts, r = [], 1.0
    for i in range(n):
        r = max(.6, min(1.0, r + rng.uniform(-.18, .18)))
        a = 2 * math.pi * i / n
        pts.append((math.cos(a) * rx * r, math.sin(a) * ry * r))
    return pts


def main():
    rng = random.Random(7)
    # Vine sheets: three overlapping sheets on the wall, a little proud of it each, staggered down.
    m = Mesh('growth_vine_sheet')
    for k, (x0, y0, w, h, z) in enumerate([(-.6, .3, 1.0, 1.3, .006), (-.25, 0, .9, 1.1, .012), (.05, .45, .7, 1.15, .018)]):
        m.quad('vine', (x0, y0, z, 0, 0), (x0 + w, y0, z, 1, 0), (x0 + w, y0 + h, z, 1, 1), (x0, y0 + h, z, 0, 1))
    m.write({'vine': 'RT_vine.png'})
    # Moss on a wall: a ragged fan of moss standing 4 mm proud.
    for name, flat in (('growth_moss_wall', False), ('growth_moss_floor', True)):
        m = Mesh(name)
        pts = patch(rng, 22, .5, .38 if not flat else .5)
        for i in range(len(pts)):
            (ax, ay), (bx, by) = pts[i], pts[(i + 1) % len(pts)]
            uv = lambda x, y: (x / .5 + .5, y / .5 + .5)
            if flat:
                m.tri('moss', (0, .004, 0, .5, .5), (ax, .004, ay, *uv(ax, ay)), (bx, .004, by, *uv(bx, by)))
            else:
                m.tri('moss', (0, .38, .004, .5, .5), (ax, .38 + ay, .004, *uv(ax, ay)), (bx, .38 + by, .004, *uv(bx, by)))
        m.write({'moss': 'RT_moss_block.png'})
    # A hanging vine: two crossed strips, 0.32 m wide, five stem segments and a tip, 1.9 m long.
    m = Mesh('growth_hanging_vine')
    seg, w = .32, .16
    for plane in range(2):
        def corner(x, y, u, v):
            return (x, y, 0, u, v) if plane == 0 else (0, y, x, u, v)
        for k in range(6):
            y0 = k * seg
            mat = 'tip' if k == 0 else 'stem'
            m.quad(mat, corner(-w, y0, 0, 0), corner(w, y0, 1, 0), corner(w, y0 + seg, 1, 1), corner(-w, y0 + seg, 0, 1))
    m.write({'stem': 'RT_cave_vines_plant.png', 'tip': 'RT_cave_vines.png'})


if __name__ == '__main__':
    main()
