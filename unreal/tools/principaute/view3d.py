import os
import json, math, sys
from PIL import Image, ImageDraw
PAL=[0xd8d2c4,0xf2f2ef,0xb9bcc0,0x3a3e44,0x141619,0x7f9db5,0xd42a2a,0xf2c21a,0x1c1c1e,0xe8742a,0x2f8f4a,0xffffff,0x2a3a4a,0xfff3c4,0xc9b79c,0x6d7278]
d=json.load(open(os.path.dirname(os.path.abspath(__file__))+'/../../GrandPrixManager/Content/Data/decor.json'))
names=sys.argv[1:]
W=600; im=Image.new('RGB',(W*len(names),W),(200,215,230)); dr=ImageDraw.Draw(im)
L=(0.4,0.8,0.45); ln=math.sqrt(sum(v*v for v in L)); L=[v/ln for v in L]
for ni,name in enumerate(names):
    m=d[name]; P=m['P']; Nn=m['N']; K=m['K']; I=m['I']
    yaw=math.radians(35); pit=math.radians(25)
    def proj(x,y,z):
        X=x*math.cos(yaw)+z*math.sin(yaw); Z=-x*math.sin(yaw)+z*math.cos(yaw)
        Y=y*math.cos(pit)+Z*math.sin(pit); D=-y*math.sin(pit)+Z*math.cos(pit)
        return X,Y,D
    pts=[proj(P[3*i],P[3*i+1],P[3*i+2]) for i in range(len(K))]
    xs=[p[0] for p in pts]; ys=[p[1] for p in pts]
    sc=0.85*W/max(max(xs)-min(xs),max(ys)-min(ys)); cx=(max(xs)+min(xs))/2; cy=(max(ys)+min(ys))/2
    tris=[]
    for t in range(0,len(I),3):
        a,b,c=I[t],I[t+1],I[t+2]
        n=(Nn[3*a],Nn[3*a+1],Nn[3*a+2])
        nv=proj(*n)
        if nv[2]>0.05: continue     # dos à la caméra (caméra vers -D)
        dep=(pts[a][2]+pts[b][2]+pts[c][2])/3
        sh=0.45+0.55*max(0,n[0]*L[0]+n[1]*L[1]+n[2]*L[2])
        col=PAL[K[a]] if K[a]!=0 else 0xf4f4f4
        rgb=tuple(int(((col>>s)&255)*sh) for s in (16,8,0))
        tris.append((dep,[(ni*W+W/2+(pts[v][0]-cx)*sc, W/2-(pts[v][1]-cy)*sc) for v in (a,b,c)],rgb))
    tris.sort(key=lambda t:-t[0])
    for _,poly,rgb in tris: dr.polygon(poly,fill=rgb)
    dr.text((ni*W+10,10),name,fill='black')
im.save('models.png')
