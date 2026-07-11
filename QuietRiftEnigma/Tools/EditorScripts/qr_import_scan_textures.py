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

# PER-SLOT classifier: match a build-piece material SLOT NAME (the
# Blender palette names -- Wood/Stone/Glass/Steel/Ember/...) to a
# (family, uv_tiling) scan, so a window's Glass pane and a torch's
# emissive Ember head are PRESERVED (family None) while metal straps
# get metal and plank slots get planks. First match wins.
SLOT_FAMILY = [
    # Preserve translucent / emissive / light-emitting slots untouched.
    (("glass", "ember", "glow", "emiss", "eye", "led", "lamp",
      "lantern", "light", "fire", "coal", "crystal"), None),
    (("steel", "gunmetal", "metal", "iron", "chrome", "alloy",
      "hinge", "bolt", "strap", "plate"), ("MetalPlates", 1.5)),
    (("stone", "rock", "concrete", "basalt", "granite"), ("StoneWall", 2.0)),
    (("thatch", "straw", "reed"), ("PlanksWorn", 3.0)),
    (("wood", "plank", "timber", "log", "bark"), ("PlanksWorn", 2.0)),
    (("rope", "fabric", "cloth", "canvas", "leather", "hide"),
     ("FabricCanvas", 2.0)),
    (("ash", " kiln", "placard"), ("StoneWall", 2.0)),
]
# Fallback when a build-piece slot matches nothing above. Keyed off the
# MESH name so a stone pillar's unnamed slot still reads as stone.
def _build_default(mesh_name):
    up = mesh_name.upper()
    if any(t in up for t in ("STONE", "PILLAR", "FOUNDATION_SQUARE_STONE")):
        return ("StoneWall", 2.0)
    if "REINFORCED" in up:
        return ("MetalPlates", 1.5)
    if any(t in up for t in ("CRATE", "BENCH")):
        return ("PlanksClean", 1.5)
    if any(t in up for t in ("PALISADE", "TORCH")):
        return ("BarkRough", 1.5)
    return ("PlanksWorn", 2.0)


BOULDER_FAMILIES = ["RockGrey", "RockCliff", "RockMossy"]

TREE_BARK = {
    "GLASSBARK": "BarkPale",
    "SLAGROOT":  "BarkRough",
    "ASTERBARK": "BarkRough",
    "PRISMLEAF": "BarkRough",
}

# Biome PROFILE NAME (BP_<Name>, lowercased) -> ground family. The 14
# canonical profiles from qr_seed_biome_profiles.py are matched by name,
# not by generic keywords (the old swamp/burn tokens matched none).
BIOME_GROUND = [
    (("basaltshelf", "magneticridges", "highrims", "canyonwebs",
      "ridgeshadows"), "RockCliff"),
    (("glassdunes", "windplains"), "GroundSand"),
    (("wetbasins", "shallowfens", "coldbasins"), "GroundRocky"),
    (("mossfields", "meltlineedges"), "Moss"),
    (("thermalcracks", "craterfloors"), "Gravel"),
]
BIOME_GROUND_DEFAULT = "GroundForest"


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

def _force_delete(path):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        try:
            unreal.EditorAssetLibrary.delete_asset(path)
        except Exception as e:
            print("[scan]   delete {} skipped: {}".format(path, e))


def _build_scan_master(default_maps):
    """(Re)build the tiling scan master. default_maps is one family's
    {suffix: texture_asset_path} used for the parameter DEFAULT textures.

    Why defaults matter: a Material compiles with its parameter DEFAULT
    values, before any instance override. A TextureSampleParameter2D
    whose sampler_type is Normal/LinearColor but whose default texture
    is the engine's sRGB DefaultTexture is a type MISMATCH -> the whole
    master fails to compile -> every instance falls back to the default
    checkerboard. Seeding each param with a matching-type scan texture
    fixes it (this is exactly how Quixel/auto-material masters are set
    up). Always rebuilt so the fix lands on existing installs."""
    path = "{}/M_QR_Scan".format(MASTER_DIR)
    _force_delete(path)
    _ensure_dir(MASTER_DIR)
    mat = _asset_tools().create_asset("M_QR_Scan", MASTER_DIR, unreal.Material,
                                      unreal.MaterialFactoryNew())
    if not mat:
        return None

    def _tex(suffix):
        p = default_maps.get(suffix)
        return unreal.load_asset(p) if p else None

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

    def tex_param(name, y, default_tex, sampler=None):
        node = MEL.create_material_expression(
            mat, unreal.MaterialExpressionTextureSampleParameter2D, -650, y)
        node.set_editor_property("parameter_name", name)
        # Default texture FIRST -- setting sampler_type validates against
        # whatever texture is currently on the node.
        if default_tex is not None:
            node.set_editor_property("texture", default_tex)
        if sampler is not None:
            node.set_editor_property("sampler_type", sampler)
        MEL.connect_material_expressions(mul, "", node, "UVs")
        return node

    base = tex_param("ColorMap", -250, _tex("Color"),
                     unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    MEL.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = tex_param("RoughnessMap", 0, _tex("Roughness"),
                      unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    MEL.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    nrm = tex_param("NormalMap", 250, _tex("NormalGL"),
                    unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    MEL.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    ao = tex_param("AOMap", 500, _tex("AmbientOcclusion"),
                   unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    MEL.connect_material_property(ao, "R",
                                  unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    print("[scan] master M_QR_Scan rebuilt (param defaults seeded)")
    return mat


# Lazy (family, tiling) -> MaterialInstanceConstant factory. Populated
# by run(); one MI per distinct (family, tiling) so BarkRough@1.5 and
# BarkRough@2.0 are different assets carrying their tuned UVTiling.
_MI = {"families": {}, "master": None, "cache": {}}


def _mi_for(family, tiling):
    if not family:
        return None
    maps = _MI["families"].get(family)
    master = _MI["master"]
    if not maps or not master:
        return None
    key = (family, round(float(tiling), 2))
    if key in _MI["cache"]:
        return _MI["cache"][key]
    _ensure_dir(MI_DIR)
    tag = "x{}".format(str(key[1]).replace(".", "p"))
    name = "MI_QR_Scan_{}_{}".format(family, tag)
    path = "{}/{}".format(MI_DIR, name)
    # Force-rebuild: the master is rebuilt each run, so a stale MI would
    # carry the OLD (broken) parent shader map.
    _force_delete(path)
    mi = _asset_tools().create_asset(name, MI_DIR,
                                     unreal.MaterialInstanceConstant,
                                     unreal.MaterialInstanceConstantFactoryNew())
    if not mi:
        _MI["cache"][key] = None
        return None
    mi.set_editor_property("parent", master)
    param_map = {"ColorMap": "Color", "NormalMap": "NormalGL",
                 "RoughnessMap": "Roughness", "AOMap": "AmbientOcclusion"}
    for param, suffix in param_map.items():
        asset_path = maps.get(suffix)
        tex = unreal.load_asset(asset_path) if asset_path else None
        if tex:
            MEL.set_material_instance_texture_parameter_value(mi, param, tex)
    MEL.set_material_instance_scalar_parameter_value(mi, "UVTiling", float(tiling))
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    _MI["cache"][key] = mi
    return mi


def _slot_target(slot_name):
    """(family, tiling) for a build-piece material slot, or None to
    PRESERVE it (glass/emissive/etc). Returns the sentinel 'unmatched'
    when no keyword hits so the caller can apply the mesh default."""
    low = slot_name.lower()
    for tokens, fam_tiling in SLOT_FAMILY:
        if any(t in low for t in tokens):
            return fam_tiling            # may be None (preserve)
    return "unmatched"


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


def _redress_build_piece(mesh, mesh_name):
    """Per-slot re-dress: each slot resolved by its OWN material name so
    glass/emissive slots survive and metal/wood/stone each get the right
    scan. Unmatched slots take the mesh's structural default."""
    default = _build_default(mesh_name)
    mats = list(mesh.static_materials)
    changed = False
    for i, sm in enumerate(mats):
        slot = str(sm.material_slot_name)
        tgt = _slot_target(slot)
        if tgt is None:
            continue                      # preserve (glass, ember, ...)
        if tgt == "unmatched":
            tgt = default
        mi = _mi_for(tgt[0], tgt[1])
        if not mi:
            continue
        mats[i] = unreal.StaticMaterial(material_interface=mi,
                                        material_slot_name=slot)
        changed = True
    if changed:
        mesh.set_editor_property("static_materials", mats)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return changed


def _redress_meshes():
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
        obj = "{}.{}".format(ad.package_name, ad.asset_name)

        if name.startswith("SM_BLD_"):
            mesh = unreal.load_asset(obj)
            if mesh and _redress_build_piece(mesh, name):
                dressed += 1
        elif name.startswith("SM_RCK_BOULDER_"):
            idx = "ABC".find(name[-1]) if name[-1] in "ABC" else 0
            mi = _mi_for(BOULDER_FAMILIES[idx % len(BOULDER_FAMILIES)], 1.0)
            mesh = unreal.load_asset(obj)
            if mesh and mi and _assign_static(mesh, None, mi):
                dressed += 1
        elif name.startswith("SM_TRE_"):
            family = None
            for token, fam in TREE_BARK.items():
                if token in name:
                    family = fam
                    break
            mi = _mi_for(family, 1.0) if family else None
            if mi:
                mesh = unreal.load_asset(obj)
                # Only the bark slots (leaf-card slots must stay alpha).
                if mesh and _assign_static(
                        mesh, lambda s: "bark" in s.lower(), mi):
                    dressed += 1
    print("[scan] {} meshes re-dressed with tiling scans".format(dressed))


# ─── 5. Biome landscape restamp ──────────────────────────────────────

def _restamp_biomes():
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
        family = BIOME_GROUND_DEFAULT
        for tokens, fam in BIOME_GROUND:
            if any(t in low for t in tokens):
                family = fam
                break
        mi = _mi_for(family, 1.0)          # ground tiles at 1x (large UVs)
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
    # Seed the master's parameter defaults from a COMPLETE family (all
    # four maps present), else the missing-map params keep the engine's
    # sRGB DefaultTexture and the Normal/Linear sampler override re-
    # introduces the compile mismatch this rewrite exists to kill.
    # GroundForest is the canonical choice; fall back to any complete set.
    _needed = ("Color", "NormalGL", "Roughness", "AmbientOcclusion")
    def _complete(m):
        return m and all(m.get(k) for k in _needed)
    default_maps = (families.get("GroundForest") if _complete(
        families.get("GroundForest")) else None)
    if default_maps is None:
        default_maps = next((m for m in families.values() if _complete(m)),
                            families.get("GroundForest") or {})
    master = _build_scan_master(default_maps)
    if not master:
        print("[scan] master creation failed -- aborting")
        return
    # Prime the lazy MI factory. Instances are created on demand per
    # (family, tiling) as _redress/_restamp request them.
    _MI["families"] = families
    _MI["master"] = master
    _MI["cache"] = {}
    # Pre-create the ground MIs so a mostly-empty mesh set still yields
    # the dev-floor material.
    ground_fams = set(fam for _tokens, fam in BIOME_GROUND)
    ground_fams.add(BIOME_GROUND_DEFAULT)
    for fam in ground_fams:
        _mi_for(fam, 1.0)
    _redress_meshes()
    _restamp_biomes()
    print("[scan] {} scan material instances built".format(
        len([v for v in _MI["cache"].values() if v])))
    print("[scan] DONE. Dev floor uses MI_QR_Scan_GroundForest_x1p0.")


if __name__ == "__main__":
    run()
