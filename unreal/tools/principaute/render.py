import math, json, sys
import design
from design import trace
from fit2 import build
from PIL import Image, ImageDraw
SC=float(sys.argv[1]) if len(sys.argv)>1 else 1.12
s=build(250,180,65,95)
s=[(q[0], q[1]*SC if q[0]=='S' else q[1]*SC, q[2])+tuple(q[3:]) for q in s]
json.dump(s,open('seg.json','w'))
p,h=trace(s,1.0); print('long',round(p[-1][2]),'cap',round(math.degrees(h),2),'res',[round(v,2) for v in p[-1][:2]])
for i,q in enumerate(s): print(i,q[0],round(q[1],1),q[2],q[6])
xs=[-q[0] for q in p]; zs=[q[1] for q in p]
W=1000; sc=(W-80)/max(max(xs)-min(xs),max(zs)-min(zs))
im=Image.new('RGB',(W,W),'white'); d=ImageDraw.Draw(im)
T=lambda X,Z:(40+(X-min(xs))*sc, W-40-(Z-min(zs))*sc)
for g in range(-1000,1500,100):
    d.line([T(g,min(zs)),T(g,max(zs))],fill=(230,230,230)); d.line([T(min(xs),g),T(max(xs),g)],fill=(230,230,230))
d.line([T(-q[0],q[1]) for q in p],fill='black',width=5)
last=-1
for q in p:
    if q[3]!=last:
        last=q[3]; nm=s[q[3]][6]
        if nm: d.text(T(-q[0],q[1]),nm,fill='red')
d.ellipse([T(0,0)[0]-7,T(0,0)[1]-7,T(0,0)[0]+7,T(0,0)[1]+7],fill='green')
im.save('design.png')
print('étendue E',round(max(xs)-min(xs)),'N',round(max(zs)-min(zs)))
