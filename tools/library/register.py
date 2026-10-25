"""Register imported library models with the engine.

  python tools/library/register.py NAME [NAME ...]

For each src/assets/library/NAME.json written by import_model.py this
appends the OBJ and its textures to src/assets.rc, adds a Mesh{id} slot to
SoftwareRenderer.h, raises FacilityModelCount, lists each material's texture
in src/assets/library/materials.txt (loaded at start-up, resource 466) and
records the model in src/assets/library/catalog.json:

  {"NAME": {"slot": fixture model index, "width": .., "depth": .., "height": .., "triangles": ..}}

Already-registered names are skipped, so it is safe to re-run.
"""
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'src')
LIB = os.path.join(SRC, 'assets', 'library')
MANIFEST_ID = 466


def read(p, enc='utf-8'):
    with open(p, 'rb') as f:
        return f.read().decode(enc)


def write(p, s, enc='utf-8'):
    with open(p, 'wb') as f:
        f.write(s.encode(enc))


def main(names):
    rc_path = os.path.join(SRC, 'assets.rc')
    rc = read(rc_path, 'cp1252')
    newline = '\r\n' if '\r\n' in rc else '\n'
    ids = [int(m) for m in re.findall(r'^(\d+)\s+RCDATA', rc, re.M)]
    next_id = max(ids + [MANIFEST_ID]) + 1
    if not re.search(r'^%d\s+RCDATA' % MANIFEST_ID, rc, re.M):
        rc = rc.rstrip() + newline + '%d RCDATA "assets/library/materials.txt"' % MANIFEST_ID + newline
    header_path = os.path.join(SRC, 'renderer', 'SoftwareRenderer.h')
    header = read(header_path)
    count_path = os.path.join(SRC, 'world', 'WorldDefinition.h')
    count_src = read(count_path)
    count = int(re.search(r'FacilityModelCount=(\d+);', count_src).group(1))
    catalog_path = os.path.join(LIB, 'catalog.json')
    catalog = json.load(open(catalog_path)) if os.path.exists(catalog_path) else {}
    materials_path = os.path.join(LIB, 'materials.txt')
    materials = read(materials_path) if os.path.exists(materials_path) else ''
    meshes = []
    for name in names:
        if name in catalog:
            print('already registered', name, 'slot', catalog[name]['slot'])
            continue
        m = json.load(open(os.path.join(LIB, name + '.json')))
        mesh_id = next_id; next_id += 1
        rc += '%d RCDATA "assets/library/%s.obj"' % (mesh_id, name) + newline
        for mat, tex in m['materials'].items():
            rc += '%d RCDATA "assets/library/%s"' % (next_id, tex) + newline
            materials += '%s %d\n' % (mat, next_id)
            next_id += 1
        meshes.append(mesh_id)
        catalog[name] = dict(slot=count, width=m['width'], depth=m['depth'], height=m['height'], triangles=m['triangles'])
        print('registered', name, 'slot', count, 'mesh', mesh_id)
        count += 1
    if not meshes:
        return
    anchor = header.index('m_facilityMeshes{')
    close = header.index('};', anchor)
    header = header[:close] + ',\n        ' + ','.join('Mesh{%d}' % i for i in meshes) + header[close:]
    count_src = re.sub(r'FacilityModelCount=\d+;', 'FacilityModelCount=%d;' % count, count_src)
    write(rc_path, rc, 'cp1252')
    write(header_path, header)
    write(count_path, count_src)
    write(materials_path, materials)
    json.dump(catalog, open(catalog_path, 'w'), indent=1)


if __name__ == '__main__':
    main(sys.argv[1:])
