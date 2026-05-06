"""
Quiet Rift: Enigma — Chest Rigs & Backpacks Asset Generator (Blender 4.x)

Run this in Blender's Scripting workspace or via:
    blender --background --python qr_generate_rigs_packs_assets.py

Reads every row in DT_Items_Master.csv whose Category is ChestRig or
Backpack (12 items total — 5 rigs + 7 packs added with the Tarkov-style
container system) and exports one bespoke placeholder FBX per row to
Content/Meshes/rigs_packs/. Sized for UE5 import (1 UU = 1 cm).

Each silhouette is composed of:
- A back panel sized roughly like the item's grid footprint
- A front face suggesting pouches / straps / closures
- Distinguishing details for tier (more straps + plates as tier rises)

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
    add_material,
    join_and_rename,
)

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/rigs_packs")
CSV_PATH = os.path.join(
    os.path.dirname(__file__),
    "../../Content/QuietRift/Data/DT_Items_Master.csv",
)

CATEGORIES = {"ChestRig", "Backpack"}

# Palette
CANVAS       = (0.62, 0.55, 0.42, 1.0)
DARK_CANVAS  = (0.40, 0.36, 0.28, 1.0)
LEATHER      = (0.42, 0.28, 0.18, 1.0)
DARK_LEATHER = (0.28, 0.18, 0.10, 1.0)
OLIVE        = (0.35, 0.40, 0.25, 1.0)
DARK_OLIVE   = (0.22, 0.26, 0.16, 1.0)
COMPOSITE    = (0.22, 0.22, 0.25, 1.0)
DARK_PLATE   = (0.13, 0.13, 0.15, 1.0)
VANGUARD_RED = (0.55, 0.10, 0.12, 1.0)
BUCKLE       = (0.25, 0.25, 0.28, 1.0)
BRASS        = (0.65, 0.50, 0.20, 1.0)


# ── Reusable primitives ──────────────────────────────────────────────────────

def _slab(x, y, z, w, d, h, color, name):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z))
    obj = bpy.context.active_object
    obj.scale = (w, d, h)
    bpy.ops.object.transform_apply(scale=True)
    add_material(obj, color, name)
    return obj


def _pouch(x, y, z, w, h, depth=0.06, color=CANVAS, name="Pouch"):
    """Small front-facing pouch on a rig/pack panel."""
    _slab(x, y + depth / 2, z, w, depth, h, color, f"{name}_Body")
    # Top flap (slight overhang)
    _slab(x, y + depth / 2, z + h / 2 + 0.005, w + 0.01, depth + 0.005, 0.01, BUCKLE, f"{name}_Flap")


def _strap(x, z_top, z_bot, width=0.02, depth=0.015, color=DARK_LEATHER, name="Strap"):
    """Vertical shoulder strap rendered as a thin slab from z_top to z_bot."""
    h = z_top - z_bot
    _slab(x, -0.06, (z_top + z_bot) / 2, width, depth, h, color, name)


def _buckle(x, y, z, w=0.04, h=0.025, name="Buckle"):
    _slab(x, y, z, w, 0.012, h, BUCKLE, name)


# ── Chest Rig Generators ─────────────────────────────────────────────────────

def gen_rig_fiberweave():
    """T0 4×2 — woven fiber chest harness with one stitched pocket and tied cord."""
    clear_scene()
    # Back panel
    _slab(0, 0, 0.30, 0.40, 0.04, 0.30, CANVAS, "Fiber_Back")
    # Front panel (slight inset)
    _slab(0, 0.04, 0.30, 0.36, 0.02, 0.26, DARK_CANVAS, "Fiber_Front")
    # Single stitched pocket
    _pouch(0, 0.05, 0.22, 0.20, 0.18, depth=0.05, color=CANVAS, name="Fiber_Pocket")
    # Shoulder cords (round cylinders for variety)
    for sx in [-0.18, 0.18]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.40, location=(sx, -0.05, 0.50))
        cord = bpy.context.active_object
        cord.rotation_euler.x = 0.2
        add_material(cord, DARK_CANVAS, "Fiber_Cord_Mat")
    # Tie knot at front center
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.025, location=(0, 0.10, 0.18))
    add_material(bpy.context.active_object, DARK_CANVAS, "Fiber_Knot_Mat")
    join_and_rename("SM_RIG_FIBERWEAVE")


def gen_rig_leather_bandolier():
    """T1 5×2 — diagonal leather bandolier with five small ammo loops."""
    clear_scene()
    # Diagonal strap (long slab rotated)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0.02, 0.30))
    strap = bpy.context.active_object
    strap.scale = (0.50, 0.04, 0.10)
    strap.rotation_euler.y = 0.5
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    add_material(strap, LEATHER, "Bandolier_Strap_Mat")
    # Five ammo loops along the strap
    for i in range(5):
        x = -0.20 + i * 0.10
        z = 0.18 + i * 0.06
        bpy.ops.mesh.primitive_cylinder_add(radius=0.015, depth=0.08, location=(x, 0.05, z))
        loop = bpy.context.active_object
        loop.rotation_euler.x = math.pi / 2
        add_material(loop, BRASS, "Bandolier_Loop_Mat")
    # Side pouch
    _pouch(0.18, 0.04, 0.20, 0.10, 0.14, depth=0.05, color=DARK_LEATHER, name="Bandolier_Pouch")
    # Buckle at shoulder end
    _buckle(-0.22, 0.04, 0.50, name="Bandolier_Buckle")
    join_and_rename("SM_RIG_LEATHER_BANDOLIER")


def gen_rig_scavenger():
    """T1 5×2 — mismatched salvaged-pouch rig with patchwork colors."""
    clear_scene()
    random.seed(42)
    # Back panel
    _slab(0, 0, 0.28, 0.50, 0.04, 0.26, DARK_CANVAS, "Scav_Back")
    # 5 mismatched front pouches
    pouch_colors = [CANVAS, LEATHER, OLIVE, DARK_LEATHER, CANVAS]
    pouch_w = [0.10, 0.08, 0.12, 0.09, 0.08]
    pouch_h = [0.14, 0.18, 0.13, 0.16, 0.15]
    x_cursor = -0.22
    for i, (c, w, h) in enumerate(zip(pouch_colors, pouch_w, pouch_h)):
        z = 0.22 + (i % 2) * 0.02
        _pouch(x_cursor + w / 2, 0.04, z, w, h, depth=0.05, color=c, name=f"Scav_Pouch_{i}")
        x_cursor += w + 0.005
    # Tied straps + visible patches
    for sx in [-0.20, 0.20]:
        _strap(sx, 0.50, 0.30, width=0.025, color=DARK_LEATHER, name="Scav_Strap")
    # Random patch repair
    _slab(0.10, 0.06, 0.16, 0.06, 0.005, 0.05, LEATHER, "Scav_Patch")
    _slab(-0.12, 0.06, 0.40, 0.05, 0.005, 0.04, OLIVE, "Scav_Patch2")
    join_and_rename("SM_RIG_SCAVENGER")


def gen_rig_patrol():
    """T2 6×3 — quick-release patrol rig with two rows of mag pouches."""
    clear_scene()
    # Back panel
    _slab(0, 0, 0.30, 0.60, 0.05, 0.36, OLIVE, "Patrol_Back")
    # Front MOLLE-style strip backing
    _slab(0, 0.05, 0.30, 0.56, 0.01, 0.32, DARK_OLIVE, "Patrol_Backing")
    # Two rows × 3 mag pouches
    for row in range(2):
        z = 0.20 + row * 0.14
        for col in range(3):
            x = -0.22 + col * 0.22
            _pouch(x, 0.06, z, 0.16, 0.12, depth=0.06, color=OLIVE, name=f"Patrol_Mag_{row}_{col}")
    # Quick-release buckle (center top)
    _buckle(0, 0.07, 0.48, w=0.06, h=0.04, name="Patrol_QR_Buckle")
    # Shoulder straps
    for sx in [-0.18, 0.18]:
        _strap(sx, 0.55, 0.28, width=0.035, color=DARK_OLIVE, name="Patrol_Shoulder_Strap")
    # Side compression buckles
    for sx in [-0.30, 0.30]:
        _buckle(sx, 0.04, 0.30, name="Patrol_Side_Buckle")
    join_and_rename("SM_RIG_PATROL")


def gen_rig_recon_plate():
    """T3 7×3 — composite plate carrier with utility pouches and visible armor plate insert."""
    clear_scene()
    # Back panel (composite)
    _slab(0, 0, 0.32, 0.66, 0.06, 0.42, COMPOSITE, "Recon_Back")
    # Front armor plate (visible darker rectangle)
    _slab(0, 0.06, 0.30, 0.30, 0.03, 0.30, DARK_PLATE, "Recon_FrontPlate")
    # MOLLE strip with utility pouches (3 pouches above plate, 3 below)
    for col in range(3):
        x = -0.22 + col * 0.22
        # Upper utility pouches
        _pouch(x, 0.09, 0.46, 0.18, 0.10, depth=0.05, color=DARK_PLATE, name=f"Recon_UpUtil_{col}")
    # Side mag pouches
    for sx, sgn in [(-0.30, -1), (0.30, 1)]:
        _pouch(sx, 0.07, 0.28, 0.12, 0.16, depth=0.06, color=COMPOSITE, name=f"Recon_SideMag_{sgn}")
    # Cummerbund wrap (thick band around lower edge)
    _slab(0, 0.04, 0.16, 0.70, 0.07, 0.06, DARK_PLATE, "Recon_Cummerbund")
    # Shoulder straps with pads
    for sx in [-0.22, 0.22]:
        _strap(sx, 0.58, 0.32, width=0.045, depth=0.020, color=DARK_PLATE, name="Recon_Shoulder_Strap")
    # Quick-release center buckle
    _buckle(0, 0.08, 0.52, w=0.07, h=0.05, name="Recon_QR_Buckle")
    # Drag handle (thick band on top)
    _slab(0, -0.05, 0.56, 0.16, 0.04, 0.025, DARK_PLATE, "Recon_DragHandle")
    join_and_rename("SM_RIG_RECON_PLATE")


# ── Backpack Generators ──────────────────────────────────────────────────────

def gen_pack_field_satchel():
    """T0 4×4 — single shoulder satchel with drawstring closure."""
    clear_scene()
    # Body (rectangular bag)
    _slab(0, 0.10, 0.30, 0.32, 0.16, 0.32, CANVAS, "Satchel_Body")
    # Top flap
    _slab(0, 0.10, 0.46, 0.34, 0.18, 0.04, DARK_CANVAS, "Satchel_Flap")
    # Drawstring (thin loops at front)
    for i in range(3):
        x = -0.10 + i * 0.10
        bpy.ops.mesh.primitive_torus_add(major_radius=0.015, minor_radius=0.003,
                                         location=(x, 0.19, 0.42))
        add_material(bpy.context.active_object, DARK_LEATHER, "Satchel_String_Mat")
    # Single shoulder strap (long diagonal)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.46))
    strap = bpy.context.active_object
    strap.scale = (0.50, 0.04, 0.025)
    strap.rotation_euler.y = 0.7
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    add_material(strap, DARK_LEATHER, "Satchel_Strap_Mat")
    # Front buckle
    _buckle(0, 0.20, 0.34, name="Satchel_Buckle")
    join_and_rename("SM_PACK_FIELD_SATCHEL")


def gen_pack_leather_knapsack():
    """T1 4×5 — boiled leather knapsack with drawstring top."""
    clear_scene()
    # Body
    _slab(0, 0.12, 0.34, 0.34, 0.20, 0.40, LEATHER, "Knapsack_Body")
    # Drawstring opening (top ring darker)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.16, depth=0.02, location=(0, 0.12, 0.55))
    add_material(bpy.context.active_object, DARK_LEATHER, "Knapsack_Top_Mat")
    # Drawstring cord (loop on top)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.04, minor_radius=0.006, location=(0, 0.12, 0.58))
    add_material(bpy.context.active_object, DARK_LEATHER, "Knapsack_Drawstring_Mat")
    # Two shoulder straps
    for sx in [-0.14, 0.14]:
        _strap(sx, 0.54, 0.18, width=0.030, color=DARK_LEATHER, name="Knapsack_Shoulder")
    # Side cinch lacing (3 small Xs)
    for i in range(3):
        z = 0.20 + i * 0.10
        for sx in [-0.18, 0.18]:
            _slab(sx, 0.11, z, 0.005, 0.005, 0.025, DARK_LEATHER, "Knapsack_Lace")
    join_and_rename("SM_PACK_LEATHER_KNAPSACK")


def gen_pack_scrap_backpack():
    """T1 5×5 — patchwork backpack stitched from salvage pieces."""
    clear_scene()
    random.seed(7)
    # Body
    _slab(0, 0.13, 0.36, 0.40, 0.22, 0.44, DARK_CANVAS, "Scrap_Body")
    # Patches in mismatched colors
    patch_colors = [LEATHER, OLIVE, CANVAS, DARK_LEATHER, OLIVE]
    for i, c in enumerate(patch_colors):
        x = random.uniform(-0.18, 0.18)
        z = random.uniform(0.20, 0.50)
        w = random.uniform(0.06, 0.10)
        h = random.uniform(0.05, 0.09)
        _slab(x, 0.245, z, w, 0.005, h, c, f"Scrap_Patch_{i}")
    # Shoulder straps
    for sx in [-0.16, 0.16]:
        _strap(sx, 0.58, 0.22, width=0.035, color=DARK_LEATHER, name="Scrap_Shoulder")
    # Top buckle closure
    _buckle(0, 0.245, 0.54, w=0.05, h=0.035, name="Scrap_TopBuckle")
    # Side cinch straps with buckles
    for sx in [-0.21, 0.21]:
        _slab(sx, 0.13, 0.30, 0.012, 0.005, 0.18, DARK_LEATHER, "Scrap_SideStrap")
        _buckle(sx, 0.13, 0.30, name="Scrap_SideBuckle")
    join_and_rename("SM_PACK_SCRAP_BACKPACK")


def gen_pack_patrol_rucksack():
    """T2 6×6 — reinforced patrol rucksack with frame stays and side pockets."""
    clear_scene()
    # Main body
    _slab(0, 0.14, 0.40, 0.46, 0.24, 0.52, OLIVE, "Patrol_Body")
    # Top compartment (slight bulge)
    _slab(0, 0.14, 0.66, 0.42, 0.20, 0.08, DARK_OLIVE, "Patrol_TopComp")
    # Two side pockets
    for sx, sgn in [(-0.27, -1), (0.27, 1)]:
        _slab(sx, 0.16, 0.34, 0.08, 0.20, 0.30, DARK_OLIVE, f"Patrol_SidePocket_{sgn}")
    # Frame stays (visible vertical bars on the back)
    for sx in [-0.10, 0.10]:
        _slab(sx, -0.02, 0.40, 0.018, 0.025, 0.50, COMPOSITE, "Patrol_FrameStay")
    # Shoulder straps with padding
    for sx in [-0.16, 0.16]:
        _strap(sx, 0.66, 0.24, width=0.045, depth=0.025, color=DARK_OLIVE, name="Patrol_Shoulder")
    # Hip belt
    _slab(0, 0.04, 0.16, 0.50, 0.06, 0.05, DARK_OLIVE, "Patrol_HipBelt")
    _buckle(0, 0.08, 0.16, w=0.05, h=0.03, name="Patrol_HipBuckle")
    # Top compression buckles
    for sx in [-0.10, 0.10]:
        _buckle(sx, 0.245, 0.66, name="Patrol_TopBuckle")
    join_and_rename("SM_PACK_PATROL_RUCKSACK")


def gen_pack_outpost_hauler():
    """T2 7×6 — big-capacity hauler with extensive side strap kit slots."""
    clear_scene()
    # Wider body
    _slab(0, 0.16, 0.42, 0.54, 0.28, 0.56, OLIVE, "Hauler_Body")
    # Lid with overhang
    _slab(0, 0.16, 0.72, 0.58, 0.30, 0.06, DARK_OLIVE, "Hauler_Lid")
    # MOLLE webbing rows on front (3 horizontal stripes)
    for i in range(3):
        z = 0.30 + i * 0.10
        _slab(0, 0.30, z, 0.46, 0.005, 0.025, DARK_OLIVE, f"Hauler_MOLLE_{i}")
    # Side strap kit slots (4 small slots on each side)
    for sx, sgn in [(-0.30, -1), (0.30, 1)]:
        for i in range(4):
            z = 0.22 + i * 0.10
            _slab(sx, 0.16, z, 0.025, 0.30, 0.05, DARK_OLIVE, f"Hauler_KitSlot_{sgn}_{i}")
            _buckle(sx + sgn * 0.012, 0.30, z, w=0.025, h=0.025, name=f"Hauler_KitBuckle_{sgn}_{i}")
    # Shoulder straps
    for sx in [-0.18, 0.18]:
        _strap(sx, 0.72, 0.26, width=0.050, depth=0.030, color=DARK_OLIVE, name="Hauler_Shoulder")
    # Heavy-duty hip belt
    _slab(0, 0.05, 0.18, 0.58, 0.07, 0.06, DARK_OLIVE, "Hauler_HipBelt")
    _buckle(0, 0.10, 0.18, w=0.06, h=0.035, name="Hauler_HipBuckle")
    join_and_rename("SM_PACK_OUTPOST_HAULER")


def gen_pack_vanguard_assault():
    """T3 6×7 — Vanguard pattern assault pack with composite back panel + crimson accent."""
    clear_scene()
    # Composite back panel
    _slab(0, -0.03, 0.42, 0.46, 0.04, 0.62, COMPOSITE, "Vanguard_BackPanel")
    # Body
    _slab(0, 0.14, 0.42, 0.42, 0.22, 0.60, DARK_PLATE, "Vanguard_Body")
    # Crimson Vanguard accent stripe (vertical center)
    _slab(0, 0.255, 0.42, 0.04, 0.005, 0.55, VANGUARD_RED, "Vanguard_Accent")
    # Top zip pocket
    _slab(0, 0.20, 0.66, 0.30, 0.10, 0.10, COMPOSITE, "Vanguard_TopPocket")
    # Side magazine pouches (compact, 2 each side)
    for sx, sgn in [(-0.25, -1), (0.25, 1)]:
        for i in range(2):
            z = 0.30 + i * 0.18
            _pouch(sx, 0.06, z, 0.10, 0.16, depth=0.05, color=DARK_PLATE, name=f"Vanguard_Mag_{sgn}_{i}")
    # Padded shoulder straps
    for sx in [-0.16, 0.16]:
        _strap(sx, 0.74, 0.24, width=0.055, depth=0.030, color=COMPOSITE, name="Vanguard_Shoulder")
    # Sternum strap
    _slab(0, -0.04, 0.55, 0.24, 0.020, 0.018, COMPOSITE, "Vanguard_SternumStrap")
    # Hip belt with red Vanguard buckle
    _slab(0, 0.04, 0.16, 0.50, 0.06, 0.06, COMPOSITE, "Vanguard_HipBelt")
    _buckle(0, 0.08, 0.16, w=0.06, h=0.04, name="Vanguard_HipBuckle")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0.10, 0.16))
    emblem = bpy.context.active_object
    emblem.scale = (0.04, 0.005, 0.025)
    bpy.ops.object.transform_apply(scale=True)
    add_material(emblem, VANGUARD_RED, "Vanguard_Emblem_Mat")
    join_and_rename("SM_PACK_VANGUARD_ASSAULT")


def gen_pack_expedition_longhaul():
    """T3 8×8 — frame pack tuned for multi-day expeditions, with bedroll loops + external attachments."""
    clear_scene()
    # External frame (visible vertical rails on the back)
    for sx in [-0.16, 0.16]:
        _slab(sx, -0.06, 0.46, 0.025, 0.030, 0.80, COMPOSITE, "Expedition_FrameRail")
    # Frame crossbars
    for z in [0.10, 0.85]:
        _slab(0, -0.06, z, 0.36, 0.025, 0.025, COMPOSITE, "Expedition_FrameCross")
    # Main body (taller and deeper)
    _slab(0, 0.18, 0.50, 0.50, 0.30, 0.70, OLIVE, "Expedition_Body")
    # Lid
    _slab(0, 0.18, 0.86, 0.54, 0.32, 0.06, DARK_OLIVE, "Expedition_Lid")
    # Top compression dome
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.20, location=(0, 0.18, 0.86))
    dome = bpy.context.active_object
    dome.scale = (1.0, 1.0, 0.4)
    bpy.ops.object.transform_apply(scale=True)
    add_material(dome, DARK_OLIVE, "Expedition_Dome_Mat")
    # Side cinch straps with bedroll attachment loops
    for sx, sgn in [(-0.28, -1), (0.28, 1)]:
        for i in range(4):
            z = 0.30 + i * 0.14
            _slab(sx, 0.18, z, 0.018, 0.32, 0.04, DARK_OLIVE, f"Expedition_Cinch_{sgn}_{i}")
            _buckle(sx + sgn * 0.012, 0.34, z, w=0.025, h=0.025,
                    name=f"Expedition_CinchBuckle_{sgn}_{i}")
    # Bedroll on top (cylinder lashed across the lid)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.10, depth=0.46, location=(0, 0.18, 0.96))
    bedroll = bpy.context.active_object
    bedroll.rotation_euler.y = math.pi / 2
    add_material(bedroll, CANVAS, "Expedition_Bedroll_Mat")
    # Bedroll lashing straps
    for sx in [-0.16, 0.16]:
        bpy.ops.mesh.primitive_torus_add(major_radius=0.11, minor_radius=0.005,
                                         location=(sx, 0.18, 0.96))
        lash = bpy.context.active_object
        lash.rotation_euler.y = math.pi / 2
        add_material(lash, DARK_LEATHER, "Expedition_Lashing_Mat")
    # Padded shoulder straps with load lifters
    for sx in [-0.18, 0.18]:
        _strap(sx, 0.85, 0.30, width=0.060, depth=0.035, color=DARK_OLIVE, name="Expedition_Shoulder")
    # Hip belt (heavy duty)
    _slab(0, 0.06, 0.18, 0.62, 0.08, 0.07, DARK_OLIVE, "Expedition_HipBelt")
    _buckle(0, 0.11, 0.18, w=0.07, h=0.045, name="Expedition_HipBuckle")
    join_and_rename("SM_PACK_EXPEDITION_LONGHAUL")


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
    print("\n=== Quiet Rift: Enigma — Rigs & Packs Asset Generator ===")
    csv_abs = os.path.abspath(CSV_PATH)
    if not os.path.isfile(csv_abs):
        print(f"ERROR: CSV not found at {csv_abs}")
        return

    with open(csv_abs, newline='', encoding='utf-8') as f:
        rows = [r for r in csv.DictReader(f)
                if r.get("Category", "").strip() in CATEGORIES]

    print(f"Container rows to generate: {len(rows)}")
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
    print("Import these FBX files into UE5 via Content Browser > Import.")


if __name__ == "__main__":
    main()
