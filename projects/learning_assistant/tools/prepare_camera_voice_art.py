"""Reproduce the approved camera icon cutouts and native smooth-wave frames."""
from pathlib import Path
import math
from extract_design_icons import extract

ROOT=Path(__file__).resolve().parents[1]
PHASES=8
DEST=ROOT/'ui/assets'
extract(ROOT/'.codex-runs/v17-chatgpt-original.png',{
    'shutter':(39,690,220,845),'light':(250,693,386,842),
    'flip180':(421,701,581,848),'mic':(765,143,948,393),
},DEST)
for frame in range(PHASES+1):
    paths=[]
    for line,color in enumerate(['#55CFC6','#42B8EB','#00B9AA']):
        phase=0 if frame==0 else (frame-1)*math.tau/PHASES
        points=[]
        for i in range(97):
            t=i/96
            amplitude=(7 if frame==0 else 30)*math.sin(math.pi*t)**1.25
            y=50+(line-1)*5+amplitude*math.sin(t*math.tau*1.4-phase+line*.8)
            points.append(f'{"M" if i==0 else "L"}{4+392*t:.2f} {y:.2f}')
        paths.append(f'<path d="{" ".join(points)}" fill="none" stroke="{color}" stroke-width="{3 if line==2 else 2}" stroke-linecap="round"/>')
    (DEST/f'wave-{frame}.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="400" height="100" viewBox="0 0 400 100">'+''.join(paths)+'</svg>',encoding='utf-8')
print('Prepared camera icons and',PHASES,'smooth recording phases plus idle')
