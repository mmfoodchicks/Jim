"""
Quiet Rift: Enigma — Attachments & Ammo Procedural Asset Generator (Blender 4.x)

Run this in Blender's Scripting workspace or via:
    blender --background --python qr_generate_attachments_ammo_assets.py

Reads every row in DT_ArmoryAttachments.csv (9 rows) and
DT_ArmoryAmmo.csv (7 rows) and exports one placeholder mesh per row
using the helpers in qr_blender_common.py. Sized for UE5 import
(1 UU = 1 cm).

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
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/attachments_ammo")
ATT_CSV = os.path.join(
    os.path.dirname(__file__),
    "../../Content/QuietRift/Data/DT_ArmoryAttachments.csv",
)
AMO_CSV = os.path.join(
    os.path.dirname(__file__),
    "../../Content/QuietRift/Data/DT_ArmoryAmmo.csv",
)

# Standard part colors (kept consistent with weapons script)
STEEL    = (0.30, 0.30, 0.33, 1.0)
GUNMETAL = (0.18, 0.18, 0.20, 1.0)
POLYMER  = (0.10, 0.10, 0.10, 1.0)
GLASS    = (0.20, 0.30, 0.40, 0.6)
BRASS    = (0.75, 0.55, 0.20, 1.0)
LEAD     = (0.42, 0.42, 0.45, 1.0)
COPPER   = (0.65, 0.42, 0.22, 1.0)
RUBBER   = (0.10, 0.10, 0.10, 1.0)


# ── Attachment Generators ─────────────────────────────────────────────────────

def gen_att_suppressor():
    """Long cylindrical suppressor with baffle rings + thread mount."""
    clear_scene()
    bpy.ops.mesh.primitive_cylinder_add(radius=0.022, depth=0.18, location=(0, 0, 0.09))
    body = bpy.context.active_object
    body.rotation_euler.y = math.pi / 2
    bpy.ops.object.transform_apply(rotation=True)
    add_material(body, GUNMETAL, "Suppressor_Body_Mat")
    # Baffle rings
    for i in range(5):
        x = 0.02 + i * 0.035
        bpy.ops.mesh.primitive_torus_add(major_radius=0.024, minor_radius=0.0025, location=(x, 0, 0.09))
        ring = bpy.context.active_object
        ring.rotation_euler.y = math.pi / 2
        add_material(ring, STEEL, "Suppressor_Ring_Mat")
    # Thread mount (rear)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.014, depth=0.025, location=(-0.0125, 0, 0.09))
    mount = bpy.context.active_object
    mount.rotation_euler.y = math.pi / 2
    add_material(mount, STEEL, "Suppressor_Mount_Mat")
    join_and_rename("SM_ATT_SUPPRESSOR")


def gen_att_compensator():
    """Short flared muzzle device with vent slots."""
    clear_scene()
    bpy.ops.mesh.primitive_cylinder_add(radius=0.020, depth=0.05, location=(0, 0, 0.025))
    body = bpy.context.active_object
    body.rotation_euler.y = math.pi / 2
    bpy.ops.object.transform_apply(rotation=True)
    add_material(body, GUNMETAL, "Compensator_Body_Mat")
    # Vent slots (top + sides)
    for i, (x, y) in enumerate([(0, 0.022), (0, -0.022), (0.012, 0), (-0.012, 0)]):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.025))
        slot = bpy.context.active_object
        slot.scale = (0.020, 0.005, 0.005)
        bpy.ops.object.transform_apply(scale=True)
        add_material(slot, POLYMER, "Compensator_Slot_Mat")
    join_and_rename("SM_ATT_COMPENSATOR")


def gen_att_vertical_grip():
    """Short vertical foregrip with mount plate."""
    clear_scene()
    # Mount plate
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.005))
    plate = bpy.context.active_object
    plate.scale = (0.05, 0.024, 0.010)
    bpy.ops.object.transform_apply(scale=True)
    add_material(plate, STEEL, "VertGrip_Plate_Mat")
    # Grip column
    bpy.ops.mesh.primitive_cylinder_add(radius=0.014, depth=0.075, location=(0, 0, -0.038))
    add_material(bpy.context.active_object, POLYMER, "VertGrip_Column_Mat")
    # Knurled bands
    for z in (-0.015, -0.045, -0.072):
        bpy.ops.mesh.primitive_torus_add(major_radius=0.015, minor_radius=0.002, location=(0, 0, z))
        add_material(bpy.context.active_object, GUNMETAL, "VertGrip_Band_Mat")
    join_and_rename("SM_ATT_VERTICAL_GRIP")


def gen_att_stock_pad():
    """Soft recoil pad — rubber block with grip ridges."""
    clear_scene()
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.025))
    pad = bpy.context.active_object
    pad.scale = (0.025, 0.040, 0.050)
    bpy.ops.object.transform_apply(scale=True)
    add_material(pad, RUBBER, "StockPad_Body_Mat")
    # Grip ridges
    for i in range(4):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0.014, 0, 0.005 + i * 0.013))
        ridge = bpy.context.active_object
        ridge.scale = (0.002, 0.038, 0.003)
        bpy.ops.object.transform_apply(scale=True)
        add_material(ridge, (0.05, 0.05, 0.05, 1.0), "StockPad_Ridge_Mat")
    join_and_rename("SM_ATT_STOCK_PAD")


def gen_att_ext_mag():
    """Extended magazine — taller box than standard."""
    clear_scene()
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, -0.075))
    body = bpy.context.active_object
    body.scale = (0.045, 0.024, 0.150)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, GUNMETAL, "ExtMag_Body_Mat")
    # Floor plate
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, -0.155))
    plate = bpy.context.active_object
    plate.scale = (0.050, 0.028, 0.015)
    bpy.ops.object.transform_apply(scale=True)
    add_material(plate, POLYMER, "ExtMag_Plate_Mat")
    # Witness holes (small dots on the side)
    for i in range(4):
        bpy.ops.mesh.primitive_cylinder_add(radius=0.003, depth=0.005, location=(0, 0.014, -0.030 - i * 0.030))
        hole = bpy.context.active_object
        hole.rotation_euler.x = math.pi / 2
        add_material(hole, (0.05, 0.05, 0.05, 1.0), "ExtMag_Hole_Mat")
    join_and_rename("SM_ATT_EXT_MAG")


def _scope_block(item_name, body_len, radius, lens_color=GLASS, mount_count=2):
    """Generic scope body used by 4X/8X/16X."""
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=body_len, location=(0, 0, 0))
    body = bpy.context.active_object
    body.rotation_euler.y = math.pi / 2
    add_material(body, GUNMETAL, f"{item_name}_Body_Mat")
    # Objective bell at front
    bpy.ops.mesh.primitive_cylinder_add(radius=radius * 1.4, depth=0.025, location=(body_len / 2, 0, 0))
    bell = bpy.context.active_object
    bell.rotation_euler.y = math.pi / 2
    add_material(bell, GUNMETAL, f"{item_name}_Bell_Mat")
    # Lens
    bpy.ops.mesh.primitive_cylinder_add(radius=radius * 1.2, depth=0.005, location=(body_len / 2 + 0.013, 0, 0))
    lens = bpy.context.active_object
    lens.rotation_euler.y = math.pi / 2
    add_material(lens, lens_color, f"{item_name}_Lens_Mat")
    # Turret housing on top
    bpy.ops.mesh.primitive_cylinder_add(radius=radius * 0.8, depth=radius * 1.6, location=(0, 0, radius * 1.4))
    add_material(bpy.context.active_object, GUNMETAL, f"{item_name}_Turret_Mat")
    # Mount blocks under
    spread = body_len * 0.35
    for i in range(mount_count):
        offset = (-spread / 2) + (i * (spread / max(mount_count - 1, 1))) if mount_count > 1 else 0
        bpy.ops.mesh.primitive_cube_add(size=1, location=(offset, 0, -radius - 0.012))
        mount = bpy.context.active_object
        mount.scale = (0.012, radius * 1.4, 0.020)
        bpy.ops.object.transform_apply(scale=True)
        add_material(mount, STEEL, f"{item_name}_Mount_Mat")


def gen_att_red_dot():
    """Small reflex sight — short housing + tiny lens."""
    clear_scene()
    # Housing
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.012))
    housing = bpy.context.active_object
    housing.scale = (0.030, 0.025, 0.025)
    bpy.ops.object.transform_apply(scale=True)
    add_material(housing, GUNMETAL, "RedDot_Housing_Mat")
    # Lens window (vertical glass slab)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.012, 0, 0.018))
    lens = bpy.context.active_object
    lens.scale = (0.002, 0.018, 0.020)
    bpy.ops.object.transform_apply(scale=True)
    add_material(lens, GLASS, "RedDot_Lens_Mat")
    # Mount base
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, -0.005))
    base = bpy.context.active_object
    base.scale = (0.034, 0.020, 0.008)
    bpy.ops.object.transform_apply(scale=True)
    add_material(base, STEEL, "RedDot_Base_Mat")
    join_and_rename("SM_ATT_RED_DOT")


def gen_att_4x_scope():
    clear_scene()
    _scope_block("Scope4X", body_len=0.14, radius=0.020)
    join_and_rename("SM_ATT_4X_SCOPE")


def gen_att_8x_scope():
    clear_scene()
    _scope_block("Scope8X", body_len=0.20, radius=0.022)
    join_and_rename("SM_ATT_8X_SCOPE")


def gen_att_16x_scope():
    clear_scene()
    _scope_block("Scope16X", body_len=0.28, radius=0.026, mount_count=3)
    join_and_rename("SM_ATT_16X_SCOPE")


# ── Ammo Generators ───────────────────────────────────────────────────────────

def _cartridge(item_id, case_len, case_radius, neck_len, neck_radius,
               projectile_len, case_color=BRASS, projectile_color=COPPER,
               marker_color=None, shotgun=False):
    """Generic cartridge: case + (optional necked-down) + projectile."""
    clear_scene()
    if shotgun:
        # Brass head
        bpy.ops.mesh.primitive_cylinder_add(radius=case_radius, depth=case_len * 0.35, location=(0, 0, case_len * 0.175))
        add_material(bpy.context.active_object, BRASS, f"{item_id}_Head_Mat")
        # Plastic hull
        bpy.ops.mesh.primitive_cylinder_add(radius=case_radius * 0.95, depth=case_len * 0.65, location=(0, 0, case_len * 0.35 + case_len * 0.325))
        hull = bpy.context.active_object
        add_material(hull, (0.55, 0.10, 0.10, 1.0), f"{item_id}_Hull_Mat")
        # Crimped tip
        bpy.ops.mesh.primitive_cone_add(radius1=case_radius * 0.95, radius2=case_radius * 0.4,
                                        depth=case_len * 0.10, location=(0, 0, case_len + case_len * 0.05))
        crimp = bpy.context.active_object
        add_material(crimp, (0.45, 0.08, 0.08, 1.0), f"{item_id}_Crimp_Mat")
    else:
        # Case body
        bpy.ops.mesh.primitive_cylinder_add(radius=case_radius, depth=case_len, location=(0, 0, case_len / 2))
        case = bpy.context.active_object
        add_material(case, case_color, f"{item_id}_Case_Mat")
        # Rim at base
        bpy.ops.mesh.primitive_torus_add(major_radius=case_radius * 1.05, minor_radius=case_radius * 0.05, location=(0, 0, 0))
        add_material(bpy.context.active_object, case_color, f"{item_id}_Rim_Mat")
        # Optional shoulder + neck
        z_top = case_len
        if neck_len > 0 and neck_radius < case_radius:
            bpy.ops.mesh.primitive_cone_add(radius1=case_radius, radius2=neck_radius,
                                            depth=case_len * 0.12, location=(0, 0, z_top + case_len * 0.06))
            add_material(bpy.context.active_object, case_color, f"{item_id}_Shoulder_Mat")
            z_top += case_len * 0.12
            bpy.ops.mesh.primitive_cylinder_add(radius=neck_radius, depth=neck_len, location=(0, 0, z_top + neck_len / 2))
            add_material(bpy.context.active_object, case_color, f"{item_id}_Neck_Mat")
            z_top += neck_len
        # Projectile (cone tip)
        proj_radius = neck_radius if neck_len > 0 else case_radius
        bpy.ops.mesh.primitive_cone_add(radius1=proj_radius, radius2=proj_radius * 0.15,
                                        depth=projectile_len, location=(0, 0, z_top + projectile_len / 2))
        add_material(bpy.context.active_object, projectile_color, f"{item_id}_Projectile_Mat")

    # Optional marker stripe (e.g. subsonic indicator)
    if marker_color is not None:
        bpy.ops.mesh.primitive_torus_add(major_radius=case_radius * 1.02, minor_radius=case_radius * 0.04,
                                         location=(0, 0, case_len * 0.65))
        add_material(bpy.context.active_object, marker_color, f"{item_id}_Marker_Mat")
    join_and_rename(f"SM_{item_id}")


def gen_amo_pistol():
    _cartridge("AMO_PISTOL", case_len=0.018, case_radius=0.0045, neck_len=0, neck_radius=0,
               projectile_len=0.010, projectile_color=COPPER)


def gen_amo_smg():
    _cartridge("AMO_SMG", case_len=0.020, case_radius=0.0048, neck_len=0, neck_radius=0,
               projectile_len=0.010, projectile_color=COPPER)


def gen_amo_subsonic_pistol():
    _cartridge("AMO_SUBSONIC_PISTOL", case_len=0.018, case_radius=0.0045, neck_len=0, neck_radius=0,
               projectile_len=0.010, projectile_color=LEAD,
               marker_color=(0.35, 0.55, 0.85, 1.0))


def gen_amo_rifle():
    _cartridge("AMO_RIFLE", case_len=0.034, case_radius=0.0060, neck_len=0.008, neck_radius=0.0042,
               projectile_len=0.014, projectile_color=COPPER)


def gen_amo_dmr():
    _cartridge("AMO_DMR", case_len=0.045, case_radius=0.0072, neck_len=0.010, neck_radius=0.0050,
               projectile_len=0.018, projectile_color=COPPER,
               marker_color=(0.85, 0.45, 0.10, 1.0))


def gen_amo_shotgun():
    _cartridge("AMO_SHOTGUN", case_len=0.060, case_radius=0.0090, neck_len=0, neck_radius=0,
               projectile_len=0, shotgun=True)


def gen_amo_sniper():
    _cartridge("AMO_SNIPER", case_len=0.060, case_radius=0.0085, neck_len=0.014, neck_radius=0.0058,
               projectile_len=0.024, projectile_color=COPPER,
               marker_color=(0.85, 0.10, 0.10, 1.0))


# ── Dispatch tables ──────────────────────────────────────────────────────────

ATT_GENERATORS = {
    "ATT_SUPPRESSOR":     gen_att_suppressor,
    "ATT_COMPENSATOR":    gen_att_compensator,
    "ATT_VERTICAL_GRIP":  gen_att_vertical_grip,
    "ATT_STOCK_PAD":      gen_att_stock_pad,
    "ATT_EXT_MAG":        gen_att_ext_mag,
    "ATT_RED_DOT":        gen_att_red_dot,
    "ATT_4X_SCOPE":       gen_att_4x_scope,
    "ATT_8X_SCOPE":       gen_att_8x_scope,
    "ATT_16X_SCOPE":      gen_att_16x_scope,
}

AMO_GENERATORS = {
    "AMO_PISTOL":           gen_amo_pistol,
    "AMO_SMG":              gen_amo_smg,
    "AMO_SUBSONIC_PISTOL":  gen_amo_subsonic_pistol,
    "AMO_RIFLE":            gen_amo_rifle,
    "AMO_DMR":              gen_amo_dmr,
    "AMO_SHOTGUN":          gen_amo_shotgun,
    "AMO_SNIPER":           gen_amo_sniper,
}


# ── Main export pipeline ───────────────────────────────────────────────────────

def _process(csv_path, id_field, generators, label):
    csv_abs = os.path.abspath(csv_path)
    if not os.path.isfile(csv_abs):
        print(f"ERROR: CSV not found at {csv_abs}")
        return
    with open(csv_abs, newline='', encoding='utf-8') as f:
        rows = [r for r in csv.DictReader(f) if r.get(id_field)]

    missing = [r[id_field] for r in rows if r[id_field] not in generators]
    if missing:
        print(f"WARN: no generator registered for {label}: {missing}")

    for row in rows:
        rid = row[id_field]
        gen = generators.get(rid)
        if gen is None:
            continue
        print(f"\n[{rid}] {row.get('DisplayName', rid)}")
        gen()
        out_path = os.path.join(OUTPUT_DIR, f"SM_{rid}.fbx")
        export_fbx(rid, out_path)


def main():
    print("\n=== Quiet Rift: Enigma — Attachments & Ammo Asset Generator ===")
    _process(ATT_CSV, "AttachmentId", ATT_GENERATORS, "attachments")
    _process(AMO_CSV, "AmmoId",       AMO_GENERATORS, "ammo")
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Import these FBX files into UE5 via Content Browser > Import.")


if __name__ == "__main__":
    main()
