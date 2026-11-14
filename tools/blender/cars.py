# Cinq monoplaces fictives, une par ère du règlement du jeu (js/reglement.js : ERAS[k].id).
import math
from mathutils import Vector
from carlib import Car, MAIN, ACC, HELM, CARBON, BLACK, VISOR, SCREEN, MIRROR, LIGHT, GREY, TYRE

BASE = dict(
    zf=1.75, zr=-1.6, tf=0.83, tr=0.8, rf=0.33, rr=0.36, wf=0.35, wr=0.43,   # roues : empattement, voies, rayons, largeurs
    nose=1.0, nose_h=0.0, nose_w=1.0,     # longueur, hauteur et largeur du nez
    fw_el=3, fw_span=1.0, fw_lift=0.07,   # aileron avant : éléments, envergure, relevé des bouts
    pod=1.0, pod_h=1.0, undercut=1.0, pod_len=1.0, inlet='oval',
    fin=1, rw_w=1.0, rw_h=0.0, rw_spoon=0.0, beam=1, rw_drs=False,
    floor='plat', barge=1, halo_r=0.032, screen=False, side_guard=False, cover_h=1.0,
)
ERAS = {
    # voiture de départ : longue, élancée, aileron avant à trois éléments, grande dérive, déflecteurs
    'fondation': dict(),
    # effet de sol : fond sculpté à tunnels, pontons très creusés, aileron avant à deux éléments enveloppant, aileron de poutre
    'effet-sol': dict(nose=0.93, nose_w=1.25, fw_el=2, fw_lift=0.16, pod=0.82, pod_h=0.92, undercut=1.8, inlet='slot',
                      fin=0, rw_w=0.94, rw_spoon=0.06, floor='tunnels', barge=0, tr=0.79),
    # hybride : plus courte, gros pontons de refroidissement, nez haut sur pylônes, aileron arrière bas et large
    'hybride': dict(zf=1.6, nose=0.86, nose_h=0.13, fw_span=0.92, fw_lift=0.04, pod=1.15, pod_h=1.08, undercut=0.6,
                    pod_len=0.92, inlet='large', rw_w=1.14, rw_h=-0.12, beam=0, fin=1, cover_h=1.06, barge=2),
    # aéro active : nez très long, aileron avant à quatre éléments, pontons étroits, très grande dérive, volet mobile
    'aero-active': dict(nose=1.1, nose_w=0.85, fw_el=4, fw_span=1.04, fw_lift=0.1, pod=0.72, pod_h=0.86, undercut=1.3,
                        inlet='slot', fin=2, rw_w=0.9, rw_h=0.1, rw_drs=True, barge=1, cover_h=0.95),
    # sécurité : plus large, aileron avant très large, gros pontons, protections latérales, pare-brise enveloppant, sans dérive
    'securite': dict(tf=0.88, tr=0.86, wf=0.37, wr=0.45, nose=0.97, nose_w=1.15, fw_el=2, fw_span=1.1, fw_lift=0.05,
                     pod=1.2, pod_h=1.1, undercut=0.8, fin=0, rw_w=1.05, rw_h=0.05, beam=0, halo_r=0.04, screen=True,
                     side_guard=True, barge=0, inlet='large'),
}


def params(era):
    p = dict(BASE); p.update(ERAS[era]); return p


def build_car(car: Car, S):
    NZ = lambda z: 2.1 + (z - 2.1) * S['nose'] if z > 2.1 else z
    dzn = NZ(3.4) - 3.4
    zf, zr = S['zf'], S['zr']
    shift_f = zf - 1.75          # empattement plus court : tout l'avant recule
    F = lambda z: z + shift_f if z > 0.9 else z   # avant de la voiture (nez, ailerons, suspensions avant)

    def livery(c):
        x, y, z = c
        if y < 0.2: return CARBON
        if abs(x) < 0.06 and y > 0.6 and -2.0 < z < 0.25: return ACC      # arête du capot
        if -0.95 < z < -0.78 and y > 0.3: return ACC
        return MAIN

    def pod_col(c):
        x, y, z = c
        if y < 0.24: return CARBON
        if -0.95 < z < -0.78 and y > 0.3: return ACC   # bande d'accent (alignée sur les sections : bord net)
        return MAIN

    def nose_col(c):
        x, y, z = c
        if z > F(NZ(3.45)): return ACC
        if y < 0.19 + S['nose_h']: return CARBON
        if abs(x) < 0.045 and y > 0.3: return ACC
        return MAIN

    # ---------- fond plat / fond à tunnels ----------
    o = [(0.25, 1.45), (0.62, 0.95), (0.82, 0.62), (0.86, -0.2), (0.84, -1.2), (0.6, -1.5), (0.55, -2.2)]
    o = [(x * (1 + (S['tr'] - 0.8) * 0.8), F(z) if z > 0.9 else z) for x, z in o]
    outline = o + [(-x, z) for x, z in reversed(o)]
    car.plate([(x, z) for x, z in outline], 0.03, CARBON, x=0.115, plane='xz')
    for s in (-1, 1):   # bords du fond relevés
        car.box((s * 0.84 * (1 + (S['tr'] - 0.8) * 0.8), 0.14, -0.4), (0.018, 0.06, 1.9), CARBON)
    if S['floor'] == 'tunnels':
        for s in (-1, 1):
            # bords du fond enroulés et entrées des tunnels (barrières verticales)
            car.tube([(s * 0.86, 0.13, 0.7), (s * 0.88, 0.16, -0.2), (s * 0.86, 0.17, -1.1), (s * 0.7, 0.2, -1.6)], 0.03, CARBON, seg=8, rb=0.018)
            for i, xx in enumerate((0.28, 0.4, 0.52, 0.64)):
                car.plate([(F(1.05) - i * 0.05, 0.05), (F(1.05) - i * 0.05, 0.16), (0.55, 0.15), (0.45, 0.06)], 0.012, CARBON, x=s * xx)
        # diffuseur haut et creusé
        car.wing(CARBON, (0, 0.13, -1.62), 0.82, 1.1, t=0.04, camber=0.0, aoa=0.42, steps=8)
        for xx in (-0.52, -0.3, -0.1, 0.1, 0.3, 0.52):
            car.plate([(-1.62, 0.12), (-2.4, 0.46), (-2.4, 0.2), (-1.62, 0.1)], 0.012, CARBON, x=xx)
    else:
        car.wing(CARBON, (0, 0.135, -1.95), 0.55, 1.06, t=0.04, camber=0.0, aoa=0.33, steps=8)
        for xx in (-0.5, -0.25, 0, 0.25, 0.5):
            car.plate([(-1.95, 0.12), (-2.47, 0.32), (-2.47, 0.18), (-1.95, 0.11)], 0.012, CARBON, x=xx)

    # ---------- coque et nez ----------
    nh, nw = S['nose_h'], S['nose_w']
    tub = [
        dict(z=3.76, y0=0.19, y1=0.25, w=0.1, n=2.2),
        dict(z=3.66, y0=0.17, y1=0.29, w=0.18, n=2.4),
        dict(z=3.35, y0=0.17, y1=0.36, w=0.25, wb=0.2),
        dict(z=2.75, y0=0.2, y1=0.47, w=0.32, wb=0.24),
        dict(z=2.1, y0=0.23, y1=0.58, w=0.42, wb=0.31),
        dict(z=1.55, y0=0.19, y1=0.66, ym=0.48, w=0.56, wb=0.42),
        dict(z=1.05, y0=0.14, y1=0.7, ym=0.46, w=0.7, wb=0.5),
        dict(z=0.4, y0=0.13, y1=0.7, ym=0.44, w=0.8, wb=0.56),
        dict(z=-0.25, y0=0.13, y1=0.66, ym=0.42, w=0.78, wb=0.56, n=2.8),
    ]
    for r in tub:
        z0 = r['z']; r['z'] = F(NZ(z0))
        if z0 > 1.5:   # nez : hauteur et largeur selon l'ère
            k = min(1, (z0 - 1.5) / 1.2)
            r['y0'] += nh * k; r['y1'] += nh * k * 0.7
            r['w'] *= 1 + (nw - 1) * k; r['wb'] = r.get('wb', r['w'] / (1 + (nw - 1) * k)) * (1 + (nw - 1) * k)
    car.loft(tub, nose_col, seg=28, sub=3, capk=ACC)
    if nh > 0.03:   # pylônes du nez haut
        for s in (-1, 1):
            car.plate([(F(NZ(3.55)), 0.12), (F(NZ(3.5)), 0.2 + nh), (F(NZ(3.1)), 0.2 + nh), (F(NZ(3.2)), 0.12)], 0.016, CARBON, x=s * 0.07)

    # cockpit : ouverture, rebord, appuie-tête
    car.ellipsoid((0, 0.69, 0.45), (0.23, 0.012, 0.48), BLACK, seg=24, rings=6)
    car.tube([(-0.26, 0.69, -0.02), (-0.27, 0.71, 0.45), (-0.2, 0.71, 0.82), (0, 0.7, 0.95), (0.2, 0.71, 0.82), (0.27, 0.71, 0.45), (0.26, 0.69, -0.02)],
             0.022, CARBON, seg=6, smooth=4)
    car.loft([dict(z=0.06, y0=0.6, y1=0.74, w=0.5), dict(z=-0.08, y0=0.6, y1=0.79, w=0.6), dict(z=-0.3, y0=0.6, y1=0.73, w=0.52)], CARBON, seg=16, sub=2)

    # ---------- capot moteur, boîte à air ----------
    ch = S['cover_h']
    cover = [
        dict(z=0.12, y0=0.4, y1=1.06, ym=0.72, w=0.4, n=2.2),
        dict(z=-0.12, y0=0.36, y1=1.13, ym=0.7, w=0.5, n=2.4),
        dict(z=-0.55, y0=0.3, y1=0.99, ym=0.6, w=0.56),
        dict(z=-1.1, y0=0.25, y1=0.8, ym=0.48, w=0.5),
        dict(z=-1.7, y0=0.22, y1=0.6, ym=0.4, w=0.34),
        dict(z=-2.2, y0=0.24, y1=0.5, ym=0.36, w=0.19),
        dict(z=-2.42, y0=0.29, y1=0.43, ym=0.36, w=0.08, n=2),
    ]
    for r in cover:
        r['y1'] = 0.3 + (r['y1'] - 0.3) * ch; r['ym'] = 0.3 + (r['ym'] - 0.3) * ch
    car.loft(cover, livery, seg=28, sub=3)
    car.ellipsoid((0, 0.97 * ch + 0.02, 0.122), (0.12, 0.07, 0.012), BLACK, seg=16, rings=5)      # prise d'air
    car.box((0, 1.17 * ch, -0.12), (0.2, 0.035, 0.06), ACC)              # caméra embarquée en T
    car.box((0, 1.13 * ch, -0.12), (0.03, 0.06, 0.04), CARBON)
    # échappement
    car.tube([(0, 0.4, -2.2), (0, 0.42, -2.55)], 0.045, GREY, seg=12, smooth=0)
    car.ellipsoid((0, 0.42, -2.555), (0.035, 0.035, 0.005), BLACK, seg=12, rings=4)

    # dérive
    rh = S['rw_h']
    if S['fin'] == 1:
        car.plate([(-0.3, 1.02 * ch), (-1.6, 0.66), (-2.1, 0.55), (-2.1, 0.72 + rh * 0.5), (-1.4, 0.9 * ch), (-0.6, 1.04 * ch)], 0.014, MAIN)
    if S['fin'] == 2:
        car.plate([(-0.3, 1.04 * ch), (-1.7, 0.78), (-2.2, 0.9 + rh), (-2.2, 1.04 + rh), (-1.5, 0.96), (-0.6, 1.1 * ch)], 0.014, MAIN)

    # ---------- pontons ----------
    pd, ph, uc, pl = S['pod'], S['pod_h'], S['undercut'], S['pod_len']
    for s in (-1, 1):
        pod = [
            dict(z=0.9, x=0.56, y0=0.27, y1=0.6, ym=0.48, w=0.36, wb=0.2, n=3.4),
            dict(z=0.75, x=0.58, y0=0.2, y1=0.62, ym=0.5, w=0.48, wb=0.24, n=3.4),
            dict(z=0.2, x=0.6, y0=0.15, y1=0.6, ym=0.48, w=0.5, wb=0.22, n=3.2),
            dict(z=-0.5, x=0.55, y0=0.15, y1=0.52, ym=0.42, w=0.42, wb=0.2),
            dict(z=-1.2, x=0.44, y0=0.15, y1=0.4, ym=0.32, w=0.3, wb=0.17),
            dict(z=-1.85, x=0.3, y0=0.16, y1=0.28, ym=0.24, w=0.14, wb=0.1, n=2.2),
        ]
        for r in pod:
            r['x'] = s * r['x'] * (0.6 + 0.4 * pd); r['w'] *= pd
            r['wb'] = max(0.06, r['wb'] * pd / uc)
            r['y1'] = 0.15 + (r['y1'] - 0.15) * ph; r['ym'] = 0.15 + (r['ym'] - 0.15) * ph
            if r['z'] > 0: r['z'] = 0.9 - (0.9 - r['z']) * pl
        p0 = pod[0]
        if S['inlet'] == 'slot':     # bouche fine et haute
            p0['y0'] = p0['y1'] - 0.13; pod[1]['y0'] -= 0.0
        elif S['inlet'] == 'large':  # grande bouche carrée
            p0['n'] = 4.5; p0['w'] *= 1.1
        car.loft(pod, pod_col, seg=24, sub=3, capk0=BLACK)
        # ouïes de refroidissement sur le dessus
        for i in range(4):
            zz = -0.25 - i * 0.12
            car.box((p0['x'] * 0.95, 0.15 + (0.5 - 0.15) * ph + 0.012 - i * 0.012, zz), (0.16 * pd, 0.012, 0.03), BLACK)
        # rétroviseurs sur le ponton (ou la coque)
        mx = s * 0.5
        car.rod((s * 0.3, 0.66, F(0.92)), (mx, 0.78, F(0.95)), 0.016, CARBON, rb=0.006)
        car.ellipsoid((mx, 0.79, F(0.96)), (0.085, 0.04, 0.05), MAIN, seg=14, rings=7)
        car.ellipsoid((mx, 0.79, F(0.912)), (0.072, 0.03, 0.004), MIRROR, seg=12, rings=4)
        if S['side_guard']:   # protection latérale renforcée devant le ponton
            car.tube([(s * 0.38, 0.55, 1.05), (s * 0.62, 0.58, 1.0), (s * 0.75, 0.5, 0.85)], 0.035, CARBON, seg=8, rb=0.02)
        # déflecteurs entre roue avant et ponton
        if S['barge'] == 1:
            for i in range(3):
                zz = F(1.25) - i * 0.12
                car.plate([(zz, 0.13), (zz, 0.42 - i * 0.04), (zz - 0.25, 0.38 - i * 0.04), (zz - 0.3, 0.13)], 0.01, CARBON, x=s * (0.4 + i * 0.07))
        elif S['barge'] == 2:
            car.plate([(F(1.2), 0.13), (F(1.15), 0.5), (0.95, 0.52), (0.9, 0.13)], 0.012, MAIN, x=s * 0.72)

    # ---------- halo, pare-brise ----------
    hr = S['halo_r']
    car.tube([(-0.29, 0.7, -0.02), (-0.29, 0.92, 0.22), (-0.24, 1.0, 0.55), (0, 1.03, 0.78), (0.24, 1.0, 0.55), (0.29, 0.92, 0.22), (0.29, 0.7, -0.02)],
             hr, CARBON, seg=10, smooth=5)
    car.tube([(0, 1.03, 0.78), (0, 0.93, 0.98), (0, 0.72, 1.12)], hr * 0.85, CARBON, seg=8, smooth=3, rb=hr * 0.55, flat_up=Vector((1, 0, 0)))
    if S['screen']:
        car.wing(VISOR, (0, 1.0, 0.86), 0.12, 0.62, t=0.03, camber=0.0, aoa=-0.6, steps=10, lift=-0.08)

    # ---------- pilote : casque, visière, volant ----------
    def helm(c):
        x, y, z = c
        if z > 0.38 and 0.83 < y < 0.9 and abs(x) < 0.13: return VISOR
        return HELM
    car.ellipsoid((0, 0.84, 0.3), (0.15, 0.157, 0.168), helm, seg=24, rings=16)
    car.box((0, 0.74, F(0.8)), (0.25, 0.1, 0.03), CARBON, rx=0.25)
    car.box((0, 0.745, F(0.785)), (0.07, 0.035, 0.01), SCREEN, rx=0.25)

    # ---------- aileron avant ----------
    car.cur_tag = 1
    fs, fl = S['fw_span'] * (S['tf'] / 0.83), S['fw_lift']
    zle = F(3.68) + dzn
    car.wing(CARBON, (0, 0.085, zle), 0.44, 1.94 * fs, t=0.08, camber=-0.06, aoa=0.05, lift=fl * 0.4, steps=18,
             cscale=lambda u: 1.0 if u < 0.15 else 1.0 - 0.25 * (u - 0.15))
    els = [(0.27, 0.14, 3.32, 0.33, MAIN), (0.19, 0.2, 3.13, 0.62, ACC), (0.14, 0.26, 3.0, 0.9, MAIN)]
    for i, (c, y, z, a, col) in enumerate(els[:S['fw_el'] - 1]):
        span = (1.8 - i * 0.16) * fs
        car.wing(col, (0, y, F(z) + dzn), c, span, t=0.09, camber=-0.08, aoa=a, lift=fl * (1 + i * 0.5), sweep=0.08 * i, steps=18,
                 cscale=lambda u: 0.2 if u < 0.12 else 1.0)
    for s in (-1, 1):
        xp = s * 0.98 * fs
        car.plate([(zle + 0.04, 0.06), (zle + 0.03, 0.15), (zle - 0.22, 0.25 + fl), (zle - 0.6, 0.27 + fl), (zle - 0.72, 0.18), (zle - 0.72, 0.06)], 0.018, MAIN, x=xp)
        car.box((xp * 0.97, 0.075, zle - 0.35), (0.05, 0.02, 0.52), CARBON)
    for s in (-1, 1):
        car.plate([(F(3.58) + dzn, 0.09), (F(3.55) + dzn, 0.22 + nh), (F(3.3) + dzn, 0.22 + nh), (F(3.27) + dzn, 0.1)], 0.018, CARBON, x=s * 0.085)
    car.cur_tag = 0

    # ---------- aileron arrière ----------
    car.cur_tag = 2
    rw = S['rw_w']
    spoon = lambda u: 1.0 + S['rw_spoon'] * 10 * max(0, 0.4 - u) if S['rw_spoon'] else 1.0
    car.wing(CARBON, (0, 0.88 + rh, -2.18), 0.4, 1.02 * rw, t=0.1, camber=-0.08, aoa=0.16, steps=14, cscale=spoon,
             lift=-S['rw_spoon'] * 1.5)
    flap_a = 0.45 if S['rw_drs'] else 0.72
    car.wing(ACC, (0, 1.0 + rh + (0.03 if S['rw_drs'] else 0), -2.52), 0.25, 1.02 * rw, t=0.09, camber=-0.08, aoa=flap_a, steps=14)
    if S['rw_drs']:   # vérin du volet mobile
        car.box((0, 1.06 + rh, -2.6), (0.08, 0.1, 0.12), CARBON)
    for s in (-1, 1):
        car.plate([(-2.14, 0.62 + rh * 0.5), (-2.1, 0.98 + rh), (-2.18, 1.16 + rh), (-2.7, 1.16 + rh), (-2.74, 0.9 + rh), (-2.5, 0.62 + rh * 0.5)], 0.02, MAIN, x=s * 0.52 * rw)
        car.plate([(-2.48, 1.16 + rh), (-2.7, 1.16 + rh), (-2.72, 1.04 + rh), (-2.58, 1.0 + rh)], 0.024, ACC, x=s * 0.52 * rw)
    if S['beam']:
        car.wing(CARBON, (0, 0.5, -2.3), 0.2, 0.9 * rw, t=0.1, camber=-0.05, aoa=0.1, steps=10)
    car.plate([(-2.3, 0.4), (-2.38, 0.95 + rh), (-2.55, 0.95 + rh), (-2.55, 0.4)], 0.035, CARBON)
    car.cur_tag = 0
    car.box((0, 0.33, -2.63), (0.11, 0.07, 0.03), LIGHT)   # feu de pluie

    # ---------- suspensions, porte-moyeux, écopes de frein ----------
    for s in (-1, 1):
        for (zfa, zra, yy, wz, wx) in ((F(1.95), F(1.55), 0.44, zf, S['tf'] - 0.11), (F(1.95), F(1.55), 0.27, zf, S['tf'] - 0.11),
                                       (-1.3, -1.75, 0.44, zr, S['tr'] - 0.12), (-1.3, -1.75, 0.25, zr, S['tr'] - 0.12)):
            car.rod((s * 0.2, yy + 0.04, zfa), (s * wx, yy, wz), 0.024, CARBON, rb=0.009)
            car.rod((s * 0.22, yy + 0.04, zra), (s * wx, yy, wz), 0.024, CARBON, rb=0.009)
        car.rod((s * 0.22, 0.56, F(1.7)), (s * (S['tf'] - 0.13), 0.29, zf), 0.014, CARBON)
        car.rod((s * 0.4, 0.5, -1.55), (s * (S['tr'] - 0.14), 0.27, zr), 0.014, CARBON)
        car.rod((s * 0.12, 0.24, F(1.85)), (s * (S['tf'] - 0.1), 0.33, zf + 0.12), 0.012, CARBON)   # biellette de direction
        for (z, r, t, w) in ((zf, S['rf'], S['tf'], S['wf']), (zr, S['rr'], S['tr'], S['wr'])):
            car.box((s * (t - w / 2 - 0.035), r, z), (0.04, 0.2, 0.18), CARBON)    # porte-moyeu
            car.ellipsoid((s * (t - w / 2 - 0.06), r + 0.08, z + 0.08), (0.035, 0.05, 0.08), CARBON, seg=10, rings=5)  # écope


def build_wheels(car: Car, S):
    for (z, r, t, w) in ((S['zf'], S['rf'], S['tf'], S['wf']), (S['zr'], S['rr'], S['tr'], S['wr'])):
        for s in (-1, 1):
            x = s * t; h = w / 2; ri = r * 0.66; b = 0.06
            prof = [(-h, ri)]
            prof += [(-h + b + b * math.cos(math.pi - math.pi / 2 * k / 4), r - b + b * math.sin(math.pi - math.pi / 2 * k / 4)) for k in range(5)]
            prof += [(h - b + b * math.cos(math.pi / 2 - math.pi / 2 * k / 4), r - b + b * math.sin(math.pi / 2 - math.pi / 2 * k / 4)) for k in range(5)]
            prof.append((h, ri))
            car.lathe(prof, (x, r, z), TYRE, seg=32)
            # jante : flasque creusée + moyeu
            for sd in (-1, 1):
                car.lathe([(sd * (h - 0.002), ri), (sd * (h - 0.03), ri * 0.92), (sd * (h - 0.05), ri * 0.35), (sd * (h - 0.03), 0.04), (sd * (h - 0.01), 0.001)],
                          (x, r, z), lambda i: GREY if i < 1 else BLACK if i < 2 else GREY, seg=24)
