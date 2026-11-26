import os
import json, math
class Mesh:
    def __init__(s): s.P=[]; s.N=[]; s.K=[]; s.I=[]
    def face(s,pts,K,inside):
        # normale de Newell, tournée à l'opposé du point « inside »
        nx=ny=nz=0.0
        for a in range(len(pts)):
            x1,y1,z1=pts[a]; x2,y2,z2=pts[(a+1)%len(pts)]
            nx+=(y1-y2)*(z1+z2); ny+=(z1-z2)*(x1+x2); nz+=(x1-x2)*(y1+y2)
        l=math.sqrt(nx*nx+ny*ny+nz*nz)
        if l<1e-9: return
        nx/=l; ny/=l; nz/=l
        c=[sum(p[i] for p in pts)/len(pts) for i in range(3)]
        if (c[0]-inside[0])*nx+(c[1]-inside[1])*ny+(c[2]-inside[2])*nz<0: nx,ny,nz=-nx,-ny,-nz
        b=len(s.K)
        for p in pts: s.P+= [round(p[0],3),round(p[1],3),round(p[2],3)]; s.N+=[round(nx,3),round(ny,3),round(nz,3)]; s.K.append(K)
        for t in range(1,len(pts)-1): s.I+=[b,b+t,b+t+1]
    def box(s,x0,x1,y0,y1,z0,z1,K,Ktop=None,bottom=False,Kside=None):
        c=((x0+x1)/2,(y0+y1)/2,(z0+z1)/2)
        V=lambda a,b,d:(a,b,d)
        s.face([V(x0,y1,z0),V(x1,y1,z0),V(x1,y1,z1),V(x0,y1,z1)],K if Ktop is None else Ktop,c)
        if bottom: s.face([V(x0,y0,z0),V(x1,y0,z0),V(x1,y0,z1),V(x0,y0,z1)],K,c)
        ks=K if Kside is None else Kside
        s.face([V(x0,y0,z1),V(x1,y0,z1),V(x1,y1,z1),V(x0,y1,z1)],ks,c)
        s.face([V(x0,y0,z0),V(x1,y0,z0),V(x1,y1,z0),V(x0,y1,z0)],ks,c)
        s.face([V(x0,y0,z0),V(x0,y0,z1),V(x0,y1,z1),V(x0,y1,z0)],ks,c)
        s.face([V(x1,y0,z0),V(x1,y0,z1),V(x1,y1,z1),V(x1,y1,z0)],ks,c)
    def prism(s,poly,y0,y1,Kside,Ktop,bands=None):
        """polygone convexe (x, z) extrudé ; bands = [(y0, y1, K)] pour les côtés"""
        cx=sum(p[0] for p in poly)/len(poly); cz=sum(p[1] for p in poly)/len(poly); c=(cx,(y0+y1)/2,cz)
        for a in range(len(poly)):
            (x1,z1),(x2,z2)=poly[a],poly[(a+1)%len(poly)]
            for (b0,b1,K) in (bands or [(y0,y1,Kside)]):
                s.face([(x1,b0,z1),(x2,b0,z2),(x2,b1,z2),(x1,b1,z1)],K,c)
        if Ktop is not None: s.face([(x,y1,z) for x,z in poly],Ktop,(cx,y1-1,cz))
    def cyl(s,x,z,y0,y1,r,seg,K,top=True,r1=None):
        r1=r if r1 is None else r1
        pts=[(math.cos(a*2*math.pi/seg),math.sin(a*2*math.pi/seg)) for a in range(seg)]
        c=(x,(y0+y1)/2,z)
        for a in range(seg):
            (c1,s1),(c2,s2)=pts[a],pts[(a+1)%seg]
            s.face([(x+r*c1,y0,z+r*s1),(x+r*c2,y0,z+r*s2),(x+r1*c2,y1,z+r1*s2),(x+r1*c1,y1,z+r1*s1)],K,c)
        if top and r1>0: s.face([(x+r1*c1,y1,z+r1*s1) for c1,s1 in pts],K,(x,y1-1,z))
    def dome(s,x,z,y0,r,seg,rings,K):
        c=(x,y0,z)
        for j in range(rings):
            a0=j/rings*math.pi/2; a1=(j+1)/rings*math.pi/2
            for a in range(seg):
                t0=a*2*math.pi/seg; t1=(a+1)*2*math.pi/seg
                P=lambda t,ph:(x+r*math.cos(ph)*math.cos(t),y0+r*math.sin(ph),z+r*math.cos(ph)*math.sin(t))
                pts=[P(t0,a0),P(t1,a0),P(t1,a1)] + ([P(t0,a1)] if j<rings-1 else [])
                s.face(pts,K,c)
    def out(s): return {'P':s.P,'N':s.N,'K':s.K,'I':s.I,'ad':-1}

def hull(m,Ls,beam,sheer0,sheer1,K=0,deckK=14):
    zs=[-0.5,-0.33,-0.13,0.07,0.23,0.37,0.45,0.5]
    bs=[0.89,0.97,1.0,0.97,0.81,0.54,0.27,0.02]
    sec=[]
    for z,b in zip(zs,bs):
        zz=z*Ls; hb=b*beam; ys=sheer0+(sheer1-sheer0)*((z+0.5)**2)
        sec.append((zz,hb,ys))
    cen=(0,0.5,0)
    for a in range(len(sec)-1):
        (z1,b1,y1),(z2,b2,y2)=sec[a],sec[a+1]
        for sg in (1,-1):
            m.face([(sg*b1,y1,z1),(sg*b2,y2,z2),(sg*b2*0.97,0.6,z2),(sg*b1*0.97,0.6,z1)],K,cen)
            m.face([(sg*b1*0.97,0.6,z1),(sg*b2*0.97,0.6,z2),(sg*b2*0.86,-0.5,z2),(sg*b1*0.86,-0.5,z1)],K,cen)
        m.face([(b1,y1,z1),(b2,y2,z2),(-b2,y2,z2),(-b1,y1,z1)],deckK,(0,-5,0))
    z0,b0,y0=sec[0]
    m.face([(b0,y0,z0),(-b0,y0,z0),(-b0*0.86,-0.5,z0),(b0*0.86,-0.5,z0)],K,(0,0.5,0))
    return sec

def yacht():
    m=Mesh(); L=30.0; B=3.7
    sec=hull(m,L,B,2.0,2.8)
    m.box(-2.9,2.9,-0.15,0.25,-16.0,-15.0,14)                       # plateforme de bain
    # pont principal : salon vitré
    poly=[(-2.8,-10),(2.8,-10),(2.7,3),(1.7,6.5),(-1.7,6.5),(-2.7,3)]
    m.prism(poly,2.2,4.9,1,1,bands=[(2.2,3.1,1),(3.1,4.3,12),(4.3,4.9,1)])
    poly2=[(-2.2,-7),(2.2,-7),(2.1,1.5),(1.2,4),(-1.2,4),(-2.1,1.5)]
    m.prism(poly2,4.9,7.1,1,1,bands=[(4.9,5.4,1),(5.4,6.6,12),(6.6,7.1,1)])
    m.box(-1.6,1.6,7.1,7.5,-6.5,1.0,1)                               # hardtop
    m.box(-1.3,1.3,7.5,8.7,-4.2,-3.6,1)                              # arceau radar
    m.cyl(0,-3.9,8.7,10.4,0.12,6,2)
    m.box(-2.95,2.95,2.75,3.0,7.0,9.0,2)                             # garde-corps avant
    return m.out()

def voilier():
    m=Mesh(); L=12.0; B=1.95
    hull(m,L,B,1.1,1.35)
    m.prism([(-1.2,-3.2),(1.2,-3.2),(1.1,1.6),(0.5,2.6),(-0.5,2.6),(-1.1,1.6)],1.1,1.9,1,1,bands=[(1.1,1.45,1),(1.45,1.75,12),(1.75,1.9,1)])
    m.cyl(0,1.2,1.2,17.0,0.11,6,1,r1=0.07)                          # mât
    m.box(-0.06,0.06,2.6,2.75,-4.0,1.2,2)                            # bôme
    m.box(-0.18,0.18,2.75,3.25,-3.8,1.0,0)                           # voile ferlée (couleur de l'instance)
    return m.out()

def casino():
    m=Mesh()
    m.box(-30,30,0,18,-18,18,14)
    # fenêtres en arcades (façade avant, côtés)
    for row,(y0,y1) in enumerate(((3,8.5),(10.5,15))):
        for k in range(-5,6):
            x=k*5.0
            if abs(x)<9 and row==0: continue
            m.box(x-1.2,x+1.2,y0,y1,18.0,18.08,12)
            if row==0: m.dome(x,18.04,y1,1.2,6,2,12) if False else None
        for k in range(-3,4):
            z=k*5.0
            for sg in (1,-1): m.box(sg*30-0.04,sg*30+0.04,y0,y1,z-1.2,z+1.2,12)
    # corniche
    m.box(-30.6,30.6,17.4,18.4,-18.6,18.6,1)
    # toit à la Mansart
    m.face([(-30,18.4,-18),(30,18.4,-18),(27,23,-15),(-27,23,-15)],15,(0,19,0))
    m.face([(-30,18.4,18),(30,18.4,18),(27,23,15),(-27,23,15)],15,(0,19,0))
    m.face([(-30,18.4,-18),(-30,18.4,18),(-27,23,15),(-27,23,-15)],15,(0,19,0))
    m.face([(30,18.4,-18),(30,18.4,18),(27,23,15),(27,23,-15)],15,(0,19,0))
    m.face([(-27,23,-15),(27,23,-15),(27,23,15),(-27,23,15)],15,(0,19,0))
    # lucarnes
    for k in range(-4,5):
        m.box(k*6-0.9,k*6+0.9,19.2,21.2,16.6,17.4,1)
    # tours d'angle
    for sg in (1,-1):
        x=sg*24
        m.box(x-4.5,x+4.5,0,31,9.5,18.5,14)
        for y0 in (4,11,18,25): m.box(x-1.1,x+1.1,y0,y0+4,18.5,18.58,12)
        m.box(x-5,x+5,30.5,31.6,9,19,1)
        m.face([(x-4.6,31.6,9.4),(x+4.6,31.6,9.4),(x,40,14)],15,(x,33,14)); m.face([(x-4.6,31.6,18.6),(x+4.6,31.6,18.6),(x,40,14)],15,(x,33,14))
        m.face([(x-4.6,31.6,9.4),(x-4.6,31.6,18.6),(x,40,14)],15,(x,33,14)); m.face([(x+4.6,31.6,9.4),(x+4.6,31.6,18.6),(x,40,14)],15,(x,33,14))
    # tambour et dôme central
    m.cyl(0,2,23,27.5,7.5,16,14,top=False)
    for k in range(16):
        a=k/16*2*math.pi; m.box(7.52*math.cos(a)-0.04+0,7.52*math.cos(a)+0.04,24,26.5,2+7.52*math.sin(a)-0.7,2+7.52*math.sin(a)+0.7,12) if abs(math.cos(a))>0.9 else None
    m.dome(0,2,27.5,7.5,16,5,15)
    m.cyl(0,2,34.8,37,0.6,6,1)
    # portique d'entrée
    m.box(-9,9,10,11.5,18,23,1)
    for k in range(-3,4):
        m.cyl(k*2.6,22.2,0,10,0.45,8,1)
    m.box(-9.5,9.5,0,0.6,18,24,14)
    return m.out()

def palais():
    m=Mesh()
    def wing(x0,x1,z0,z1,h):
        m.box(x0,x1,0,h,z0,z1,0,Ktop=9)
        m.box(x0-0.4,x1+0.4,h-0.6,h+0.3,z0-0.4,z1+0.4,1,Ktop=9)
    wing(-48,48,14,30,16); wing(-48,-32,-30,14,14); wing(32,48,-30,14,14); wing(-32,32,-30,-16,14)
    for k in range(-11,12):
        x=k*4.0
        for y0 in (2.5,7,11.5): m.box(x-0.8,x+0.8,y0,y0+2.6,30,30.06,12)
    # tour crénelée
    m.box(-50,-38,0,26,22,34,0,Ktop=0)
    for a in range(5):
        for side in range(4):
            t=a*2.6
            if side==0: m.box(-50+t,-49+t,26,27.4,33.2,34,0)
            if side==1: m.box(-50+t,-49+t,26,27.4,22,22.8,0)
            if side==2: m.box(-50,-49.2,26,27.4,22+t,23+t,0)
            if side==3: m.box(-38.8,-38,26,27.4,22+t,23+t,0)
    for y0 in (5,12,19): m.box(-45,-43,y0,y0+3,34,34.06,12)
    # portail
    m.box(-4,4,0,8,30,31.5,1)
    m.box(-2.2,2.2,0,5.5,31.5,31.56,4)
    return m.out()

def lampadaire():
    m=Mesh()
    m.cyl(0,0,0,0.5,0.16,6,3)
    m.cyl(0,0,0.5,8.0,0.09,6,3,r1=0.06)
    m.box(-0.04,0.04,7.9,8.0,0,1.6,3)
    m.box(-0.25,0.25,7.75,7.95,1.2,2.0,3)
    m.box(-0.2,0.2,7.72,7.75,1.25,1.95,13)
    return m.out()

if __name__=='__main__':
    p=os.path.dirname(os.path.abspath(__file__))+'/../../GrandPrixManager/Content/Data/decor.json'
    d=json.load(open(p))
    for name,f in (('yacht',yacht),('voilier',voilier),('casino',casino),('palais',palais),('lampadaire',lampadaire)):
        d[name]=f(); print(name,len(d[name]['K']),'sommets',len(d[name]['I'])//3,'triangles')
    json.dump(d,open(p,'w'),separators=(',',':'))
