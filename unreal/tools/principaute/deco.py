import os
import json, math, random
from geo import *
R=random.Random(20261126)
OUT={}
WZ=0.8
def left_off(chain, D):
    """chain : [(x, z, k)] axe lissé ; D(k) distance ; renvoie points à gauche"""
    out=[]
    for j,(x,z,k) in enumerate(chain):
        a=chain[max(0,j-1)]; b=chain[min(len(chain)-1,j+1)]
        tx,tz=b[0]-a[0],b[1]-a[1]; l=math.hypot(tx,tz) or 1; tx/=l; tz/=l
        d=D(k); out.append((x+tz*d, z-tx*d))
    return out
def D_base(k):
    if k<MK['Chicane']-5: return WL[k]+17
    if k<MK['Tabac']: return WL[k]+15
    return WL[k]+11
# tribunes : emplacements choisis ici, avant l'eau, pour que le quai s'écarte derrière elles (elles ne pendent pas au-dessus du bassin)
def stand_pts(x,z,yaw,s,length=38):
    """emprise des tribunes d'Unreal : rangs de -6 m (côté piste) à +8 m (toit) autour du centre, sur toute la longueur"""
    fx,fz=math.sin(yaw),math.cos(yaw); ax,az=-s*fz,s*fx     # ax,az : s'éloigne de la piste (latéral positif = à droite = (-fz, fx))
    return [(x+fx*a+ax*b, z+fz*a+az*b) for a in range(-int(length/2)-2,int(length/2)+3,3) for b in (-6,-3,0,4,8)]
PLAN=[]
def plan_stand(k0,k1,side):
    k=k0
    while k+18<=k1:
        i=(k+10)%N; s=-1 if side<0 else 1; yaw=math.atan2(TX[i],TZ[i])
        for extra in (8,9.5):
            x,z=lat(i,s*((WL[i] if s<0 else WR[i])+extra))
            # chaque point de l'emprise reste à plus de 1,5 m des murs de TOUS les échantillons (virages, épingles)
            if all(clear_of_track(px,pz,1.5) for px,pz in stand_pts(x,z,yaw,s)): PLAN.append((i,x,z,yaw,s)); break
        k+=22
plan_stand(MK['Quai']+6,MK['Tabac']-4,-1)
# (pas de tribune dans le virage du Tabac : à l'intérieur d'un virage, elle mord sur la piste)
plan_stand(MK['Piscine']+4,MK['Piscine 2']-6,-1)
plan_stand(MK['Sainte-Dévote']-6,MK['Sainte-Dévote']+14,-1)
plan_stand(MK['Rascasse']-6,MK['Rascasse']+12,-1)
def D_of(k):
    d=D_base(k); w=0.0
    for (i,x,z,yaw,s) in PLAN:
        if s<0:
            t=abs(k-i); w=max(w,1.0 if t<=18 else max(0.0,1-(t-18)/14))
    if w>0: d+=(max(d,WL[k]+19.5)-d)*(0.5-0.5*math.cos(math.pi*w))
    return d
cA=smooth_line(MK['Portier']+12, MK['Chicane']-5)
cB=smooth_line(MK['Chicane']+8, MK['Tabac']+10)
cC=smooth_line(MK['Tabac']+10, MK['Piscine 2']+24)
A=left_off(cA,D_of)[::4]; B=left_off(cB,D_of)[::4]; C=left_off(cC,D_of)[::4]
NE=[EN(*p) for p in [(6000,5700),(1700,1250),(1250,800),(1060,600),(985,530)]]
SQ=[EN(*p) for p in [(110,-215),(230,-250),(380,-258),(520,-238),(610,-195),(650,-168)]]
ROCK=[EN(*p) for p in [(690,-230),(665,-360),(570,-570),(330,-650),(60,-690),(-250,-660),(-600,-720)]]
FAR=[EN(*p) for p in [(-5000,-5000),(-2000,-9500),(4000,-9000),(9000,-3000),(9500,3000)]]
water=NE+A+B+C+SQ+ROCK+FAR
OUT['waterZ']=WZ; OUT['water']=[[round(a,1),round(b,1)] for a,b in water]
OUT['quays']=[[[round(a,1),round(b,1)] for a,b in NE[1:]+A+B+C+SQ+ROCK[:2]]]
# digues : nord (depuis la côte après le tunnel) et sud (pointe du rocher)
J1=(EN(700,150),EN(770,-55)); J2=(EN(645,-170),EN(705,-112))
OUT['jetties']=[[*J1[0],*J1[1],16],[*J2[0],*J2[1],12]]
inside=lambda x,z: pip(x,z,water)
# relief : pente vers l'intérieur des terres (côte ~ du SO au NE), rocher en plateau
p0=EN(-600,-720); p1=EN(1700,1250); dx,dz=p1[0]-p0[0],p1[1]-p0[1]; l=math.hypot(dx,dz)
nrm=(-dz/l,dx/l)
# l'intérieur des terres est du côté de la ligne de départ
if (0-p0[0])*nrm[0]+(0-p0[1])*nrm[1]<0: nrm=(-nrm[0],-nrm[1])
OUT['hills']={'p0':list(p0),'n':[round(nrm[0],4),round(nrm[1],4)],'start':300,'grad':0.32,'max':480}
RK=EN(200,-450)
OUT['plateaus']=[[RK[0],RK[1],430,140,0.0,58]]
def inland(x,z): return (x-p0[0])*nrm[0]+(z-p0[1])*nrm[1]
def on_rock(x,z,m=1.0):
    u=(x-RK[0])/430; v=(z-RK[1])/140; return u*u+v*v<m
# zones réservées (est, nord)
PAD=[EN(*p) for p in [(8,-232),(8,218),(78,218),(78,95),(205,95),(198,58),(88,-196),(42,-236)]]
reserved=[PAD]
def in_reserved(x,z): return any(pip(x,z,p) for p in reserved)
occ=Occ()
def ok_rect(poly,margin):
    for (x,z) in poly+[(sum(p[0] for p in poly)/4,sum(p[1] for p in poly)/4)]:
        if inside(x,z) or in_reserved(x,z) or not clear_of_track(x,z,margin): return False
    return occ.free(poly)
B_OUT=[]
def put(x,z,yaw,w,d,h,kind,base=-999,margin=2.5,force=False):
    poly=rect(x,z,yaw,w,d)
    if not force and not ok_rect(poly,margin): return False
    occ.add(poly); B_OUT.append([round(x,1),round(z,1),round(yaw,3),round(w,1),round(d,1),round(h,1),kind,round(base,2)]); return True
def k_of(name): return MK[name]
def wrap(k): return k%N
# --- monuments
LM=[]
def landmark(name,k,side,setback,depth,scale=1.0,dyaw=0.0):
    i=wrap(k); s=1 if side>0 else -1
    lat_=(WR[i] if s>0 else WL[i])+setback+depth/2
    x,z=lat(i,s*lat_)
    yaw=math.atan2(TZ[i],-TX[i]) if s>0 else math.atan2(-TZ[i],TX[i])
    LM.append([name,round(x,1),round(z,1),round(yaw+dyaw,3),scale])
    return x,z,yaw
# casino (dehors du virage du casino, à gauche) ; grand hôtel à l'extérieur de Massenet (droite)
cx,cz,cy=landmark('casino',k_of('Casino')+14,-1,9,42)
occ.add(rect(cx,cz,cy,74,46))
hx,hz,hy=landmark('hotel',k_of('Massenet')+20,1,6,30)
occ.add(rect(hx,hz,hy,70,32))
# jardin du casino (intérieur du virage)
gi=wrap(k_of('Casino')+8); gx,gz=lat(gi,WR[gi]+30)
GARD=[[round(gx,1),round(gz,1),26]]
occ.add(rect(gx,gz,0,52,52))
# palais sur le rocher
px,pz=EN(330,-430); LM.append(['palais',round(px,1),round(pz,1),round(math.atan2(0,1),3),1.0]); occ.add(rect(px,pz,0,100,60))
OUT['landmarks']=LM; OUT['gardens']=GARD
# --- tribunes (le long du port) : réservent la place
ST=[]
for (i,x,z,yaw,sd) in PLAN:
    if any(inside(px,pz) for px,pz in stand_pts(x,z,yaw,sd)): continue
    if not occ.free(rect(x,z,yaw,14,38)): continue
    ST.append([round(x,1),round(z,1),round(yaw,3),36]); occ.add(rect(x,z,yaw,14,38))
OUT['stands']=ST
# tribunes du départ (BuildStands, à gauche de K -60 à +12) : place prise
for K in range(-62,16,6):
    i=wrap(K); x,z=lat(i,-(WL[i]+10)); occ.add(rect(x,z,math.atan2(TX[i],TZ[i]),16,10))
# hôtel au-dessus du tunnel (Unreal : base = altitude de la piste + 6,5 m)
TUN=[near(*json.load(open('points.json'))['tunnel'][0]), near(*json.load(open('points.json'))['tunnel'][1])]
for k in range(TUN[0]+6, TUN[0]+110, 12):
    i=wrap(k); x,z=lat(i,10); yaw=math.atan2(TX[i],TZ[i])
    poly=rect(x,z,yaw,30,26); occ.add(poly)
    B_OUT.append([round(x,1),round(z,1),round(yaw,3),30,26,round(24+6*math.sin(k*0.07),1),1,round(Y[i]+6.6,2)])
# --- façades le long de la piste
def zone(k,s):
    k=wrap(k)
    if k>=1555 or k<200: return (18,42,(0.55,0.45,0)) if s<0 else None
    if k<240: return (16,30,(0.6,0.4,0))
    if k<480: return (16,36,(0.5,0.5,0)) if s<0 else (24,58,(0.25,0.55,0.2))
    if k<520: return (22,36,(0.6,0.4,0))
    if k<600: return (18,30,(0.7,0.3,0))
    if k<690: return (20,44,(0.4,0.5,0.1))
    if k<870: return (24,48,(0.3,0.5,0.2))
    if k<1185: return (30,70,(0.1,0.5,0.4)) if s>0 else None
    if k<1340: return (18,44,(0.4,0.5,0.1)) if s>0 else None
    if k<1505: return None
    return (12,24,(1,0,0)) if s<0 else None
def pick(wts):
    r=R.random(); a=0
    for i,w in enumerate(wts):
        a+=w
        if r<a: return i
    return 0
for s in (-1,1):
    k=0
    while k<N:
        z_=zone(k,s)
        if not z_: k+=4; continue
        i=wrap(k)
        L_=R.uniform(16,30); Dp=R.uniform(14,24)
        wall=(WR[i] if s>0 else WL[i])
        x,z=lat(i,s*(wall+3.2+Dp/2))
        yaw=math.atan2(TX[i],TZ[i])
        h=R.uniform(z_[0],z_[1]); kind=pick(z_[2])
        if put(x,z,yaw,Dp,L_,h,kind,margin=2.6): k+=int((L_+R.uniform(0.5,3))/2)
        else: k+=3
# deuxième rangée derrière la première
for s in (-1,1):
    for k in range(0,N,9):
        z_=zone(k,s)
        if not z_: continue
        i=wrap(k); wall=(WR[i] if s>0 else WL[i])
        Dp=R.uniform(16,26); L_=R.uniform(18,30)
        x,z=lat(i,s*(wall+30+Dp/2+R.uniform(0,10)))
        put(x,z,math.atan2(TX[i],TZ[i])+R.uniform(-0.15,0.15),Dp,L_,R.uniform(z_[0]+6,z_[1]+20),pick(z_[2]),margin=12)
# --- coteau : immeubles jusqu'à ~1 km de la piste
nb=0
for t in range(9000):
    e=R.uniform(-650,1500); n=R.uniform(-760,1300); x,z=EN(e,n)
    if inside(x,z): continue
    d,i=tdist(x,z)
    if d>1100 or d<30: continue
    rock=on_rock(x,z,0.92)
    if rock: h=R.uniform(9,19); kind=0
    else:
        dd=inland(x,z)
        h=R.uniform(18,48)+ (R.uniform(0,70) if R.random()<0.35 else 0)
        if e>650 and n>350: h+=R.uniform(10,60)        # tours au nord-est
        kind=pick((0.15,0.5,0.35) if h>40 else (0.45,0.5,0.05))
    w=R.uniform(16,30); dp=R.uniform(14,26)
    if R.random()<min(1.0,1.6-d/800):
        th=math.atan2(dx,dz)+(math.pi/2 if R.random()<0.5 else 0)+R.uniform(-0.06,0.06)
        if nb<1150 and put(x,z,th,w,dp,h,kind,margin=14): nb+=1
OUT['buildings']=B_OUT
print('immeubles',len(B_OUT),'coteau',nb)
# --- yachts
Y_OUT=[]; yocc=Occ(30)
def yacht(x,z,yaw,L,kind=0):
    poly=rect(x,z,yaw,L*0.24+1.2,L+2)
    for (px_,pz_) in poly:
        if not inside(px_,pz_): return False
    for J in OUT['jetties']:
        jp=rect((J[0]+J[2])/2,(J[1]+J[3])/2,math.atan2(J[2]-J[0],J[3]-J[1]),J[4]+2,math.hypot(J[2]-J[0],J[3]-J[1]))
        if sat(poly,jp): return False
    if not yocc.free(poly): return False
    yocc.add(poly); Y_OUT.append([round(x,1),round(z,1),round(yaw,3),round(L,1),kind,R.randrange(8)]); return True
def moor(line,Lmin,Lmax,sail=0.0,step_extra=2.0):
    """yachts amarrés par l'arrière le long d'une ligne de quai (l'eau à gauche du sens de la ligne)"""
    j=0; acc=0.0
    seg=[(line[a],line[a+1]) for a in range(len(line)-1)]
    pos=0.0; total=sum(math.hypot(b[0]-a[0],b[1]-a[1]) for a,b in seg)
    while pos<total:
        L=R.uniform(Lmin,Lmax); kind=1 if R.random()<sail else 0
        if kind: L=R.uniform(11,18)
        beam=L*0.24
        # point sur la ligne
        p=pos; 
        for a,b in seg:
            sl=math.hypot(b[0]-a[0],b[1]-a[1])
            if p<=sl:
                t=p/sl; x=a[0]+(b[0]-a[0])*t; z=a[1]+(b[1]-a[1])*t; tx=(b[0]-a[0])/sl; tz=(b[1]-a[1])/sl; break
            p-=sl
        nx,nz=tz,-tx      # gauche
        yaw=math.atan2(nx,nz)
        yacht(x+nx*(L/2+1.2),z+nz*(L/2+1.2),yaw,L,kind)
        pos+=beam+step_extra
moor(B,14,34,0.15); moor(C,12,26,0.3)
moor([EN(*p) for p in [(110,-212),(230,-247),(380,-255),(520,-235),(610,-192)]][::-1],16,40,0.1)
# pontons au milieu du bassin (est-ouest), yachts des deux côtés
PON=[]
for n0 in (-10,-75,-140,-200):
    a=EN(255,n0); b=EN(560,n0+10)
    PON.append([round(a[0],1),round(a[1],1),round(b[0],1),round(b[1],1)])
    for side in (1,-1):
        L0=math.hypot(b[0]-a[0],b[1]-a[1]); tx=(b[0]-a[0])/L0; tz=(b[1]-a[1])/L0
        nx,nz=(tz,-tx) if side>0 else (-tz,tx)
        p=4.0
        while p<L0-4:
            L=R.uniform(12,24); kind=1 if R.random()<0.35 else 0
            if kind: L=R.uniform(10,16)
            x=a[0]+tx*p; z=a[1]+tz*p
            yacht(x+nx*(L/2+2.2),z+nz*(L/2+2.2),math.atan2(-nx,-nz),L,kind)
            p+=L*0.24+1.6
OUT['pontoons']=PON
# grands yachts le long de la digue nord (côté port)
(ax,az),(bx,bz)=J1; L0=math.hypot(bx-ax,bz-az); tx,tz=(bx-ax)/L0,(bz-az)/L0
p=10
while p<L0-10:
    L=R.uniform(45,78); nx,nz=(-tz,tx)
    x=ax+tx*p; z=az+tz*p
    # parallèle à la digue, du côté du bassin (ouest)
    for sgn in (1,-1):
        if yacht(x+sgn*nx*(8+L*0.13+2),z+sgn*nz*(8+L*0.13+2),math.atan2(tx,tz),L): break
    p+=L+6
# au mouillage, en mer
for t in range(60):
    e=R.uniform(760,1700); n=R.uniform(-600,500); x,z=EN(e,n)
    L=R.uniform(25,90) if R.random()<0.7 else R.uniform(11,18)
    yacht(x,z,R.uniform(0,6.283),L,0 if L>20 else 1)
OUT['yachts']=Y_OUT
print('yachts',len(Y_OUT))
# --- arbres et lampadaires
TR=[]
def tree(x,z,kind,sc=1.0,m=3.0):
    if inside(x,z) or not clear_of_track(x,z,m): return
    for p in [ (x,z) ]:
        if not occ.free(rect(x,z,0,3,3)): return
    TR.append([round(x,1),round(z,1),kind,round(sc,2)])
# palmiers le long du quai et de la piscine, jardin du casino
for chain,step in ((cB,8),(cC,8)):
    pts=left_off(chain,lambda k: WL[k]+3.2)
    for j in range(0,len(pts),step): tree(pts[j][0],pts[j][1],0,R.uniform(0.9,1.25),m=1.5)
for a in range(14):
    t=a/14*6.283; tree(gx+math.cos(t)*22,gz+math.sin(t)*22,0,1.1,m=1.0)
for a in range(6):
    t=a/6*6.283; tree(gx+math.cos(t)*10,gz+math.sin(t)*10,1,0.8,m=1.0)
# arbres sur le coteau et le rocher
for t in range(4000):
    e=R.uniform(-650,1500); n=R.uniform(-760,1300); x,z=EN(e,n)
    if inside(x,z) or in_reserved(x,z): continue
    d,i=tdist(x,z)
    if d<14 or d>1000: continue
    kind=2 if (on_rock(x,z) and R.random()<0.4) or inland(x,z)>650 and R.random()<0.5 else (0 if R.random()<0.35 else 1)
    tree(x,z,kind,R.uniform(0.75,1.3),m=6)
    if len(TR)>1000: break
OUT['trees']=TR
LP=[]
for k in range(0,N,16):
    i=wrap(k)
    if TUN[0]-4<=i<=TUN[1]+4: continue
    for s in (-1,1):
        if s>0 and (i>N-160 or i<60): continue
        wall=(WR[i] if s>0 else WL[i]); x,z=lat(i,s*(wall+1.4))
        if inside(x,z) or not occ.free(rect(x,z,0,1,1)): continue
        if min(math.hypot(X[q]-x,Z[q]-z) for q in range(N))<6.5: continue   # pas sur l'asphalte (épingle : l'autre branche passe tout près)
        yaw=math.atan2(TZ[i],-TX[i]) if s>0 else math.atan2(-TZ[i],TX[i])
        LP.append([round(x,1),round(z,1),round(yaw,3)])
OUT['lamps']=LP
# piscine (à droite de la section de la piscine)
i=wrap(k_of('Piscine')+38); x,z=lat(i,WR[i]+24)
OUT['pool']=[round(x,1),round(z,1),round(math.atan2(TX[i],TZ[i]),3),22,50]
OUT['tunnel']=TUN
print('arbres',len(TR),'lampadaires',len(LP),'tribunes',len(ST),'tunnel',TUN)
json.dump(OUT,open(os.path.dirname(os.path.abspath(__file__))+'/../../GrandPrixManager/Content/Data/tracks/principaute_deco.json','w'),separators=(',',':'))
# rendu de contrôle
v=View(1600,250)
v.poly([(a,b) for a,b in water],fill=(120,170,215))
v.poly(PAD,outline=(200,120,0))
for b in B_OUT:
    col={0:(200,180,150),1:(170,170,180),2:(120,150,190)}[b[6]]
    v.poly(rect(b[0],b[1],b[2],b[3],b[4]),fill=col,outline=(90,90,90))
for y in Y_OUT: v.poly(rect(y[0],y[1],y[2],y[3]*0.24,y[3]),fill=(255,255,255) if y[4]==0 else (250,200,120))
for s in ST: v.poly(rect(s[0],s[1],s[2],14,s[3]),fill=(40,70,160))
for t in TR: x,y=v.T(t[0],t[1]); v.d.ellipse([x-2,y-2,x+2,y+2],fill=(40,140,60))
for j in OUT['jetties']: v.d.line([v.T(j[0],j[1]),v.T(j[2],j[3])],fill=(150,150,150),width=int(j[4]*v.sc))
for l in LM: v.poly(rect(l[1],l[2],l[3],60,40),fill=(230,120,60))
v.track(); v.grid()
v.im.save('deco.png')
