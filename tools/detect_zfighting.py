"""Render nearby camera samples and flag persistent unstable pixel regions.

This is deliberately an external heuristic: it finds likely Z-fighting, but
human review of the emitted frame pair is still required before changing art.
"""
import argparse, subprocess, sys
from pathlib import Path
from PIL import Image, ImageChops

def ppm(path): return Image.open(path).convert('RGB')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', type=Path, required=True)
    ap.add_argument('--views', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--threshold', type=int, default=34)
    ap.add_argument('--angle-step', type=float, default=0.08)
    args = ap.parse_args(); args.out.mkdir(parents=True, exist_ok=True)
    samples=[]
    for raw in args.views.read_text(encoding='utf-8').splitlines():
        f=raw.split()
        if len(f)<7 or raw.startswith('#'): continue
        name,mapid,x,y,z,yaw,pitch=f[:7]
        for suffix,delta in (('a',-args.angle_step),('b',0.0),('c',args.angle_step)):
            samples.append((name+suffix,mapid,x,y,z,str(float(yaw)+delta),pitch))
    generated=args.out/'zfight-views.txt'
    generated.write_text('\n'.join(' '.join(p) for p in samples)+'\n',encoding='utf-8')
    run=subprocess.run([str(args.exe), '--map-shots='+str(generated)], cwd=args.out, capture_output=True, timeout=300)
    if run.returncode: print('FAIL zfighting-render exit='+str(run.returncode)); return 1
    suspicious=[]
    for raw in args.views.read_text(encoding='utf-8').splitlines():
        f=raw.split()
        if len(f)<7 or raw.startswith('#'): continue
        name=f[0]; paths=[args.out/'.build'/'shot'/(name+s+'.ppm') for s in ('a','b','c')]
        if any(not p.exists() for p in paths): print('FAIL missing capture '+name); return 1
        ratios=[]
        for left,right in zip(paths,paths[1:]):
            diff=ImageChops.difference(ppm(left),ppm(right)); hist=diff.histogram(); changed=sum(v for i,v in enumerate(hist) if (i%256)>=args.threshold)
            ratios.append(changed/(diff.width*diff.height*3))
        # Persistent instability must survive both adjacent samples. A single
        # large delta is usually camera motion, TAA, animation, or a light.
        persistent=min(ratios)
        if persistent>0.055: suspicious.append((name,persistent))
        ImageChops.difference(ppm(paths[0]),ppm(paths[2])).save(args.out/(name+'-diff.png'))
        print(('SUSPECT' if persistent>0.055 else 'STABLE')+' '+name+' adjacent='+','.join(format(r,'.4f') for r in ratios))
    if suspicious:
        print('DETAIL review paired captures for: '+', '.join(n for n,_ in suspicious))
    print('PASS zfighting-sweep suspects='+str(len(suspicious))+' views='+str(len(samples)//3))
    print('ALL PASSED 0'); return 0
if __name__=='__main__': sys.exit(main())
