"""
qr_build_qr_materials.py -- build the QR material library and dress
EVERY generated mesh (wildlife, flora, trees, rocks, weapons, stations,
walls, food, handheld items, POI props, Remnant, Vanguard) with real,
description-tuned materials instead of import-grey.

What it does:
  1. Creates 3 parameterized MASTER materials under
     /Game/QuietRift/Materials/Masters:
       M_QR_Master              opaque    (BaseColor/Roughness/Metallic/
                                           EmissiveColor*EmissiveStrength)
       M_QR_Master_Foliage      two-sided opaque (crystalline flora --
                                           prismatic cellulose leaves)
       M_QR_Master_Translucent  glass/crystal (adds Opacity param)
  2. Creates one MaterialInstanceConstant per material SLOT NAME under
     /Game/QuietRift/Materials/Instances, colored from:
       a. the explicit spec table below (canonical palette + wildlife
          species zones from the GDD descriptions), else
       b. keyword heuristics on the slot name (horn->crystal,
          mane/glow/ember->emissive, plate/shell->ceramic, ...).
  3. Walks every StaticMesh + SkeletalMesh under /Game/Meshes and
     assigns the matching MIC into each material slot by name.

The Blender generators name their material slots consistently
("Wildlife_<Species>_<Part>", "Flora_<Species>_<Part>", palette names
like Gunmetal/Wood/Stone for weapons + walls), so slot-name matching
colors both the static and the rigged mesh sets from one table.

Run from the UE Python console (idempotent; re-run any time):
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_build_qr_materials.py').read())
  run()               # create missing + assign
  run(force=True)     # re-assign every slot even if already set
"""

import unreal

MASTER_DIR = "/Game/QuietRift/Materials/Masters"
MI_DIR = "/Game/QuietRift/Materials/Instances"
MESH_ROOT = "/Game/Meshes"

MEL = unreal.MaterialEditingLibrary


# ─── Spec tables ─────────────────────────────────────────────────────
# spec = (master, (r,g,b), roughness, metallic, emissive_rgb_or_None, strength)
#   master: "opaque" | "foliage" | "glass"

def S(rgb, rough=0.75, metal=0.0, emis=None, strength=0.0, master="opaque"):
    return (master, rgb, rough, metal, emis, strength)


# Canonical Blender palette names (weapons, walls, stations use these
# verbatim as slot names).
PALETTE_SPECS = {
    "Steel":            S((0.55, 0.55, 0.58), 0.40, 1.0),
    "DarkSteel":        S((0.30, 0.31, 0.34), 0.45, 1.0),
    "Gunmetal":         S((0.24, 0.25, 0.29), 0.45, 1.0),
    "Brass":            S((0.72, 0.55, 0.25), 0.35, 1.0),
    "Polymer":          S((0.12, 0.12, 0.13), 0.60, 0.0),
    "Glass":            S((0.75, 0.85, 0.90), 0.10, 0.0, master="glass"),
    "Wood":             S((0.45, 0.32, 0.20), 0.80),
    "DarkWood":         S((0.28, 0.20, 0.13), 0.85),
    "Stone":            S((0.55, 0.53, 0.50), 0.90),
    "DarkStone":        S((0.32, 0.31, 0.30), 0.92),
    "Rock":             S((0.42, 0.40, 0.38), 0.95),
    "DarkRock":         S((0.25, 0.24, 0.23), 0.95),
    "VanguardRed":      S((0.55, 0.10, 0.10), 0.55),
    "ProgenitorStone":  S((0.60, 0.62, 0.66), 0.30, 0.1,
                          (0.30, 0.80, 0.90), 0.4),
    "GlowCyan":         S((0.20, 0.70, 0.80), 0.30, 0.0, (0.20, 0.85, 0.95), 5.0),
    "GlowGold":         S((0.85, 0.70, 0.25), 0.30, 0.0, (0.95, 0.75, 0.25), 5.0),
    "GlowRed":          S((0.80, 0.15, 0.10), 0.30, 0.0, (0.90, 0.15, 0.08), 5.0),
    "GlowViolet":       S((0.55, 0.20, 0.75), 0.30, 0.0, (0.60, 0.20, 0.85), 5.0),
    "Ember":            S((0.90, 0.35, 0.05), 0.40, 0.0, (1.00, 0.35, 0.05), 3.0),
    "Building_Thatch":  S((0.55, 0.45, 0.25), 0.95),
    "Station_Rope":     S((0.50, 0.42, 0.28), 0.90),
    "Station_Coal":     S((0.05, 0.05, 0.05), 0.85),
    "Station_KilnAsh":  S((0.60, 0.58, 0.55), 0.95),
    "Station_Ash":      S((0.58, 0.56, 0.53), 0.95),
    "Station_AmmoBox":  S((0.25, 0.30, 0.20), 0.60),
    "Station_Placard":  S((0.70, 0.65, 0.50), 0.55),
    # Shared wildlife accents from the static generator.
    "Wildlife_EyeGlint": S((0.95, 0.85, 0.30), 0.10, 0.0,
                           (0.95, 0.85, 0.30), 4.0),
    "Wildlife_Tusk":     S((0.92, 0.88, 0.78), 0.55),
    "Wildlife_LanternMite": S((0.30, 0.60, 0.30), 0.50, 0.0,
                              (0.25, 0.65, 0.25), 3.5),
}

# Wildlife species zone colors (Body / Accent + specials), straight from
# the species descriptions. Keys are matched against the species token in
# "Wildlife_<Token>_<Part>" by prefix, so the static generator's short
# tokens ("Ridgeback") and the rigged generator's long ones
# ("RidgebackGrazer") both hit the same row.
#   token: { part_keyword: spec }   ("*" = any unmatched part)
WILDLIFE = {
    "Ridgeback":   {"Body": S((0.65, 0.55, 0.40), 0.85), "*": S((0.55, 0.45, 0.32), 0.80)},
    "Glasshorn":   {"Body": S((0.65, 0.55, 0.40), 0.85),
                    "Horn": S((0.85, 0.92, 0.95), 0.20, 0.0, (0.50, 0.85, 0.95), 2.5, "glass"),
                    "*": S((0.60, 0.50, 0.38), 0.85)},
    "Ashback":     {"Body": S((0.45, 0.35, 0.25), 0.85), "*": S((0.20, 0.18, 0.18), 0.85)},
    "Hookjaw":     {"Body": S((0.20, 0.10, 0.10), 0.80), "*": S((0.10, 0.05, 0.05), 0.70)},
    "Thornhide":   {"Body": S((0.45, 0.35, 0.25), 0.85), "*": S((0.20, 0.15, 0.10), 0.75)},
    "MireOx":      {"Body": S((0.65, 0.55, 0.40), 0.90), "*": S((0.18, 0.14, 0.10), 0.80)},
    "LanternMite": {"*": S((0.30, 0.55, 0.30), 0.60, 0.0, (0.25, 0.65, 0.25), 3.0)},
    "BurrowEel":   {"Body": S((0.45, 0.50, 0.35), 0.80), "*": S((0.45, 0.15, 0.15), 0.60)},
    "Ironmantle":  {"Body": S((0.30, 0.30, 0.35), 0.55, 0.6), "*": S((0.18, 0.18, 0.20), 0.65)},
    "PaleRafter":  {"Body": S((0.85, 0.85, 0.80), 0.75), "*": S((0.30, 0.20, 0.20), 0.70)},
    "Stonebelly":  {"Body": S((0.55, 0.55, 0.50), 0.90), "*": S((0.40, 0.35, 0.30), 0.95)},
    "Embermane":   {"Body": S((0.20, 0.10, 0.10), 0.75),
                    "Mane": S((0.85, 0.30, 0.05), 0.40, 0.0, (0.85, 0.30, 0.05), 4.0),
                    "*": S((0.18, 0.10, 0.10), 0.70)},
    "PebbleSkitter":  {"Body": S((0.40, 0.40, 0.40), 0.90), "*": S((0.85, 0.78, 0.70), 0.80)},
    "GleamLarver":    {"Body": S((0.85, 0.82, 0.75), 0.35), "*": S((0.72, 0.74, 0.78), 0.20, 0.4)},
    "Shardback":      {"Body": S((0.78, 0.72, 0.60), 0.85), "*": S((0.88, 0.85, 0.78), 0.60)},
    "SiltStrider":    {"Body": S((0.40, 0.30, 0.20), 0.85), "*": S((0.20, 0.55, 0.55), 0.50)},
    "Pillarback":     {"Body": S((0.12, 0.10, 0.08), 0.95),
                       "Glow": S((0.55, 0.48, 0.30), 0.60, 0.0, (0.55, 0.48, 0.30), 3.0),
                       "*": S((0.18, 0.15, 0.12), 0.90)},
    "BasinTreader":   {"Body": S((0.45, 0.45, 0.48), 0.85), "*": S((0.80, 0.78, 0.72), 0.75)},
    "Nestweaver":     {"Body": S((0.60, 0.60, 0.58), 0.75), "*": S((0.92, 0.88, 0.80), 0.25)},
    "Milkbladder":    {"Body": S((0.65, 0.55, 0.42), 0.85), "*": S((0.95, 0.92, 0.85), 0.60)},
    "BoneLantern":    {"Body": S((0.92, 0.90, 0.85), 0.55),
                       "*": S((0.20, 0.45, 0.65), 0.30, 0.0, (0.20, 0.45, 0.65), 4.0)},
    "Latchfin":       {"Body": S((0.20, 0.15, 0.10), 0.70), "*": S((0.70, 0.65, 0.55), 0.40)},
    "Crackrunner":    {"Body": S((0.10, 0.08, 0.08), 0.70), "*": S((0.55, 0.25, 0.10), 0.60)},
    "RidgeCourser":   {"Body": S((0.55, 0.45, 0.32), 0.85), "*": S((0.15, 0.13, 0.10), 0.70)},
    "Vaultback":      {"Body": S((0.35, 0.30, 0.25), 0.90), "*": S((0.10, 0.08, 0.08), 0.85)},
    "Tetherback":     {"Body": S((0.50, 0.45, 0.38), 0.90), "*": S((0.72, 0.62, 0.48), 0.80)},
    "SutureWisp":     {"Body": S((0.05, 0.04, 0.06), 0.30, 0.0, (0.20, 0.05, 0.40), 1.5),
                       "*": S((0.55, 0.20, 0.75), 0.25, 0.0, (0.55, 0.20, 0.75), 4.0)},
    "NeedleMaw":      {"Body": S((0.78, 0.72, 0.62), 0.85), "*": S((0.55, 0.10, 0.10), 0.50)},
    "DriftStalker":   {"Body": S((0.40, 0.38, 0.35), 0.75),
                       "Canopy": S((0.20, 0.18, 0.20), 0.20, 0.0, master="glass"),
                       "*": S((0.80, 0.40, 0.10), 0.40, 0.0, (0.80, 0.40, 0.10), 3.0)},
    "VaneRipper":     {"Body": S((0.18, 0.16, 0.18), 0.75), "*": S((0.45, 0.42, 0.38), 0.65)},
    "Glassjaw":       {"Body": S((0.75, 0.70, 0.55), 0.30, 0.0, (0.55, 0.50, 0.35), 1.0, "glass"),
                       "*": S((0.85, 0.55, 0.60), 0.60)},
    "SiltHound":      {"Body": S((0.10, 0.08, 0.06), 0.60), "*": S((0.20, 0.50, 0.50), 0.30)},
    "TrenchDigger":   {"Body": S((0.10, 0.08, 0.08), 0.85),
                       "Collar": S((0.45, 0.35, 0.25), 0.70),
                       "*": S((0.82, 0.78, 0.65), 0.60)},
    "CarrionChoir":   {"Body": S((0.78, 0.75, 0.65), 0.65),
                       "*": S((0.30, 0.55, 0.70), 0.40, 0.0, (0.30, 0.55, 0.70), 3.0)},
    "Fogleech":       {"*": S((0.12, 0.22, 0.12), 0.70, 0.0, (0.08, 0.18, 0.08), 1.5)},
    "Ironstag":       {"Body": S((0.15, 0.10, 0.10), 0.75),
                       "Plate": S((0.30, 0.28, 0.35), 0.45, 0.7),
                       "*": S((0.65, 0.22, 0.08), 0.35, 0.0, (0.65, 0.22, 0.08), 3.5)},
    "Shellmaw":       {"Body": S((0.38, 0.28, 0.20), 0.95), "*": S((0.65, 0.48, 0.40), 0.60)},
}

# Keyword fallback for slots no table row covers. First hit wins.
KEYWORD_SPECS = [
    (("glass", "crystal", "prism", "horn", "shard"),
     S((0.70, 0.85, 0.88), 0.15, 0.0, (0.35, 0.70, 0.80), 1.2, "glass")),
    (("glow", "ember", "mane", "vein", "lume"),
     S((0.85, 0.45, 0.15), 0.35, 0.0, (0.90, 0.45, 0.12), 3.0)),
    (("leaf", "frond", "canopy", "bloom", "petal", "grass", "blade"),
     S((0.35, 0.60, 0.45), 0.45, 0.0, (0.10, 0.30, 0.20), 0.4, "foliage")),
    (("trunk", "bark", "root", "branch"), S((0.38, 0.30, 0.24), 0.85)),
    (("tusk", "bone", "needle", "ivory", "shell"), S((0.90, 0.86, 0.76), 0.55)),
    (("plate", "armor", "armour", "mantle", "carapace"), S((0.30, 0.30, 0.34), 0.55, 0.4)),
    (("metal", "steel", "iron", "gun", "barrel"), S((0.40, 0.41, 0.44), 0.45, 1.0)),
    (("rock", "stone", "boulder", "slag"), S((0.42, 0.40, 0.38), 0.95)),
    (("wood", "plank", "timber"), S((0.45, 0.32, 0.20), 0.80)),
    (("cloth", "fabric", "leather", "strap", "thatch"), S((0.45, 0.38, 0.28), 0.85)),
    (("meat", "flesh", "maw"), S((0.60, 0.25, 0.22), 0.55)),
    (("body",), S((0.55, 0.48, 0.38), 0.85)),
]

DEFAULT_SPEC = S((0.50, 0.48, 0.45), 0.80)


# ─── Master material construction ────────────────────────────────────

def _asset_tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def _ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _build_master(name, two_sided=False, translucent=False):
    path = "{}/{}".format(MASTER_DIR, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)

    mat = _asset_tools().create_asset(name, MASTER_DIR, unreal.Material,
                                      unreal.MaterialFactoryNew())
    if not mat:
        return None

    base = MEL.create_material_expression(
        mat, unreal.MaterialExpressionVectorParameter, -700, -300)
    base.set_editor_property("parameter_name", "BaseColor")
    base.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = MEL.create_material_expression(
        mat, unreal.MaterialExpressionScalarParameter, -700, -100)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.7)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal = MEL.create_material_expression(
        mat, unreal.MaterialExpressionScalarParameter, -700, 0)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.0)
    MEL.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)

    emis = MEL.create_material_expression(
        mat, unreal.MaterialExpressionVectorParameter, -700, 150)
    emis.set_editor_property("parameter_name", "EmissiveColor")
    emis.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 1))
    estr = MEL.create_material_expression(
        mat, unreal.MaterialExpressionScalarParameter, -700, 300)
    estr.set_editor_property("parameter_name", "EmissiveStrength")
    estr.set_editor_property("default_value", 0.0)
    mul = MEL.create_material_expression(
        mat, unreal.MaterialExpressionMultiply, -400, 200)
    MEL.connect_material_expressions(emis, "", mul, "A")
    MEL.connect_material_expressions(estr, "", mul, "B")
    MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    if two_sided:
        mat.set_editor_property("two_sided", True)
    if translucent:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        op = MEL.create_material_expression(
            mat, unreal.MaterialExpressionScalarParameter, -700, 450)
        op.set_editor_property("parameter_name", "Opacity")
        op.set_editor_property("default_value", 0.45)
        MEL.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    print("[qr-mat] master {} created".format(name))
    return mat


# ─── Spec resolution ─────────────────────────────────────────────────

def _spec_for_slot(slot):
    if slot in PALETTE_SPECS:
        return PALETTE_SPECS[slot]
    if slot.startswith("Wildlife_"):
        rest = slot[len("Wildlife_"):]
        token, _, part = rest.partition("_")
        # Case-insensitive prefix match: the rigged generator's .title()
        # lowercases interior capitals ("RidgebackGrazer" ->
        # "Ridgebackgrazer"), and the static generator uses short tokens
        # ("Ridgeback") -- lowering both sides makes every variant hit.
        tl = token.lower()
        for key, zones in WILDLIFE.items():
            kl = key.lower()
            if tl.startswith(kl) or kl.startswith(tl):
                for part_key, spec in zones.items():
                    if part_key != "*" and part_key.lower() in part.lower():
                        return spec
                if "Body" in zones and part.lower() == "body":
                    return zones["Body"]
                return zones.get("*", zones.get("Body", DEFAULT_SPEC))
    low = slot.lower()
    for keywords, spec in KEYWORD_SPECS:
        if any(k in low for k in keywords):
            return spec
    return DEFAULT_SPEC


def _get_or_create_mic(slot, masters, created):
    safe = "MI_" + "".join(c if (c.isalnum() or c == "_") else "_" for c in slot)
    path = "{}/{}".format(MI_DIR, safe)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)

    master_kind, rgb, rough, metal, emis, strength = _spec_for_slot(slot)
    master = masters.get(master_kind) or masters["opaque"]
    mic = _asset_tools().create_asset(safe, MI_DIR,
                                      unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
    if not mic:
        return None
    mic.set_editor_property("parent", master)
    MEL.set_material_instance_vector_parameter_value(
        mic, "BaseColor", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    MEL.set_material_instance_scalar_parameter_value(mic, "Roughness", rough)
    MEL.set_material_instance_scalar_parameter_value(mic, "Metallic", metal)
    if emis:
        MEL.set_material_instance_vector_parameter_value(
            mic, "EmissiveColor", unreal.LinearColor(emis[0], emis[1], emis[2], 1.0))
        MEL.set_material_instance_scalar_parameter_value(
            mic, "EmissiveStrength", strength)
    unreal.EditorAssetLibrary.save_loaded_asset(mic)
    created.append(safe)
    return mic


# ─── Mesh walking + assignment ───────────────────────────────────────

def _all_meshes():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([MESH_ROOT], True)
    out = []
    for ad in registry.get_assets_by_path(MESH_ROOT, recursive=True):
        try:
            cls = str(ad.asset_class_path.asset_name)
        except Exception:
            continue
        if cls in ("StaticMesh", "SkeletalMesh"):
            out.append(("{}.{}".format(ad.package_name, ad.asset_name), cls))
    return out


def _assign_static(mesh, masters, created, force):
    mats = list(mesh.static_materials)
    changed = False
    for i, sm in enumerate(mats):
        slot = str(sm.material_slot_name)
        if not slot:
            continue
        existing = sm.material_interface
        if existing and not force and str(existing.get_name()).startswith("MI_"):
            continue
        mic = _get_or_create_mic(slot, masters, created)
        if not mic:
            continue
        mats[i] = unreal.StaticMaterial(material_interface=mic,
                                        material_slot_name=slot)
        changed = True
    if changed:
        mesh.set_editor_property("static_materials", mats)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return changed


def _assign_skeletal(mesh, masters, created, force):
    mats = list(mesh.materials)
    changed = False
    for i, sm in enumerate(mats):
        slot = str(sm.material_slot_name)
        if not slot:
            continue
        existing = sm.material_interface
        if existing and not force and str(existing.get_name()).startswith("MI_"):
            continue
        mic = _get_or_create_mic(slot, masters, created)
        if not mic:
            continue
        mats[i] = unreal.SkeletalMaterial(material_interface=mic,
                                          material_slot_name=slot)
        changed = True
    if changed:
        mesh.set_editor_property("materials", mats)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return changed


def run(force=False):
    print("\n=== qr_build_qr_materials ===")
    _ensure_dir(MASTER_DIR)
    _ensure_dir(MI_DIR)

    masters = {
        "opaque":  _build_master("M_QR_Master"),
        "foliage": _build_master("M_QR_Master_Foliage", two_sided=True),
        "glass":   _build_master("M_QR_Master_Translucent", translucent=True),
    }
    if not masters["opaque"]:
        print("[qr-mat] master creation failed -- aborting")
        return

    created = []
    touched = 0
    meshes = _all_meshes()
    print("[qr-mat] {} meshes under {}".format(len(meshes), MESH_ROOT))
    for path, cls in meshes:
        mesh = unreal.load_asset(path)
        if not mesh:
            continue
        if cls == "StaticMesh":
            if _assign_static(mesh, masters, created, force):
                touched += 1
        else:
            if _assign_skeletal(mesh, masters, created, force):
                touched += 1

    print("[qr-mat] done -- {} MICs created, {} meshes re-dressed.".format(
        len(created), touched))
    print("[qr-mat] Instances live in {} -- tune any color there and it".format(MI_DIR))
    print("[qr-mat] propagates to every mesh sharing that slot name.")


if __name__ == "__main__":
    run()
