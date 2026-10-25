"""Import one model from the purchased library into src/assets/library/.

Run under Blender (it reads FBX, OBJ and GLB the same way):

  blender -b --python tools/library/import_model.py -- --src "O:/retro official/.../thing.fbx"
      --name thing --root "O:/retro official/.../pack" [--objects REGEX] [--exclude REGEX]
      [--max-tex 512] [--yaw DEG] [--tex-suffix _summer]

It joins the chosen mesh objects, applies transforms, puts the footprint centre
at the origin with the base at z=0, prefixes every material with the model name
so packs never collide, and exports an OBJ (Y up) beside one PNG/JPEG per
material. Textures are found by file name anywhere under --root, converted from
TGA/BMP to PNG, and scaled down to --max-tex. Images packed inside the file
(GLB) are written out directly. Materials with no texture get a
4x4 swatch of their base colour. A JSON manifest records the model's size
(width = X, depth = OBJ Z, height = Y), triangle count and material -> texture
map for tools/library/register.py.
"""
import bpy, sys, os, re, json, argparse, mathutils

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'src', 'assets', 'library')


def args():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument('--src', required=True)
    p.add_argument('--name', default=None)
    p.add_argument('--pick', action='append', default=[],
                   help='NAME=REGEX: export the objects matching REGEX as NAME; repeatable, one scene load')
    p.add_argument('--root', default=None)
    p.add_argument('--objects', default=None)
    p.add_argument('--exclude', default=None)
    p.add_argument('--max-tex', type=int, default=512)
    p.add_argument('--yaw', type=float, default=0)
    p.add_argument('--tex-suffix', default='',
                   help='also look for TEXTURE+SUFFIX (e.g. _summer for seasonal packs)')
    return p.parse_args(argv)


def find_file(root, name):
    if not name:
        return None
    if os.path.isfile(name):
        return name
    base = os.path.basename(name.replace('\\', '/')).lower()
    stem = os.path.splitext(base)[0]
    best = None
    for d, _, files in os.walk(root):
        for f in files:
            fl = f.lower()
            if fl == base:
                return os.path.join(d, f)
            if best is None and os.path.splitext(fl)[0] == stem and fl.endswith(('.png', '.jpg', '.jpeg', '.tga', '.bmp')):
                best = os.path.join(d, f)
    return best


def material_image(mat):
    if not mat or not mat.use_nodes:
        return None
    # Prefer the image feeding Base Color, then any image texture node.
    for n in mat.node_tree.nodes:
        if n.type == 'BSDF_PRINCIPLED':
            link = n.inputs['Base Color'].links
            if link and link[0].from_node.type == 'TEX_IMAGE' and link[0].from_node.image:
                return link[0].from_node.image
    for n in mat.node_tree.nodes:
        if n.type == 'TEX_IMAGE' and n.image:
            return n.image
    return None


def base_colour(mat):
    if mat and mat.use_nodes:
        for n in mat.node_tree.nodes:
            if n.type == 'BSDF_PRINCIPLED':
                return tuple(n.inputs['Base Color'].default_value)[:3]
    return (.5, .5, .5)


def main():
    a = args()
    os.makedirs(OUT, exist_ok=True)
    root = a.root or os.path.dirname(a.src)
    picks = [tuple(p.split('=', 1)) for p in a.pick] or [(a.name, a.objects)]
    if any(n is None for n, _ in picks):
        raise SystemExit('give --name or --pick NAME=REGEX')
    bpy.ops.wm.read_factory_settings(use_empty=True)
    ext = os.path.splitext(a.src)[1].lower()
    if ext == '.fbx':
        bpy.ops.import_scene.fbx(filepath=a.src)
    elif ext == '.obj':
        bpy.ops.wm.obj_import(filepath=a.src)
    elif ext in ('.glb', '.gltf'):
        bpy.ops.import_scene.gltf(filepath=a.src)
    else:
        raise SystemExit('unsupported source ' + ext)
    source = [o for o in bpy.data.objects if o.type == 'MESH']
    for name, pattern in picks:
        export(a, root, source, name, pattern)


def export(a, root, source, name, pattern):
    meshes = [o for o in source if not pattern or re.search(pattern, o.name)]
    if a.exclude:
        meshes = [o for o in meshes if not re.search(a.exclude, o.name)]
    if not meshes:
        raise SystemExit('no mesh objects selected')
    bpy.ops.object.select_all(action='DESELECT')
    copies = []
    for o in meshes:
        c = o.copy(); c.data = o.data.copy(); c.parent = None; c.matrix_world = o.matrix_world.copy()
        bpy.context.collection.objects.link(c); copies.append(c)
        # Private material copies, so renaming never leaks into the next pick.
        for slot in c.material_slots:
            if slot.material:
                slot.material = slot.material.copy()
    for c in copies:
        c.select_set(True)
    bpy.context.view_layer.objects.active = copies[0]
    if len(copies) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if a.yaw:
        rot = mathutils.Matrix.Rotation(a.yaw * 3.14159265 / 180, 4, 'Z')
        o.data.transform(rot)
    vs = [v.co for v in o.data.vertices]
    mn = mathutils.Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
    mx = mathutils.Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    o.data.transform(mathutils.Matrix.Translation(-mathutils.Vector(((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z))))
    o.data.update()
    size = mx - mn
    # Materials: prefix, and give every one a texture file.
    prefix = re.sub(r'[^A-Za-z0-9]+', '_', name).strip('_').lower()
    materials = {}
    seen = {}
    for slot in o.material_slots:
        mat = slot.material
        if not mat or mat.name in seen:
            continue
        clean = prefix + '_' + re.sub(r'[^A-Za-z0-9]+', '_', re.sub(r'\.\d+$', '', mat.name)).strip('_').lower()
        while clean in materials:
            clean += '_x'
        seen[mat.name] = clean
        img = material_image(mat)
        tex_name = None
        if img is not None:
            path = bpy.path.abspath(img.filepath) if img.filepath else ''
            found = find_file(root, path) if path else None
            if found is None and img.name:
                found = find_file(root, img.name)
            if found is None and a.tex_suffix:
                stem = os.path.splitext(os.path.basename((path or img.name).replace(chr(92), '/')))[0]
                found = find_file(root, stem + a.tex_suffix + '.png')
            if found is None and (img.packed_file or img.has_data) and img.size[0] > 0:
                # Embedded (GLB) image: write its pixels straight out.
                w, h = img.size
                if max(w, h) > a.max_tex:
                    f = a.max_tex / max(w, h)
                    img.scale(max(1, int(w * f)), max(1, int(h * f)))
                tex_name = clean + '.png'
                img.filepath_raw = os.path.join(OUT, tex_name); img.file_format = 'PNG'; img.save()
            elif found:
                src = bpy.data.images.load(found, check_existing=True)
                w, h = src.size
                keep = found.lower().endswith(('.png', '.jpg', '.jpeg')) and max(w, h) <= a.max_tex
                out_ext = os.path.splitext(found)[1].lower() if keep else '.png'
                if out_ext == '.jpeg':
                    out_ext = '.jpg'
                tex_name = clean + out_ext
                dest = os.path.join(OUT, tex_name)
                if keep:
                    with open(found, 'rb') as fi, open(dest, 'wb') as fo:
                        fo.write(fi.read())
                else:
                    if max(w, h) > a.max_tex:
                        f = a.max_tex / max(w, h)
                        src.scale(max(1, int(w * f)), max(1, int(h * f)))
                    src.filepath_raw = dest; src.file_format = 'PNG'; src.save()
        if tex_name is None and a.tex_suffix:
            # No image node: seasonal packs name textures after the material.
            found = find_file(root, re.sub(r'\.\d+$', '', mat.name) + a.tex_suffix + '.png')
            if found:
                tex_name = clean + '.png'
                src = bpy.data.images.load(found, check_existing=True)
                w, h = src.size
                if max(w, h) > a.max_tex:
                    f = a.max_tex / max(w, h)
                    src.scale(max(1, int(w * f)), max(1, int(h * f)))
                src.filepath_raw = os.path.join(OUT, tex_name); src.file_format = 'PNG'; src.save()
        if tex_name is None:
            tex_name = clean + '.png'
            img = bpy.data.images.new(clean, 4, 4, alpha=False)
            r, g, b = base_colour(mat)
            img.pixels = [r, g, b, 1.0] * 16
            img.filepath_raw = os.path.join(OUT, tex_name); img.file_format = 'PNG'; img.save()
        materials[clean] = tex_name
        mat.name = clean
    tris = sum(len(p.vertices) - 2 for p in o.data.polygons)
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True)
    obj_path = os.path.join(OUT, name + '.obj')
    bpy.ops.wm.obj_export(filepath=obj_path, export_selected_objects=True, forward_axis='NEGATIVE_Z', up_axis='Y',
                          export_materials=True, export_normals=True, export_uv=True, apply_modifiers=True)
    mtl = os.path.join(OUT, name + '.mtl')
    if os.path.exists(mtl):
        os.remove(mtl)
    manifest = dict(name=name, source=a.src, width=round(size.x, 3), depth=round(size.y, 3), height=round(size.z, 3),
                    triangles=tris, materials=materials)
    with open(os.path.join(OUT, name + '.json'), 'w') as f:
        json.dump(manifest, f, indent=1)
    print('IMPORTED', json.dumps(manifest))
    bpy.data.objects.remove(o, do_unlink=True)


main()
