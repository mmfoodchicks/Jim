"""
qr_crystalline_flora.py — give project flora the canon crystalline look:
"prismatic cellulose with crystalline light-pipe veins — look like
stained glass, function like normal leaves."

Implementation: for every project-owned material referenced by flora
meshes (SM_PLT_/SM_FLO_/SM_BSH_/SM_TRE_ under /Game/Meshes), inject a
subtle fresnel-driven emissive:

    Emissive = BaseColorInput × Fresnel(exp 3) × CrystalTint

Edge-on leaf surfaces catch a cool violet-cyan glow (light-pipe rim),
face-on they stay natural — reads as translucent crystal veining at
dusk and under Jovianlight without turning the forest neon.

Rules honored: Fab pack materials untouched; idempotent (materials with
a connected EmissiveColor are skipped). Trees get a fainter tint than
plants so bark doesn't glow.

Run in the UE Python console:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_crystalline_flora.py').read())
  run()
"""

import unreal

MEL = unreal.MaterialEditingLibrary

# mesh prefix -> emissive tint (RGB, linear). Magnitude = strength.
CATEGORY_TINT = [
    ("SM_PLT_", (0.10, 0.22, 0.30)),
    ("SM_FLO_", (0.12, 0.20, 0.34)),
    ("SM_BSH_", (0.08, 0.16, 0.22)),
    ("SM_TRE_", (0.04, 0.08, 0.12)),   # canopy hint only — bark stays dark
]


def _tint_for(mesh_name):
    for prefix, tint in CATEGORY_TINT:
        if mesh_name.startswith(prefix):
            return tint
    return None


def _has_emissive(mat):
    try:
        return MEL.get_material_property_input_node(
            mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR) is not None
    except Exception:
        return False


def _inject_crystal(mat, tint):
    base_expr = None
    try:
        base_expr = MEL.get_material_property_input_node(
            mat, unreal.MaterialProperty.MP_BASE_COLOR)
    except Exception:
        base_expr = None

    fresnel = MEL.create_material_expression(
        mat, unreal.MaterialExpressionFresnel, -900, 700)
    fresnel.set_editor_property("exponent", 3.0)

    tint_c = MEL.create_material_expression(
        mat, unreal.MaterialExpressionConstant3Vector, -900, 800)
    tint_c.set_editor_property(
        "constant", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))

    mul_rim = MEL.create_material_expression(
        mat, unreal.MaterialExpressionMultiply, -700, 750)
    MEL.connect_material_expressions(fresnel, "", mul_rim, "A")
    MEL.connect_material_expressions(tint_c,  "", mul_rim, "B")

    final_expr = mul_rim
    if base_expr:
        # Modulate by the leaf's own color so veins pick up the atlas
        # hues (stained-glass, not uniform glow).
        mul_col = MEL.create_material_expression(
            mat, unreal.MaterialExpressionMultiply, -520, 730)
        MEL.connect_material_expressions(base_expr, "", mul_col, "A")
        MEL.connect_material_expressions(mul_rim,   "", mul_col, "B")
        final_expr = mul_col

    ok = MEL.connect_material_property(
        final_expr, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if not ok:
        print("[crystal]   emissive connect failed on {}".format(mat.get_name()))
        return False

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return True


def run():
    print("\n=== qr_crystalline_flora ===")
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path("/Game/Meshes", recursive=True)

    done = set()
    injected = skipped = 0
    for a in assets:
        if a.asset_class_path.asset_name != "StaticMesh":
            continue
        mesh_name = str(a.asset_name)
        tint = _tint_for(mesh_name)
        if not tint:
            continue
        mesh = unreal.load_asset(str(a.package_name))
        if not mesh:
            continue
        for i in range(mesh.get_num_sections(0)):
            mat = mesh.get_material(i)
            while mat and isinstance(mat, unreal.MaterialInstance):
                mat = mat.get_editor_property("parent")
            if not mat or not isinstance(mat, unreal.Material):
                continue
            path = mat.get_path_name()
            if not path.startswith("/Game/") or path in done:
                continue
            done.add(path)
            if _has_emissive(mat):
                skipped += 1
                continue
            if _inject_crystal(mat, tint):
                injected += 1
                print("[crystal]   + {} tint={}".format(mat.get_name(), tint))

    print("[crystal] injected {}, already-emissive {}.".format(injected, skipped))
    print("[crystal] Flora now carries the stained-glass rim under low sun / Jovianlight.")


if __name__ == "__main__":
    run()
