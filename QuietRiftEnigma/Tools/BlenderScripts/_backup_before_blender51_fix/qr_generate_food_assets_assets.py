"""
Quiet Rift: Enigma â€” Food Procedural Asset Generator (Blender 4.x)

Upgraded in Batch 4 of the Blender detail pass â€” every food item flows
through qr_blender_detail.py for production finalization (deduped per-
food materials, smooth shading, bevels, smart UV, sockets, UCX
collision).

Reads every row in DT_FoodNutritionStats.csv (15 items: raw / cooked /
jerky meat, grain sack, tuber crate, aroma pod, bowls of porridge and
stew, lattice tuber, mawcap cap, nullmint herb, courser/dray meat cuts,
field ration) and exports one bespoke placeholder FBX per row.

Per-item sockets:
    SOCKET_PickupPoint  â€” where the player grabs the item
    SOCKET_EatPoint     â€” where the eat-anim mouth target lands

Bowls also get SOCKET_GraspRim at the rim for cooking-station handoffs.

Usage inside Blender:
    1. Open Blender > Scripting tab
    2. Open this file
    3. Set OUTPUT_DIR
    4. Press Run Script
"""

import bpy
import csv
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from qr_blender_common import (  # noqa: E402
    SCALE,
    clear_scene,
    export_fbx,
)
from qr_blender_detail import (  # noqa: E402
    palette_material,
    get_or_create_material,
    assign_material,
    add_socket,
    finalize_asset,
)

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/food_assets")
CSV_PATH = os.path.join(
    os.path.dirname(__file__),
    "../../Content/QuietRift/Data/DT_FoodNutritionStats.csv",
)


def _mat(slot, color, roughness=0.85, emissive=None):
    return get_or_create_material(slot, color, roughness=roughness,
                                   metallic=0.0, emissive=emissive)


def _add(obj, mat):
    assign_material(obj, mat)
    return obj


def _finalize_food(name, pickup, eat=None, grasp_rim=None, lods=None):
    add_socket("PickupPoint", location=pickup)
    if eat is not None:
        add_socket("EatPoint", location=eat)
    if grasp_rim is not None:
        add_socket("GraspRim", location=grasp_rim)
    finalize_asset(name,
                    bevel_width=0.0015, bevel_angle_deg=30,
                    smooth_angle_deg=45, collision="convex",
                    lods=list(lods) if lods else None,
                    pivot="bottom_center")


# â”€â”€ Generators â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def gen_raw_meat():
    """Wet red slab of raw meat."""
    clear_scene()
    slab_mat = _mat("Food_RawMeat_Slab", (0.55, 0.10, 0.12, 1.0), roughness=0.55)
    fat_mat = _mat("Food_Fat", (0.85, 0.78, 0.65, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0, 0, 0.025))
    slab = bpy.context.active_object
    slab.scale = (1.4, 0.9, 0.30); bpy.ops.object.transform_apply(scale=True)
    _add(slab, slab_mat)
    for i in range(3):
        bpy.ops.mesh.primitive_cube_add(size=1, location=((i - 1) * 0.05, 0, 0.045))
        streak = bpy.context.active_object
        streak.scale = (0.04, 0.07, 0.005); bpy.ops.object.transform_apply(scale=True)
        _add(streak, fat_mat)
    _finalize_food("SM_FOD_RAW_MEAT", pickup=(0, 0, 0.06), eat=(0, 0, 0.05))


def gen_grain_sack():
    """Burlap sack of grain with tied neck and spilled grain pile."""
    clear_scene()
    body_mat = _mat("Food_GrainSack_Burlap", (0.72, 0.62, 0.40, 1.0))
    tie_mat = _mat("Food_GrainSack_Tie", (0.45, 0.35, 0.20, 1.0))
    grain_mat = _mat("Food_Grain", (0.85, 0.72, 0.45, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, location=(0, 0, 0.18))
    sack = bpy.context.active_object
    sack.scale = (1.0, 1.0, 1.4); bpy.ops.object.transform_apply(scale=True)
    _add(sack, body_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.06, depth=0.06, location=(0, 0, 0.40))
    _add(bpy.context.active_object, tie_mat)
    for i in range(8):
        ang = (i / 8.0) * math.tau
        x = math.cos(ang) * 0.20; y = math.sin(ang) * 0.20
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.012, subdivisions=1, location=(x, y, 0.012))
        _add(bpy.context.active_object, grain_mat)
    _finalize_food("SM_FOD_GRAIN_SACK", pickup=(0, 0, 0.42), eat=(0, 0, 0.30),
                    lods=(0.50,))


def gen_tuber_crate():
    """Wooden crate filled with tubers."""
    clear_scene()
    wood_mat = palette_material("Wood")
    cavity_mat = _mat("Food_TuberCrate_Cavity", (0.20, 0.15, 0.10, 1.0))
    tuber_mat = _mat("Food_Tuber", (0.78, 0.62, 0.42, 1.0))
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.15))
    crate = bpy.context.active_object
    crate.scale = (0.40, 0.30, 0.30); bpy.ops.object.transform_apply(scale=True)
    _add(crate, wood_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.30))
    cavity = bpy.context.active_object
    cavity.scale = (0.36, 0.26, 0.04); bpy.ops.object.transform_apply(scale=True)
    _add(cavity, cavity_mat)
    for i in range(7):
        ang = (i / 7.0) * math.tau
        x = math.cos(ang) * 0.10; y = math.sin(ang) * 0.07
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.06, location=(x, y, 0.32))
        tub = bpy.context.active_object
        tub.scale = (1.2, 0.9, 0.7); bpy.ops.object.transform_apply(scale=True)
        _add(tub, tuber_mat)
    _finalize_food("SM_FOD_TUBER_CRATE", pickup=(0, 0, 0.36), eat=(0, 0, 0.32),
                    lods=(0.50,))


def gen_aroma_pod():
    """Exotic seed pod with vivid stripes."""
    clear_scene()
    body_mat = _mat("Food_AromaPod_Body", (0.85, 0.30, 0.55, 1.0))
    stripe_mat = _mat("Food_AromaPod_Stripe", (0.30, 0.12, 0.20, 1.0))
    stem_mat = _mat("Food_AromaPod_Stem", (0.20, 0.40, 0.20, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.06, location=(0, 0, 0.06))
    pod = bpy.context.active_object
    pod.scale = (1.0, 1.0, 1.8); bpy.ops.object.transform_apply(scale=True)
    _add(pod, body_mat)
    for i in range(4):
        ang = (i / 4.0) * math.tau
        x = math.cos(ang) * 0.06; y = math.sin(ang) * 0.06
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.06))
        stripe = bpy.context.active_object
        stripe.scale = (0.005, 0.005, 0.10); stripe.rotation_euler.z = ang
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(stripe, stripe_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.006, depth=0.02, location=(0, 0, 0.13))
    _add(bpy.context.active_object, stem_mat)
    _finalize_food("SM_FOD_AROMA_POD", pickup=(0, 0, 0.13), eat=(0, 0, 0.10))


def gen_cooked_meat():
    """Browned cooked meat slab with seared crosshatch."""
    clear_scene()
    slab_mat = _mat("Food_CookedMeat_Slab", (0.42, 0.20, 0.10, 1.0), roughness=0.65)
    sear_mat = _mat("Food_CookedMeat_Sear", (0.15, 0.08, 0.05, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0, 0, 0.025))
    slab = bpy.context.active_object
    slab.scale = (1.4, 0.9, 0.32); bpy.ops.object.transform_apply(scale=True)
    _add(slab, slab_mat)
    for i in range(3):
        bpy.ops.mesh.primitive_cube_add(size=1, location=((i - 1) * 0.04, 0, 0.052))
        mark = bpy.context.active_object
        mark.scale = (0.012, 0.07, 0.003); bpy.ops.object.transform_apply(scale=True)
        _add(mark, sear_mat)
    _finalize_food("SM_FOD_COOKED_MEAT", pickup=(0, 0, 0.06), eat=(0, 0, 0.05))


def gen_jerky_strip():
    """Curled dry strip of jerky."""
    clear_scene()
    body_mat = _mat("Food_Jerky_Body", (0.30, 0.15, 0.08, 1.0))
    curl_mat = _mat("Food_Jerky_Curl", (0.25, 0.12, 0.06, 1.0))
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.012))
    strip = bpy.context.active_object
    strip.scale = (0.12, 0.025, 0.012); bpy.ops.object.transform_apply(scale=True)
    _add(strip, body_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.025))
    curl = bpy.context.active_object
    curl.scale = (0.04, 0.025, 0.005); bpy.ops.object.transform_apply(scale=True)
    _add(curl, curl_mat)
    _finalize_food("SM_FOD_JERKY_STRIP", pickup=(0, 0, 0.03), eat=(0, 0, 0.02))


def _bowl(rim_y_radius=0.10, depth=0.06, surface_color=(0.85, 0.78, 0.55, 1.0),
           surface_name="Food_Porridge_Surface"):
    """Shared bowl chassis: wooden bowl + colored surface fill."""
    bowl_mat = palette_material("Wood")
    surface_mat = _mat(surface_name, surface_color, roughness=0.55)
    bpy.ops.mesh.primitive_cylinder_add(radius=rim_y_radius, depth=depth, location=(0, 0, depth / 2))
    _add(bpy.context.active_object, bowl_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=rim_y_radius * 0.85, depth=0.02, location=(0, 0, depth + 0.005))
    _add(bpy.context.active_object, surface_mat)
    return rim_y_radius, depth


def gen_hot_porridge():
    """Wood bowl with steaming oat-colored porridge + steam wisps."""
    clear_scene()
    rim, depth = _bowl(rim_y_radius=0.10, depth=0.06,
                        surface_color=(0.85, 0.78, 0.55, 1.0),
                        surface_name="Food_Porridge_Surface")
    steam_mat = _mat("Food_Steam", (0.92, 0.92, 0.95, 0.4), roughness=0.20,
                      emissive=(0.8, 0.8, 0.85, 0.6))
    for i in range(3):
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.025, location=((i - 1) * 0.03, 0, 0.13))
        wisp = bpy.context.active_object
        wisp.scale = (1.0, 1.0, 1.5); bpy.ops.object.transform_apply(scale=True)
        _add(wisp, steam_mat)
    _finalize_food("SM_FOD_HOT_PORRIDGE", pickup=(0, 0, 0.09), eat=(0, 0, 0.08),
                    grasp_rim=(rim, 0, depth))


def gen_tuber_stew():
    """Wood bowl with chunky stew and tuber pieces."""
    clear_scene()
    rim, depth = _bowl(rim_y_radius=0.11, depth=0.07,
                        surface_color=(0.55, 0.30, 0.15, 1.0),
                        surface_name="Food_Stew_Broth")
    chunk_mat = _mat("Food_Stew_Chunk", (0.75, 0.60, 0.40, 1.0))
    for i in range(5):
        ang = (i / 5.0) * math.tau
        x = math.cos(ang) * 0.05; y = math.sin(ang) * 0.05
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.025, location=(x, y, 0.085))
        chunk = bpy.context.active_object
        chunk.scale = (1.2, 0.9, 0.7); bpy.ops.object.transform_apply(scale=True)
        _add(chunk, chunk_mat)
    _finalize_food("SM_FOD_TUBER_STEW", pickup=(0, 0, 0.10), eat=(0, 0, 0.09),
                    grasp_rim=(rim, 0, depth))


def gen_lattice_tuber():
    """Single tuber with lattice surface ridges."""
    clear_scene()
    body_mat = _mat("Food_LatticeTuber_Body", (0.78, 0.62, 0.42, 1.0))
    ridge_mat = _mat("Food_LatticeTuber_Ridge", (0.55, 0.42, 0.28, 1.0))
    tip_mat = _mat("Food_LatticeTuber_Tip", (0.45, 0.35, 0.22, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.08, location=(0, 0, 0.06))
    body = bpy.context.active_object
    body.scale = (1.4, 1.0, 0.8); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    for i in range(6):
        ang = (i / 6.0) * math.tau
        x = math.cos(ang) * 0.07; y = math.sin(ang) * 0.05
        bpy.ops.mesh.primitive_torus_add(major_radius=0.015, minor_radius=0.004, location=(x, y, 0.07))
        ridge = bpy.context.active_object
        ridge.rotation_euler = (math.pi / 4, 0, ang)
        _add(ridge, ridge_mat)
    bpy.ops.mesh.primitive_cone_add(radius1=0.012, radius2=0.0, depth=0.04, location=(0.12, 0, 0.06))
    tip = bpy.context.active_object
    tip.rotation_euler.y = math.pi / 2
    _add(tip, tip_mat)
    _finalize_food("SM_FOD_LATTICE_TUBER", pickup=(0, 0, 0.10), eat=(0, 0, 0.06))


def gen_mawcap_cap():
    """Mushroom cap (just the harvested top, no stalk)."""
    clear_scene()
    gill_mat = _mat("Food_Mawcap_Gills", (0.55, 0.40, 0.45, 1.0))
    top_mat = _mat("Food_Mawcap_Top", (0.45, 0.55, 0.30, 1.0))
    rim_mat = _mat("Food_Mawcap_Rim", (0.62, 0.42, 0.50, 1.0))
    bpy.ops.mesh.primitive_cylinder_add(radius=0.09, depth=0.02, location=(0, 0, 0.01))
    _add(bpy.context.active_object, gill_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0, 0, 0.02))
    cap = bpy.context.active_object
    cap.scale.z = 0.55; bpy.ops.object.transform_apply(scale=True)
    _add(cap, top_mat)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.06, minor_radius=0.008, location=(0, 0, 0.045))
    _add(bpy.context.active_object, rim_mat)
    _finalize_food("SM_FOD_MAWCAP_CAP", pickup=(0, 0, 0.08), eat=(0, 0, 0.04))


def gen_nullmint_node():
    """Small herb sprig â€” pale leaves clustered around a node."""
    clear_scene()
    node_mat = _mat("Food_Nullmint_Node", (0.85, 0.92, 0.78, 1.0))
    leaf_mat = _mat("Food_Nullmint_Leaf", (0.55, 0.78, 0.55, 1.0))
    stem_mat = _mat("Food_Nullmint_Stem", (0.45, 0.62, 0.40, 1.0))
    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.025, location=(0, 0, 0.04))
    _add(bpy.context.active_object, node_mat)
    for i in range(6):
        ang = (i / 6.0) * math.tau
        x = math.cos(ang) * 0.04; y = math.sin(ang) * 0.04
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.022, subdivisions=2, location=(x, y, 0.045))
        leaf = bpy.context.active_object
        leaf.scale = (1.4, 0.4, 0.15); leaf.rotation_euler.z = ang
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(leaf, leaf_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.006, depth=0.04, location=(0, 0, 0.02))
    _add(bpy.context.active_object, stem_mat)
    _finalize_food("SM_FOD_NULLMINT_NODE", pickup=(0, 0, 0.07), eat=(0, 0, 0.04))


def gen_reed_porridge():
    """Wood bowl with green-tinted reed porridge + floating reed bits."""
    clear_scene()
    rim, depth = _bowl(rim_y_radius=0.10, depth=0.06,
                        surface_color=(0.65, 0.75, 0.45, 1.0),
                        surface_name="Food_ReedPorridge_Surface")
    reed_mat = _mat("Food_ReedPorridge_Reed", (0.40, 0.55, 0.30, 1.0))
    for i in range(4):
        ang = (i / 4.0) * math.tau
        x = math.cos(ang) * 0.04; y = math.sin(ang) * 0.04
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.075))
        bit = bpy.context.active_object
        bit.scale = (0.018, 0.004, 0.003); bit.rotation_euler.z = ang
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(bit, reed_mat)
    _finalize_food("SM_FOD_REED_PORRIDGE", pickup=(0, 0, 0.09), eat=(0, 0, 0.08),
                    grasp_rim=(rim, 0, depth))


def gen_courser_meat():
    """Lean cut from a Courser â€” long elegant red strip."""
    clear_scene()
    cut_mat = _mat("Food_CourserMeat_Cut", (0.50, 0.12, 0.10, 1.0), roughness=0.55)
    sinew_mat = _mat("Food_Sinew", (0.85, 0.78, 0.65, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0, 0, 0.03))
    cut = bpy.context.active_object
    cut.scale = (2.0, 0.7, 0.30); bpy.ops.object.transform_apply(scale=True)
    _add(cut, cut_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.055))
    sinew = bpy.context.active_object
    sinew.scale = (0.18, 0.04, 0.003); bpy.ops.object.transform_apply(scale=True)
    _add(sinew, sinew_mat)
    _finalize_food("SM_FOD_COURSER_MEAT", pickup=(0, 0, 0.07), eat=(0, 0, 0.05))


def gen_dray_meat():
    """Heavy slab from a Dray â€” thick dark cut with bone fragment."""
    clear_scene()
    slab_mat = _mat("Food_DrayMeat_Slab", (0.42, 0.10, 0.08, 1.0), roughness=0.55)
    bone_mat = _mat("Food_Bone", (0.92, 0.88, 0.78, 1.0), roughness=0.65)
    fat_mat = _mat("Food_Fat", (0.85, 0.78, 0.65, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.13, location=(0, 0, 0.05))
    slab = bpy.context.active_object
    slab.scale = (1.5, 1.1, 0.40); bpy.ops.object.transform_apply(scale=True)
    _add(slab, slab_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.018, depth=0.10, location=(0, 0.04, 0.075))
    bone = bpy.context.active_object
    bone.rotation_euler.y = math.pi / 2
    _add(bone, bone_mat)
    for i in range(4):
        bpy.ops.mesh.primitive_cube_add(size=1, location=((i - 1.5) * 0.04, -0.02, 0.085))
        marb = bpy.context.active_object
        marb.scale = (0.03, 0.05, 0.004); bpy.ops.object.transform_apply(scale=True)
        _add(marb, fat_mat)
    _finalize_food("SM_FOD_DRAY_MEAT", pickup=(0, 0, 0.10), eat=(0, 0, 0.08))


def gen_field_ration():
    """Sealed olive-drab ration pack with stamped panel + tear notch."""
    clear_scene()
    pack_mat = _mat("Food_FieldRation_Pack", (0.35, 0.40, 0.25, 1.0))
    panel_mat = _mat("Food_FieldRation_Stamp", (0.85, 0.80, 0.40, 1.0))
    notch_mat = _mat("Food_FieldRation_Notch", (0.20, 0.22, 0.15, 1.0))
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.025))
    pack = bpy.context.active_object
    pack.scale = (0.13, 0.09, 0.04); bpy.ops.object.transform_apply(scale=True)
    _add(pack, pack_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.05))
    panel = bpy.context.active_object
    panel.scale = (0.08, 0.04, 0.002); bpy.ops.object.transform_apply(scale=True)
    _add(panel, panel_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.075, 0, 0.025))
    notch = bpy.context.active_object
    notch.scale = (0.005, 0.04, 0.022); bpy.ops.object.transform_apply(scale=True)
    _add(notch, notch_mat)
    _finalize_food("SM_FOD_FIELD_RATION", pickup=(0, 0, 0.06), eat=(0, 0, 0.04))


# â”€â”€ Dispatch table â€” every FoodId in the CSV must map to a generator. â”€â”€â”€â”€â”€â”€â”€â”€

GENERATORS = {
    "FOD_RAW_MEAT":       gen_raw_meat,
    "FOD_GRAIN_SACK":     gen_grain_sack,
    "FOD_TUBER_CRATE":    gen_tuber_crate,
    "FOD_AROMA_POD":      gen_aroma_pod,
    "FOD_COOKED_MEAT":    gen_cooked_meat,
    "FOD_JERKY_STRIP":    gen_jerky_strip,
    "FOD_HOT_PORRIDGE":   gen_hot_porridge,
    "FOD_TUBER_STEW":     gen_tuber_stew,
    "FOD_LATTICE_TUBER":  gen_lattice_tuber,
    "FOD_MAWCAP_CAP":     gen_mawcap_cap,
    "FOD_NULLMINT_NODE":  gen_nullmint_node,
    "FOD_REED_PORRIDGE":  gen_reed_porridge,
    "FOD_COURSER_MEAT":   gen_courser_meat,
    "FOD_DRAY_MEAT":      gen_dray_meat,
    "FOD_FIELD_RATION":   gen_field_ration,
}


def main():
    print("\n=== Quiet Rift: Enigma â€” Food Asset Generator (Batch 4 detail upgrade) ===")
    csv_abs = os.path.abspath(CSV_PATH)
    if not os.path.isfile(csv_abs):
        print(f"ERROR: CSV not found at {csv_abs}")
        return
    with open(csv_abs, newline='', encoding='utf-8') as f:
        rows = [r for r in csv.DictReader(f) if r.get("FoodId")]
    missing = [r["FoodId"] for r in rows if r["FoodId"] not in GENERATORS]
    if missing:
        print(f"WARN: no generator registered for: {missing}")
    for row in rows:
        fid = row["FoodId"]
        gen = GENERATORS.get(fid)
        if gen is None:
            continue
        print(f"\n[{fid}] {row.get('DisplayName', fid)}")
        gen()
        out_path = os.path.join(OUTPUT_DIR, f"SM_{fid}.fbx")
        export_fbx(fid, out_path)
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX exports SM_<id> + UCX_ collision + SOCKET_PickupPoint /")
    print("SOCKET_EatPoint, plus SOCKET_GraspRim on bowl-based items.")


if __name__ == "__main__":
    main()
