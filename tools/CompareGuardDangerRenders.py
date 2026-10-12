"""Convert actual Vulkan danger captures losslessly and assemble an observation sheet."""
import argparse,json
from pathlib import Path
from PIL import Image,ImageDraw
parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path)
directory=parser.parse_args().directory.resolve()
names=('danger-ready','danger-escape','danger-clear')
sheet=Image.new('RGB',(1920,384),(18,18,18));draw=ImageDraw.Draw(sheet)
for index,name in enumerate(names):
    with Image.open(directory/(name+'.ppm')) as source:
        if source.size!=(1280,720):raise ValueError('Expected 1280 x 720 native captures')
        source.save(directory/(name+'.png'))
        sheet.paste(source.resize((640,360)),(index*640,24))
    draw.text((index*640+8,6),name,fill=(230,230,230))
sheet.save(directory/'danger-observation.png')
print(json.dumps({'captures':list(names),'note':'Cameras follow the encounter focus; runtime logs prove actual displacement, survival and recovery. No pixel-difference quality score is inferred.'},indent=2))
