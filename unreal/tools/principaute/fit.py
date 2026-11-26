import math
from design import SEG, trace, solve
base=[list(s) for s in SEG]
base[21][1]=300   # rayon du tunnel
base[20][1]=90
def run(br, ex, tun):
    s=[list(q) for q in base]; s[6][1]=br; s[22][1]=ex; s[21][2]=tun
    s[38][1]=180; s[26][1]=190
    s=[tuple(q) for q in s]
    s=solve(s,38,26)
    return s
best=None
for br in range(250,451,20):
  for ex in range(80,301,20):
    for tun in (40,45,50,55,60):
      try: s=run(br,ex,tun)
      except ZeroDivisionError: continue
      p,_=trace(s,2.0)
      err=(s[38][1]-185)**2+(s[26][1]-190)**2
      if best is None or err<best[0]: best=(err,br,ex,tun,s[38][1],s[26][1],p[-1][2])
print(best)
