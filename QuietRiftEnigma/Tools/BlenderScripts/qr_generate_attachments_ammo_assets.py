"""
Quiet Rift: Enigma — Attachments & Ammo Procedural Asset Generator (Blender 4.x)

Upgraded in Batch 2 of the Blender detail pass — every attachment + cartridge
flows through qr_blender_detail.py for production finalization (palette
dedupe, smooth shading, bevels, smart UV, sockets, UCX collision).

Reads every row in DT_ArmoryAttachments.csv (9 attachments) and
DT_ArmoryAmmo.csv (7 ammo types) and exports one placeholder mesh per row.

Sockets:
  - Attachments: SOCKET_AttachFront / SOCKET_AttachRear (mount points)
  - Ammo: SOCKET_Tip (projectile end) / SOCKET_Base (case head)

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
    add_panel_seam_strip,
    add_rivet_ring,
    finalize_asset,
)

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/attachments_ammo")
ATT_CSV = os.path.join(os.path.dirname(__file__),
                       "../../Content/QuietRift/Data/DT_ArmoryAttachments.csv")
AMO_CSV = os.path.join(os.path.dirname(__file__),
                       "../../Content/QuietRift/Data/DT_ArmoryAmmo.csv")


def _add(obj, mat):
    assign_material(obj, mat)
    return obj


def _finalize_attachment(name, front=(0.0, 0, 0), rear=(0.0, 0, 0), lods=(0.5,)):
    add_socket("AttachFront", location=front)
    add_socket("AttachRear", location=rear)
    finalize_asset(name,
                   bevel_width=0.0015, bevel_angle_deg=30,
                   smooth_angle_deg=55, collision="convex",
                   lods=list(lods), pivot="geometry_center")


def _finalize_ammo(name, tip=(0, 0, 0), base=(0, 0, 0)):
    add_socket("Tip", location=tip)
    add_socket("Base", location=base)
    finalize_asset(name,
                   bevel_width=0.0008, bevel_angle_deg=30,
                   smooth_angle_deg=45, collision="convex",
                   lods=None, pivot="geometry_center")


# ── Attachment Generators ─────────────────────────────────────────────────────

def gen_att_suppressor():
    """Long cylindrical suppressor with baffle rings + thread mount."""
    clear_scene()
    body_mat = palette_material("Gunmetal")
    ring_mat = palette_material("Steel")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.022, depth=0.18, location=(0, 0, 0.09))
    body = bpy.context.active_object
    body.rotation_euler.y = math.pi / 2
    bpy.ops.object.transform_apply(rotation=True)
    _add(body, body_mat)
    for i in range(5):
        x = 0.02 + i * 0.035
        bpy.ops.mesh.primitive_torus_add(major_radius=0.024, minor_radius=0.0025, location=(x, 0, 0.09))
        ring = bpy.context.active_object
        ring.rotation_euler.y = math.pi / 2
        _add(ring, ring_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.014, depth=0.025, location=(-0.0125, 0, 0.09))
    mount = bpy.context.active_object
    mount.rotation_euler.y = math.pi / 2
    _add(mount, ring_mat)
    add_rivet_ring(center=(-0.0125, 0, 0.09), radius=0.011, count=6,
                    rivet_radius=0.002, depth=0.001, normal_axis='X',
                    material_name="DarkSteel")
    _finalize_attachment("SM_ATT_SUPPRESSOR", front=(0.18, 0, 0.09), rear=(-0.025, 0, 0.09))


def gen_att_compensator():
    """Short flared muzzle device with vent slots."""
    clear_scene()
    body_mat = palette_material("Gunmetal")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.020, depth=0.05, location=(0, 0, 0.025))
    body = bpy.context.active_object
    body.rotation_euler.y = math.pi / 2
    bpy.ops.object.transform_apply(rotation=True)
    _add(body, body_mat)
    for x, y in [(0, 0.022), (0, -0.022), (0.012, 0), (-0.012, 0)]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.025))
        slot = bpy.context.active_object
        slot.scale = (0.020, 0.005, 0.005); bpy.ops.object.transform_apply(scale=True)
        _add(slot, palette_material("Polymer"))
    _finalize_attachment("SM_ATT_COMPENSATOR", front=(0.05, 0, 0.025), rear=(0.0, 0, 0.025))


def gen_att_vertical_grip():
    """Short vertical foregrip with mount plate."""
    clear_scene()
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.005))
    plate = bpy.context.active_object
    plate.scale = (0.05, 0.024, 0.010); bpy.ops.object.transform_apply(scale=True)
    _add(plate, palette_material("Steel"))
    bpy.ops.mesh.primitive_cylinder_add(radius=0.014, depth=0.075, location=(0, 0, -0.038))
    _add(bpy.context.active_object, palette_material("Polymer"))
    for z in (-0.015, -0.045, -0.072):
        bpy.ops.mesh.primitive_torus_add(major_radius=0.015, minor_radius=0.002, location=(0, 0, z))
        _add(bpy.context.active_object, palette_material("Gunmetal"))
    _finalize_attachment("SM_ATT_VERTICAL_GRIP",
                          front=(0.025, 0, 0.005), rear=(-0.025, 0, 0.005))


def gen_att_stock_pad():
    """Soft recoil pad — rubber block with grip ridges."""
    clear_scene()
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.025))
    pad = bpy.context.active_object
    pad.scale = (0.025, 0.040, 0.050); bpy.ops.object.transform_apply(scale=True)
    _add(pad, palette_material("Rubber"))
    for i in range(4):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0.014, 0, 0.005 + i * 0.013))
        ridge = bpy.context.active_object
        ridge.scale = (0.002, 0.038, 0.003); bpy.ops.object.transform_apply(scale=True)
        _add(ridge, get_or_create_material("StockPad_RidgeAccent", (0.05, 0.05, 0.05, 1.0), roughness=0.85))
    _finalize_attachment("SM_ATT_STOCK_PAD", front=(0.012, 0, 0.025), rear=(-0.012, 0, 0.025))


def gen_att_ext_mag():
    """Extended magazine — taller box than standard."""
    clear_scene()
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, -0.075))
    body = bpy.context.active_object
    body.scale = (0.045, 0.024, 0.150); bpy.ops.object.transform_apply(scale=True)
    _add(body, palette_material("Gunmetal"))
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, -0.155))
    plate = bpy.context.active_object
    plate.scale = (0.050, 0.028, 0.015); bpy.ops.object.transform_apply(scale=True)
    _add(plate, palette_material("Polymer"))
    for i in range(4):
        bpy.ops.mesh.primitive_cylinder_add(radius=0.003, depth=0.005, location=(0, 0.014, -0.030 - i * 0.030))
        hole = bpy.context.active_object
        hole.rotation_euler.x = math.pi / 2
        _add(hole, palette_material("DarkSteel"))
    _finalize_attachment("SM_ATT_EXT_MAG", front=(0, 0, 0.0), rear=(0, 0, -0.16))


def _scope_block(item_name, body_len, radius, mount_count=2):
    """Generic scope body shared between 4X/8X/16X."""
    body_mat = palette_material("Gunmetal")
    glass = palette_material("Glass")
    steel_mat = palette_material("Steel")
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=body_len, location=(0, 0, 0))
    body = bpy.context.active_object
    body.rotation_euler.y = math.pi / 2
    _add(body, body_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=radius * 1.4, depth=0.025, location=(body_len / 2, 0, 0))
    bell = bpy.context.active_object
    bell.rotation_euler.y = math.pi / 2
    _add(bell, body_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=radius * 1.2, depth=0.005, location=(body_len / 2 + 0.013, 0, 0))
    lens = bpy.context.active_object
    lens.rotation_euler.y = math.pi / 2
    _add(lens, glass)
    bpy.ops.mesh.primitive_cylinder_add(radius=radius * 0.8, depth=radius * 1.6, location=(0, 0, radius * 1.4))
    _add(bpy.context.active_object, body_mat)
    spread = body_len * 0.35
    for i in range(mount_count):
        offset = (-spread / 2) + (i * (spread / max(mount_count - 1, 1))) if mount_count > 1 else 0
        bpy.ops.mesh.primitive_cube_add(size=1, location=(offset, 0, -radius - 0.012))
        mount = bpy.context.active_object
        mount.scale = (0.012, radius * 1.4, 0.020); bpy.ops.object.transform_apply(scale=True)
        _add(mount, steel_mat)
    # Body knurling rings
    for ring_x in [-body_len * 0.12, body_len * 0.12]:
        bpy.ops.mesh.primitive_torus_add(major_radius=radius * 1.05, minor_radius=radius * 0.04,
                                          location=(ring_x, 0, 0))
        ring = bpy.context.active_object
        ring.rotation_euler.y = math.pi / 2
        _add(ring, palette_material("DarkSteel"))


def gen_att_red_dot():
    """Small reflex sight."""
    clear_scene()
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.012))
    housing = bpy.context.active_object
    housing.scale = (0.030, 0.025, 0.025); bpy.ops.object.transform_apply(scale=True)
    _add(housing, palette_material("Gunmetal"))
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.012, 0, 0.018))
    lens = bpy.context.active_object
    lens.scale = (0.002, 0.018, 0.020); bpy.ops.object.transform_apply(scale=True)
    _add(lens, palette_material("Glass"))
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, -0.005))
    base = bpy.context.active_object
    base.scale = (0.034, 0.020, 0.008); bpy.ops.object.transform_apply(scale=True)
    _add(base, palette_material("Steel"))
    _finalize_attachment("SM_ATT_RED_DOT",
                          front=(0.013, 0, 0.018), rear=(-0.013, 0, 0.018))


def gen_att_4x_scope():
    clear_scene()
    _scope_block("Scope4X", body_len=0.14, radius=0.020)
    _finalize_attachment("SM_ATT_4X_SCOPE",
                          front=(0.080, 0, 0), rear=(-0.080, 0, 0))


def gen_att_8x_scope():
    clear_scene()
    _scope_block("Scope8X", body_len=0.20, radius=0.022)
    _finalize_attachment("SM_ATT_8X_SCOPE",
                          front=(0.115, 0, 0), rear=(-0.115, 0, 0))


def gen_att_16x_scope():
    clear_scene()
    _scope_block("Scope16X", body_len=0.28, radius=0.026, mount_count=3)
    _finalize_attachment("SM_ATT_16X_SCOPE",
                          front=(0.155, 0, 0), rear=(-0.155, 0, 0))


# ── Ammo Generators ───────────────────────────────────────────────────────────

def _cartridge(item_id, case_len, case_radius, neck_len, neck_radius,
               projectile_len, case_color_name="Brass",
               projectile_color_name="Copper", marker_color=None, shotgun=False):
    clear_scene()
    case_mat = palette_material(case_color_name)
    proj_mat = palette_material(projectile_color_name)
    if shotgun:
        bpy.ops.mesh.primitive_cylinder_add(radius=case_radius, depth=case_len * 0.35, location=(0, 0, case_len * 0.175))
        _add(bpy.context.active_object, palette_material("Brass"))
        bpy.ops.mesh.primitive_cylinder_add(radius=case_radius * 0.95, depth=case_len * 0.65,
                                             location=(0, 0, case_len * 0.35 + case_len * 0.325))
        _add(bpy.context.active_object,
              get_or_create_material("Ammo_ShotHull", (0.55, 0.10, 0.10, 1.0), roughness=0.65))
        bpy.ops.mesh.primitive_cone_add(radius1=case_radius * 0.95, radius2=case_radius * 0.4,
                                        depth=case_len * 0.10, location=(0, 0, case_len + case_len * 0.05))
        _add(bpy.context.active_object,
              get_or_create_material("Ammo_ShotCrimp", (0.45, 0.08, 0.08, 1.0), roughness=0.65))
    else:
        bpy.ops.mesh.primitive_cylinder_add(radius=case_radius, depth=case_len, location=(0, 0, case_len / 2))
        _add(bpy.context.active_object, case_mat)
        bpy.ops.mesh.primitive_torus_add(major_radius=case_radius * 1.05, minor_radius=case_radius * 0.05, location=(0, 0, 0))
        _add(bpy.context.active_object, case_mat)
        z_top = case_len
        if neck_len > 0 and neck_radius < case_radius:
            bpy.ops.mesh.primitive_cone_add(radius1=case_radius, radius2=neck_radius,
                                            depth=case_len * 0.12, location=(0, 0, z_top + case_len * 0.06))
            _add(bpy.context.active_object, case_mat)
            z_top += case_len * 0.12
            bpy.ops.mesh.primitive_cylinder_add(radius=neck_radius, depth=neck_len, location=(0, 0, z_top + neck_len / 2))
            _add(bpy.context.active_object, case_mat)
            z_top += neck_len
        proj_radius = neck_radius if neck_len > 0 else case_radius
        bpy.ops.mesh.primitive_cone_add(radius1=proj_radius, radius2=proj_radius * 0.15,
                                        depth=projectile_len, location=(0, 0, z_top + projectile_len / 2))
        _add(bpy.context.active_object, proj_mat)
    if marker_color is not None:
        bpy.ops.mesh.primitive_torus_add(major_radius=case_radius * 1.02,
                                          minor_radius=case_radius * 0.04,
                                          location=(0, 0, case_len * 0.65))
        _add(bpy.context.active_object,
              get_or_create_material(f"Ammo_Marker_{item_id}", marker_color,
                                      roughness=0.40, emissive=marker_color))
    _finalize_ammo(f"SM_{item_id}",
                    tip=(0, 0, case_len + (projectile_len if not shotgun else 0)),
                    base=(0, 0, 0))


def gen_amo_pistol():
    _cartridge("AMO_PISTOL", case_len=0.018, case_radius=0.0045, neck_len=0, neck_radius=0,
               projectile_len=0.010)


def gen_amo_smg():
    _cartridge("AMO_SMG", case_len=0.020, case_radius=0.0048, neck_len=0, neck_radius=0,
               projectile_len=0.010)


def gen_amo_subsonic_pistol():
    _cartridge("AMO_SUBSONIC_PISTOL", case_len=0.018, case_radius=0.0045, neck_len=0, neck_radius=0,
               projectile_len=0.010, projectile_color_name="Lead",
               marker_color=(0.35, 0.55, 0.85, 1.0))


def gen_amo_rifle():
    _cartridge("AMO_RIFLE", case_len=0.034, case_radius=0.0060, neck_len=0.008, neck_radius=0.0042,
               projectile_len=0.014)


def gen_amo_dmr():
    _cartridge("AMO_DMR", case_len=0.045, case_radius=0.0072, neck_len=0.010, neck_radius=0.0050,
               projectile_len=0.018, marker_color=(0.85, 0.45, 0.10, 1.0))


def gen_amo_shotgun():
    _cartridge("AMO_SHOTGUN", case_len=0.060, case_radius=0.0090, neck_len=0, neck_radius=0,
               projectile_len=0, shotgun=True)


def gen_amo_sniper():
    _cartridge("AMO_SNIPER", case_len=0.060, case_radius=0.0085, neck_len=0.014, neck_radius=0.0058,
               projectile_len=0.024, marker_color=(0.85, 0.10, 0.10, 1.0))


# ── Dispatch ──────────────────────────────────────────────────────────────────

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
    print("\n=== Quiet Rift: Enigma — Attachments & Ammo Asset Generator (Batch 2 detail upgrade) ===")
    _process(ATT_CSV, "AttachmentId", ATT_GENERATORS, "attachments")
    _process(AMO_CSV, "AmmoId",       AMO_GENERATORS, "ammo")
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX exports: SM_<id> mesh, UCX_ collision, plus")
    print("SOCKET_AttachFront/AttachRear (attachments) or SOCKET_Tip/Base (ammo).")


if __name__ == "__main__":
    main()
