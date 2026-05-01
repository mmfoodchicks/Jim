"""
Quiet Rift: Enigma — Food Procedural Asset Generator (Blender 4.x)

Run this in Blender's Scripting workspace or via:
    blender --background --python qr_generate_food_assets_assets.py

Reads every row in DT_FoodNutritionStats.csv and exports one placeholder
mesh per food id using the helpers in qr_blender_common.py. Each mesh
is a recognizable silhouette (raw meat slab, grain sack, tuber crate,
mushroom cap, ration pack, etc.) sized for UE5 import (1 UU = 1 cm).

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
    add_material,
    join_and_rename,
)

# ── Configuration ──────────────────────────────────────────────────────────────
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/food_assets")
CSV_PATH = os.path.join(
    os.path.dirname(__file__),
    "../../Content/QuietRift/Data/DT_FoodNutritionStats.csv",
)


# ── Generator Functions (one per FoodId) ──────────────────────────────────────

def gen_raw_meat():
    """Wet red slab of raw meat."""
    clear_scene()
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0, 0, 0.025))
    slab = bpy.context.active_object
    slab.scale = (1.4, 0.9, 0.30)
    bpy.ops.object.transform_apply(scale=True)
    add_material(slab, (0.55, 0.10, 0.12, 1.0), "RawMeat_Slab_Mat")
    # Fat marbling streaks
    for i in range(3):
        bpy.ops.mesh.primitive_cube_add(size=1, location=((i - 1) * 0.05, 0, 0.045))
        streak = bpy.context.active_object
        streak.scale = (0.04, 0.07, 0.005)
        bpy.ops.object.transform_apply(scale=True)
        add_material(streak, (0.85, 0.78, 0.65, 1.0), "RawMeat_Fat_Mat")
    join_and_rename("SM_FOD_RAW_MEAT")


def gen_grain_sack():
    """Burlap sack of grain with tied neck."""
    clear_scene()
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, location=(0, 0, 0.18))
    sack = bpy.context.active_object
    sack.scale = (1.0, 1.0, 1.4)
    bpy.ops.object.transform_apply(scale=True)
    add_material(sack, (0.72, 0.62, 0.40, 1.0), "GrainSack_Body_Mat")
    # Tied neck
    bpy.ops.mesh.primitive_cylinder_add(radius=0.06, depth=0.06, location=(0, 0, 0.40))
    add_material(bpy.context.active_object, (0.45, 0.35, 0.20, 1.0), "GrainSack_Tie_Mat")
    # Spilled grain pile at base
    for i in range(8):
        angle = (i / 8.0) * math.tau
        x = math.cos(angle) * 0.20
        y = math.sin(angle) * 0.20
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.012, subdivisions=1, location=(x, y, 0.012))
        add_material(bpy.context.active_object, (0.85, 0.72, 0.45, 1.0), "GrainSack_Grain_Mat")
    join_and_rename("SM_FOD_GRAIN_SACK")


def gen_tuber_crate():
    """Wooden crate filled with tubers."""
    clear_scene()
    # Crate walls
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.15))
    crate = bpy.context.active_object
    crate.scale = (0.40, 0.30, 0.30)
    bpy.ops.object.transform_apply(scale=True)
    add_material(crate, (0.55, 0.40, 0.25, 1.0), "TuberCrate_Wood_Mat")
    # Hollow cavity (slightly smaller darker box on top)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.30))
    cavity = bpy.context.active_object
    cavity.scale = (0.36, 0.26, 0.04)
    bpy.ops.object.transform_apply(scale=True)
    add_material(cavity, (0.20, 0.15, 0.10, 1.0), "TuberCrate_Cavity_Mat")
    # Tubers spilling out top
    for i in range(7):
        angle = (i / 7.0) * math.tau
        x = math.cos(angle) * 0.10
        y = math.sin(angle) * 0.07
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.06, location=(x, y, 0.32))
        tub = bpy.context.active_object
        tub.scale = (1.2, 0.9, 0.7)
        bpy.ops.object.transform_apply(scale=True)
        add_material(tub, (0.78, 0.62, 0.42, 1.0), "TuberCrate_Tuber_Mat")
    join_and_rename("SM_FOD_TUBER_CRATE")


def gen_aroma_pod():
    """Exotic seed pod with vivid stripes."""
    clear_scene()
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.06, location=(0, 0, 0.06))
    pod = bpy.context.active_object
    pod.scale = (1.0, 1.0, 1.8)
    bpy.ops.object.transform_apply(scale=True)
    add_material(pod, (0.85, 0.30, 0.55, 1.0), "AromaPod_Body_Mat")
    # Vertical stripes
    for i in range(4):
        angle = (i / 4.0) * math.tau
        x = math.cos(angle) * 0.06
        y = math.sin(angle) * 0.06
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.06))
        stripe = bpy.context.active_object
        stripe.scale = (0.005, 0.005, 0.10)
        stripe.rotation_euler.z = angle
        bpy.ops.object.transform_apply(scale=True)
        add_material(stripe, (0.30, 0.12, 0.20, 1.0), "AromaPod_Stripe_Mat")
    # Tiny stem
    bpy.ops.mesh.primitive_cylinder_add(radius=0.006, depth=0.02, location=(0, 0, 0.13))
    add_material(bpy.context.active_object, (0.20, 0.40, 0.20, 1.0), "AromaPod_Stem_Mat")
    join_and_rename("SM_FOD_AROMA_POD")


def gen_cooked_meat():
    """Browned cooked meat slab with seared crosshatch."""
    clear_scene()
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0, 0, 0.025))
    slab = bpy.context.active_object
    slab.scale = (1.4, 0.9, 0.32)
    bpy.ops.object.transform_apply(scale=True)
    add_material(slab, (0.42, 0.20, 0.10, 1.0), "CookedMeat_Slab_Mat")
    # Sear marks (darker stripes on top)
    for i in range(3):
        bpy.ops.mesh.primitive_cube_add(size=1, location=((i - 1) * 0.04, 0, 0.052))
        mark = bpy.context.active_object
        mark.scale = (0.012, 0.07, 0.003)
        bpy.ops.object.transform_apply(scale=True)
        add_material(mark, (0.15, 0.08, 0.05, 1.0), "CookedMeat_Sear_Mat")
    join_and_rename("SM_FOD_COOKED_MEAT")


def gen_jerky_strip():
    """Curled dry strip of jerky."""
    clear_scene()
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.012))
    strip = bpy.context.active_object
    strip.scale = (0.12, 0.025, 0.012)
    bpy.ops.object.transform_apply(scale=True)
    add_material(strip, (0.30, 0.15, 0.08, 1.0), "Jerky_Body_Mat")
    # Curl bend in the middle
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.025))
    curl = bpy.context.active_object
    curl.scale = (0.04, 0.025, 0.005)
    bpy.ops.object.transform_apply(scale=True)
    add_material(curl, (0.25, 0.12, 0.06, 1.0), "Jerky_Curl_Mat")
    join_and_rename("SM_FOD_JERKY_STRIP")


def gen_hot_porridge():
    """Wood bowl with steaming oat-colored porridge."""
    clear_scene()
    # Bowl outer
    bpy.ops.mesh.primitive_cylinder_add(radius=0.10, depth=0.06, location=(0, 0, 0.03))
    bowl = bpy.context.active_object
    add_material(bowl, (0.42, 0.30, 0.20, 1.0), "Porridge_Bowl_Mat")
    # Porridge surface
    bpy.ops.mesh.primitive_cylinder_add(radius=0.085, depth=0.02, location=(0, 0, 0.06))
    surface = bpy.context.active_object
    add_material(surface, (0.85, 0.78, 0.55, 1.0), "Porridge_Surface_Mat")
    # Steam wisps
    for i in range(3):
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.025, location=((i - 1) * 0.03, 0, 0.13))
        wisp = bpy.context.active_object
        wisp.scale = (1.0, 1.0, 1.5)
        bpy.ops.object.transform_apply(scale=True)
        add_material(wisp, (0.92, 0.92, 0.95, 0.4), "Porridge_Steam_Mat")
    join_and_rename("SM_FOD_HOT_PORRIDGE")


def gen_tuber_stew():
    """Wood bowl with chunky stew and tuber pieces sticking out."""
    clear_scene()
    # Bowl
    bpy.ops.mesh.primitive_cylinder_add(radius=0.11, depth=0.07, location=(0, 0, 0.035))
    add_material(bpy.context.active_object, (0.42, 0.30, 0.20, 1.0), "Stew_Bowl_Mat")
    # Stew broth
    bpy.ops.mesh.primitive_cylinder_add(radius=0.095, depth=0.02, location=(0, 0, 0.07))
    add_material(bpy.context.active_object, (0.55, 0.30, 0.15, 1.0), "Stew_Broth_Mat")
    # Tuber chunks
    for i in range(5):
        angle = (i / 5.0) * math.tau
        x = math.cos(angle) * 0.05
        y = math.sin(angle) * 0.05
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.025, location=(x, y, 0.085))
        chunk = bpy.context.active_object
        chunk.scale = (1.2, 0.9, 0.7)
        bpy.ops.object.transform_apply(scale=True)
        add_material(chunk, (0.75, 0.60, 0.40, 1.0), "Stew_Chunk_Mat")
    join_and_rename("SM_FOD_TUBER_STEW")


def gen_lattice_tuber():
    """Single tuber with lattice surface ridges."""
    clear_scene()
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.08, location=(0, 0, 0.06))
    body = bpy.context.active_object
    body.scale = (1.4, 1.0, 0.8)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, (0.78, 0.62, 0.42, 1.0), "LatticeTuber_Body_Mat")
    # Lattice ridges
    for i in range(6):
        angle = (i / 6.0) * math.tau
        x = math.cos(angle) * 0.07
        y = math.sin(angle) * 0.05
        bpy.ops.mesh.primitive_torus_add(major_radius=0.015, minor_radius=0.004, location=(x, y, 0.07))
        ridge = bpy.context.active_object
        ridge.rotation_euler = (math.pi / 4, 0, angle)
        add_material(ridge, (0.55, 0.42, 0.28, 1.0), "LatticeTuber_Ridge_Mat")
    # Stub root tip
    bpy.ops.mesh.primitive_cone_add(radius1=0.012, radius2=0.0, depth=0.04, location=(0.12, 0, 0.06))
    tip = bpy.context.active_object
    tip.rotation_euler.y = math.pi / 2
    add_material(tip, (0.45, 0.35, 0.22, 1.0), "LatticeTuber_Tip_Mat")
    join_and_rename("SM_FOD_LATTICE_TUBER")


def gen_mawcap_cap():
    """Mushroom cap (just the harvested top, no stalk)."""
    clear_scene()
    # Underside (gills)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.09, depth=0.02, location=(0, 0, 0.01))
    gills = bpy.context.active_object
    add_material(gills, (0.55, 0.40, 0.45, 1.0), "MawcapCap_Gills_Mat")
    # Cap dome
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0, 0, 0.02))
    cap = bpy.context.active_object
    cap.scale.z = 0.55
    bpy.ops.object.transform_apply(scale=True)
    add_material(cap, (0.45, 0.55, 0.30, 1.0), "MawcapCap_Top_Mat")
    # Aperture rim
    bpy.ops.mesh.primitive_torus_add(major_radius=0.06, minor_radius=0.008, location=(0, 0, 0.045))
    add_material(bpy.context.active_object, (0.62, 0.42, 0.50, 1.0), "MawcapCap_Rim_Mat")
    join_and_rename("SM_FOD_MAWCAP_CAP")


def gen_nullmint_node():
    """Small herb sprig — pale leaves clustered around a node."""
    clear_scene()
    # Central node
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.025, location=(0, 0, 0.04))
    add_material(bpy.context.active_object, (0.85, 0.92, 0.78, 1.0), "Nullmint_Node_Mat")
    # Leaves (flat ovals around node)
    for i in range(6):
        angle = (i / 6.0) * math.tau
        x = math.cos(angle) * 0.04
        y = math.sin(angle) * 0.04
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.022, subdivisions=2, location=(x, y, 0.045))
        leaf = bpy.context.active_object
        leaf.scale = (1.4, 0.4, 0.15)
        leaf.rotation_euler.z = angle
        bpy.ops.object.transform_apply(scale=True)
        add_material(leaf, (0.55, 0.78, 0.55, 1.0), "Nullmint_Leaf_Mat")
    # Stem
    bpy.ops.mesh.primitive_cylinder_add(radius=0.006, depth=0.04, location=(0, 0, 0.02))
    add_material(bpy.context.active_object, (0.45, 0.62, 0.40, 1.0), "Nullmint_Stem_Mat")
    join_and_rename("SM_FOD_NULLMINT_NODE")


def gen_reed_porridge():
    """Wood bowl with green-tinted reed porridge."""
    clear_scene()
    bpy.ops.mesh.primitive_cylinder_add(radius=0.10, depth=0.06, location=(0, 0, 0.03))
    add_material(bpy.context.active_object, (0.42, 0.30, 0.20, 1.0), "ReedPorridge_Bowl_Mat")
    # Porridge surface (green-ish)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.085, depth=0.02, location=(0, 0, 0.06))
    add_material(bpy.context.active_object, (0.65, 0.75, 0.45, 1.0), "ReedPorridge_Surface_Mat")
    # Reed bits floating
    for i in range(4):
        angle = (i / 4.0) * math.tau
        x = math.cos(angle) * 0.04
        y = math.sin(angle) * 0.04
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.075))
        bit = bpy.context.active_object
        bit.scale = (0.018, 0.004, 0.003)
        bit.rotation_euler.z = angle
        bpy.ops.object.transform_apply(scale=True)
        add_material(bit, (0.40, 0.55, 0.30, 1.0), "ReedPorridge_Reed_Mat")
    join_and_rename("SM_FOD_REED_PORRIDGE")


def gen_courser_meat():
    """Lean cut from a Courser — long elegant red strip."""
    clear_scene()
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0, 0, 0.03))
    cut = bpy.context.active_object
    cut.scale = (2.0, 0.7, 0.30)
    bpy.ops.object.transform_apply(scale=True)
    add_material(cut, (0.50, 0.12, 0.10, 1.0), "CourserMeat_Cut_Mat")
    # Sinew streak
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.055))
    sinew = bpy.context.active_object
    sinew.scale = (0.18, 0.04, 0.003)
    bpy.ops.object.transform_apply(scale=True)
    add_material(sinew, (0.85, 0.78, 0.65, 1.0), "CourserMeat_Sinew_Mat")
    join_and_rename("SM_FOD_COURSER_MEAT")


def gen_dray_meat():
    """Heavy slab from a Dray — thick dark cut."""
    clear_scene()
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.13, location=(0, 0, 0.05))
    slab = bpy.context.active_object
    slab.scale = (1.5, 1.1, 0.40)
    bpy.ops.object.transform_apply(scale=True)
    add_material(slab, (0.42, 0.10, 0.08, 1.0), "DrayMeat_Slab_Mat")
    # Bone fragment
    bpy.ops.mesh.primitive_cylinder_add(radius=0.018, depth=0.10, location=(0, 0.04, 0.075))
    bone = bpy.context.active_object
    bone.rotation_euler.y = math.pi / 2
    add_material(bone, (0.92, 0.88, 0.78, 1.0), "DrayMeat_Bone_Mat")
    # Fat marbling
    for i in range(4):
        bpy.ops.mesh.primitive_cube_add(size=1, location=((i - 1.5) * 0.04, -0.02, 0.085))
        marb = bpy.context.active_object
        marb.scale = (0.03, 0.05, 0.004)
        bpy.ops.object.transform_apply(scale=True)
        add_material(marb, (0.85, 0.78, 0.65, 1.0), "DrayMeat_Fat_Mat")
    join_and_rename("SM_FOD_DRAY_MEAT")


def gen_field_ration():
    """Sealed olive-drab ration pack with stamped panel."""
    clear_scene()
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.025))
    pack = bpy.context.active_object
    pack.scale = (0.13, 0.09, 0.04)
    bpy.ops.object.transform_apply(scale=True)
    add_material(pack, (0.35, 0.40, 0.25, 1.0), "FieldRation_Pack_Mat")
    # Stamped panel
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.05))
    panel = bpy.context.active_object
    panel.scale = (0.08, 0.04, 0.002)
    bpy.ops.object.transform_apply(scale=True)
    add_material(panel, (0.85, 0.80, 0.40, 1.0), "FieldRation_Panel_Mat")
    # Tear notch
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.075, 0, 0.025))
    notch = bpy.context.active_object
    notch.scale = (0.005, 0.04, 0.022)
    bpy.ops.object.transform_apply(scale=True)
    add_material(notch, (0.20, 0.22, 0.15, 1.0), "FieldRation_Notch_Mat")
    join_and_rename("SM_FOD_FIELD_RATION")


# ── Dispatch table — every FoodId in the CSV must map to a generator. ────────

GENERATORS = {
    "FOD_RAW_MEAT":        gen_raw_meat,
    "FOD_GRAIN_SACK":      gen_grain_sack,
    "FOD_TUBER_CRATE":     gen_tuber_crate,
    "FOD_AROMA_POD":       gen_aroma_pod,
    "FOD_COOKED_MEAT":     gen_cooked_meat,
    "FOD_JERKY_STRIP":     gen_jerky_strip,
    "FOD_HOT_PORRIDGE":    gen_hot_porridge,
    "FOD_TUBER_STEW":      gen_tuber_stew,
    "FOD_LATTICE_TUBER":   gen_lattice_tuber,
    "FOD_MAWCAP_CAP":      gen_mawcap_cap,
    "FOD_NULLMINT_NODE":   gen_nullmint_node,
    "FOD_REED_PORRIDGE":   gen_reed_porridge,
    "FOD_COURSER_MEAT":    gen_courser_meat,
    "FOD_DRAY_MEAT":       gen_dray_meat,
    "FOD_FIELD_RATION":    gen_field_ration,
}


# ── Main export pipeline ───────────────────────────────────────────────────────

def main():
    print("\n=== Quiet Rift: Enigma — Food Asset Generator ===")
    csv_abs = os.path.abspath(CSV_PATH)
    if not os.path.isfile(csv_abs):
        print(f"ERROR: CSV not found at {csv_abs}")
        return

    with open(csv_abs, newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        rows = [r for r in reader if r.get("FoodId")]

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
    print("Import these FBX files into UE5 via Content Browser > Import.")


if __name__ == "__main__":
    main()
