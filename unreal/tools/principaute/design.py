import math, json, copy
from PIL import Image, ImageDraw
# segments : (type, a, b, alt au début, extra mur gauche, extra mur droit, nom)
# virage : R/L rayon angle ; droite : S longueur
SEG=[
 ('S',150,0, 7.0, 2.0,0,'ligne'),
 ('R',400,6, 7.4, 2.0,0,''),
 ('S',95,0, 7.8, 2.0,0,''),
 ('R',17,92, 8.3, 7.0,0.5,'Sainte-Dévote'),
 ('S',60,0, 9.5, 1.0,0.5,''),
 ('L',300,28, 12.0, 0.5,0.5,'Beau Rivage'),
 ('S',420,0, 16.0, 0.5,0.8,''),
 ('L',55,78, 35.5, 0.5,2.5,'Massenet'),
 ('S',35,0, 39.5, 0.5,1.0,''),
 ('R',20,100, 40.5, 4.0,0.5,'Casino'),
 ('S',40,0, 41.5, 1.0,0.5,''),
 ('L',80,12, 41.2, 0.5,0.5,''),
 ('S',170,0, 40.5, 0.5,0.5,''),
 ('R',14,115, 33.5, 2.5,0.4,'Mirabeau'),
 ('S',110,0, 32.5, 0.5,0.5,''),
 ('L',12.5,185, 26.5, 0.4,2.5,'Épingle'),
 ('S',45,0, 25.5, 0.5,0.5,''),
 ('R',14,90, 23.5, 2.0,0.4,'Mirabeau bas'),
 ('S',55,0, 21.0, 0.5,0.5,''),
 ('R',16,95, 14.0, 2.5,0.4,'Portier'),
 ('S',100,0, 12.5, 0.8,0.8,''),
 ('R',350,55, 11.0, 0.8,0.8,'Tunnel'),
 ('S',230,0, 8.8, 0.8,0.8,''),
 ('L',13,55, 4.4, 0.5,9.0,'Chicane'),
 ('S',12,0, 4.2, 0.5,4.0,''),
 ('R',13,65, 4.0, 3.0,0.5,''),
 ('S',170,0, 3.8, 0.6,0.6,'Quai'),
 ('L',35,70, 3.5, 0.5,3.0,'Tabac'),
 ('S',85,0, 3.4, 0.6,0.6,''),
 ('L',40,30, 3.3, 0.5,1.5,'Piscine'),
 ('R',40,32, 3.3, 1.5,0.5,''),
 ('S',95,0, 3.4, 0.6,0.6,''),
 ('R',16,45, 3.5, 2.0,0.4,'Piscine 2'),
 ('L',16,52, 3.6, 0.4,2.0,''),
 ('S',55,0, 3.8, 0.6,0.6,''),
 ('R',12.5,125, 4.1, 2.5,0.4,'Rascasse'),
 ('S',35,0, 4.6, 0.6,0.6,''),
 ('R',20,50, 5.2, 2.0,0.4,'Noghès'),
 ('S',240,0, 5.8, 2.0,0,''),
]
def trace(seg, step=1.0):
    pts=[(0.0,0.0,0.0,0)]; h=0.0; x=z=0.0; s=0.0
    for si,sg in enumerate(seg):
        if sg[0]=='S':
            n=max(1,math.ceil(sg[1]/step))
            for k in range(n): x+=math.sin(h)*sg[1]/n; z+=math.cos(h)*sg[1]/n; s+=sg[1]/n; pts.append((x,z,s,si))
        else:
            a=(1 if sg[0]=='L' else -1)*sg[2]*math.pi/180; L=sg[1]*abs(a); n=max(1,math.ceil(L/step))
            for k in range(n): h+=a/n/2; x+=math.sin(h)*L/n; z+=math.cos(h)*L/n; h+=a/n/2; s+=L/n; pts.append((x,z,s,si))
    return pts,h
def solve(seg,i1,i2):
    for it in range(40):
        p,_=trace(seg,2.0); ex,ez=p[-1][0],p[-1][1]
        if math.hypot(ex,ez)<0.02: break
        J=[]
        for i in (i1,i2):
            l2=[list(q) for q in seg]; l2[i][1]+=1.0; q,_=trace(l2,2.0); J.append((q[-1][0]-ex,q[-1][1]-ez))
        a,b=J[0]; c,d=J[1]; det=a*d-b*c
        seg[i1]=tuple([seg[i1][0],seg[i1][1]+(-ex*d+ez*c)/det]+list(seg[i1][2:]))
        seg[i2]=tuple([seg[i2][0],seg[i2][1]+(-a*ez+b*ex)/det]+list(seg[i2][2:]))
    return seg
if __name__=='__main__':
    p,h=trace(SEG); print('cap',round(math.degrees(h),2),'résidu',[round(v,1) for v in p[-1][:2]],'long',round(p[-1][2]))
    seg=solve([tuple(s) for s in SEG],38,26)
    print('après', seg[38][1], seg[26][1])
    p,h=trace(seg); print('long', round(p[-1][2]), 'résidu', p[-1][:2])
    json.dump(seg,open('seg.json','w'))
    # rendu (est = -x)
    xs=[-q[0] for q in p]; zs=[q[1] for q in p]
    W=1000; sc=(W-80)/max(max(xs)-min(xs),max(zs)-min(zs))
    im=Image.new('RGB',(W,W),'white'); d=ImageDraw.Draw(im)
    T=lambda X,Z:(40+(X-min(xs))*sc, W-40-(Z-min(zs))*sc)
    d.line([T(-q[0],q[1]) for q in p],fill='black',width=5)
    last=-1
    for q in p:
        if q[3]!=last:
            last=q[3]; nm=seg[q[3]][6]
            if nm: d.text(T(-q[0],q[1]),nm,fill='red')
    d.ellipse([T(0,0)[0]-7,T(0,0)[1]-7,T(0,0)[0]+7,T(0,0)[1]+7],fill='green')
    # échelle 100 m
    d.line([(40,W-20),(40+100*sc,W-20)],fill='blue',width=3)
    im.save('design.png')
