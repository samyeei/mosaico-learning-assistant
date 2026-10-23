"""Build bounded UI icon cutouts from the user-approved ChatGPT design board.

Only removes the connected pale exterior; enclosed glass highlights stay intact.
The source PNG is retained with the design evidence, not fetched at build time.
"""
from pathlib import Path
from collections import deque
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
regions={
    'camera':(1005,108,1130,225), 'chat':(1174,107,1300,222),
    'wifi':(1340,114,1468,223), 'chip':(1012,278,1127,400),
    'back':(1180,290,1290,381), 'sun':(1344,276,1466,402),
    'speaker':(1012,458,1127,566), 'rotation_lock':(1174,450,1298,571),
    'gear':(1343,451,1466,567), 'mic':(1012,667,1138,827),
}
def extract(source_path,regions,destination):
    source=Image.open(source_path).convert('RGB')
    destination.mkdir(parents=True,exist_ok=True)
    for name,rect in regions.items():
        tile=source.crop(rect).convert('RGBA');w,h=tile.size;px=tile.load()
        seen=set();todo=deque([(x,y) for x in range(w) for y in (0,h-1)]+[(x,y) for y in range(h) for x in (0,w-1)])
        while todo:
            x,y=todo.popleft()
            if (x,y) in seen or not (0<=x<w and 0<=y<h):continue
            seen.add((x,y));r,g,b,_=px[x,y]
            if min(r,g,b)<165 or g-r>38 or b-r>48:continue
            px[x,y]=(r,g,b,0)
            todo.extend(((x-1,y),(x+1,y),(x,y-1),(x,y+1)))
        # Discard isolated exterior shadow flecks left by the pale background mask.
        seen=set()
        for y in range(h):
            for x in range(w):
                if (x,y) in seen or not px[x,y][3]:continue
                component=[];todo=deque([(x,y)])
                while todo:
                    u,v=todo.popleft()
                    if not (0<=u<w and 0<=v<h) or (u,v) in seen:continue
                    seen.add((u,v))
                    if not px[u,v][3]:continue
                    component.append((u,v));todo.extend(((u-1,v),(u+1,v),(u,v-1),(u,v+1)))
                if len(component)<24:
                    for u,v in component:px[u,v]=(*px[u,v][:3],0)
        side=max(w,h)+8;canvas=Image.new('RGBA',(side,side))
        canvas.alpha_composite(tile,((side-w)//2,(side-h)//2))
        canvas.resize((128,128),Image.Resampling.LANCZOS).save(destination/f'generated-{name}.png')

if __name__=='__main__':
    extract(ROOT/'.codex-runs/v16-chatgpt-original.png',regions,ROOT/'ui/assets')
    print('Extracted',len(regions),'ChatGPT icons')
