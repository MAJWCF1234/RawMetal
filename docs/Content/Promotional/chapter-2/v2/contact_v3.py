"""The trailer's contact sheet: one frame every 2.5 s (from the composed frames), time-stamped.

  python contact_ch2v2.py FRAMES OUT.png
"""
import os, sys
from PIL import Image, ImageDraw
sys.path.insert(0, 'N:/rawmetal/.build/promo-1.0')
import look

src, out = sys.argv[1], sys.argv[2]
frames = sorted(int(f[1:5]) for f in os.listdir(src) if f.startswith('f') and f.endswith('.png'))
picks = [f for f in range(0, frames[-1] + 1, 75) if f in frames]
cols, w, h = 6, 320, 180
rows = (len(picks) + cols - 1) // cols
sheet = Image.new('RGB', (cols * w, rows * (h + 22)), (14, 14, 15))
d = ImageDraw.Draw(sheet); fnt = look.mono(15)
for i, f in enumerate(picks):
    x, y = i % cols * w, i // cols * (h + 22)
    sheet.paste(Image.open(os.path.join(src, 'f%04d.png' % f)).convert('RGB').resize((w, h), Image.LANCZOS), (x, y + 22))
    d.text((x + 4, y + 3), '%d:%04.1f' % (f // 1800, f / 30 % 60), font=fnt, fill=(200, 204, 200))
sheet.save(out)
print(out, sheet.size, len(picks), 'frames')
