import math, json
from lay import trace, solve
from PIL import Image, ImageDraw
lay=[['S',150],['R',300,8],['S',200],
 ['R',16,95],                       # Sainte-Dévote
 ['S',120],['L',250,18],['S',260],  # Beau Rivage (montée)
 ['L',45,105],                      # Massenet
 ['S',40],['R',22,75],['S',30],['L',60,15],['S',150],   # Casino puis descente
 ['R',13,105],['S',90],             # Mirabeau
 ['L',9,190],['S',60],              # épingle
 ['R',14,85],['S',70],              # Mirabeau bas
 ['R',14,95],['S',90],              # Portier
 ['R',260,55],['S',250],            # tunnel
 ['L',12,55],['S',15],['R',12,60],['S',130],   # chicane
 ['L',35,60],['S',110],             # Tabac
 ['L',35,30],['R',40,35],['S',100], # piscine 1
 ['R',20,45],['L',20,50],['S',60],  # piscine 2
 ['R',12,140],['S',40],             # Rascasse
 ['R',18,85],['S',100]]             # Noghès
import sys
names={0:'dep',3:'SD',7:'Mass',9:'Cas',13:'Mir',15:'Epi',17:'MirB',19:'Port',21:'Tun',23:'Chic',27:'Tab',29:'Pisc',32:'Pisc2',35:'Rasc',37:'Nog'}
p,h,_=trace(lay); print('heading',math.degrees(h),'res',p[-1], 'len', sum(math.hypot(p[i][0]-p[i-1][0],p[i][1]-p[i-1][1]) for i in range(1,len(p))))
#lay=solve(lay, 2, 22)
p,h,marks=trace(lay); print('after', p[-1], lay[2], lay[22]); L=sum(math.hypot(p[i][0]-p[i-1][0],p[i][1]-p[i-1][1]) for i in range(1,len(p))); print('len',L)
xs=[q[0] for q in p]; zs=[q[1] for q in p]; mn=min(min(xs),min(zs))
W=900; sc=(W-80)/max(max(xs)-min(xs),max(zs)-min(zs))
im=Image.new('RGB',(W,W),'white'); d=ImageDraw.Draw(im)
T=lambda q:(40+(q[0]-min(xs))*sc, W-40-(q[1]-min(zs))*sc)
d.line([T(q) for q in p],fill='black',width=4)
for i,m in enumerate(marks):
    if i in names: d.text(T(p[m]),names[i],fill='red')
d.ellipse([T(p[0])[0]-6,T(p[0])[1]-6,T(p[0])[0]+6,T(p[0])[1]+6],fill='green')
im.save('lay.png'); json.dump(lay,open('lay.json','w'))
