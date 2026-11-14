# Décors de circuit (design original) : chaque élément est un petit modèle instancié par le jeu (js/decor.js).
# Repère local : x le long de la piste, y haut, z vers la piste (face avant), en mètres.
import math
from carlib import Car

# palette des décors : 0 = teinte de l'instance (écurie, façade), 11 = panneau publicitaire texturé
PPAL = ['ffffff', 'f2f2ef', 'b9bcc0', '3a3e44', '141619', '7f9db5', 'd42a2a', 'f2c21a', '1c1c1e', 'e8742a',
        '2f8f4a', 'ffffff', '2a3a4a', 'fff3c4', 'c9b79c', '6d7278']
TINT, WHITE, CONC, METAL, DARK, GLASS, RED, YEL, TYRE, ORANGE, GREEN, AD, DGLASS, LAMP, BEIGE, ROOF = range(16)
STOREY = 3.4


def garage(c: Car):
    """Module de stand : 12 m de large, 3 portes de garage, étage vitré, auvent, bandeau aux couleurs de l'écurie."""
    c.box((0, 0.1, 0), (12, 0.2, 16), CONC)
    for x in (-5.7, -1.9, 1.9, 5.7):
        c.box((x, 2.4, 0), (0.6, 4.6, 16), WHITE)
    c.box((0, 2.4, -7.85), (12, 4.6, 0.3), TINT)                 # fond du garage aux couleurs de l'écurie
    c.box((0, 2.4, -8.05), (12, 4.6, 0.1), WHITE)                # mur extérieur arrière
    for x in (-3.8, 0, 3.8):                                      # établis, pneus rangés, écrans
        c.box((x - 1.0, 0.6, -6.9), (1.2, 1.0, 1.4), METAL)
        c.box((x, 3.0, -7.6), (1.6, 0.9, 0.08), DARK)
        c.tube([(x + 1.0, 0.2, -6.8), (x + 1.0, 1.4, -6.8)], 0.32, TYRE, seg=8, smooth=0)
    c.box((0, 4.75, 0.3), (12, 0.5, 15.4), WHITE)               # plafond
    c.box((0, 4.45, 7.9), (11.4, 0.7, 0.12), TINT)              # bandeau au-dessus des portes
    c.box((0, 6.8, -1), (12, 3.6, 13.6), DGLASS)                 # étage vitré
    for x in range(-6, 7, 2):
        c.box((x, 6.8, 5.82), (0.18, 3.6, 0.12), METAL)          # montants
    c.box((0, 5.15, 7.2), (12, 0.25, 2.4), WHITE)                # balcon
    c.box((0, 5.85, 8.35), (12, 1.0, 0.06), GLASS)               # garde-corps vitré
    c.box((0, 8.85, 0.2), (12.4, 0.5, 16.6), CONC)               # toit
    c.box((0, 8.85, 8.5), (12.4, 0.8, 0.14), TINT)               # bandeau de toit aux couleurs de l'écurie
    c.box((3, 9.6, -3), (2.2, 1.0, 1.6), METAL)                  # climatisation


def motorhome(c: Car):
    """Motorhome d'écurie (paddock) : semi-remorque aux couleurs de l'écurie, cabine, auvent."""
    c.box((-1.2, 2.25, 0), (11.6, 3.5, 2.5), TINT)
    c.box((-1.2, 2.9, 0), (11.0, 0.8, 2.54), DGLASS)
    c.box((-1.2, 0.55, 0), (11.6, 0.3, 2.3), METAL)
    c.box((5.8, 1.85, 0), (2.3, 2.7, 2.45), WHITE)
    c.box((6.96, 2.3, 0), (0.04, 1.0, 2.2), DGLASS)
    for x in (-6.0, -5.0, -4.0, 4.2, 6.2):
        for z in (-1.0, 1.0):
            c.tube([(x, 0.5, z - 0.15), (x, 0.5, z + 0.15)], 0.5, TYRE, seg=8, smooth=0)
    c.box((-1.2, 3.9, 2.4), (10.0, 0.08, 2.4), WHITE, rx=-0.12)  # auvent
    for x in (-5.8, 3.4):
        c.rod((x, 0, 3.5), (x, 3.75, 3.5), 0.05, METAL, seg=4)


def panneau(c: Car):
    """Grand panneau publicitaire sur deux pieds, éclairé par trois projecteurs."""
    for x in (-4, 4):
        c.box((x, 2.9, -0.35), (0.4, 5.8, 0.4), METAL)
    c.box((0, 5.6, -0.2), (12.4, 4.4, 0.3), METAL)
    c.panel([(-6, 3.6, -0.04), (6, 3.6, -0.04), (6, 7.6, -0.04), (-6, 7.6, -0.04)], AD, [(0, 0), (1, 0), (1, 1), (0, 1)])
    for x in (-4, 0, 4):
        c.rod((x, 7.8, -0.1), (x, 8.3, 0.8), 0.04, METAL, seg=4)
        c.box((x, 8.3, 0.85), (0.7, 0.18, 0.35), LAMP)


def commissaires(c: Car):
    """Poste de commissaires : socle, cabine vitrée, toit orange, mât à drapeau."""
    c.box((0, 0.4, 0), (2.4, 0.8, 2.4), CONC)
    c.box((0, 1.9, 0), (2.2, 2.2, 2.2), WHITE)
    c.box((0, 2.35, 0), (2.24, 0.9, 2.24), DGLASS)
    c.box((0, 1.05, 0), (2.24, 0.25, 2.24), ORANGE)
    c.box((0, 3.1, 0.1), (2.8, 0.2, 2.9), ORANGE)
    c.rod((1.25, 0, 1.25), (1.25, 4.6, 1.25), 0.035, METAL, seg=4)
    c.box((1.25, 4.25, 1.75), (0.03, 0.6, 0.9), YEL)


def pneus(c: Car):
    """Mur de pneus de 6 m avec bande de protection (couleur du circuit)."""
    for i in range(6):
        c.tube([(-2.5 + i, 0, -0.1), (-2.5 + i, 1.15, -0.1)], 0.42, TYRE, seg=6, smooth=0)
    c.box((0, 0.6, 0.4), (6.0, 1.15, 0.08), TINT)
    c.box((0, 1.12, 0.42), (6.0, 0.08, 0.1), WHITE)


def camera(c: Car):
    """Tour de télévision : échafaudage, plateforme, caméra et parasol."""
    for x in (-1, 1):
        for z in (-1, 1):
            c.rod((x, 0, z), (x * 0.9, 7, z * 0.9), 0.07, METAL, seg=4)
    for y in (2.2, 4.6):
        c.rod((-1, y, 1), (1, y + 1.2, 1), 0.04, METAL, seg=4)
        c.rod((1, y, -1), (-1, y + 1.2, -1), 0.04, METAL, seg=4)
        c.rod((1, y, 1), (1, y + 1.2, -1), 0.04, METAL, seg=4)
        c.rod((-1, y, -1), (-1, y + 1.2, 1), 0.04, METAL, seg=4)
    c.box((0, 7.05, 0), (2.4, 0.15, 2.4), CONC)
    for x, z in ((-1.15, 1.15), (1.15, 1.15), (1.15, -1.15), (-1.15, -1.15)):
        c.rod((x, 7.1, z), (x, 8.1, z), 0.03, METAL, seg=4)
    c.box((0, 8.1, 1.15), (2.3, 0.06, 0.06), YEL)
    c.box((0, 7.9, 0.2), (0.5, 0.45, 0.8), DARK)
    c.tube([(0, 7.95, 0.6), (0, 7.95, 1.0)], 0.12, DARK, seg=8, smooth=0)
    c.rod((-0.6, 7.1, -0.4), (-0.6, 9.2, -0.4), 0.03, METAL, seg=4)
    c.ellipsoid((-0.6, 9.2, -0.4), (1.1, 0.25, 1.1), RED, seg=10, rings=3)


def projecteur(c: Car):
    """Mât d'éclairage (course au crépuscule) : 22 m, rampe de projecteurs tournée vers la piste."""
    c.tube([(0, 0, 0), (0, 22, 0)], 0.25, CONC, seg=8, smooth=0)
    c.box((0, 22.4, 0.3), (3.4, 1.6, 0.3), METAL, rx=0.35)
    for x in (-1.1, 0, 1.1):
        for y in (21.95, 22.85):
            c.box((x, y, 0.5 - (y - 22.4) * 0.35), (0.9, 0.6, 0.06), LAMP, rx=0.35)


def pilier(c: Car):
    """Pilier de passerelle (escalier fermé)."""
    c.box((0, 4.3, 0), (3.2, 8.6, 3.2), CONC)
    for y in (1.5, 3.5, 5.5):
        c.box((0, y, 1.62), (2.4, 0.6, 0.05), DGLASS)
    c.box((0, 8.75, 0), (3.6, 0.3, 3.6), WHITE)


def tablier(c: Car):
    """Tablier de passerelle (20 m, étiré à la largeur de la piste), banderoles publicitaires des deux côtés."""
    c.box((0, 7.2, 0), (20, 0.5, 3), WHITE)
    for z in (-1.4, 1.4):
        c.box((0, 9.4, z), (20, 0.3, 0.3), WHITE)
        for i in range(11):
            c.rod((-10 + i * 2, 7.4, z), (-10 + i * 2, 9.3, z), 0.08, WHITE, seg=4)
    c.box((0, 9.6, 0), (20, 0.15, 3.2), CONC)
    for z, sgn in ((1.58, 1), (-1.58, -1)):
        xs = (-9.5, 9.5) if sgn > 0 else (9.5, -9.5)
        c.panel([(xs[0], 7.5, z), (xs[1], 7.5, z), (xs[1], 9.2, z), (xs[0], 9.2, z)], AD, [(0, 0), (1, 0), (1, 1), (0, 1)])


def immeuble(c: Car, kind, floors):
    """Immeuble de 20 × 20 m (étiré par le jeu) : 'verre' tour vitrée, 'logements' bandeaux de fenêtres et balcons, 'ancien' corniche et toit."""
    H = floors * STOREY
    if kind == 'verre':
        c.box((0, H / 2, 0), (20, H, 20), DGLASS)
        step = max(1, floors // 8)
        for f in range(step, floors, step):
            c.box((0, f * STOREY, 0), (20.3, 0.3, 20.3), TINT)
        c.box((0, H + 0.4, 0), (20.4, 0.8, 20.4), TINT)
        c.box((3, H + 1.6, -2), (8, 2.4, 6), ROOF)
        c.rod((-5, H, 5), (-5, H + 7, 5), 0.12, METAL, seg=4)
    elif kind == 'logements':
        c.box((0, H / 2, 0), (20, H, 20), TINT)
        for f in range(floors):
            c.box((0, f * STOREY + 1.7, 0), (20.08, 1.3, 20.08), DGLASS)
            if floors <= 12 and f > 0:
                c.box((0, f * STOREY + 0.1, 10.6), (16, 0.15, 1.3), WHITE)
        c.box((0, H + 0.6, 0), (20.6, 1.2, 20.6), WHITE)
        c.box((-4, H + 1.8, 2), (5, 2.0, 5), ROOF)
    else:
        c.box((0, H / 2, 0), (20, H, 20), TINT)
        c.box((0, 1.6, 0), (20.1, 3.2, 20.1), BEIGE)
        for f in range(1, floors):
            c.box((0, f * STOREY + 1.7, 0), (20.06, 1.5, 20.06), DGLASS)
        c.box((0, H + 0.3, 0), (21, 0.6, 21), WHITE)
        # toit à deux pans
        bm = c.bm
        ridge = H + 5.5
        pts = [(-10.5, H + 0.6, -10.5), (10.5, H + 0.6, -10.5), (10.5, H + 0.6, 10.5), (-10.5, H + 0.6, 10.5), (-10.5, ridge, 0), (10.5, ridge, 0)]
        from mathutils import Vector
        V = [bm.verts.new(Vector(p)) for p in pts]
        faces = [c._face([V[0], V[1], V[5], V[4]], ROOF), c._face([V[3], V[4], V[5], V[2]], ROOF),
                 c._face([V[0], V[4], V[3]], BEIGE), c._face([V[1], V[2], V[5]], BEIGE), c._face([V[0], V[3], V[2], V[1]], ROOF)]
        c._finish(None, faces)


BUILDINGS = {'verre': (8, 16, 32), 'logements': (5, 10, 18), 'ancien': (3, 5, 8)}
PROPS = {'garage': garage, 'motorhome': motorhome, 'panneau': panneau, 'commissaires': commissaires, 'pneus': pneus,
         'camera': camera, 'projecteur': projecteur, 'pilier': pilier, 'tablier': tablier}
for _k, _fs in BUILDINGS.items():
    for _f in _fs:
        PROPS['imm-%s-%d' % (_k, _f)] = (lambda k, f: (lambda c: immeuble(c, k, f)))(_k, _f)
