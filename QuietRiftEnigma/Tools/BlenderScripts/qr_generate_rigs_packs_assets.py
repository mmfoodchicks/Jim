"""
Quiet Rift: Enigma — Chest Rigs & Backpacks Asset Generator (Blender 4.x)

Upgraded in Batch 4 of the Blender detail pass — every container flows
through qr_blender_detail.py (palette dedupe, smooth shading, bevels,
smart UV, sockets, UCX collision).

Reads every row in DT_Items_Master.csv whose Category is ChestRig or
Backpack (12 items: 5 rigs + 7 packs added with the Tarkov-style
container system) and exports one bespoke placeholder FBX per row.

Per-item sockets exported into the FBX:
    Chest rigs:  SOCKET_LeftShoulder / SOCKET_RightShoulder / SOCKET_FrontCenter
    Backpacks:   SOCKET_LeftStrap   / SOCKET_RightStrap     / SOCKET_TopHandle
    Both:        SOCKET_HipPoint (where applicable)

Convention:
    Origin at bottom-center of the back panel, +Y points away from
    the wearer's torso (the "front" of the rig/pack), +Z is up.

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
import random
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

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/rigs_packs")
CSV_PATH = os.path.join(
    os.path.dirname(__file__),
    "../../Content/QuietRift/Data/DT_Items_Master.csv",
)

CATEGORIES = {"ChestRig", "Backpack"}


def _add(obj, mat):
    assign_material(obj, mat)
    return obj


# ── Reusable primitives ──────────────────────────────────────────────────────

def _slab(x, y, z, w, d, h, mat, name):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z))
    obj = bpy.context.active_object
    obj.scale = (w, d, h); bpy.ops.object.transform_apply(scale=True)
    _add(obj, mat); obj.name = name
    return obj


def _pouch(x, y, z, w, h, depth=0.06, body_mat=None, flap_mat=None, name="Pouch"):
    if body_mat is None:
        body_mat = palette_material("Canvas")
    if flap_mat is None:
        flap_mat = palette_material("DarkSteel")
    _slab(x, y + depth / 2, z, w, depth, h, body_mat, f"{name}_Body")
    _slab(x, y + depth / 2, z + h / 2 + 0.005, w + 0.01, depth + 0.005, 0.01, flap_mat, f"{name}_Flap")


def _strap(x, z_top, z_bot, width=0.02, depth=0.015, mat=None, name="Strap"):
    if mat is None:
        mat = palette_material("DarkLeather")
    h = z_top - z_bot
    _slab(x, -0.06, (z_top + z_bot) / 2, width, depth, h, mat, name)


def _buckle(x, y, z, w=0.04, h=0.025, mat=None, name="Buckle"):
    if mat is None:
        mat = palette_material("DarkSteel")
    _slab(x, y, z, w, 0.012, h, mat, name)


def _finalize_container(name, *, kind, hip_point=None):
    """kind in {'rig','pack'} — chooses the socket layout."""
    if kind == "rig":
        add_socket("LeftShoulder",  location=(-0.18, -0.04, 0.50))
        add_socket("RightShoulder", location=( 0.18, -0.04, 0.50))
        add_socket("FrontCenter",   location=( 0.00,  0.06, 0.30))
    else:  # pack
        add_socket("LeftStrap",  location=(-0.18, -0.06, 0.65))
        add_socket("RightStrap", location=( 0.18, -0.06, 0.65))
        add_socket("TopHandle",  location=( 0.00, -0.04, 0.85))
    if hip_point is not None:
        add_socket("HipPoint", location=hip_point)
    finalize_asset(name,
                    bevel_width=0.002, bevel_angle_deg=30,
                    smooth_angle_deg=50, collision="convex",
                    lods=[0.50], pivot="bottom_center")


# ── Chest Rig Generators ─────────────────────────────────────────────────────

def gen_rig_fiberweave():
    """T0 4×2 — woven fiber chest harness with one pocket and tied cord."""
    clear_scene()
    canvas_mat = palette_material("Canvas")
    dark_mat = palette_material("DarkCanvas")
    _slab(0, 0, 0.30, 0.40, 0.04, 0.30, canvas_mat, "Fiber_Back")
    _slab(0, 0.04, 0.30, 0.36, 0.02, 0.26, dark_mat, "Fiber_Front")
    _pouch(0, 0.05, 0.22, 0.20, 0.18, depth=0.05, body_mat=canvas_mat,
            flap_mat=dark_mat, name="Fiber_Pocket")
    for sx in [-0.18, 0.18]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.40, location=(sx, -0.05, 0.50))
        cord = bpy.context.active_object
        cord.rotation_euler.x = 0.2
        _add(cord, dark_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.025, location=(0, 0.10, 0.18))
    _add(bpy.context.active_object, dark_mat)
    _finalize_container("SM_RIG_FIBERWEAVE", kind="rig")


def gen_rig_leather_bandolier():
    """T1 5×2 — diagonal leather bandolier with five brass ammo loops."""
    clear_scene()
    leather_mat = palette_material("Leather")
    dark_leather = palette_material("DarkLeather")
    brass_mat = palette_material("Brass")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0.02, 0.30))
    strap = bpy.context.active_object
    strap.scale = (0.50, 0.04, 0.10); strap.rotation_euler.y = 0.5
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    _add(strap, leather_mat)
    for i in range(5):
        x = -0.20 + i * 0.10
        z = 0.18 + i * 0.06
        bpy.ops.mesh.primitive_cylinder_add(radius=0.015, depth=0.08, location=(x, 0.05, z))
        loop = bpy.context.active_object
        loop.rotation_euler.x = math.pi / 2
        _add(loop, brass_mat)
    _pouch(0.18, 0.04, 0.20, 0.10, 0.14, depth=0.05, body_mat=dark_leather,
            flap_mat=palette_material("DarkSteel"), name="Bandolier_Pouch")
    _buckle(-0.22, 0.04, 0.50, name="Bandolier_Buckle")
    _finalize_container("SM_RIG_LEATHER_BANDOLIER", kind="rig")


def gen_rig_scavenger():
    """T1 5×2 — mismatched salvaged-pouch rig with patchwork colors."""
    clear_scene()
    random.seed(42)
    dark_canvas = palette_material("DarkCanvas")
    canvas_mat = palette_material("Canvas")
    leather_mat = palette_material("Leather")
    olive_mat = palette_material("Olive")
    dark_leather = palette_material("DarkLeather")
    _slab(0, 0, 0.28, 0.50, 0.04, 0.26, dark_canvas, "Scav_Back")
    pouch_mats = [canvas_mat, leather_mat, olive_mat, dark_leather, canvas_mat]
    pouch_w = [0.10, 0.08, 0.12, 0.09, 0.08]
    pouch_h = [0.14, 0.18, 0.13, 0.16, 0.15]
    x_cursor = -0.22
    for i, (m, w, h) in enumerate(zip(pouch_mats, pouch_w, pouch_h)):
        z = 0.22 + (i % 2) * 0.02
        _pouch(x_cursor + w / 2, 0.04, z, w, h, depth=0.05, body_mat=m,
                flap_mat=palette_material("DarkSteel"), name=f"Scav_Pouch_{i}")
        x_cursor += w + 0.005
    for sx in [-0.20, 0.20]:
        _strap(sx, 0.50, 0.30, width=0.025, mat=dark_leather, name="Scav_Strap")
    _slab(0.10, 0.06, 0.16, 0.06, 0.005, 0.05, leather_mat, "Scav_Patch")
    _slab(-0.12, 0.06, 0.40, 0.05, 0.005, 0.04, olive_mat, "Scav_Patch2")
    _finalize_container("SM_RIG_SCAVENGER", kind="rig")


def gen_rig_patrol():
    """T2 6×3 — quick-release patrol rig with two rows of mag pouches."""
    clear_scene()
    olive_mat = palette_material("Olive")
    dark_olive = palette_material("DarkOlive")
    _slab(0, 0, 0.30, 0.60, 0.05, 0.36, olive_mat, "Patrol_Back")
    _slab(0, 0.05, 0.30, 0.56, 0.01, 0.32, dark_olive, "Patrol_Backing")
    for row in range(2):
        z = 0.20 + row * 0.14
        for col in range(3):
            x = -0.22 + col * 0.22
            _pouch(x, 0.06, z, 0.16, 0.12, depth=0.06, body_mat=olive_mat,
                    flap_mat=dark_olive, name=f"Patrol_Mag_{row}_{col}")
    _buckle(0, 0.07, 0.48, w=0.06, h=0.04, name="Patrol_QR_Buckle")
    for sx in [-0.18, 0.18]:
        _strap(sx, 0.55, 0.28, width=0.035, mat=dark_olive, name="Patrol_Shoulder_Strap")
    for sx in [-0.30, 0.30]:
        _buckle(sx, 0.04, 0.30, name="Patrol_Side_Buckle")
    # Velcro-strip seam across waist for visual interest
    add_panel_seam_strip((-0.28, 0.06, 0.13), (0.28, 0.06, 0.13),
                          width=0.005, depth=0.002, material_name="DarkOlive",
                          name="Patrol_VelcroSeam")
    _finalize_container("SM_RIG_PATROL", kind="rig", hip_point=(0, 0.05, 0.13))


def gen_rig_recon_plate():
    """T3 7×3 — composite plate carrier with utility pouches and visible plate insert."""
    clear_scene()
    composite_mat = palette_material("Steel")
    dark_plate = palette_material("DarkSteel")
    _slab(0, 0, 0.32, 0.66, 0.06, 0.42, composite_mat, "Recon_Back")
    _slab(0, 0.06, 0.30, 0.30, 0.03, 0.30, dark_plate, "Recon_FrontPlate")
    for col in range(3):
        x = -0.22 + col * 0.22
        _pouch(x, 0.09, 0.46, 0.18, 0.10, depth=0.05, body_mat=dark_plate,
                flap_mat=composite_mat, name=f"Recon_UpUtil_{col}")
    for sx, sgn in [(-0.30, -1), (0.30, 1)]:
        _pouch(sx, 0.07, 0.28, 0.12, 0.16, depth=0.06, body_mat=composite_mat,
                flap_mat=dark_plate, name=f"Recon_SideMag_{sgn}")
    _slab(0, 0.04, 0.16, 0.70, 0.07, 0.06, dark_plate, "Recon_Cummerbund")
    for sx in [-0.22, 0.22]:
        _strap(sx, 0.58, 0.32, width=0.045, depth=0.020, mat=dark_plate, name="Recon_Shoulder")
    _buckle(0, 0.08, 0.52, w=0.07, h=0.05, name="Recon_QR_Buckle")
    _slab(0, -0.05, 0.56, 0.16, 0.04, 0.025, dark_plate, "Recon_DragHandle")
    add_rivet_grid(origin=(-0.10, 0.075, 0.30),
                    spacing=(0.05, 0.0), rows=1, cols=5,
                    rivet_radius=0.0035, depth=0.002, normal_axis='Y',
                    material_name="DarkSteel")
    _finalize_container("SM_RIG_RECON_PLATE", kind="rig", hip_point=(0, 0.05, 0.16))


# ── Backpack Generators ──────────────────────────────────────────────────────

def gen_pack_field_satchel():
    """T0 4×4 — single shoulder satchel with drawstring closure."""
    clear_scene()
    canvas_mat = palette_material("Canvas")
    dark_canvas = palette_material("DarkCanvas")
    leather_mat = palette_material("DarkLeather")
    _slab(0, 0.10, 0.30, 0.32, 0.16, 0.32, canvas_mat, "Satchel_Body")
    _slab(0, 0.10, 0.46, 0.34, 0.18, 0.04, dark_canvas, "Satchel_Flap")
    for i in range(3):
        x = -0.10 + i * 0.10
        bpy.ops.mesh.primitive_torus_add(major_radius=0.015, minor_radius=0.003, location=(x, 0.19, 0.42))
        _add(bpy.context.active_object, leather_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.46))
    strap = bpy.context.active_object
    strap.scale = (0.50, 0.04, 0.025); strap.rotation_euler.y = 0.7
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    _add(strap, leather_mat)
    _buckle(0, 0.20, 0.34, name="Satchel_Buckle")
    _finalize_container("SM_PACK_FIELD_SATCHEL", kind="pack")


def gen_pack_leather_knapsack():
    """T1 4×5 — boiled leather knapsack with drawstring top."""
    clear_scene()
    leather_mat = palette_material("Leather")
    dark_leather = palette_material("DarkLeather")
    _slab(0, 0.12, 0.34, 0.34, 0.20, 0.40, leather_mat, "Knapsack_Body")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.16, depth=0.02, location=(0, 0.12, 0.55))
    _add(bpy.context.active_object, dark_leather)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.04, minor_radius=0.006, location=(0, 0.12, 0.58))
    _add(bpy.context.active_object, dark_leather)
    for sx in [-0.14, 0.14]:
        _strap(sx, 0.54, 0.18, width=0.030, mat=dark_leather, name="Knapsack_Shoulder")
    for i in range(3):
        z = 0.20 + i * 0.10
        for sx in [-0.18, 0.18]:
            _slab(sx, 0.11, z, 0.005, 0.005, 0.025, dark_leather, "Knapsack_Lace")
    _finalize_container("SM_PACK_LEATHER_KNAPSACK", kind="pack")


def gen_pack_scrap_backpack():
    """T1 5×5 — patchwork backpack stitched from salvage."""
    clear_scene()
    random.seed(7)
    dark_canvas = palette_material("DarkCanvas")
    leather_mat = palette_material("Leather")
    olive_mat = palette_material("Olive")
    canvas_mat = palette_material("Canvas")
    dark_leather = palette_material("DarkLeather")
    _slab(0, 0.13, 0.36, 0.40, 0.22, 0.44, dark_canvas, "Scrap_Body")
    patch_mats = [leather_mat, olive_mat, canvas_mat, dark_leather, olive_mat]
    for i, m in enumerate(patch_mats):
        x = random.uniform(-0.18, 0.18); z = random.uniform(0.20, 0.50)
        w = random.uniform(0.06, 0.10); h = random.uniform(0.05, 0.09)
        _slab(x, 0.245, z, w, 0.005, h, m, f"Scrap_Patch_{i}")
    for sx in [-0.16, 0.16]:
        _strap(sx, 0.58, 0.22, width=0.035, mat=dark_leather, name="Scrap_Shoulder")
    _buckle(0, 0.245, 0.54, w=0.05, h=0.035, name="Scrap_TopBuckle")
    for sx in [-0.21, 0.21]:
        _slab(sx, 0.13, 0.30, 0.012, 0.005, 0.18, dark_leather, "Scrap_SideStrap")
        _buckle(sx, 0.13, 0.30, name="Scrap_SideBuckle")
    _finalize_container("SM_PACK_SCRAP_BACKPACK", kind="pack")


def gen_pack_patrol_rucksack():
    """T2 6×6 — reinforced patrol rucksack with frame stays + side pockets + hip belt."""
    clear_scene()
    olive_mat = palette_material("Olive")
    dark_olive = palette_material("DarkOlive")
    composite_mat = palette_material("Steel")
    _slab(0, 0.14, 0.40, 0.46, 0.24, 0.52, olive_mat, "Patrol_Body")
    _slab(0, 0.14, 0.66, 0.42, 0.20, 0.08, dark_olive, "Patrol_TopComp")
    for sx, sgn in [(-0.27, -1), (0.27, 1)]:
        _slab(sx, 0.16, 0.34, 0.08, 0.20, 0.30, dark_olive, f"Patrol_SidePocket_{sgn}")
    for sx in [-0.10, 0.10]:
        _slab(sx, -0.02, 0.40, 0.018, 0.025, 0.50, composite_mat, "Patrol_FrameStay")
    for sx in [-0.16, 0.16]:
        _strap(sx, 0.66, 0.24, width=0.045, depth=0.025, mat=dark_olive, name="Patrol_Shoulder")
    _slab(0, 0.04, 0.16, 0.50, 0.06, 0.05, dark_olive, "Patrol_HipBelt")
    _buckle(0, 0.08, 0.16, w=0.05, h=0.03, name="Patrol_HipBuckle")
    for sx in [-0.10, 0.10]:
        _buckle(sx, 0.245, 0.66, name="Patrol_TopBuckle")
    _finalize_container("SM_PACK_PATROL_RUCKSACK", kind="pack",
                         hip_point=(0, 0.07, 0.16))


def gen_pack_outpost_hauler():
    """T2 7×6 — big-capacity hauler with extensive side strap kit slots."""
    clear_scene()
    olive_mat = palette_material("Olive")
    dark_olive = palette_material("DarkOlive")
    _slab(0, 0.16, 0.42, 0.54, 0.28, 0.56, olive_mat, "Hauler_Body")
    _slab(0, 0.16, 0.72, 0.58, 0.30, 0.06, dark_olive, "Hauler_Lid")
    for i in range(3):
        z = 0.30 + i * 0.10
        _slab(0, 0.30, z, 0.46, 0.005, 0.025, dark_olive, f"Hauler_MOLLE_{i}")
    for sx, sgn in [(-0.30, -1), (0.30, 1)]:
        for i in range(4):
            z = 0.22 + i * 0.10
            _slab(sx, 0.16, z, 0.025, 0.30, 0.05, dark_olive, f"Hauler_KitSlot_{sgn}_{i}")
            _buckle(sx + sgn * 0.012, 0.30, z, w=0.025, h=0.025, name=f"Hauler_KitBuckle_{sgn}_{i}")
    for sx in [-0.18, 0.18]:
        _strap(sx, 0.72, 0.26, width=0.050, depth=0.030, mat=dark_olive, name="Hauler_Shoulder")
    _slab(0, 0.05, 0.18, 0.58, 0.07, 0.06, dark_olive, "Hauler_HipBelt")
    _buckle(0, 0.10, 0.18, w=0.06, h=0.035, name="Hauler_HipBuckle")
    _finalize_container("SM_PACK_OUTPOST_HAULER", kind="pack",
                         hip_point=(0, 0.09, 0.18))


def gen_pack_vanguard_assault():
    """T3 6×7 — Vanguard pattern assault pack with composite back panel + crimson accent."""
    clear_scene()
    composite_mat = palette_material("Steel")
    dark_plate = palette_material("DarkSteel")
    crimson_mat = palette_material("VanguardRed")
    light_emblem = get_or_create_material("PackEmblem_Light", (0.95, 0.92, 0.88, 1.0), roughness=0.6)
    _slab(0, -0.03, 0.42, 0.46, 0.04, 0.62, composite_mat, "Vanguard_BackPanel")
    _slab(0, 0.14, 0.42, 0.42, 0.22, 0.60, dark_plate, "Vanguard_Body")
    _slab(0, 0.255, 0.42, 0.04, 0.005, 0.55, crimson_mat, "Vanguard_Accent")
    _slab(0, 0.20, 0.66, 0.30, 0.10, 0.10, composite_mat, "Vanguard_TopPocket")
    for sx, sgn in [(-0.25, -1), (0.25, 1)]:
        for i in range(2):
            z = 0.30 + i * 0.18
            _pouch(sx, 0.06, z, 0.10, 0.16, depth=0.05, body_mat=dark_plate,
                    flap_mat=composite_mat, name=f"Vanguard_Mag_{sgn}_{i}")
    for sx in [-0.16, 0.16]:
        _strap(sx, 0.74, 0.24, width=0.055, depth=0.030, mat=composite_mat, name="Vanguard_Shoulder")
    _slab(0, -0.04, 0.55, 0.24, 0.020, 0.018, composite_mat, "Vanguard_SternumStrap")
    _slab(0, 0.04, 0.16, 0.50, 0.06, 0.06, composite_mat, "Vanguard_HipBelt")
    _buckle(0, 0.08, 0.16, w=0.06, h=0.04, name="Vanguard_HipBuckle")
    _slab(0, 0.10, 0.16, 0.04, 0.005, 0.025, light_emblem, "Vanguard_Emblem")
    _finalize_container("SM_PACK_VANGUARD_ASSAULT", kind="pack",
                         hip_point=(0, 0.07, 0.16))


def gen_pack_expedition_longhaul():
    """T3 8×8 — frame pack tuned for multi-day expeditions."""
    clear_scene()
    composite_mat = palette_material("Steel")
    olive_mat = palette_material("Olive")
    dark_olive = palette_material("DarkOlive")
    dark_leather = palette_material("DarkLeather")
    canvas_mat = palette_material("Canvas")
    for sx in [-0.16, 0.16]:
        _slab(sx, -0.06, 0.46, 0.025, 0.030, 0.80, composite_mat, "Expedition_FrameRail")
    for z in [0.10, 0.85]:
        _slab(0, -0.06, z, 0.36, 0.025, 0.025, composite_mat, "Expedition_FrameCross")
    _slab(0, 0.18, 0.50, 0.50, 0.30, 0.70, olive_mat, "Expedition_Body")
    _slab(0, 0.18, 0.86, 0.54, 0.32, 0.06, dark_olive, "Expedition_Lid")
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.20, location=(0, 0.18, 0.86))
    dome = bpy.context.active_object
    dome.scale = (1.0, 1.0, 0.4); bpy.ops.object.transform_apply(scale=True)
    _add(dome, dark_olive)
    for sx, sgn in [(-0.28, -1), (0.28, 1)]:
        for i in range(4):
            z = 0.30 + i * 0.14
            _slab(sx, 0.18, z, 0.018, 0.32, 0.04, dark_olive, f"Expedition_Cinch_{sgn}_{i}")
            _buckle(sx + sgn * 0.012, 0.34, z, w=0.025, h=0.025, name=f"Expedition_CinchBuckle_{sgn}_{i}")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.10, depth=0.46, location=(0, 0.18, 0.96))
    bedroll = bpy.context.active_object
    bedroll.rotation_euler.y = math.pi / 2
    _add(bedroll, canvas_mat)
    for sx in [-0.16, 0.16]:
        bpy.ops.mesh.primitive_torus_add(major_radius=0.11, minor_radius=0.005, location=(sx, 0.18, 0.96))
        lash = bpy.context.active_object
        lash.rotation_euler.y = math.pi / 2
        _add(lash, dark_leather)
    for sx in [-0.18, 0.18]:
        _strap(sx, 0.85, 0.30, width=0.060, depth=0.035, mat=dark_olive, name="Expedition_Shoulder")
    _slab(0, 0.06, 0.18, 0.62, 0.08, 0.07, dark_olive, "Expedition_HipBelt")
    _buckle(0, 0.11, 0.18, w=0.07, h=0.045, name="Expedition_HipBuckle")
    _finalize_container("SM_PACK_EXPEDITION_LONGHAUL", kind="pack",
                         hip_point=(0, 0.10, 0.18))


# ── Dispatch ──────────────────────────────────────────────────────────────────

GENERATORS = {
    "RIG_FIBERWEAVE":            gen_rig_fiberweave,
    "RIG_LEATHER_BANDOLIER":     gen_rig_leather_bandolier,
    "RIG_SCAVENGER":             gen_rig_scavenger,
    "RIG_PATROL":                gen_rig_patrol,
    "RIG_RECON_PLATE":           gen_rig_recon_plate,
    "PACK_FIELD_SATCHEL":        gen_pack_field_satchel,
    "PACK_LEATHER_KNAPSACK":     gen_pack_leather_knapsack,
    "PACK_SCRAP_BACKPACK":       gen_pack_scrap_backpack,
    "PACK_PATROL_RUCKSACK":      gen_pack_patrol_rucksack,
    "PACK_OUTPOST_HAULER":       gen_pack_outpost_hauler,
    "PACK_VANGUARD_ASSAULT":     gen_pack_vanguard_assault,
    "PACK_EXPEDITION_LONGHAUL":  gen_pack_expedition_longhaul,
}


def main():
    print("\n=== Quiet Rift: Enigma — Rigs & Packs Asset Generator (Batch 4 detail upgrade) ===")
    csv_abs = os.path.abspath(CSV_PATH)
    if not os.path.isfile(csv_abs):
        print(f"ERROR: CSV not found at {csv_abs}")
        return
    with open(csv_abs, newline='', encoding='utf-8') as f:
        rows = [r for r in csv.DictReader(f) if r.get("Category", "").strip() in CATEGORIES]
    missing = [r["ItemId"] for r in rows if r["ItemId"] not in GENERATORS]
    if missing:
        print(f"WARN: no generator registered for: {missing}")
    for row in rows:
        rid = row["ItemId"].strip()
        gen = GENERATORS.get(rid)
        if gen is None:
            continue
        print(f"\n[{rid}] {row.get('Item', rid)} ({row.get('Category', '?')})")
        gen()
        out_path = os.path.join(OUTPUT_DIR, f"SM_{rid}.fbx")
        export_fbx(rid, out_path)
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX exports: SM_<id> mesh, UCX_ collision, LOD1, plus")
    print("SOCKET_LeftShoulder/RightShoulder/FrontCenter (rigs) or")
    print("SOCKET_LeftStrap/RightStrap/TopHandle (packs), and SOCKET_HipPoint where applicable.")


if __name__ == "__main__":
    main()
