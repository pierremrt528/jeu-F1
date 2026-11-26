import math, numpy as np
from common import *
XA=np.array(X); ZA=np.array(Z); WLA=np.array(WL); WRA=np.array(WR); YA=np.array(Y)
TXA=np.array(TX); TZA=np.array(TZ)
WMAX=np.maximum(WLA,WRA)
def EN(e,n): return (-e,n)          # (est, nord) -> (x, z) du jeu
def tdist(x,z):
    d=np.hypot(XA-x,ZA-z); i=int(np.argmin(d)); return d[i],i
def clear_of_track(x,z,m):
    """vrai si le point est à plus de (mur + m) de toute la piste"""
    d=np.hypot(XA-x,ZA-z)-WMAX
    return d.min()>m
def pip(x,z,poly):
    c=False; n=len(poly)
    for k in range(n):
        x1,z1=poly[k]; x2,z2=poly[(k+1)%n]
        if (z1>z)!=(z2>z) and x < (x2-x1)*(z-z1)/(z2-z1)+x1: c=not c
    return c
def rect(x,z,yaw,w,d):
    """coins d'un rectangle : w le long de x local, d le long de z local ; yaw = cap (sin, cos)"""
    fx,fz=math.sin(yaw),math.cos(yaw); rx,rz=fz,-fx
    return [(x+rx*sx*w/2+fx*sz*d/2, z+rz*sx*w/2+fz*sz*d/2) for sx,sz in ((-1,-1),(1,-1),(1,1),(-1,1))]
def sat(a,b):
    for poly in (a,b):
        for k in range(4):
            x1,z1=poly[k]; x2,z2=poly[(k+1)%4]; nx,nz=z2-z1,-(x2-x1)
            pa=[nx*p[0]+nz*p[1] for p in a]; pb=[nx*p[0]+nz*p[1] for p in b]
            if max(pa)<min(pb) or max(pb)<min(pa): return False
    return True
class Occ:
    def __init__(s,cell=40): s.c=cell; s.g={}
    def keys(s,poly):
        xs=[p[0] for p in poly]; zs=[p[1] for p in poly]
        for i in range(int(min(xs)//s.c),int(max(xs)//s.c)+1):
            for j in range(int(min(zs)//s.c),int(max(zs)//s.c)+1): yield (i,j)
    def free(s,poly):
        for k in s.keys(poly):
            for q in s.g.get(k,()):
                if sat(poly,q): return False
        return True
    def add(s,poly):
        for k in s.keys(poly): s.g.setdefault(k,[]).append(poly)
def smooth_line(i0,i1,r=20):
    """axe lissé de la piste (±r échantillons) entre i0 et i1"""
    out=[]
    for i in range(i0,i1+1):
        ks=[(i+k)%N for k in range(-r,r+1)]
        out.append((sum(X[k] for k in ks)/len(ks), sum(Z[k] for k in ks)/len(ks), i%N))
    return out
