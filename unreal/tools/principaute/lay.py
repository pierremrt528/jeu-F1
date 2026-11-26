import math, json, sys
def trace(lay, step=2.0):
    pts=[(0.0,0.0)]; h=0.0; x=z=0.0; marks=[]
    for sg in lay:
        marks.append(len(pts)-1)
        if sg[0]=='S':
            n=max(1,math.ceil(sg[1]/step))
            for k in range(n): x+=math.sin(h)*sg[1]/n; z+=math.cos(h)*sg[1]/n; pts.append((x,z))
        else:
            a=(1 if sg[0]=='L' else -1)*sg[2]*math.pi/180; L=sg[1]*abs(a); n=max(1,math.ceil(L/step))
            for k in range(n): h+=a/n/2; x+=math.sin(h)*L/n; z+=math.cos(h)*L/n; h+=a/n/2; pts.append((x,z))
    return pts, h, marks
def solve(lay, i1, i2):
    # ajuste les longueurs des droites i1, i2 pour fermer la boucle
    import copy
    for it in range(30):
        p,h,_=trace(lay); ex,ez=p[-1]
        if math.hypot(ex,ez)<0.05: break
        J=[]
        for i in (i1,i2):
            l2=copy.deepcopy(lay); l2[i][1]+=1.0; q,_,_=trace(l2); J.append((q[-1][0]-ex,q[-1][1]-ez))
        a,b=J[0]; c,d=J[1]; det=a*d-b*c
        dx=(-ex*d+ez*c)/det; dy=(-a*ez+b*ex)/det
        lay[i1][1]+=dx; lay[i2][1]+=dy
    return lay
