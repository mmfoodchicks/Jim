"""
qr_add_wind_sway.py — inject SimpleGrassWind world-position-offset sway
into every FOLIAGE material the project owns, so trees and plants move
instead of standing frozen.

What it touches (OURS — generated/imported by our pipeline):
  • materials referenced by SM_TRE_* / SM_PLT_* / SM_GRS_* / SM_BSH_*
    mesh slots under /Game/Meshes (the Blender-baked flora atlases)
Fab pack contents are never edited (borrowed assets rule) — pack plants
that need sway should be re-pathed through our masters instead.

Per-category intensity: grass/plants sway hard, bushes medium, trees
subtle (trunk-safe amplitude — no vertex weights exist on the baked
meshes, so the whole mesh moves; low intensity reads as canopy sway
without visible root sliding).

Idempotent: a material that already drives World Position Offset is
left alone. Re-run safe.

Run in the UE Python console:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_add_wind_sway.py').read())
  run()
"""

import unreal

MEL = unreal.MaterialEditingLibrary

GRASS_WIND_FN = "/Engine/Functions/Engine_MaterialFunctions01/WorldPositionOffset/SimpleGrassWind.SimpleGrassWind"

# mesh-name prefix -> (WindIntensity, WindWeight, WindSpeed)
CATEGORY_WIND = [
    ("SM_GRS_", (8.0,  1.00, 1.6)),
    ("SM_PLT_", (5.0,  0.85, 1.2)),
    ("SM_BSH_", (4.0,  0.70, 1.0)),
    ("SM_FLO_", (5.0,  0.85, 1.2)),
    ("SM_TRE_", (1.6,  0.35, 0.7)),
]


def _wind_params_for(mesh_name):
    for prefix, params in CATEGORY_WIND:
        if mesh_name.startswith(prefix):
            return params
    return None


def _material_has_wpo(mat):
    """True when the material already drives World Position Offset."""
    try:
        conn = MEL.get_material_property_input_node(
            mat, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        return conn is not None
    except Exception:
        return False


def _inject_wind(mat, intensity, weight, speed):
    fn = unreal.load_asset(GRASS_WIND_FN)
    if not fn:
        print("[wind]   SimpleGrassWind function not found — engine content missing?")
        return False

    call = MEL.create_material_expression(
        mat, unreal.MaterialExpressionMaterialFunctionCall, -900, 400)
    call.set_editor_property("material_function", fn)

    def _const(value, x, y):
        c = MEL.create_material_expression(
            mat, unreal.MaterialExpressionConstant, x, y)
        c.set_editor_property("r", value)
        return c

    # SimpleGrassWind inputs: WindIntensity, WindWeight, WindSpeed,
    # AdditionalWPO (unused).
    MEL.connect_material_expressions(_const(intensity, -1150, 320), "", call, "WindIntensity")
    MEL.connect_material_expressions(_const(weight,    -1150, 400), "", call, "WindWeight")
    MEL.connect_material_expressions(_const(speed,     -1150, 480), "", call, "WindSpeed")

    ok = MEL.connect_material_property(
        call, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    if not ok:
        print("[wind]   WPO connect failed on {}".format(mat.get_name()))
        return False

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return True


def run():
    print("\n=== qr_add_wind_sway ===")
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path("/Game/Meshes", recursive=True)

    done_mats = set()
    injected = skipped = 0
    for a in assets:
        if a.asset_class_path.asset_name != "StaticMesh":
            continue
        mesh_name = str(a.asset_name)
        params = _wind_params_for(mesh_name)
        if not params:
            continue
        mesh = unreal.load_asset(str(a.package_name))
        if not mesh:
            continue
        for i in range(mesh.get_num_sections(0)):
            mat = mesh.get_material(i)
            # Walk instances up to their parent Material (we edit the
            # graph, which only exists on the base material).
            while mat and isinstance(mat, unreal.MaterialInstance):
                mat = mat.get_editor_property("parent")
            if not mat or not isinstance(mat, unreal.Material):
                continue
            path = mat.get_path_name()
            if not path.startswith("/Game/"):
                continue  # engine/pack material — not ours to edit
            if path in done_mats:
                continue
            done_mats.add(path)
            if _material_has_wpo(mat):
                skipped += 1
                continue
            wi, ww, ws = params
            if _inject_wind(mat, wi, ww, ws):
                injected += 1
                print("[wind]   + {} (I={} W={} S={})".format(mat.get_name(), wi, ww, ws))

    print("[wind] injected {}, already-swaying {}. Foliage now moves.".format(injected, skipped))


if __name__ == "__main__":
    run()
