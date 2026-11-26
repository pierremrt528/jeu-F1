import math, json, sys
import design
from design import trace, solve
def build(br, ex, tun, pool=95, tex=90, pit=85):
    s=[list(q) for q in design.SEG]
    s[21][1]=300; s[20][1]=tex; s[6][1]=br; s[22][1]=ex; s[21][2]=tun; s[35][2]=125-(tun-55)
    s[31][1]=pool; s[28][1]=pit
    s[38][1]=180; s[26][1]=190
    return solve([tuple(q) for q in s],38,26)
if __name__=='__main__':
    best=None
    for br in range(250,451,20):
      for ex in range(100,321,20):
        for tun in (45,50,55,60,65,70):
          for pool in (60,95):
            s=build(br,ex,tun,pool)
            p,h=trace(s,2.0)
            err=(s[38][1]-200)**2+(s[26][1]-190)**2
            if best is None or err<best[0]: best=(err,br,ex,tun,pool,round(s[38][1]),round(s[26][1]),round(p[-1][2]),round(math.degrees(h)))
    print(best)
