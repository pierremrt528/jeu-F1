# Génère les 5 monoplaces (une par ère du règlement) dans Blender et les exporte pour le jeu.
# Blender : onglet Scripting > Ouvrir ce fichier > Exécuter (ou via l'add-on Blender MCP : exec(open(chemin).read())).
#   - jeu web : assets/cars/<ère>.bin (version allégée pour téléphone, DETAIL 0.6)
#   - Unreal  : tools/blender/out/car_<ère>.json (format de Content/Data/car.json), à copier dans unreal/GrandPrixManager/Content/Data
import os, sys, importlib, bpy
D = os.path.dirname(os.path.abspath(globals().get('__file__') or globals().get('CHEMIN', '.')))
ROOT = os.path.dirname(os.path.dirname(D))
sys.path[:] = [D] + [p for p in sys.path if p != D]
for m in ('carlib', 'cars'): sys.modules.pop(m, None)
import carlib, cars
importlib.reload(carlib); importlib.reload(cars)
os.makedirs(os.path.join(D, 'out'), exist_ok=True)
for ob in list(bpy.data.objects):
    if ob.name.startswith('GPM_'): bpy.data.objects.remove(ob, do_unlink=True)
mats = carlib.materials()
coll = bpy.data.collections.get('Monoplaces') or bpy.data.collections.new('Monoplaces')
if coll.name not in bpy.context.scene.collection.children: bpy.context.scene.collection.children.link(coll)
for det in (0.6, 1.0):
    carlib.DETAIL = det
    for i, era in enumerate(cars.ERAS):
        S = cars.params(era)
        c = carlib.Car('GPM_' + era); cars.build_car(c, S); body = c.build(mats, coll)
        w = carlib.Car('GPM_' + era + '_roues'); cars.build_wheels(w, S); wh = w.build(mats, coll, sharp_deg=50)
        for ob in (body, wh): ob.matrix_world.translation.x = (i - 2) * 3.0
        meta = {'id': era, 'wheels': [[S['tf'], S['zf'], S['rf'], S['wf']], [S['tr'], S['zr'], S['rr'], S['wr']]]}
        tmp = os.path.join(D, 'out', '_tmp')
        if det < 1:
            carlib.export_json(body, wh, tmp + '.json', os.path.join(ROOT, 'assets', 'cars', era + '.bin'), meta)
        else:
            carlib.export_json(body, wh, os.path.join(D, 'out', 'car_%s.json' % era), tmp + '.bin', meta)
        if det < 1:   # garde seulement la version détaillée dans la scène
            for ob in (body, wh): bpy.data.objects.remove(ob, do_unlink=True)
for f in ('_tmp.json', '_tmp.bin'):
    p = os.path.join(D, 'out', f)
    if os.path.exists(p): os.remove(p)
print('Monoplaces exportées')
