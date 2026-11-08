"""Capture the optional GVIEW_UI_AUDIT firmware's final LCD frames over USB."""
import argparse
from pathlib import Path
import time
import re
import serial
from PIL import Image, ImageDraw

parser=argparse.ArgumentParser()
parser.add_argument('--port',default='/dev/cu.usbmodem1301')
parser.add_argument('--output',type=Path,default=Path('artifacts/ui-audit'))
args=parser.parse_args()
args.output.mkdir(parents=True,exist_ok=True)
frames={}
complete=[]
with serial.Serial(args.port,115200,timeout=.3) as device:
    deadline=time.monotonic()+240
    while time.monotonic()<deadline:
        line=device.readline().decode('ascii','ignore').strip()
        if '@UI_DONE' in line: break
        if '@UI_CHECK' in line: print(line,flush=True)
        if 'Guru Meditation' in line or 'assert failed' in line: raise RuntimeError(line)
        match=re.search(r'@UI (\w+) (\d+) ([0-9a-f:,]+)',line)
        if match:
            name,y,runs=match.groups()
            pixels=[]
            for color,count in re.findall(r'([0-9a-f]{4}):([0-9a-f]+),',runs):
                value=int(color,16)
                rgb=((value>>11)*255//31,((value>>5)&63)*255//63,(value&31)*255//31)
                pixels.extend([rgb]*int(count,16))
            if len(pixels)==240:frames.setdefault(name,{})[int(y)]=pixels
        end=re.search(r'@UI_END (\w+)',line)
        if end:
            name=end.group(1)
            if len(frames.get(name,{}))==240:
                im=Image.new('RGB',(240,240))
                im.putdata([pixel for y in range(240) for pixel in frames[name][y]])
                im.save(args.output/(name+'.png'))
                complete.append(name)
                print('captured',name,flush=True)
            else: print('incomplete',name,len(frames.get(name,{})),flush=True)
if complete:
    sheet=Image.new('RGB',(5*260,((len(complete)+4)//5)*270),'#ddd')
    draw=ImageDraw.Draw(sheet)
    for i,name in enumerate(complete):
        x=(i%5)*260+10;y=(i//5)*270+24
        sheet.paste(Image.open(args.output/(name+'.png')),(x,y))
        draw.text((x,y-18),name,fill='black')
    sheet.save(args.output/'contact-sheet.png')
print('frames',len(complete),flush=True)
