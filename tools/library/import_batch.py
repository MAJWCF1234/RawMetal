"""Import a batch of library models in one go (several Blender processes side by side).

  python tools/library/import_batch.py tools/library/batches/parts-1.5.6.json [--only NAME[,NAME]] [--jobs 4]

The batch file is a JSON list of jobs. Each job is one Blender run of import_model.py:

  {"src": "Models/GLB/Furniture/bed_1.glb", "name": "fur_bed"}
  {"src": "Environments/DINER/Models/Objects.fbx", "root": "Environments/DINER",
   "pick": {"home_fryer": "^Fryer$", "home_oven": "^Oven$"}, "scale": 0.7}

Paths are under the packs folder ("O:/retro official", or RM_PACKS). Optional keys: root (where
textures are looked for; default the source's folder), pick (NAME: REGEX, several models from one
scene), objects, exclude, yaw, scale and max_tex (default 256). After the runs it lists every model
the batch names with its size, triangles and any 4x4 swatch (a texture the import could not find):
check those before tools/library/register.py. Nothing leaves this machine.
"""
import json, os, subprocess, sys, concurrent.futures as cf
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LIB = os.path.join(ROOT, 'src', 'assets', 'library')
PACKS = os.environ.get('RM_PACKS', 'O:/retro official')
BLENDER = os.environ.get('RM_BLENDER', 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe')


def names(job):
    return list(job['pick']) if 'pick' in job else [job['name']]


def run(job):
    src = os.path.join(PACKS, job['src'])
    root = os.path.join(PACKS, job['root']) if 'root' in job else os.path.dirname(src)
    cmd = [BLENDER, '-b', '--python', os.path.join(ROOT, 'tools', 'library', 'import_model.py'), '--',
           '--src', src, '--root', root, '--max-tex', str(job.get('max_tex', 256))]
    if 'pick' in job:
        for n, rx in job['pick'].items():
            cmd += ['--pick', '%s=%s' % (n, rx)]
    else:
        cmd += ['--name', job['name']]
    for key in ('objects', 'exclude', 'yaw', 'scale'):
        if key in job:
            cmd += ['--' + key, str(job[key])]
    p = subprocess.run(cmd, capture_output=True, text=True, errors='replace')
    ok = p.returncode == 0 and all(os.path.exists(os.path.join(LIB, n + '.json')) for n in names(job))
    tail = '' if ok else '\n'.join((p.stdout + p.stderr).splitlines()[-12:])
    return job, ok, tail


def report(model):
    m = json.load(open(os.path.join(LIB, model + '.json')))
    swatches = []
    for mat, tex in m['materials'].items():
        try:
            w, h = Image.open(os.path.join(LIB, tex)).size
        except OSError:
            w = h = 0
        if w <= 4 or h <= 4:
            swatches.append(mat)
    return m, swatches


def main(argv):
    path = argv[0]
    only = None
    jobs_n = 4
    if '--only' in argv:
        only = set(argv[argv.index('--only') + 1].split(','))
    if '--jobs' in argv:
        jobs_n = int(argv[argv.index('--jobs') + 1])
    jobs = json.load(open(path))
    if only:
        jobs = [j for j in jobs if only & set(names(j))]
    failed = 0
    with cf.ThreadPoolExecutor(jobs_n) as pool:
        for job, ok, tail in pool.map(run, jobs):
            if not ok:
                failed += 1
                print('FAILED', ', '.join(names(job)), job['src'])
                print(tail)
    bad = 0
    for job in jobs:
        for n in names(job):
            if not os.path.exists(os.path.join(LIB, n + '.json')):
                continue
            m, swatches = report(n)
            bad += bool(swatches)
            print('%-28s %6.2f x %5.2f x %5.2f m %6d tris%s' % (n, m['width'], m['depth'], m['height'], m['triangles'],
                                                               '  SWATCH ' + ','.join(swatches) if swatches else ''))
    print('%d jobs, %d failed, %d models with 4x4 swatches' % (len(jobs), failed, bad))
    return 1 if failed or bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
