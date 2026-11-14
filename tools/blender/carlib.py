# Générateur de monoplaces (design original, univers fictif) pour Blender.
# Coordonnées locales = repère du jeu web (three.js) : x latéral, y haut, z vers l'avant, en mètres, sol à y = 0.
# Chaque face porte un indice de palette K (matériau « K<n> ») et une étiquette tag (0, 1 = aileron avant, 2 = aileron arrière).
import bpy, bmesh, math
from mathutils import Vector, Matrix

# palette du jeu (0 livrée, 1 accent, 2 casque, puis couleurs fixes sRGB) — mêmes rôles que car.json / PaintRough d'Unreal
PAL = ['d8262b', 'f4f4f4', 'ffc21a', '17191d', '08090a', '0d1013', '1f9e55', '5d6c7c', 'ff2a2a', '2a2d33', '1a1a1c']
CARBON, BLACK, VISOR, SCREEN, MIRROR, LIGHT, GREY, TYRE = 3, 4, 5, 6, 7, 8, 9, 10
MAIN, ACC, HELM = 0, 1, 2
DETAIL = 1.0   # 1 = Unreal ; ~0.6 = jeu web (téléphone)


def dq(n, mn=3, even=False):
    v = max(mn, int(round(n * DETAIL)))
    return v + (v % 2) if even else v


def srgb(h, a=1):
    c = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return [(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4) for x in c] + [a]


def materials():
    out = []
    for k, h in enumerate(PAL):
        name = 'K%d' % k
        m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        m.use_nodes = True
        b = m.node_tree.nodes.get('Principled BSDF')
        if b:
            b.inputs['Base Color'].default_value = srgb(h)
            b.inputs['Roughness'].default_value = [0.3, 0.3, 0.22, 0.42, 0.85, 0.05, 0.3, 0.1, 0.3, 0.6, 0.9][k]
            b.inputs['Metallic'].default_value = 0.85 if k == 7 else 0.2 if k == 9 else 0.0
            if 'Coat Weight' in b.inputs and k in (0, 1, 2):
                b.inputs['Coat Weight'].default_value = 0.8
        out.append(m)
    return out


class Car:
    def __init__(self, name):
        self.bm = bmesh.new()
        self.tag = self.bm.faces.layers.int.new('tag')
        self.uvl = self.bm.loops.layers.uv.new('UVMap')
        self.cur_tag = 0
        self.name = name

    # ---------- primitives ----------
    def _face(self, verts, k):
        try:
            f = self.bm.faces.new(verts)
        except ValueError:
            return None
        f.material_index = k
        f[self.tag] = self.cur_tag
        return f

    def _finish(self, verts, faces):
        """Oriente les faces d'une pièce fermée vers l'extérieur."""
        bmesh.ops.recalc_face_normals(self.bm, faces=[f for f in faces if f])

    def grid(self, rings, kfn, close_u=True, cap0=True, cap1=True, capk=None, capk0=None):
        """rings : liste d'anneaux (listes de Vector). kfn(centre) -> K, ou entier."""
        bm = self.bm
        V = [[bm.verts.new(p) for p in r] for r in rings]
        faces = []
        n = len(rings[0])
        kf = (lambda c: kfn) if isinstance(kfn, int) else kfn
        for i in range(len(V) - 1):
            for j in range(n if close_u else n - 1):
                a, b = V[i][j], V[i][(j + 1) % n]
                c, d = V[i + 1][(j + 1) % n], V[i + 1][j]
                cen = (a.co + b.co + c.co + d.co) / 4
                faces.append(self._face([a, b, c, d], kf(cen)))
        for ci, (cap, ring) in enumerate(((cap0, V[0]), (cap1, V[-1]))):
            if cap:
                cen = sum((v.co for v in ring), Vector()) / len(ring)
                k = capk0 if (ci == 0 and capk0 is not None) else capk if capk is not None else kf(cen)
                if (ring[0].co - ring[n // 2].co).length < 1e-5:
                    continue
                c = bm.verts.new(cen)
                for j in range(n):
                    faces.append(self._face([ring[j], ring[(j + 1) % n], c], k))
        self._finish(None, faces)
        return faces

    # ---------- carrosserie lissée (sections superellipse) ----------
    @staticmethod
    def ring(s, seg):
        n = s.get('n', 2.6); e = 2 / n
        ym = s.get('ym', (s['y0'] + s['y1']) / 2); cx = s.get('x', 0.0)
        sp = lambda v: math.copysign(abs(v) ** e, v)
        pts = []
        for k in range(seg):
            a = k / seg * math.pi * 2; c, si = math.cos(a), math.sin(a)
            top = si >= 0
            w = s['w'] if top else s.get('wb', s['w'])
            x = cx + w / 2 * sp(c)
            y = ym + (s['y1'] - ym) * sp(si) if top else ym + (ym - s['y0']) * sp(si)
            pts.append(Vector((x, y, s['z'])))
        return pts

    @staticmethod
    def interp(secs, sub=3):
        """Interpolation Catmull-Rom de toutes les valeurs entre les sections (surface plus douce)."""
        keys = set()
        for s in secs: keys |= set(s)
        full = []
        for s in secs:
            d = dict(s); d.setdefault('n', 2.6); d.setdefault('x', 0.0)
            d.setdefault('ym', (d['y0'] + d['y1']) / 2); d.setdefault('wb', d['w'])
            full.append(d)
        out = []
        N = len(full)
        for i in range(N - 1):
            p0, p1, p2, p3 = full[max(i - 1, 0)], full[i], full[i + 1], full[min(i + 2, N - 1)]
            for t in range(sub):
                u = t / sub
                d = {}
                for k in p1:
                    a, b, c, e = p0[k], p1[k], p2[k], p3[k]
                    d[k] = 0.5 * ((2 * b) + (-a + c) * u + (2 * a - 5 * b + 4 * c - e) * u * u + (-a + 3 * b - 3 * c + e) * u ** 3)
                out.append(d)
        out.append(full[-1])
        return out

    def loft(self, secs, kfn, seg=28, sub=3, cap0=True, cap1=True, capk=None, capk0=None):
        seg = dq(seg, 8, True); sub = sub if DETAIL > 0.8 else max(1, sub - 1)
        rings = [self.ring(s, seg) for s in self.interp(secs, sub)]
        return self.grid(rings, kfn, True, cap0, cap1, capk, capk0)

    # ---------- profil d'aile ----------
    def wing(self, k, le, chord, span, t=0.09, camber=-0.06, aoa=0.0, steps=14, nchord=12,
             lift=0.0, sweep=0.0, cscale=None, x0=None, tipk=None):
        """Élément d'aile : bord d'attaque à le = (x centre, y, z), corde vers -z, envergure sur x.
        lift / sweep : relevé et flèche en bout d'aile (quadratique), cscale(u) : corde selon |u| (0 centre, 1 bout)."""
        steps = dq(steps, 4); nchord = dq(nchord, 6)
        prof_up, prof_lo = [], []
        for i in range(nchord + 1):
            u = (1 - math.cos(i / nchord * math.pi)) / 2
            th = 5 * t * (0.2969 * math.sqrt(u) - 0.126 * u - 0.3516 * u * u + 0.2843 * u ** 3 - 0.1036 * u ** 4)
            cm = camber * 4 * u * (1 - u)
            prof_up.append((u, cm + th)); prof_lo.append((u, cm - th))
        prof = prof_up + prof_lo[::-1][1:-1]
        ca, sa = math.cos(aoa), math.sin(aoa)
        rings = []
        xa = -span / 2 if x0 is None else x0[0]
        xb = span / 2 if x0 is None else x0[1]
        for j in range(steps + 1):
            x = xa + (xb - xa) * j / steps
            uu = abs(x - le[0]) / (span / 2) if span else 0
            c = chord * (cscale(uu) if cscale else 1)
            dy = lift * uu * uu; dz = -sweep * uu * uu
            r = []
            for (u, v) in prof:
                zz, yy = -u * c, v * c
                y2 = yy * ca - zz * sa; z2 = yy * sa + zz * ca
                r.append(Vector((x, le[1] + dy + y2, le[2] + dz + z2)))
            rings.append(r)
        return self.grid(rings, k, True, True, True, tipk)

    # ---------- plaque (dérive, plaques latérales) ----------
    def plate(self, pts, th, k, x=0.0, plane='zy', bevel=0.0):
        """pts : contour (u, v). plane 'zy' : u = z, v = y, épaisseur sur x ; 'xz' : vue de dessus (u = x, v = z), épaisseur sur y."""
        bm = self.bm
        def P(u, v, s):
            if plane == 'zy': return Vector((x + s * th / 2, v, u))
            return Vector((u, x + s * th / 2, v))
        A = [bm.verts.new(P(u, v, -1)) for u, v in pts]
        B = [bm.verts.new(P(u, v, 1)) for u, v in pts]
        faces = [self._face(A[::-1], k), self._face(B, k)]
        n = len(pts)
        for i in range(n):
            faces.append(self._face([A[i], A[(i + 1) % n], B[(i + 1) % n], B[i]], k))
        self._finish(None, faces)
        return faces

    # ---------- tube (halo, bras de suspension) ----------
    def tube(self, pts, r, k, seg=10, smooth=4, rb=None, flat_up=Vector((0, 1, 0)), cap=True):
        """pts : points de passage (lissés) ; r : rayon ; rb : 2e demi-axe (section elliptique, aplatie selon flat_up)."""
        seg = dq(seg, 4); smooth = dq(smooth, 1) if smooth else 0
        P = [Vector(p) for p in pts]
        if smooth and len(P) > 2:
            Q = []
            for i in range(len(P) - 1):
                p0, p1, p2, p3 = P[max(i - 1, 0)], P[i], P[i + 1], P[min(i + 2, len(P) - 1)]
                for t in range(smooth):
                    u = t / smooth
                    Q.append(0.5 * ((2 * p1) + (-p0 + p2) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u * u + (-p0 + 3 * p1 - 3 * p2 + p3) * u ** 3))
            Q.append(P[-1]); P = Q
        rings = []
        for i, p in enumerate(P):
            t = (P[min(i + 1, len(P) - 1)] - P[max(i - 1, 0)]).normalized()
            up = flat_up - t * t.dot(flat_up)
            if up.length < 1e-4: up = Vector((1, 0, 0)) - t * t.x
            up.normalize(); side = t.cross(up)
            rr = []
            for j in range(seg):
                a = j / seg * math.pi * 2
                rr.append(p + side * (r * math.cos(a)) + up * ((rb if rb else r) * math.sin(a)))
            rings.append(rr)
        return self.grid(rings, k, True, cap, cap)

    def rod(self, a, b, r, k, rb=None, seg=6):
        return self.tube([a, b], r, k, seg=seg, smooth=0, rb=rb)

    def panel(self, pts, k, uvs):
        """Quadrilatère texturé (panneau publicitaire) : pts 4 coins (ordre trigonométrique vu de face), uvs 4 couples."""
        V = [self.bm.verts.new(Vector(p)) for p in pts]
        f = self._face(V, k)
        if f:
            for l, uv in zip(f.loops, uvs): l[self.uvl].uv = uv
        return [f]

    def box(self, c, size, k, rx=0.0):
        bm = self.bm
        hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
        R = Matrix.Rotation(rx, 3, 'X')
        V = [bm.verts.new(Vector(c) + R @ Vector((sx * hx, sy * hy, sz * hz))) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
        idx = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
        faces = [self._face([V[i] for i in q], k) for q in idx]
        self._finish(None, faces)
        return faces

    def ellipsoid(self, c, r, kfn, seg=16, rings=10):
        """Ellipsoïde (casque, rétroviseur) ; r = (rx, ry, rz)."""
        seg = dq(seg, 6); rings = dq(rings, 3)
        R = []
        for i in range(1, rings):
            th = math.pi * i / rings
            R.append([Vector((c[0] + r[0] * math.sin(th) * math.cos(2 * math.pi * j / seg),
                              c[1] + r[1] * math.cos(th),
                              c[2] + r[2] * math.sin(th) * math.sin(2 * math.pi * j / seg))) for j in range(seg)])
        bm = self.bm
        V = [[bm.verts.new(p) for p in r_] for r_ in R]
        top = bm.verts.new(Vector((c[0], c[1] + r[1], c[2]))); bot = bm.verts.new(Vector((c[0], c[1] - r[1], c[2])))
        kf = (lambda q: kfn) if isinstance(kfn, int) else kfn
        faces = []
        for i in range(len(V) - 1):
            for j in range(seg):
                q = [V[i][j], V[i][(j + 1) % seg], V[i + 1][(j + 1) % seg], V[i + 1][j]]
                faces.append(self._face(q, kf(sum((v.co for v in q), Vector()) / 4)))
        for j in range(seg):
            faces.append(self._face([top, V[0][(j + 1) % seg], V[0][j]], kf(top.co)))
            faces.append(self._face([bot, V[-1][j], V[-1][(j + 1) % seg]], kf(bot.co)))
        self._finish(None, faces)
        return faces

    def lathe(self, prof, axis_c, k, seg=28, x_axis=True):
        """Révolution autour d'un axe parallèle à x passant par axis_c ; prof : liste (dx, r) ; k entier ou fn(i_profil)."""
        seg = dq(seg, 8)
        rings = []
        for (dx, r) in prof:
            rr = []
            for j in range(seg):
                a = j / seg * math.pi * 2
                rr.append(Vector((axis_c[0] + dx, axis_c[1] + r * math.cos(a), axis_c[2] + r * math.sin(a))))
            rings.append(rr)
        bm = self.bm
        V = [[bm.verts.new(p) for p in r_] for r_ in rings]
        faces = []
        for i in range(len(V) - 1):
            kk = k(i) if callable(k) else k
            for j in range(seg):
                faces.append(self._face([V[i][j], V[i][(j + 1) % seg], V[i + 1][(j + 1) % seg], V[i + 1][j]], kk))
        # orientation : normale vers l'extérieur du profil (rayon croissant ou faces latérales)
        for f in faces:
            if not f: continue
            c = f.calc_center_median(); f.normal_update()
            radial = Vector((0, c.y - axis_c[1], c.z - axis_c[2]))
            sidev = Vector((c.x - axis_c[0], 0, 0))
            # face plutôt radiale ou plutôt latérale
            ref = radial if abs(f.normal.dot(radial.normalized() if radial.length else radial)) > abs(f.normal.x) else sidev
            if f.normal.dot(ref) < 0: f.normal_flip()
        return faces

    # ---------- sortie ----------
    def build(self, mats, coll=None, sharp_deg=38):
        me = bpy.data.meshes.new(self.name)
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=1e-6)
        self.bm.to_mesh(me)
        self.bm.free()
        for m in mats: me.materials.append(m)
        for p in me.polygons: p.use_smooth = True
        me.set_sharp_from_angle(angle=math.radians(sharp_deg))
        ob = bpy.data.objects.new(self.name, me)
        (coll or bpy.context.scene.collection).objects.link(ob)
        # repère three.js -> Blender (x_b = -x, y_b = z, z_b = y), rotation propre
        ob.matrix_world = Matrix(((-1, 0, 0, 0), (0, 0, 1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
        return ob


def export_json(obj_body, obj_wheels, path_unreal, path_web, meta):
    """Écrit le format car.json d'Unreal (non indexé) et un binaire indexé compact pour le jeu web."""
    import json, struct
    dg = bpy.context.evaluated_depsgraph_get()

    def tris(ob):
        e = ob.evaluated_get(dg); me = e.to_mesh()
        me.calc_loop_triangles()
        cn = me.corner_normals
        tag = me.attributes.get('tag')
        out = []
        for t in me.loop_triangles:
            k = int(me.materials[t.material_index].name[1:])
            tg = tag.data[t.polygon_index].value if tag else 0
            vs = [(tuple(me.vertices[me.loops[l].vertex_index].co), tuple(cn[l].vector)) for l in t.loops]
            out.append((tg, k, vs))
        e.to_mesh_clear()
        return out

    body = tris(obj_body)
    wheels = tris(obj_wheels) if obj_wheels else []
    # ailerons contigus : corps, puis aileron avant (1), puis aileron arrière (2)
    body.sort(key=lambda t: t[0])
    pal = ['main', 'accent', 'helmet'] + [None] * (len(PAL) - 3)
    for i in range(3, len(PAL)):
        c = srgb(PAL[i]); pal[i] = ''.join('%02x' % round(v * 255) for v in c[:3])
    P, N, K, R = [], [], [], {}
    r3 = lambda v: round(v, 3); r2 = lambda v: round(v, 2)
    vi = 0
    for tg, k, vs in body + wheels:
        if tg:
            key = 'fw' if tg == 1 else 'rw'
            if key not in R: R[key] = [vi, 0]
            R[key][1] = vi + 3 - R[key][0]
        for (p, n) in vs:
            P += [r3(p[0]), r3(p[1]), r3(p[2])]; N += [r2(n[0]), r2(n[1]), r2(n[2])]; K.append(k)
            vi += 1
    with open(path_unreal, 'w') as f:
        json.dump({'palette': pal, 'P': P, 'N': N, 'K': K, 'R': R, 'meta': meta}, f, separators=(',', ':'))

    # web : sommets indexés (position int16 au 1/8000 m, normale int8, K uint8), indices uint16, sans les roues
    verts, index, vmap = [], [], {}
    ranges = {}
    for tg, k, vs in body:
        for (p, n) in vs:
            q = (round(p[0] * 8000), round(p[1] * 8000), round(p[2] * 8000), round(n[0] * 127), round(n[1] * 127), round(n[2] * 127), k)
            i = vmap.get(q)
            if i is None:
                i = vmap[q] = len(verts); verts.append(q)
            if tg:
                key = 'fw' if tg == 1 else 'rw'
                if key not in ranges: ranges[key] = [len(index), 0]
                ranges[key][1] = len(index) + 1 - ranges[key][0]
            index.append(i)
    assert len(verts) < 65536, len(verts)
    head = json.dumps({'v': 1, 'nv': len(verts), 'ni': len(index), 'ranges': ranges, 'meta': meta}).encode()
    head += b' ' * ((4 - len(head) % 4) % 4)
    with open(path_web, 'wb') as f:
        f.write(b'GPMC'); f.write(struct.pack('<I', len(head))); f.write(head)
        f.write(struct.pack('<%dh' % (len(verts) * 3), *[c for v in verts for c in v[:3]]))
        f.write(struct.pack('<%db' % (len(verts) * 3), *[c for v in verts for c in v[3:6]]))
        f.write(bytes([v[6] for v in verts]))
        pad = (4 - (len(verts) * 10) % 4) % 4
        f.write(b'\0' * pad)
        f.write(struct.pack('<%dH' % len(index), *index))
    return {'tris_body': len(body), 'tris_wheels': len(wheels), 'web_verts': len(verts)}


def materials_for(prefix, pal, rough=None):
    out = []
    for k, h in enumerate(pal):
        name = '%s%d' % (prefix, k)
        m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        m.use_nodes = True
        b = m.node_tree.nodes.get('Principled BSDF')
        if b:
            b.inputs['Base Color'].default_value = srgb(h)
            b.inputs['Roughness'].default_value = (rough or {}).get(k, 0.7)
        out.append(m)
    return out


def export_prop(ob, path, ad_k=None, meta=None):
    """Décor : binaire GPMC v2 indexé (positions int16 au 1/200 m, normales int8, K uint8, indices uint16, puis uv int16 au 1/1000).
    Les triangles du panneau publicitaire (K = ad_k) sont rangés à la fin (groupe de matériau texturé : 'ad' = premier indice)."""
    import json, struct
    dg = bpy.context.evaluated_depsgraph_get()
    e = ob.evaluated_get(dg); me = e.to_mesh(); me.calc_loop_triangles()
    cn = me.corner_normals; uvl = me.uv_layers.active
    tris = []
    for t in me.loop_triangles:
        k = int(''.join(ch for ch in me.materials[t.material_index].name if ch.isdigit()))
        vs = [(tuple(me.vertices[me.loops[l].vertex_index].co), tuple(cn[l].vector), tuple(uvl.data[l].uv) if uvl else (0, 0)) for l in t.loops]
        tris.append((1 if k == ad_k else 0, k, vs))
    e.to_mesh_clear()
    tris.sort(key=lambda t: t[0])
    verts, index, vmap, ad = [], [], {}, None
    for isad, k, vs in tris:
        if isad and ad is None: ad = len(index)
        for (p, n, uv) in vs:
            q = (round(p[0] * 200), round(p[1] * 200), round(p[2] * 200), round(n[0] * 127), round(n[1] * 127), round(n[2] * 127), k,
                 round(uv[0] * 1000) if isad else 0, round(uv[1] * 1000) if isad else 0)
            i = vmap.get(q)
            if i is None: i = vmap[q] = len(verts); verts.append(q)
            index.append(i)
    assert len(verts) < 65536
    head = json.dumps({'v': 2, 'nv': len(verts), 'ni': len(index), 'ad': ad, 'q': 200, 'meta': meta or {}}).encode()
    head += b' ' * ((4 - len(head) % 4) % 4)
    nv = len(verts)
    with open(path, 'wb') as f:
        f.write(b'GPMC'); f.write(struct.pack('<I', len(head))); f.write(head)
        f.write(struct.pack('<%dh' % (nv * 3), *[c for v in verts for c in v[:3]]))
        f.write(struct.pack('<%db' % (nv * 3), *[c for v in verts for c in v[3:6]]))
        f.write(bytes([v[6] for v in verts]))
        f.write(b'\0' * ((4 - (nv * 10) % 4) % 4))
        f.write(struct.pack('<%dH' % len(index), *index))
        f.write(b'\0' * ((4 - (len(index) * 2) % 4) % 4))
        f.write(struct.pack('<%dh' % (nv * 2), *[c for v in verts for c in v[7:9]]))
    return {'tris': len(tris), 'verts': nv}
