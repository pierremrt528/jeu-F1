# Génère les décors de circuit (decors.py) dans Blender et les exporte pour le jeu : assets/decor/<nom>.bin
# Blender : onglet Scripting > Ouvrir ce fichier > Exécuter (ou via Blender MCP : CHEMIN = '…/generer-decors.py'; exec(open(CHEMIN).read())).
import os, sys, bpy
D = os.path.dirname(os.path.abspath(globals().get('__file__') or globals().get('CHEMIN', '.')))
ROOT = os.path.dirname(os.path.dirname(D))
sys.path[:] = [D] + [p for p in sys.path if p != D]
for m in ('carlib', 'decors'): sys.modules.pop(m, None)
import carlib, decors
carlib.DETAIL = 1.0
OUT = os.path.join(ROOT, 'assets', 'decor'); os.makedirs(OUT, exist_ok=True)
for ob in list(bpy.data.objects):
    if ob.name.startswith('DEC_'): bpy.data.objects.remove(ob, do_unlink=True)
mats = carlib.materials_for('P', decors.PPAL, {5: 0.1, 12: 0.08, 8: 0.9})
coll = bpy.data.collections.get('Decors') or bpy.data.collections.new('Decors')
if coll.name not in bpy.context.scene.collection.children: bpy.context.scene.collection.children.link(coll)
x = 0
for name, fn in decors.PROPS.items():
    c = carlib.Car('DEC_' + name); fn(c); ob = c.build(mats, coll, sharp_deg=30)
    ob.matrix_world.translation = (x, 40, 0); x += 26
    carlib.export_prop(ob, os.path.join(OUT, name + '.bin'), ad_k=decors.AD, meta={'id': name})
print('Décors exportés')
