"""
Quiet Rift: Enigma — Vanguard Concordat Asset Generator (Blender 4.x)

Upgraded in Batch 6 of the Blender detail pass — every Vanguard outpost
tier + shared prop flows through qr_blender_detail.py for production
finalization (palette dedupe, smooth shading, bevels, smart UV, sockets,
UCX collision, LOD chain).

Generates placeholder meshes for every variant of EQRVanguardHardpointTier
(ListeningPost / ForwardPost / Hardpoint / InnerSanctum / Concordat)
plus shared faction props (Barricade, Banner). Sized for UE5 import
(1 UU = 1 cm).

Per-asset sockets:
    Outposts:
        SOCKET_GarrisonPoint   — primary NPC garrison spawn
        SOCKET_BannerSpawn     — colors / standard placement point
    Multi-garrison outposts also expose:
        SOCKET_GuardPostA      — secondary patrol point
        SOCKET_GuardPostB      — tertiary patrol point (Sanctum / Concordat)
    Props:
        SOCKET_AnchorPoint     — placement origin

Usage inside Blender:
    1. Open Blender > Scripting tab
    2. Open this file
    3. Set OUTPUT_DIR
    4. Press Run Script
"""

import bpy
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
    add_rivet_grid,
    finalize_asset,
)

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/vanguard_assets")


def _add(obj, mat):
    assign_material(obj, mat)
    return obj


def _slab(x, y, z, w, d, h, mat, name):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z))
    obj = bpy.context.active_object
    obj.scale = (w, d, h); bpy.ops.object.transform_apply(scale=True)
    _add(obj, mat); obj.name = name
    return obj


def _antenna(x, y, z_base, height=2.4):
    dark_steel = palette_material("DarkSteel")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.025, depth=height, location=(x, y, z_base + height / 2))
    _add(bpy.context.active_object, dark_steel)
    _slab(x, y, z_base + height * 0.85, 0.30, 0.015, 0.015, dark_steel, "Antenna_Spar")


def _crate(x, y, z, w=0.8, d=0.6, h=0.6, mat=None):
    if mat is None:
        mat = palette_material("Wood")
    _slab(x, y, z + h / 2, w, d, h, mat, "Crate")


def _sandbag_wall(x, y, length=2.0, layers=2):
    sand_mat = get_or_create_material("Vanguard_Sandbag", (0.55, 0.48, 0.32, 1.0), roughness=0.95)
    count = max(1, int(length / 0.45))
    for layer in range(layers):
        offset = 0.22 if layer % 2 else 0
        for i in range(count):
            cx = x - length / 2 + offset + i * 0.45
            _slab(cx, y, 0.18 + layer * 0.30, 0.40, 0.30, 0.28, sand_mat,
                   f"Sandbag_{layer}_{i}")


def _finalize_outpost(name, garrison, banner, guards=None, lods=(0.50, 0.20)):
    add_socket("GarrisonPoint", location=garrison)
    add_socket("BannerSpawn", location=banner)
    if guards:
        for i, loc in enumerate(guards):
            add_socket(f"GuardPost{chr(ord('A') + i)}", location=loc)
    finalize_asset(name,
                    bevel_width=0.005, bevel_angle_deg=30,
                    smooth_angle_deg=50, collision="convex",
                    lods=list(lods), pivot="bottom_center")


# ── Outpost Tier Generators ──────────────────────────────────────────────────

def gen_listening_post():
    """Minimal scout perch: antenna + spotter crate + small banner stake."""
    clear_scene()
    wood_mat = palette_material("Wood")
    crimson_mat = palette_material("VanguardRed")
    _antenna(0, 0, 0.0, height=3.0)
    _crate(1.0, 0.5, 0.0, w=0.7, d=0.5, h=0.5)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.015, depth=1.4, location=(-0.6, 0.2, 0.7))
    _add(bpy.context.active_object, wood_mat)
    _slab(-0.6, 0.2, 1.25, 0.005, 0.20, 0.16, crimson_mat, "Listening_Flag")
    _finalize_outpost("SM_VAN_OUT_ListeningPost",
                       garrison=(0.5, 0.5, 0), banner=(-0.6, 0.2, 1.40))


def gen_forward_post():
    """Light infantry: short sandbag arc + canvas tent + ammo crates."""
    clear_scene()
    canvas_mat = get_or_create_material("Vanguard_Canvas", (0.42, 0.45, 0.30, 1.0), roughness=0.95)
    _sandbag_wall(0, -1.2, length=3.0, layers=2)
    bpy.ops.mesh.primitive_cone_add(radius1=0.9, radius2=0.0, depth=1.6, vertices=3, location=(0, 0.4, 0.8))
    tent = bpy.context.active_object
    tent.rotation_euler = (math.pi / 2, math.pi / 2, 0)
    _add(tent, canvas_mat)
    _crate(-1.6, 0.5, 0.0)
    _crate(1.6, 0.5, 0.0)
    _finalize_outpost("SM_VAN_OUT_ForwardPost",
                       garrison=(0, 0.4, 0), banner=(0, -1.6, 1.4),
                       guards=[(-1.6, 0.5, 0)])


def gen_hardpoint():
    """Fortified bunker: concrete walls + roof + slit window + emplacement."""
    clear_scene()
    steel_mat = palette_material("Steel")
    dark_steel = palette_material("DarkSteel")
    _slab(0, 0, 1.0, 3.6, 2.8, 2.0, steel_mat, "Hardpoint_Shell")
    _slab(0, 0, 2.05, 4.0, 3.2, 0.20, dark_steel, "Hardpoint_Roof")
    slit_mat = get_or_create_material("Vanguard_Slit", (0.05, 0.05, 0.06, 1.0), roughness=0.95)
    _slab(0, -1.42, 1.40, 2.4, 0.05, 0.20, slit_mat, "Hardpoint_Slit")
    _sandbag_wall(2.6, -1.6, length=2.0, layers=2)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.10, depth=0.4, location=(0, 0, 2.35))
    _add(bpy.context.active_object, dark_steel)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.025, depth=0.7, location=(0.20, 0, 2.45))
    barrel = bpy.context.active_object
    barrel.rotation_euler.y = math.pi / 2
    _add(barrel, dark_steel)
    # Roof seam panel + bolt rivets along the front
    add_panel_seam_strip((-1.95, -1.55, 2.05), (1.95, -1.55, 2.05),
                          width=0.04, depth=0.012, material_name="DarkSteel",
                          name="Hardpoint_RoofSeam")
    add_rivet_grid(origin=(-1.7, -1.42, 2.10), spacing=(0.40, 0.0),
                    rows=1, cols=9, rivet_radius=0.025, depth=0.015,
                    normal_axis='Z', material_name="DarkSteel")
    _finalize_outpost("SM_VAN_OUT_Hardpoint",
                       garrison=(0, 0, 0), banner=(2.6, -1.0, 1.4),
                       guards=[(2.6, -1.6, 0)])


def gen_inner_sanctum():
    """Heavy walls + parapet + flanking towers — elite garrison."""
    clear_scene()
    steel_mat = palette_material("Steel")
    dark_steel = palette_material("DarkSteel")
    crimson_mat = palette_material("VanguardRed")
    _slab(0, 0, 1.6, 5.0, 4.0, 3.2, steel_mat, "Sanctum_Keep")
    for i in range(5):
        _slab((i - 2) * 1.0, 1.95, 3.40, 0.40, 0.30, 0.40, dark_steel, "Sanctum_Crenel")
    for sx in [-1, 1]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.7, depth=4.4, vertices=8, location=(sx * 2.8, 0, 2.2))
        _add(bpy.context.active_object, steel_mat)
        bpy.ops.mesh.primitive_cone_add(radius1=0.85, radius2=0.0, depth=0.8, vertices=8, location=(sx * 2.8, 0, 4.8))
        _add(bpy.context.active_object, crimson_mat)
    _slab(0, -2.05, 1.0, 1.4, 0.10, 1.8, dark_steel, "Sanctum_Gate")
    # Decorative belt seam around the keep
    for sy_band in [0.40, 2.80]:
        add_panel_seam_strip((-2.5, 2.01, sy_band), (2.5, 2.01, sy_band),
                              width=0.06, depth=0.018, material_name="DarkSteel",
                              name=f"Sanctum_Band_{int(sy_band*10)}")
    _finalize_outpost("SM_VAN_OUT_InnerSanctum",
                       garrison=(0, 0, 0), banner=(0, -2.10, 2.5),
                       guards=[(2.8, 0, 0), (-2.8, 0, 0)])


def gen_concordat():
    """The main colony core — central tower + ringed walls + banner mast."""
    clear_scene()
    steel_mat = palette_material("Steel")
    dark_steel = palette_material("DarkSteel")
    crimson_mat = palette_material("VanguardRed")
    for i in range(12):
        ang = (i / 12.0) * math.tau
        x = math.cos(ang) * 6.0
        y = math.sin(ang) * 6.0
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 1.0))
        seg = bpy.context.active_object
        seg.scale = (0.4, 1.6, 2.0); seg.rotation_euler.z = ang + math.pi / 2
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(seg, steel_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=2.0, depth=8.0, vertices=8, location=(0, 0, 4.0))
    _add(bpy.context.active_object, steel_mat)
    bpy.ops.mesh.primitive_cone_add(radius1=2.2, radius2=0.0, depth=2.4, vertices=8, location=(0, 0, 9.2))
    _add(bpy.context.active_object, crimson_mat)
    for sx, sy in [(3.5, 0), (-3.5, 0), (0, 3.5), (0, -3.5)]:
        _slab(sx, sy, 1.4, 1.6, 1.6, 2.8, dark_steel, "Concordat_InnerBldg")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.05, depth=2.0, location=(0, 0, 11.4))
    _add(bpy.context.active_object, dark_steel)
    _slab(0.4, 0, 11.6, 0.005, 0.8, 0.6, crimson_mat, "Concordat_Flag")
    # Central tower vertical groove seams
    for ang in [0, math.tau / 4, 2 * math.tau / 4, 3 * math.tau / 4]:
        x = math.cos(ang) * 2.05
        y = math.sin(ang) * 2.05
        add_panel_seam_strip((x, y, 0.5), (x, y, 7.5),
                              width=0.05, depth=0.02, material_name="DarkSteel",
                              name=f"Concordat_TowerSeam_{int(ang*100)}")
    _finalize_outpost("SM_VAN_OUT_Concordat",
                       garrison=(0, 0, 0), banner=(0, 0, 11.5),
                       guards=[(3.5, 0, 0), (-3.5, 0, 0), (0, 3.5, 0)],
                       lods=(0.50, 0.15))


# ── Shared Props ──────────────────────────────────────────────────────────────

def gen_barricade():
    """Hesco-style barricade segment — fillable wall block with frame."""
    clear_scene()
    sand_mat = get_or_create_material("Vanguard_Sandbag", (0.55, 0.48, 0.32, 1.0), roughness=0.95)
    frame_mat = palette_material("DarkSteel")
    _slab(0, 0, 0.55, 1.2, 0.5, 1.1, sand_mat, "Barricade_Body")
    for sx, sy in [(0.6, 0.25), (-0.6, 0.25), (0.6, -0.25), (-0.6, -0.25)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.02, depth=1.10, location=(sx, sy, 0.55))
        _add(bpy.context.active_object, frame_mat)
    for sx in [0.6, -0.6]:
        _slab(sx, 0, 1.10, 0.04, 0.5, 0.02, frame_mat, "Barricade_TopBand")
    add_socket("AnchorPoint", location=(0, 0, 0.0))
    finalize_asset("SM_VAN_PRP_Barricade",
                    bevel_width=0.003, bevel_angle_deg=30,
                    smooth_angle_deg=50, collision="convex",
                    lods=[0.50], pivot="bottom_center")


def gen_banner():
    """Standalone Vanguard banner on a stake — used as map marker prop."""
    clear_scene()
    wood_mat = palette_material("Wood")
    crimson_mat = palette_material("VanguardRed")
    emblem_mat = get_or_create_material("Vanguard_BannerEmblem",
                                          (0.95, 0.92, 0.88, 1.0), roughness=0.65)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.03, depth=2.4, location=(0, 0, 1.2))
    _add(bpy.context.active_object, wood_mat)
    _slab(0, 0, 2.20, 0.50, 0.025, 0.025, wood_mat, "Banner_Cross")
    _slab(0, 0, 1.70, 0.005, 0.45, 0.85, crimson_mat, "Banner_Cloth")
    _slab(0, 0, 1.70, 0.006, 0.18, 0.30, emblem_mat, "Banner_Emblem")
    add_socket("AnchorPoint", location=(0, 0, 0.0))
    finalize_asset("SM_VAN_PRP_Banner",
                    bevel_width=0.0015, bevel_angle_deg=30,
                    smooth_angle_deg=45, collision="convex",
                    lods=[0.50], pivot="bottom_center")


# ── Dispatch ──────────────────────────────────────────────────────────────────

GENERATORS = {
    "VAN_OUT_ListeningPost":  gen_listening_post,
    "VAN_OUT_ForwardPost":    gen_forward_post,
    "VAN_OUT_Hardpoint":      gen_hardpoint,
    "VAN_OUT_InnerSanctum":   gen_inner_sanctum,
    "VAN_OUT_Concordat":      gen_concordat,
    "VAN_PRP_Barricade":      gen_barricade,
    "VAN_PRP_Banner":         gen_banner,
}


def main():
    print("\n=== Quiet Rift: Enigma — Vanguard Asset Generator (Batch 6 detail upgrade) ===")
    for asset_id, gen in GENERATORS.items():
        print(f"\n[{asset_id}]")
        gen()
        out_path = os.path.join(OUTPUT_DIR, f"SM_{asset_id}.fbx")
        export_fbx(asset_id, out_path)
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX exports SM_<id> + UCX_ collision + LOD chain. Outposts add")
    print("SOCKET_GarrisonPoint / SOCKET_BannerSpawn / SOCKET_GuardPost*; props use AnchorPoint.")


if __name__ == "__main__":
    main()
