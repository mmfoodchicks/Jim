"""
qr_import_scan_textures.py -- wire the CC0 photo-scan library
(Content/QuietRift/ScanTextures/<Family>/T_<Family>_<Map>.jpg,
downloaded from ambientCG -- public domain) into the game:

  1. Imports every scan JPG as a UTexture2D (sRGB only on Color,
     green-channel flip on the OpenGL-convention normals).
  2. Builds M_QR_Scan -- a tiling PBR master (Color/Normal/Roughness/
     AO texture params + UVTiling scalar).
  3. Creates MI_QR_Scan_<Family> per scan set.
  4. Re-dresses the surfaces where TILING photo detail beats the
     per-asset baked atlases: build pieces (wood/stone/metal),
     boulders, tree bark slots, and the dev-map ground.
  5. Restamps biome profiles' LandscapeMaterial to matching scan MIs
     so worldgen terrain paint uses photo ground.

Small props keep their baked atlases (right tool at their scale).

Run AFTER qr_seed_items has imported the meshes:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_import_scan_textures.py').read())
  run()

Idempotent -- textures/materials are created once, assignment re-runs
are safe.
"""

import os
import unreal

SCAN_DISK = os.path.normpath(os.path.join(
    unreal.Paths.project_dir(), "Content", "QuietRift", "ScanTextures"))
SCAN_PKG = "/Game/QuietRift/ScanTextures"
MASTER_DIR = "/Game/QuietRift/Materials/Masters"
MI_DIR = "/Game/QuietRift/Materials/Scan"
MESH_ROOT = "/Game/Meshes"

MEL = unreal.MaterialEditingLibrary

MAP_SUFFIXES = {
    "Color": dict(srgb=True),
    "NormalGL": dict(srgb=False, normal=True),
    "Roughness": dict(srgb=False),
    "AmbientOcclusion": dict(srgb=False),
    "Displacement": dict(srgb=False),
}

# Mesh-name regex fragment -> (family, uv_tiling). Order matters.
BUILD_RULES = [
    ("REINFORCED", ("MetalPlates", 1.5)),
    ("METAL",      ("MetalPlates", 1.5)),
    ("STONE",      ("StoneWall", 2.0)),
    ("WOOD",       ("PlanksWorn", 2.0)),
    ("THATCH",     ("PlanksWorn", 2.0)),
    ("PALISADE",   ("BarkRough", 2.0)),
    ("TORCH",      ("BarkRough", 1.0)),
    ("BENCH",      ("PlanksClean", 1.5)),
    ("CRATE",      ("PlanksClean", 1.5)),
    ("",           ("PlanksWorn", 2.0)),      # BLD fallback
]

BOULDER_FAMILIES = ["RockGrey", "RockCliff", "RockMossy"]

TREE_BARK = {
    "GLASSBARK": "BarkPale",
    "SLAGROOT":  "BarkRough",
    "ASTERBARK": "BarkRough",
    "PRISMLEAF": "BarkRough",
}

# Biome profile keyword -> ground family.
BIOME_GROUND = [
    (("basalt", "ridge", "highland", "cliff"), "RockCliff"),
    (("sand", "dune", "sink"), "GroundSand"),
    (("swamp", "mire", "silt"), "GroundRocky"),
    (("burn", "ash", "cinder"), "Gravel"),
    (("", ), "GroundForest"),
]


def _asset_tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def _ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


# ─── 1. Texture import ───────────────────────────────────────────────

def _import_textures():
    if not os.path.isdir(SCAN_DISK):
        print("[scan] {} missing -- pull the repo's ScanTextures".format(SCAN_DISK))
        return {}
    families = {}
    tasks = []
    for family in sorted(os.listdir(SCAN_DISK)):
        fdir = os.path.join(SCAN_DISK, family)
        if not os.path.isdir(fdir):
            continue
        families[family] = {}
        for fn in sorted(os.listdir(fdir)):
            if not fn.lower().endswith((".jpg", ".png")):
                continue
            asset_name = os.path.splitext(fn)[0]
            dest = "{}/{}".format(SCAN_PKG, family)
            asset_path = "{}/{}".format(dest, asset_name)
            for suffix in MAP_SUFFIXES:
                if asset_name.endswith("_" + suffix):
                    families[family][suffix] = asset_path
                    break
            if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
                continue
            t = unreal.AssetImportTask()
            t.set_editor_property("filename", os.path.join(fdir, fn))
            t.set_editor_property("destination_path", dest)
            t.set_editor_property("automated", True)
            t.set_editor_property("save", True)
            tasks.append(t)
    if tasks:
        _asset_tools().import_asset_tasks(tasks)
    # Post-import texture settings.
    fixed = 0
    for family, maps in families.items():
        for suffix, asset_path in maps.items():
            tex = unreal.load_asset(asset_path)
            if not tex:
                continue
            spec = MAP_SUFFIXES[suffix]
            try:
                tex.set_editor_property("srgb", spec.get("srgb", False))
                if spec.get("normal"):
                    tex.set_editor_property(
                        "compression_settings",
                        unreal.TextureCompressionSettings.TC_NORMALMAP)
                    # ambientCG normals are OpenGL-convention (+Y up);
                    # UE expects DirectX (-Y), so flip green.
                    tex.set_editor_property("flip_green_channel", True)
                unreal.EditorAssetLibrary.save_loaded_asset(tex)
                fixed += 1
            except Exception as e:
                print("[scan]   texture settings failed {}: {}".format(asset_path, e))
    print("[scan] {} families, {} textures configured".format(len(families), fixed))
    return families


# ─── 2. Scan master material ─────────────────────────────────────────

def _build_scan_master():
    path = "{}/M_QR_Scan".format(MASTER_DIR)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    _ensure_dir(MASTER_DIR)
    mat = _asset_tools().create_asset("M_QR_Scan", MASTER_DIR, unreal.Material,
                                      unreal.MaterialFactoryNew())
    if not mat:
        return None

    coord = MEL.create_material_expression(
        mat, unreal.MaterialExpressionTextureCoordinate, -1100, 0)
    tiling = MEL.create_material_expression(
        mat, unreal.MaterialExpressionScalarParameter, -1100, 150)
    tiling.set_editor_property("parameter_name", "UVTiling")
    tiling.set_editor_property("default_value", 1.0)
    mul = MEL.create_material_expression(
        mat, unreal.MaterialExpressionMultiply, -900, 50)
    MEL.connect_material_expressions(coord, "", mul, "A")
    MEL.connect_material_expressions(tiling, "", mul, "B")

    def tex_param(name, y, sampler=None):
        node = MEL.create_material_expression(
            mat, unreal.MaterialExpressionTextureSampleParameter2D, -650, y)
        node.set_editor_property("parameter_name", name)
        if sampler is not None:
            node.set_editor_property("sampler_type", sampler)
        MEL.connect_material_expressions(mul, "", node, "UVs")
        return node

    base = tex_param("ColorMap", -250)
    MEL.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = tex_param("RoughnessMap", 0,
                      unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    MEL.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    nrm = tex_param("NormalMap", 250,
                    unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    MEL.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    ao = tex_param("AOMap", 500,
                   unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    MEL.connect_material_property(ao, "R",
                                  unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    print("[scan] master M_QR_Scan created")
    return mat


def _get_or_create_mi(family, maps, master, tiling=1.0):
    _ensure_dir(MI_DIR)
    name = "MI_QR_Scan_{}".format(family)
    path = "{}/{}".format(MI_DIR, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    mi = _asset_tools().create_asset(name, MI_DIR,
                                     unreal.MaterialInstanceConstant,
                                     unreal.MaterialInstanceConstantFactoryNew())
    if not mi:
        return None
    mi.set_editor_property("parent", master)
    param_map = {"ColorMap": "Color", "NormalMap": "NormalGL",
                 "RoughnessMap": "Roughness", "AOMap": "AmbientOcclusion"}
    for param, suffix in param_map.items():
        asset_path = maps.get(suffix)
        tex = unreal.load_asset(asset_path) if asset_path else None
        if tex:
            MEL.set_material_instance_texture_parameter_value(mi, param, tex)
    MEL.set_material_instance_scalar_parameter_value(mi, "UVTiling", tiling)
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    return mi


# ─── 4. Surface re-dress ─────────────────────────────────────────────

def _assign_static(mesh, slot_filter, mi):
    mats = list(mesh.static_materials)
    changed = False
    for i, sm in enumerate(mats):
        slot = str(sm.material_slot_name)
        if slot_filter and not slot_filter(slot):
            continue
        mats[i] = unreal.StaticMaterial(material_interface=mi,
                                        material_slot_name=slot)
        changed = True
    if changed:
        mesh.set_editor_property("static_materials", mats)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return changed


def _redress_meshes(mis):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([MESH_ROOT], True)
    dressed = 0
    for ad in registry.get_assets_by_path(MESH_ROOT, recursive=True):
        try:
            if str(ad.asset_class_path.asset_name) != "StaticMesh":
                continue
        except Exception:
            continue
        name = str(ad.asset_name)
        target = None
        if name.startswith("SM_BLD_"):
            for token, (family, tiling) in BUILD_RULES:
                if token in name.upper():
                    target = mis.get(family)
                    break
        elif name.startswith("SM_RCK_BOULDER_"):
            idx = "ABC".find(name[-1]) if name[-1] in "ABC" else 0
            target = mis.get(BOULDER_FAMILIES[idx % len(BOULDER_FAMILIES)])
        elif name.startswith("SM_TRE_"):
            for token, family in TREE_BARK.items():
                if token in name:
                    target = mis.get(family)
                    break
            if target:
                mesh = unreal.load_asset("{}.{}".format(ad.package_name, ad.asset_name))
                if mesh and _assign_static(
                        mesh, lambda s: "Bark" in s or "bark" in s, target):
                    dressed += 1
                continue
        if target is None:
            continue
        mesh = unreal.load_asset("{}.{}".format(ad.package_name, ad.asset_name))
        if mesh and _assign_static(mesh, None, target):
            dressed += 1
    print("[scan] {} meshes re-dressed with tiling scans".format(dressed))


# ─── 5. Biome landscape restamp ──────────────────────────────────────

def _restamp_biomes(mis):
    biome_dir = "/Game/QuietRift/Data/Biomes"
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    if not unreal.EditorAssetLibrary.does_directory_exist(biome_dir):
        print("[scan] no biome profiles yet -- run qr_seed_biome_profiles first")
        return
    stamped = 0
    for ad in registry.get_assets_by_path(biome_dir, recursive=True):
        profile = unreal.load_asset("{}.{}".format(ad.package_name, ad.asset_name))
        if not profile:
            continue
        low = str(ad.asset_name).lower()
        family = "GroundForest"
        for tokens, fam in BIOME_GROUND:
            if any(t and t in low for t in tokens):
                family = fam
                break
        mi = mis.get(family)
        if not mi:
            continue
        try:
            profile.set_editor_property("landscape_material", mi)
            unreal.EditorAssetLibrary.save_loaded_asset(profile)
            stamped += 1
        except Exception:
            pass
    print("[scan] {} biome profiles now paint photo ground".format(stamped))


def run():
    print("\n=== qr_import_scan_textures ===")
    families = _import_textures()
    if not families:
        return
    master = _build_scan_master()
    if not master:
        print("[scan] master creation failed -- aborting")
        return
    mis = {}
    for family, maps in families.items():
        mi = _get_or_create_mi(family, maps, master)
        if mi:
            mis[family] = mi
    print("[scan] {} scan material instances ready".format(len(mis)))
    _redress_meshes(mis)
    _restamp_biomes(mis)
    print("[scan] DONE. Dev floor: qr_create_test_maps now prefers")
    print("[scan] MI_QR_Scan_GroundForest when present.")


if __name__ == "__main__":
    run()
