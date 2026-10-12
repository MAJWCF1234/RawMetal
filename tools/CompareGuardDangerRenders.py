"""Convert actual Vulkan danger captures losslessly and assemble an observation sheet."""
import argparse,json
from pathlib import Path
from PIL import Image,ImageDraw
parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path)
parser.add_argument('--names',nargs='+',default=['danger-ready','danger-escape','danger-clear'])
parser.add_argument('--columns',type=int,default=3)
parser.add_argument('--output',default='danger-observation.png')
args=parser.parse_args();directory=args.directory.resolve();names=args.names
if not 1<=args.columns<=4:parser.error('columns must be between one and four')
columns=min(args.columns,len(names));rows=(len(names)+columns-1)//columns
sheet=Image.new('RGB',(columns*640,rows*384),(18,18,18));draw=ImageDraw.Draw(sheet)
for index,name in enumerate(names):
    with Image.open(directory/(name+'.ppm')) as source:
        if source.size!=(1280,720):raise ValueError('Expected 1280 x 720 native captures')
        source.save(directory/(name+'.png'))
        sheet.paste(source.resize((640,360)),((index%columns)*640,(index//columns)*384+24))
    draw.text(((index%columns)*640+8,(index//columns)*384+6),name,fill=(230,230,230))
sheet.save(directory/args.output)
print(json.dumps({'captures':list(names),'note':'Cameras follow the encounter focus; runtime logs prove actual displacement, survival and recovery. No pixel-difference quality score is inferred.'},indent=2))
