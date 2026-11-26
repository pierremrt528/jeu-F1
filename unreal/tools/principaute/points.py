import math, json
from design import trace
seg=[tuple(q) for q in json.load(open('seg.json'))]
# la ligne recule de 100 m : 0 = ligne d'arrivée (le tracé commence 100 m avant l'ancienne origine)
SHIFT=100.0
p,h=trace(seg,0.5)
L=p[-1][2]
HALF=5.0
# altitude et murs par mètre : marqueurs au début de chaque segment
starts={}
for q in p:
    if q[3] not in starts: starts[q[3]]=q[2]
marks=sorted((starts[i],seg[i][3]) for i in starts)+[(L,seg[0][3])]
def alt_at(s):
    for k in range(len(marks)-1):
        if marks[k][0]<=s<=marks[k+1][0]:
            t=(s-marks[k][0])/max(1e-6,marks[k+1][0]-marks[k][0]); return marks[k][1]+(marks[k+1][1]-marks[k][1])*t
    return marks[-1][1]
n=int(L)
A=[alt_at(i) for i in range(n)]
WLr=[0]*n; WRr=[0]*n; R=[999]*n
for q in p:
    i=int(q[2])
    if i<n: sg=seg[q[3]]; WLr[i]=HALF+1.75+sg[4]; WRr[i]=HALF+1.75+sg[5]; R[i]=sg[1] if sg[0]!='S' else 999
def sm(a,r):
    m=len(a); return [sum(a[(i+k)%m] for k in range(-r,r+1))/(2*r+1) for i in range(m)]
A=sm(sm(A,12),12); WLs=sm(WLr,8); WRs=sm(WRr,8)
# murs : jamais moins que le brut là où le brut est plus large (dégagements) ; lissé mais pas rogné
WLs=[max(a,min(b,a+3)) if b>a else a for a,b in zip(WLs,WLr)]
WRs=[max(a,min(b,a+3)) if b>a else a for a,b in zip(WRs,WRr)]
# points : pas adapté au rayon
pts=[]; last=-99; byS={}
for q in p: byS.setdefault(int(q[2]*2),q)
s=0.0
while s<L-1:
    i=int(s); q=byS.get(int(s*2)) or min(p,key=lambda r:abs(r[2]-s))
    rr=min(R[(i+k)%n] for k in range(-6,7))
    pts.append([round(q[0],1),round(q[1],1),round(A[i%n],2),round(WLs[i%n],1),round(WRs[i%n],1)])
    s+=max(2.5,min(8.0,rr*0.28))
# départ : 100 m avant l'origine du dessin, sur la droite de retour (Noghès → ligne)
sl=SHIFT; qs=min(p,key=lambda r:abs(r[2]-sl))
start=[round(qs[0],1),round(qs[1],1)]
# repères (pour le décor) : position de chaque début de segment
names={seg[i][6]:[round(min(p,key=lambda r:abs(r[2]-starts[i]))[0],1),round(min(p,key=lambda r:abs(r[2]-starts[i]))[1],1)] for i in starts if seg[i][6]}
tun0=min(p,key=lambda r:abs(r[2]-(starts[21]+40))); tun1=min(p,key=lambda r:abs(r[2]-(starts[22]+20)))
info={'start':start,'marks':names,'tunnel':[[round(tun0[0],1),round(tun0[1],1)],[round(tun1[0],1),round(tun1[1],1)]],'len':L}
json.dump({'points':pts,**info},open('points.json','w'))
print(len(pts),'points, long',round(L),'départ',start,'tunnel',info['tunnel'])
print('alt min/max',min(A),max(A))
