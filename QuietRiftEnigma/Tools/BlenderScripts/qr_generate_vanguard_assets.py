"""
Quiet Rift: Enigma — Vanguard Concordat Asset Generator (Blender 4.x)

Run this in Blender's Scripting workspace or via:
    blender --background --python qr_generate_vanguard_assets.py

Generates placeholder meshes for every variant of EQRVanguardHardpointTier
(ListeningPost / ForwardPost / Hardpoint / InnerSanctum / Concordat) plus
shared faction props (barricade, banner) using the helpers in
qr_blender_common.py. Sized for UE5 import (1 UU = 1 cm).

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
    add_material,
    join_and_rename,
)

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/vanguard_assets")

# Vanguard palette — Colonial Authority steel + faction crimson
STEEL      = (0.30, 0.32, 0.36, 1.0)
DARK_STEEL = (0.18, 0.20, 0.24, 1.0)
SAND       = (0.55, 0.48, 0.32, 1.0)
CANVAS     = (0.42, 0.45, 0.30, 1.0)
WOOD       = (0.45, 0.30, 0.18, 1.0)
CRIMSON    = (0.55, 0.10, 0.12, 1.0)


def _antenna(x, y, z_base, height=2.4):
    """Tall thin antenna mast with cross spar."""
    bpy.ops.mesh.primitive_cylinder_add(radius=0.025, depth=height, location=(x, y, z_base + height / 2))
    add_material(bpy.context.active_object, DARK_STEEL, "Antenna_Mast_Mat")
    # Cross spar near top
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z_base + height * 0.85))
    spar = bpy.context.active_object
    spar.scale = (0.30, 0.015, 0.015)
    bpy.ops.object.transform_apply(scale=True)
    add_material(spar, DARK_STEEL, "Antenna_Spar_Mat")


def _crate(x, y, z, w=0.8, d=0.6, h=0.6, color=WOOD):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z + h / 2))
    crate = bpy.context.active_object
    crate.scale = (w, d, h)
    bpy.ops.object.transform_apply(scale=True)
    add_material(crate, color, "Crate_Mat")


def _sandbag_wall(x, y, length=2.0, layers=2):
    """Row of stacked sandbag cubes."""
    count = max(1, int(length / 0.45))
    for layer in range(layers):
        offset = 0.22 if layer % 2 else 0
        for i in range(count):
            cx = x - length / 2 + offset + i * 0.45
            bpy.ops.mesh.primitive_cube_add(size=1, location=(cx, y, 0.18 + layer * 0.30))
            bag = bpy.context.active_object
            bag.scale = (0.40, 0.30, 0.28)
            bpy.ops.object.transform_apply(scale=True)
            add_material(bag, SAND, "Sandbag_Mat")


# ── Outpost Tier Generators ──────────────────────────────────────────────────

def gen_listening_post():
    """Minimal scout perch: antenna mast + spotter crate, no fortification."""
    clear_scene()
    _antenna(0, 0, 0.0, height=3.0)
    _crate(1.0, 0.5, 0.0, w=0.7, d=0.5, h=0.5)
    # Small painted crimson banner stake
    bpy.ops.mesh.primitive_cylinder_add(radius=0.015, depth=1.4, location=(-0.6, 0.2, 0.7))
    add_material(bpy.context.active_object, WOOD, "Listening_Stake_Mat")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.6, 0.2, 1.25))
    flag = bpy.context.active_object
    flag.scale = (0.005, 0.20, 0.16)
    bpy.ops.object.transform_apply(scale=True)
    add_material(flag, CRIMSON, "Listening_Flag_Mat")
    join_and_rename("SM_VAN_OUT_ListeningPost")


def gen_forward_post():
    """Light infantry: short sandbag arc + canvas tent + ammo crates."""
    clear_scene()
    _sandbag_wall(0, -1.2, length=3.0, layers=2)
    # Tent (triangular prism via cone with 3 verts)
    bpy.ops.mesh.primitive_cone_add(radius1=0.9, radius2=0.0, depth=1.6, vertices=3, location=(0, 0.4, 0.8))
    tent = bpy.context.active_object
    tent.rotation_euler = (math.pi / 2, math.pi / 2, 0)
    add_material(tent, CANVAS, "ForwardPost_Tent_Mat")
    _crate(-1.6, 0.5, 0.0)
    _crate(1.6, 0.5, 0.0)
    join_and_rename("SM_VAN_OUT_ForwardPost")


def gen_hardpoint():
    """Fortified bunker: concrete walls + roof + slit window + emplacement."""
    clear_scene()
    # Bunker shell (low concrete box)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.0))
    shell = bpy.context.active_object
    shell.scale = (3.6, 2.8, 2.0)
    bpy.ops.object.transform_apply(scale=True)
    add_material(shell, STEEL, "Hardpoint_Shell_Mat")
    # Roof slab overhang
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 2.05))
    roof = bpy.context.active_object
    roof.scale = (4.0, 3.2, 0.20)
    bpy.ops.object.transform_apply(scale=True)
    add_material(roof, DARK_STEEL, "Hardpoint_Roof_Mat")
    # Firing slit (dark slot)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, -1.42, 1.40))
    slit = bpy.context.active_object
    slit.scale = (2.4, 0.05, 0.20)
    bpy.ops.object.transform_apply(scale=True)
    add_material(slit, (0.05, 0.05, 0.06, 1.0), "Hardpoint_Slit_Mat")
    # Side sandbag wing
    _sandbag_wall(2.6, -1.6, length=2.0, layers=2)
    # Mounted weapon emplacement on roof
    bpy.ops.mesh.primitive_cylinder_add(radius=0.10, depth=0.4, location=(0, 0, 2.35))
    add_material(bpy.context.active_object, DARK_STEEL, "Hardpoint_MountPost_Mat")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.025, depth=0.7, location=(0.20, 0, 2.45))
    barrel = bpy.context.active_object
    barrel.rotation_euler.y = math.pi / 2
    add_material(barrel, DARK_STEEL, "Hardpoint_Barrel_Mat")
    join_and_rename("SM_VAN_OUT_Hardpoint")


def gen_inner_sanctum():
    """Heavy walls + parapet + flanking towers — elite garrison."""
    clear_scene()
    # Central keep block
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.6))
    keep = bpy.context.active_object
    keep.scale = (5.0, 4.0, 3.2)
    bpy.ops.object.transform_apply(scale=True)
    add_material(keep, STEEL, "Sanctum_Keep_Mat")
    # Crenellations (parapet teeth)
    for i in range(5):
        bpy.ops.mesh.primitive_cube_add(size=1, location=((i - 2) * 1.0, 1.95, 3.40))
        tooth = bpy.context.active_object
        tooth.scale = (0.40, 0.30, 0.40)
        bpy.ops.object.transform_apply(scale=True)
        add_material(tooth, DARK_STEEL, "Sanctum_Crenel_Mat")
    # Flanking towers
    for sx in [-1, 1]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.7, depth=4.4, vertices=8, location=(sx * 2.8, 0, 2.2))
        tower = bpy.context.active_object
        add_material(tower, STEEL, f"Sanctum_Tower_{sx}_Mat")
        # Tower cap
        bpy.ops.mesh.primitive_cone_add(radius1=0.85, radius2=0.0, depth=0.8, vertices=8, location=(sx * 2.8, 0, 4.8))
        add_material(bpy.context.active_object, CRIMSON, f"Sanctum_TowerCap_{sx}_Mat")
    # Front gate (dark slab)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, -2.05, 1.0))
    gate = bpy.context.active_object
    gate.scale = (1.4, 0.10, 1.8)
    bpy.ops.object.transform_apply(scale=True)
    add_material(gate, DARK_STEEL, "Sanctum_Gate_Mat")
    join_and_rename("SM_VAN_OUT_InnerSanctum")


def gen_concordat():
    """The main colony core — central tower + ringed walls + banner mast."""
    clear_scene()
    # Outer ring wall (low ring of 12 segments)
    for i in range(12):
        angle = (i / 12.0) * math.tau
        x = math.cos(angle) * 6.0
        y = math.sin(angle) * 6.0
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 1.0))
        seg = bpy.context.active_object
        seg.scale = (0.4, 1.6, 2.0)
        seg.rotation_euler.z = angle + math.pi / 2
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        add_material(seg, STEEL, "Concordat_Wall_Mat")
    # Central tower
    bpy.ops.mesh.primitive_cylinder_add(radius=2.0, depth=8.0, vertices=8, location=(0, 0, 4.0))
    add_material(bpy.context.active_object, STEEL, "Concordat_Tower_Mat")
    # Tower roof spire
    bpy.ops.mesh.primitive_cone_add(radius1=2.2, radius2=0.0, depth=2.4, vertices=8, location=(0, 0, 9.2))
    add_material(bpy.context.active_object, CRIMSON, "Concordat_Spire_Mat")
    # Inner ring buildings (4 small cubes flanking the tower)
    for sx, sy in [(3.5, 0), (-3.5, 0), (0, 3.5), (0, -3.5)]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(sx, sy, 1.4))
        bld = bpy.context.active_object
        bld.scale = (1.6, 1.6, 2.8)
        bpy.ops.object.transform_apply(scale=True)
        add_material(bld, DARK_STEEL, "Concordat_InnerBldg_Mat")
    # Banner mast on central tower
    bpy.ops.mesh.primitive_cylinder_add(radius=0.05, depth=2.0, location=(0, 0, 11.4))
    add_material(bpy.context.active_object, DARK_STEEL, "Concordat_Mast_Mat")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.4, 0, 11.6))
    flag = bpy.context.active_object
    flag.scale = (0.005, 0.8, 0.6)
    bpy.ops.object.transform_apply(scale=True)
    add_material(flag, CRIMSON, "Concordat_Flag_Mat")
    join_and_rename("SM_VAN_OUT_Concordat")


# ── Shared Props ──────────────────────────────────────────────────────────────

def gen_barricade():
    """Hesco-style barricade segment — single fillable wall block."""
    clear_scene()
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.55))
    body = bpy.context.active_object
    body.scale = (1.2, 0.5, 1.1)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, SAND, "Barricade_Body_Mat")
    # Wire frame outline (4 vertical bars)
    for sx, sy in [(0.6, 0.25), (-0.6, 0.25), (0.6, -0.25), (-0.6, -0.25)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.02, depth=1.10, location=(sx, sy, 0.55))
        add_material(bpy.context.active_object, DARK_STEEL, "Barricade_Frame_Mat")
    # Top wire band
    for sx in [0.6, -0.6]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(sx, 0, 1.10))
        bar = bpy.context.active_object
        bar.scale = (0.04, 0.5, 0.02)
        bpy.ops.object.transform_apply(scale=True)
        add_material(bar, DARK_STEEL, "Barricade_TopBand_Mat")
    join_and_rename("SM_VAN_PRP_Barricade")


def gen_banner():
    """Standalone Vanguard banner on a stake — used as map marker prop."""
    clear_scene()
    bpy.ops.mesh.primitive_cylinder_add(radius=0.03, depth=2.4, location=(0, 0, 1.2))
    add_material(bpy.context.active_object, WOOD, "Banner_Mast_Mat")
    # Crossbar
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 2.20))
    bar = bpy.context.active_object
    bar.scale = (0.50, 0.025, 0.025)
    bpy.ops.object.transform_apply(scale=True)
    add_material(bar, WOOD, "Banner_Cross_Mat")
    # Hanging banner cloth
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.70))
    cloth = bpy.context.active_object
    cloth.scale = (0.005, 0.45, 0.85)
    bpy.ops.object.transform_apply(scale=True)
    add_material(cloth, CRIMSON, "Banner_Cloth_Mat")
    # Inner emblem strip
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.70))
    emb = bpy.context.active_object
    emb.scale = (0.006, 0.18, 0.30)
    bpy.ops.object.transform_apply(scale=True)
    add_material(emb, (0.95, 0.92, 0.88, 1.0), "Banner_Emblem_Mat")
    join_and_rename("SM_VAN_PRP_Banner")


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
    print("\n=== Quiet Rift: Enigma — Vanguard Asset Generator ===")
    for asset_id, gen in GENERATORS.items():
        print(f"\n[{asset_id}]")
        gen()
        out_path = os.path.join(OUTPUT_DIR, f"SM_{asset_id}.fbx")
        export_fbx(asset_id, out_path)
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Import these FBX files into UE5 via Content Browser > Import.")


if __name__ == "__main__":
    main()
