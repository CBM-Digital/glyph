#!/usr/bin/env python3
"""Rebuild selected source atlases. Requires Pillow; originals stay untouched."""
from pathlib import Path
from PIL import Image
import argparse, shutil, json, hashlib, math, wave, struct
parser=argparse.ArgumentParser()
parser.add_argument('--source', type=Path, default=Path.home()/'Downloads')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]/'examples/arcade/assets'
original=root/'original'; original.mkdir(parents=True,exist_ok=True)
manifest={}
def atlas(name,pack,selections):
    out=Image.new('RGBA',(64*len(selections),64))
    frames={}
    for i,(label,relative) in enumerate(selections.items()):
        source=args.source/pack/relative
        sprite=Image.open(source).convert('RGBA')
        # Preserve original pixels, scale only small tiles by an integer factor.
        factor=max(1,min(3,56//max(sprite.size)))
        if factor>1:sprite=sprite.resize((sprite.width*factor,sprite.height*factor),Image.Resampling.NEAREST)
        if max(sprite.size)>56:sprite.thumbnail((56,56),Image.Resampling.LANCZOS)
        out.alpha_composite(sprite,(i*64+(64-sprite.width)//2,(64-sprite.height)//2))
        frames[label]=[i*64,0,64,64]
        manifest[str(source.relative_to(args.source))]=hashlib.sha256(source.read_bytes()).hexdigest()
    out.save(original/(name+'.png'))
    (original/(name+'.frames.json')).write_text(json.dumps(frames,indent=2)+'\n')
    shutil.copyfile(args.source/pack/'License.txt',original/(name+'-LICENSE.txt'))
atlas('ski','kenney_tiny-ski',dict(skier='Tiles/tile_0082.png',pine='Tiles/tile_0006.png',hut='Tiles/tile_0078.png',rock='Tiles/tile_0081.png',flag='Tiles/tile_0008.png',snow='Tiles/tile_0000.png'))
atlas('space','kenney_space-shooter-extension',dict(ship='PNG/Sprites/Ships/spaceShips_001.png',station='PNG/Sprites/Station/spaceStation_018.png',cargo='PNG/Sprites/Parts/spaceParts_001.png',asteroid='PNG/Sprites/Meteors/spaceMeteors_001.png',planet='PNG/Sprites/Meteors/spaceMeteors_004.png'))
fonts=root/'fonts';fonts.mkdir(exist_ok=True)
for name in ['NotoSans-Regular.ttf','NotoSans-SemiBold.ttf','OFL.txt']:shutil.copyfile(args.source/'Noto_Sans'/name,fonts/name)
(original/'sources.json').write_text(json.dumps(manifest,indent=2)+'\n')
# Original synthesized audio; no downloaded sound licenses or runtime synthesis cost.
audio=root/'audio';audio.mkdir(exist_ok=True)
def sound(name,notes,step,volume=.25):
    samples=[];rate=24000
    for freq in notes:
        for i in range(int(rate*step)):
            t=i/rate;envelope=min(1,t/.012)*max(0,1-t/step)**1.8
            value=(math.sin(2*math.pi*freq*t)+.2*math.sin(4*math.pi*freq*t))*envelope*volume
            samples.append(int(max(-1,min(1,value))*32767))
    with wave.open(str(audio/(name+'.wav')),'wb') as f:
        f.setnchannels(1);f.setsampwidth(2);f.setframerate(rate);f.writeframes(struct.pack('<'+'h'*len(samples),*samples))
sound('launch',[196,294,392],.055)
sound('dock',[523.25,659.25,783.99,1046.5],.1)
sound('impact',[110,73.4,49],.065)
sound('brake',[440,330,220],.055)
sound('gate',[659.25,987.77],.065)
sound('land',[164.81,246.94],.055)
sound('comet-theme',[130.81,196,261.63,196,146.83,220,293.66,220,164.81,246.94,329.63,246.94,146.83,220,196,146.83],.35,.075)
sound('alpine-theme',[196,246.94,293.66,392,329.63,293.66,246.94,220,174.61,220,261.63,349.23,293.66,261.63,220,196],.3,.06)
print('Imported named atlases, fonts, and eight sound cues/loops.')
