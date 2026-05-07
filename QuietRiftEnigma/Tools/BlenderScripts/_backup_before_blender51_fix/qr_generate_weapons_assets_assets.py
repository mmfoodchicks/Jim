"""
Quiet Rift: Enigma — Weapons Procedural Asset Generator (Blender 4.x)

Upgraded in Batch 2 of the Blender detail pass — every weapon flows
through qr_blender_detail.py for production finalization (palette
dedupe, smooth shading, bevels, smart UV, sockets, UCX collision, LODs)
and grows extra detail (panel seams on receivers, sight blocks, picatinny
rail texture, charging handles, ejection ports).

Reads every row in DT_ArmoryWeapons.csv and exports one placeholder
mesh per weapon id. Conventions:
    +X is the muzzle direction. Origin sits at the front of the grip.
Per-weapon sockets exported into the FBX:
    SOCKET_Muzzle, SOCKET_OpticRail, SOCKET_MagWell, SOCKET_GripRoot,
    SOCKET_StockEnd (where applicable)

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
    assign_material,
    add_socket,
    add_panel_seam_strip,
    add_rivet_grid,
    finalize_asset,
)

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/weapons_assets")
CSV_PATH = os.path.join(
    os.path.dirname(__file__),
    "../../Content/QuietRift/Data/DT_ArmoryWeapons.csv",
)


def _add(obj, mat):
    assign_material(obj, mat)
    return obj


# ── Reusable part builders ────────────────────────────────────────────────────

def _barrel(length, radius, x_origin, mat=None):
    if mat is None:
        mat = palette_material("Gunmetal")
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=length,
                                         location=(x_origin + length / 2, 0, 0))
    b = bpy.context.active_object
    b.rotation_euler.y = math.pi / 2
    _add(b, mat)
    return b


def _receiver(length, height, depth, x_origin, mat=None):
    if mat is None:
        mat = palette_material("Steel")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x_origin + length / 2, 0, 0))
    r = bpy.context.active_object
    r.scale = (length, depth, height)
    bpy.ops.object.transform_apply(scale=True)
    _add(r, mat)
    return r


def _grip(x_origin, height=0.08, depth=0.025, mat=None):
    if mat is None:
        mat = palette_material("Polymer")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x_origin, 0, -height / 2 - 0.015))
    g = bpy.context.active_object
    g.scale = (0.035, depth, height)
    g.rotation_euler.y = -0.25
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    _add(g, mat)
    return g


def _magazine(x_origin, length=0.05, depth=0.02, drop=0.07, mat=None):
    if mat is None:
        mat = palette_material("Gunmetal")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x_origin, 0, -drop / 2 - 0.015))
    m = bpy.context.active_object
    m.scale = (length, depth, drop)
    bpy.ops.object.transform_apply(scale=True)
    _add(m, mat)
    return m


def _stock(x_origin, length=0.18, height=0.045, depth=0.025, material_name="Polymer"):
    mat = palette_material(material_name)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x_origin - length / 2, 0, 0))
    s = bpy.context.active_object
    s.scale = (length, depth, height)
    bpy.ops.object.transform_apply(scale=True)
    _add(s, mat)
    return s


def _scope(x_center, body_len=0.12, radius=0.018, height=0.045):
    glass = palette_material("Glass")
    body_mat = palette_material("Gunmetal")
    steel_mat = palette_material("Steel")
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=body_len, location=(x_center, 0, height))
    body = bpy.context.active_object
    body.rotation_euler.y = math.pi / 2
    _add(body, body_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=radius * 1.4, depth=0.025, location=(x_center + body_len / 2, 0, height))
    bell = bpy.context.active_object
    bell.rotation_euler.y = math.pi / 2
    _add(bell, body_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=radius * 1.2, depth=0.005, location=(x_center + body_len / 2 + 0.013, 0, height))
    lens = bpy.context.active_object
    lens.rotation_euler.y = math.pi / 2
    _add(lens, glass)
    for offset in (-body_len / 2 + 0.02, body_len / 2 - 0.02):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x_center + offset, 0, height - 0.025))
        mount = bpy.context.active_object
        mount.scale = (0.012, 0.018, 0.020)
        bpy.ops.object.transform_apply(scale=True)
        _add(mount, steel_mat)


def _picatinny_rail(x_start, x_end, z_top, mat=None):
    """Top rail with stepped slot pattern — represented as a slab + 6 rivet bumps."""
    if mat is None:
        mat = palette_material("DarkSteel")
    length = x_end - x_start
    bpy.ops.mesh.primitive_cube_add(size=1, location=((x_start + x_end) / 2, 0, z_top))
    rail = bpy.context.active_object
    rail.scale = (length, 0.014, 0.006)
    bpy.ops.object.transform_apply(scale=True)
    _add(rail, mat)
    # Rail slot pattern
    add_rivet_grid(origin=(x_start + 0.01, 0.0, z_top + 0.004),
                    spacing=(length / 8.0, 0.0), rows=1, cols=8,
                    rivet_radius=0.0035, depth=0.002, normal_axis='Z',
                    material_name="DarkSteel")


def _bipod(x_origin):
    mat = palette_material("Steel")
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.005, depth=0.12, location=(x_origin, side * 0.025, -0.06))
        leg = bpy.context.active_object
        leg.rotation_euler.x = side * 0.25
        _add(leg, mat)


def _finalize_weapon(name, muzzle_x, has_optic_rail=True, optic_x=0.18, optic_z=0.045,
                     mag_x=0.13, grip_x=0.06, stock_end_x=-0.20):
    """Add canonical sockets + finalize. All offsets in meters."""
    add_socket("Muzzle", location=(muzzle_x, 0, 0))
    add_socket("MagWell", location=(mag_x, 0, -0.020))
    add_socket("GripRoot", location=(grip_x, 0, -0.025))
    if has_optic_rail:
        add_socket("OpticRail", location=(optic_x, 0, optic_z))
    if stock_end_x is not None:
        add_socket("StockEnd", location=(stock_end_x, 0, 0))
    finalize_asset(name,
                   bevel_width=0.0025, bevel_angle_deg=30,
                   smooth_angle_deg=55, collision="convex",
                   lods=[0.50], pivot="geometry_center")


# ── Generators ────────────────────────────────────────────────────────────────

def gen_service_pistol():
    """Compact service pistol — short slide, polymer frame, single mag."""
    clear_scene()
    _receiver(length=0.16, height=0.05, depth=0.028, x_origin=0)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.08, 0, 0.035))
    slide = bpy.context.active_object
    slide.scale = (0.16, 0.028, 0.025)
    bpy.ops.object.transform_apply(scale=True)
    _add(slide, palette_material("Gunmetal"))
    # Slide serrations as rivet grid
    add_rivet_grid(origin=(0.13, -0.014, 0.030),
                    spacing=(0.006, 0.0), rows=1, cols=5,
                    rivet_radius=0.002, depth=0.001, normal_axis='Y',
                    material_name="DarkSteel")
    _barrel(length=0.04, radius=0.006, x_origin=0.16)
    _grip(x_origin=0.02, height=0.10, depth=0.025)
    _magazine(x_origin=0.02, length=0.035, depth=0.022, drop=0.06)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.015, minor_radius=0.003, location=(0.04, 0, -0.02))
    _add(bpy.context.active_object, palette_material("Gunmetal"))
    _finalize_weapon("SM_WPN_SERVICE_PISTOL", muzzle_x=0.20, has_optic_rail=False,
                      mag_x=0.02, grip_x=0.02, stock_end_x=None)


def gen_smg_compact():
    """Compact SMG — short barrel, vertical mag, folding stock."""
    clear_scene()
    _receiver(length=0.28, height=0.045, depth=0.030, x_origin=0)
    _barrel(length=0.16, radius=0.008, x_origin=0.28)
    _grip(x_origin=0.05, height=0.085)
    _magazine(x_origin=0.10, length=0.04, depth=0.022, drop=0.13)
    _stock(x_origin=0, length=0.10, height=0.030, depth=0.022)
    _picatinny_rail(0.06, 0.26, 0.030)
    add_panel_seam_strip((0.02, 0.015, 0.0), (0.26, 0.015, 0.0),
                          width=0.003, depth=0.002, material_name="DarkSteel",
                          name="SMG_RecvSeam")
    _finalize_weapon("SM_WPN_SMG_COMPACT", muzzle_x=0.44,
                      optic_x=0.16, mag_x=0.10, grip_x=0.05, stock_end_x=-0.10)


def gen_carbine():
    """General-purpose patrol rifle."""
    clear_scene()
    _receiver(length=0.30, height=0.05, depth=0.032, x_origin=0)
    _barrel(length=0.32, radius=0.009, x_origin=0.30)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.022, depth=0.20, location=(0.40, 0, 0))
    hg = bpy.context.active_object
    hg.rotation_euler.y = math.pi / 2
    _add(hg, palette_material("Polymer"))
    # Handguard cooling slots via panel seams
    for sy in [-0.018, 0.018]:
        add_panel_seam_strip((0.32, sy, 0.0), (0.50, sy, 0.0),
                              width=0.002, depth=0.0015, material_name="DarkSteel",
                              name=f"Carbine_HGSlot_{int(sy*1000)}")
    _grip(x_origin=0.06, height=0.085)
    _magazine(x_origin=0.12, length=0.045, depth=0.025, drop=0.10)
    _stock(x_origin=0, length=0.22, height=0.045, depth=0.028)
    _picatinny_rail(0.05, 0.28, 0.030)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.05, 0, 0.045))
    rsight = bpy.context.active_object
    rsight.scale = (0.012, 0.018, 0.022); bpy.ops.object.transform_apply(scale=True)
    _add(rsight, palette_material("Gunmetal"))
    _finalize_weapon("SM_WPN_CARBINE", muzzle_x=0.62, optic_x=0.20,
                      mag_x=0.12, grip_x=0.06, stock_end_x=-0.22)


def gen_dmr():
    """Marksman rifle — longer barrel, scope, fixed stock."""
    clear_scene()
    _receiver(length=0.36, height=0.052, depth=0.033, x_origin=0)
    _barrel(length=0.46, radius=0.010, x_origin=0.36)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.020, depth=0.32, location=(0.52, 0, 0))
    hg = bpy.context.active_object
    hg.rotation_euler.y = math.pi / 2
    _add(hg, palette_material("Polymer"))
    _grip(x_origin=0.07, height=0.085)
    _magazine(x_origin=0.14, length=0.045, depth=0.025, drop=0.10)
    _stock(x_origin=0, length=0.26, height=0.050, depth=0.030)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.13, 0, 0.038))
    cheek = bpy.context.active_object
    cheek.scale = (0.16, 0.026, 0.020); bpy.ops.object.transform_apply(scale=True)
    _add(cheek, palette_material("Polymer"))
    _scope(x_center=0.18, body_len=0.18, radius=0.020, height=0.055)
    add_panel_seam_strip((0.02, 0.016, 0.0), (0.34, 0.016, 0.0),
                          width=0.003, depth=0.002, material_name="DarkSteel",
                          name="DMR_RecvSeam")
    _finalize_weapon("SM_WPN_DMR", muzzle_x=0.82, optic_x=0.18, optic_z=0.075,
                      mag_x=0.14, grip_x=0.07, stock_end_x=-0.26)


def gen_pump_shotgun():
    """Pump-action shotgun — wood furniture, tubular magazine."""
    clear_scene()
    _receiver(length=0.30, height=0.062, depth=0.040, x_origin=0)
    _barrel(length=0.46, radius=0.013, x_origin=0.30)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.026, depth=0.16, location=(0.46, 0, -0.012))
    pump = bpy.context.active_object
    pump.rotation_euler.y = math.pi / 2
    _add(pump, palette_material("Wood"))
    bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.42, location=(0.50, 0, -0.022))
    tubmag = bpy.context.active_object
    tubmag.rotation_euler.y = math.pi / 2
    _add(tubmag, palette_material("Gunmetal"))
    _grip(x_origin=0.05, height=0.090, mat=palette_material("Wood"))
    _stock(x_origin=0, length=0.26, height=0.055, depth=0.030, material_name="Wood")
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.005, location=(0.76, 0, 0.018))
    _add(bpy.context.active_object, palette_material("Brass"))
    add_panel_seam_strip((0.02, 0.020, 0.025), (0.28, 0.020, 0.025),
                          width=0.003, depth=0.002, material_name="DarkSteel",
                          name="Pump_RecvSeam")
    _finalize_weapon("SM_WPN_PUMP_SHOTGUN", muzzle_x=0.76, has_optic_rail=False,
                      mag_x=0.05, grip_x=0.05, stock_end_x=-0.26)


def gen_bolt_sniper():
    """Bolt-action sniper — heavy receiver, scope, bolt handle."""
    clear_scene()
    _receiver(length=0.32, height=0.050, depth=0.034, x_origin=0)
    _barrel(length=0.62, radius=0.010, x_origin=0.32)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.005, depth=0.07, location=(0.10, 0.030, 0.020))
    bolt = bpy.context.active_object
    bolt.rotation_euler.x = math.pi / 2
    _add(bolt, palette_material("Gunmetal"))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.010, location=(0.10, 0.066, 0.020))
    _add(bpy.context.active_object, palette_material("Gunmetal"))
    _grip(x_origin=0.06, height=0.085, mat=palette_material("Wood"))
    _magazine(x_origin=0.13, length=0.038, depth=0.024, drop=0.06)
    _stock(x_origin=0, length=0.30, height=0.055, depth=0.034, material_name="Wood")
    _scope(x_center=0.22, body_len=0.24, radius=0.022, height=0.060)
    add_panel_seam_strip((0.02, 0.017, 0.0), (0.30, 0.017, 0.0),
                          width=0.003, depth=0.002, material_name="DarkSteel",
                          name="Bolt_RecvSeam")
    _finalize_weapon("SM_WPN_BOLT_SNIPER", muzzle_x=0.94, optic_x=0.22, optic_z=0.090,
                      mag_x=0.13, grip_x=0.06, stock_end_x=-0.30)


def gen_longrange_sniper():
    """Long-range sniper — extra-long heavy barrel, big scope, bipod, muzzle brake."""
    clear_scene()
    _receiver(length=0.34, height=0.054, depth=0.038, x_origin=0)
    _barrel(length=0.78, radius=0.012, x_origin=0.34)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.020, depth=0.04, location=(1.14, 0, 0))
    mb = bpy.context.active_object
    mb.rotation_euler.y = math.pi / 2
    _add(mb, palette_material("Gunmetal"))
    for i in range(3):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(1.14 + (i - 1) * 0.012, 0, 0.018))
        slot = bpy.context.active_object
        slot.scale = (0.005, 0.024, 0.006); bpy.ops.object.transform_apply(scale=True)
        _add(slot, palette_material("Polymer"))
    bpy.ops.mesh.primitive_cylinder_add(radius=0.005, depth=0.07, location=(0.10, 0.030, 0.022))
    bolt = bpy.context.active_object
    bolt.rotation_euler.x = math.pi / 2
    _add(bolt, palette_material("Gunmetal"))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.010, location=(0.10, 0.066, 0.022))
    _add(bpy.context.active_object, palette_material("Gunmetal"))
    _grip(x_origin=0.07, height=0.090)
    _magazine(x_origin=0.14, length=0.040, depth=0.026, drop=0.07)
    _stock(x_origin=0, length=0.34, height=0.060, depth=0.040)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.16, 0, 0.044))
    cheek = bpy.context.active_object
    cheek.scale = (0.18, 0.030, 0.022); bpy.ops.object.transform_apply(scale=True)
    _add(cheek, palette_material("Polymer"))
    _scope(x_center=0.26, body_len=0.30, radius=0.026, height=0.070)
    _bipod(x_origin=0.95)
    add_panel_seam_strip((0.02, 0.019, 0.0), (0.32, 0.019, 0.0),
                          width=0.003, depth=0.002, material_name="DarkSteel",
                          name="LRS_RecvSeam")
    _finalize_weapon("SM_WPN_LONGRANGE_SNIPER", muzzle_x=1.16, optic_x=0.26, optic_z=0.105,
                      mag_x=0.14, grip_x=0.07, stock_end_x=-0.34)


# ── Dispatch ──────────────────────────────────────────────────────────────────

GENERATORS = {
    "WPN_SERVICE_PISTOL":   gen_service_pistol,
    "WPN_SMG_COMPACT":      gen_smg_compact,
    "WPN_CARBINE":          gen_carbine,
    "WPN_DMR":              gen_dmr,
    "WPN_PUMP_SHOTGUN":     gen_pump_shotgun,
    "WPN_BOLT_SNIPER":      gen_bolt_sniper,
    "WPN_LONGRANGE_SNIPER": gen_longrange_sniper,
}


def main():
    print("\n=== Quiet Rift: Enigma — Weapons Asset Generator (Batch 2 detail upgrade) ===")
    csv_abs = os.path.abspath(CSV_PATH)
    if not os.path.isfile(csv_abs):
        print(f"ERROR: CSV not found at {csv_abs}")
        return
    with open(csv_abs, newline='', encoding='utf-8') as f:
        rows = [r for r in csv.DictReader(f) if r.get("WeaponId")]
    missing = [r["WeaponId"] for r in rows if r["WeaponId"] not in GENERATORS]
    if missing:
        print(f"WARN: no generator registered for: {missing}")
    for row in rows:
        wid = row["WeaponId"]
        gen = GENERATORS.get(wid)
        if gen is None:
            continue
        print(f"\n[{wid}] {row.get('DisplayName', wid)}  (Tier {row.get('Tier','?')})")
        gen()
        out_path = os.path.join(OUTPUT_DIR, f"SM_{wid}.fbx")
        export_fbx(wid, out_path)
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX exports: SM_<id> mesh, UCX_ collision, LOD1, plus SOCKET_Muzzle,")
    print("SOCKET_OpticRail, SOCKET_MagWell, SOCKET_GripRoot, SOCKET_StockEnd empties.")


if __name__ == "__main__":
    main()
