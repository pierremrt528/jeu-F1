# Grand Prix Manager : crée les textures et les matériaux du jeu dans Content/GPM.
# Lancé une fois par « POUR JOUER.command » (commandlet Python d'Unreal), puis à chaque nouvelle version du script.
# Le jeu charge ces matériaux par leur nom (GPMArt dans GrandPrixManager.cpp) ; s'ils manquent, il garde les couleurs simples.
import os
import traceback
import unreal

VERSION = 2
TOOLS = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(TOOLS)
TEX_SRC = os.path.join(TOOLS, "Textures")
TEX_DIR = "/Game/GPM/Textures"
MAT_DIR = "/Game/GPM/Materials"

tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
MP = unreal.MaterialProperty
ALWAYS = getattr(getattr(unreal, "PropertyAccessChangeNotifyMode", None), "ALWAYS", None)
errors = []


def setp(obj, name, value):
    """Change une propriété en prévenant l'objet (comme dans l'éditeur), pour que la texture soit recalculée."""
    try:
        obj.set_editor_property(name, value, ALWAYS)
    except TypeError:
        obj.set_editor_property(name, value)


def log(msg):
    unreal.log("GPMAssets: " + msg)


def fail(msg):
    errors.append(msg)
    unreal.log_error("GPMAssets: ERREUR " + msg)


# ---------- textures ----------

def import_texture(file_name, asset_name, normal=False, linear=False, sky=False):
    path = os.path.join(TEX_SRC, file_name)
    if not os.path.exists(path):
        fail("texture manquante " + path)
        return None
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", TEX_DIR)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    tex = unreal.load_asset(TEX_DIR + "/" + asset_name)
    if tex is None:
        fail("import raté " + file_name)
        return None
    if normal:
        setp(tex, "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        setp(tex, "srgb", False)
        setp(tex, "flip_green_channel", True)   # photo au format OpenGL (three.js)
    else:
        # photos en niveaux de gris (nuages) : format couleur ordinaire, pour que le matériau sache les lire
        setp(tex, "compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        if linear:
            setp(tex, "srgb", False)
    if sky:
        setp(tex, "address_y", unreal.TextureAddress.TA_CLAMP)   # bande de ciel : pas de répétition en hauteur
        setp(tex, "lod_group", unreal.TextureGroup.TEXTUREGROUP_SKYBOX)
    else:
        setp(tex, "lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    eal.save_loaded_asset(tex, False)
    log("texture " + asset_name)
    return tex


# ---------- petites briques de graphe ----------

def material(name):
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    return mat


def node(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def scalar(mat, name, value, x, y):
    return node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def vector(mat, name, rgb, x, y):
    return node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def const(mat, value, x, y):
    return node(mat, unreal.MaterialExpressionConstant, x, y, r=value)


def link(a, out, b, inp):
    if not mel.connect_material_expressions(a, out, b, inp):
        fail("lien %s.%s -> %s.%s" % (a.get_name(), out, b.get_name(), inp))


def output(a, out, prop):
    if not mel.connect_material_property(a, out, prop):
        fail("sortie %s.%s -> %s" % (a.get_name(), out, prop))


def mul(mat, a, aout, b, bout, x, y):
    m = node(mat, unreal.MaterialExpressionMultiply, x, y)
    link(a, aout, m, "A")
    link(b, bout, m, "B")
    return m


def lerp(mat, a, aout, b, bout, alpha, alpha_out, x, y, const_a=None, const_b=None):
    e = node(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    if a is not None:
        link(a, aout, e, "A")
    elif const_a is not None:
        e.set_editor_property("const_a", const_a)
    if b is not None:
        link(b, bout, e, "B")
    elif const_b is not None:
        e.set_editor_property("const_b", const_b)
    link(alpha, alpha_out, e, "Alpha")
    return e


def finish(mat, instanced=False):
    if instanced:
        mat.set_editor_property("used_with_instanced_static_meshes", True)
    try:
        mel.layout_material_expressions(mat)
    except Exception:
        pass
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat, False)
    log("matériau " + mat.get_name())


# ---------- matériaux ----------

def make_lit():
    """Couleurs par sommet (voiture, vibreurs, murs, formes simples).
    Rugosité = Rough × alpha du sommet (la voiture code sa peinture brillante, son carbone, ses pneus mats dans l'alpha),
    métal = Metal × (1 − alpha)."""
    mat = material("M_GPM_Lit")
    vc = node(mat, unreal.MaterialExpressionVertexColor, -900, 0)
    col = vector(mat, "Color", (1, 1, 1), -900, -200)
    output(mul(mat, vc, "", col, "", -550, -100), "", MP.MP_BASE_COLOR)
    rough = scalar(mat, "Rough", 0.6, -900, 220)
    output(mul(mat, rough, "", vc, "A", -550, 200), "", MP.MP_ROUGHNESS)
    inv = node(mat, unreal.MaterialExpressionOneMinus, -700, 360)
    link(vc, "A", inv, "")
    metal = scalar(mat, "Metal", 0.0, -900, 420)
    output(mul(mat, metal, "", inv, "", -550, 380), "", MP.MP_METALLIC)
    output(scalar(mat, "Spec", 0.5, -550, 520), "", MP.MP_SPECULAR)
    output(vector(mat, "Glow", (0, 0, 0), -550, 640), "", MP.MP_EMISSIVE_COLOR)
    finish(mat, instanced=True)


def make_ground(tex_default, normal_default):
    """Sol photo projeté à plat (asphalte, herbe, sable, gravier) : pas besoin d'UV, grande variation pour casser
    la répétition, couleur désaturée puis teintée, relief, et piste mouillée (Wet)."""
    mat = material("M_GPM_Ground")
    wp = node(mat, unreal.MaterialExpressionWorldPosition, -1700, 0)
    xy = node(mat, unreal.MaterialExpressionComponentMask, -1500, 0, r=True, g=True, b=False, a=False)
    link(wp, "", xy, "")
    uv = mul(mat, xy, "", scalar(mat, "TexScale", 0.001, -1500, 140), "", -1300, 0)
    tex = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1050, -200, parameter_name="Tex", texture=tex_default)
    link(uv, "", tex, "UVs")
    uv2 = mul(mat, uv, "", const(mat, 0.117, -1300, 160), "", -1150, 160)
    tex2 = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1050, 160, parameter_name="Tex", texture=tex_default)
    link(uv2, "", tex2, "UVs")
    # couleur : photo désaturée (Desat), variation à grande échelle, teinte (Color), couleur du sommet
    desat = node(mat, unreal.MaterialExpressionDesaturation, -800, -200)
    link(tex, "RGB", desat, "")
    link(scalar(mat, "Desat", 0.0, -1000, -330), "", desat, "Fraction")
    var = lerp(mat, None, "", None, "", tex2, "G", -800, 120, const_a=0.72, const_b=1.3)
    c1 = mul(mat, desat, "", var, "", -600, -120)
    c2 = mul(mat, c1, "", vector(mat, "Color", (1, 1, 1), -800, -400), "", -450, -200)
    vc = node(mat, unreal.MaterialExpressionVertexColor, -800, 300)
    c3 = mul(mat, c2, "", vc, "", -300, -100)
    wet = scalar(mat, "Wet", 0.0, -600, 520)
    dark = mul(mat, c3, "", lerp(mat, None, "", None, "", wet, "", -300, 40, const_a=1.0, const_b=0.55), "", -120, -60)
    output(dark, "", MP.MP_BASE_COLOR)
    # rugosité, reflets
    rough = lerp(mat, scalar(mat, "Rough", 0.85, -450, 380), "", None, "", wet, "", -120, 360, const_b=0.12)
    output(rough, "", MP.MP_ROUGHNESS)
    output(scalar(mat, "Spec", 0.35, -120, 480), "", MP.MP_SPECULAR)
    # relief (Bump = 0 : plat)
    ntex = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1050, 600, parameter_name="NTex",
                texture=normal_default, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    link(uv, "", ntex, "UVs")
    flat = node(mat, unreal.MaterialExpressionConstant3Vector, -800, 760, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    nrm = lerp(mat, flat, "", ntex, "RGB", scalar(mat, "Bump", 1.0, -800, 860), "", -450, 700)
    output(nrm, "", MP.MP_NORMAL)
    finish(mat)


def make_inst():
    """Formes instanciées du décor (arbres, immeubles, tribunes, montagnes) : chaque instance a sa teinte
    (données d'instance 0 = luminosité, 1 = mélange Color → Color2) ; Windows = 1 : bandeaux vitrés des immeubles."""
    mat = material("M_GPM_Inst")
    cd0 = node(mat, unreal.MaterialExpressionPerInstanceCustomData, -1300, 0, data_index=0)
    cd1 = node(mat, unreal.MaterialExpressionPerInstanceCustomData, -1300, 140, data_index=1)
    hue = lerp(mat, vector(mat, "Color", (0.5, 0.5, 0.5), -1100, -240), "", vector(mat, "Color2", (0.5, 0.5, 0.5), -1100, -100), "", cd1, "", -850, -150)
    bright = lerp(mat, None, "", None, "", cd0, "", -850, 40, const_a=0.72, const_b=1.28)
    base = mul(mat, hue, "", bright, "", -650, -80)
    # bandeaux vitrés : un étage sur deux (3,3 m), seulement sur les faces verticales
    wp = node(mat, unreal.MaterialExpressionWorldPosition, -1500, 360)
    z = node(mat, unreal.MaterialExpressionComponentMask, -1300, 360, r=False, g=False, b=True, a=False)
    link(wp, "", z, "")
    floors = mul(mat, z, "", const(mat, 1.0 / 330.0, -1300, 480), "", -1150, 380)
    frac = node(mat, unreal.MaterialExpressionFrac, -1000, 380)
    link(floors, "", frac, "")
    shift = node(mat, unreal.MaterialExpressionAdd, -880, 380)
    link(frac, "", shift, "A")
    shift.set_editor_property("const_b", -0.08)
    band = node(mat, unreal.MaterialExpressionRound, -760, 380)
    link(shift, "", band, "")
    nws = node(mat, unreal.MaterialExpressionVertexNormalWS, -1300, 600)
    nz = node(mat, unreal.MaterialExpressionComponentMask, -1150, 600, r=False, g=False, b=True, a=False)
    link(nws, "", nz, "")
    nabs = node(mat, unreal.MaterialExpressionAbs, -1000, 600)
    link(nz, "", nabs, "")
    side = node(mat, unreal.MaterialExpressionOneMinus, -880, 600)
    link(nabs, "", side, "")
    w1 = mul(mat, band, "", side, "", -650, 460)
    win = mul(mat, w1, "", scalar(mat, "Windows", 0.0, -800, 720), "", -500, 520)
    color = lerp(mat, base, "", vector(mat, "Glass", (0.05, 0.08, 0.12), -650, 220), "", win, "", -300, 0)
    output(color, "", MP.MP_BASE_COLOR)
    rough = lerp(mat, scalar(mat, "Rough", 0.85, -500, 300), "", None, "", win, "", -300, 300, const_b=0.06)
    output(rough, "", MP.MP_ROUGHNESS)
    output(scalar(mat, "Spec", 0.4, -300, 450), "", MP.MP_SPECULAR)
    output(vector(mat, "Glow", (0, 0, 0), -300, 560), "", MP.MP_EMISSIVE_COLOR)
    finish(mat, instanced=True)


def make_sky():
    """Ciel en dégradé, soleil et chaînes de montagnes à l'horizon (couleurs par sommet, sans éclairage, comme le jeu web).
    Bright = intensité (le soleil brille fort pour le halo), Gloom = grisaille de la pluie (vers Grey)."""
    mat = material("M_GPM_Sky")
    setp(mat, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    setp(mat, "two_sided", True)
    vc = node(mat, unreal.MaterialExpressionVertexColor, -900, 0)
    grey = vector(mat, "Grey", (0.33, 0.35, 0.38), -900, 160)
    mix = lerp(mat, vc, "", grey, "", scalar(mat, "Gloom", 0.0, -900, 300), "", -600, 60)
    output(mul(mat, mix, "", scalar(mat, "Bright", 1.0, -600, 260), "", -350, 100), "", MP.MP_EMISSIVE_COLOR)
    finish(mat)


def make_clouds(tex, alpha):
    """Couche de nuages photographiques par-dessus le dégradé (comme le jeu web) : Drift = dérive lente,
    Gloom = ciel couvert sous la pluie (nuages plus denses et plus gris)."""
    mat = material("M_GPM_Clouds")
    setp(mat, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    setp(mat, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    setp(mat, "two_sided", True)
    uv0 = node(mat, unreal.MaterialExpressionTextureCoordinate, -1500, 0)
    t = node(mat, unreal.MaterialExpressionTime, -1500, 160)
    drift = mul(mat, t, "", scalar(mat, "Drift", 0.0012, -1500, 260), "", -1300, 180)
    shift = node(mat, unreal.MaterialExpressionAppendVector, -1150, 180)
    link(drift, "", shift, "A")
    link(const(mat, 0.0, -1300, 320), "", shift, "B")
    uv = node(mat, unreal.MaterialExpressionAdd, -1000, 60)
    link(uv0, "", uv, "A")
    link(shift, "", uv, "B")
    col = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -800, -120, parameter_name="Tex", texture=tex)
    link(uv, "", col, "UVs")
    msk = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -800, 200, parameter_name="Mask", texture=alpha,
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    link(uv, "", msk, "UVs")
    gloom = scalar(mat, "Gloom", 0.0, -800, 420)
    tint = lerp(mat, vector(mat, "Tint", (1, 1, 1), -600, -300), "", vector(mat, "Grey", (0.42, 0.44, 0.47), -600, -180), "",
                gloom, "", -400, -240)
    c1 = mul(mat, col, "RGB", tint, "", -250, -150)
    output(mul(mat, c1, "", scalar(mat, "Bright", 1.0, -400, -40), "", -100, -100), "", MP.MP_EMISSIVE_COLOR)
    dens = node(mat, unreal.MaterialExpressionAdd, -500, 380)
    link(scalar(mat, "Opacity", 1.0, -700, 340), "", dens, "A")
    link(mul(mat, gloom, "", const(mat, 1.5, -700, 520), "", -600, 460), "", dens, "B")
    a = mul(mat, msk, "R", dens, "", -300, 300)
    sat = node(mat, unreal.MaterialExpressionSaturate, -150, 300)
    link(a, "", sat, "")
    output(sat, "", MP.MP_OPACITY)
    finish(mat)


def main():
    log("version %d, projet %s" % (VERSION, PROJECT))
    textures = {}
    for file_name, asset_name, normal, linear, sky in [
        ("asphalte.jpg", "T_Asphalt", False, False, False),
        ("asphalte-normal.jpg", "T_Asphalt_N", True, False, False),
        ("herbe.jpg", "T_Grass", False, False, False),
        ("gravier.jpg", "T_Gravel", False, False, False),
        ("sable.jpg", "T_Sand", False, False, False),
        ("nuages.jpg", "T_Clouds", False, False, True),
        ("nuages-alpha.jpg", "T_CloudsA", False, True, True),
    ]:
        try:
            textures[asset_name] = import_texture(file_name, asset_name, normal, linear, sky)
        except Exception:
            fail("texture %s : %s" % (asset_name, traceback.format_exc()))
    for fn, args in [(make_lit, ()), (make_ground, (textures.get("T_Asphalt"), textures.get("T_Asphalt_N"))), (make_inst, ()),
                     (make_sky, ()), (make_clouds, (textures.get("T_Clouds"), textures.get("T_CloudsA")))]:
        try:
            fn(*args)
        except Exception:
            fail("%s : %s" % (fn.__name__, traceback.format_exc()))
    stamp_dir = os.path.join(PROJECT, "Content", "GPM")
    os.makedirs(stamp_dir, exist_ok=True)
    with open(os.path.join(stamp_dir, "assets-v%d.txt" % VERSION), "w") as f:
        f.write("erreurs : %d\n" % len(errors))
        for e in errors:
            f.write(e + "\n")
    log("terminé, %d erreur(s)" % len(errors))


main()
