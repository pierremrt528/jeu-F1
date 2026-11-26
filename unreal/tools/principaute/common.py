import os
import json, math
from PIL import Image, ImageDraw
T=json.load(open(os.path.dirname(os.path.abspath(__file__))+'/../../GrandPrixManager/Content/Data/tracks/principaute.json'))
N=T['N']; X=T['X']; Y=T['Y']; Z=T['Z']; WL=T['WL']; WR=T['WR']
TX=[0]*N; TZ=[0]*N
for i in range(N):
    a=(i-1)%N; b=(i+1)%N; tx=X[b]-X[a]; tz=Z[b]-Z[a]; l=math.hypot(tx,tz) or 1; TX[i]=tx/l; TZ[i]=tz/l
P=json.load(open('points.json'))
def near(x,z):
    return min(range(N),key=lambda i:(X[i]-x)**2+(Z[i]-z)**2)
MK={k:near(*v) for k,v in P['marks'].items()}
def lat(i,l):  # l>0 = droite
    return (X[i]-TZ[i]*l, Z[i]+TX[i]*l)
def dist_track(x,z):
    return min(math.hypot(X[i]-x,Z[i]-z) for i in range(0,N,2))
class View:
    def __init__(s,W=1400,pad=300):
        xs=[-v for v in X]; s.x0=min(xs)-pad; s.x1=max(xs)+pad; s.z0=min(Z)-pad; s.z1=max(Z)+pad
        s.sc=W/max(s.x1-s.x0,s.z1-s.z0); s.W=W
        s.im=Image.new('RGB',(W,W),(235,230,220)); s.d=ImageDraw.Draw(s.im)
    def T(s,x,z): return ((-x-s.x0)*s.sc, s.W-(z-s.z0)*s.sc)
    def grid(s):
        for g in range(-2000,2000,100):
            s.d.line([s.T(-g,s.z0),s.T(-g,s.z1)],fill=(210,205,195)); s.d.line([s.T(-s.x0,g),s.T(-s.x1,g)],fill=(210,205,195))
            s.d.text(s.T(-g,s.z0+5),str(-g),fill=(120,120,120)); s.d.text(s.T(-s.x0-5,g),str(g),fill=(120,120,120))
    def track(s):
        s.d.line([s.T(X[i],Z[i]) for i in list(range(N))+[0]],fill='black',width=max(2,int(10*s.sc)))
        for k,i in MK.items(): s.d.text(s.T(X[i],Z[i]),k,fill='red')
    def poly(s,pts,fill=None,outline=None,width=1):
        s.d.polygon([s.T(*p) for p in pts],fill=fill,outline=outline)
