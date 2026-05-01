"""
Quiet Rift: Enigma — Handheld Items Procedural Asset Generator (Blender 4.x)

Run this in Blender's Scripting workspace or via:
    blender --background --python qr_generate_items_handheld_assets.py

Reads every "handheld" row in DT_Items_Master.csv (Equipment, Consumable,
Item, Tech, Leadership, Weapon Kit, Equipment/Logistics, Equipment/Armor,
Component/Container) and exports one placeholder mesh per row using the
helpers in qr_blender_common.py. Each silhouette is parameterized by an
ItemId substring match so similar items share a template but stay
distinguishable. Sized for UE5 import (1 UU = 1 cm).

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
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/items_handheld")
CSV_PATH = os.path.join(
    os.path.dirname(__file__),
    "../../Content/QuietRift/Data/DT_Items_Master.csv",
)

HANDHELD_CATEGORIES = {
    "Equipment", "Consumable", "Item", "Tech", "Leadership", "Weapon Kit",
    "Equipment / Logistics", "Equipment / Armor", "Component / Container",
}

CATEGORY_COLORS = {
    "Consumable":              (0.55, 0.60, 0.65, 1.0),
    "Equipment":               (0.35, 0.30, 0.25, 1.0),
    "Item":                    (0.45, 0.45, 0.48, 1.0),
    "Tech":                    (0.30, 0.65, 0.75, 1.0),
    "Leadership":              (0.85, 0.70, 0.30, 1.0),
    "Weapon Kit":              (0.40, 0.45, 0.30, 1.0),
    "Equipment / Logistics":   (0.55, 0.40, 0.25, 1.0),
    "Equipment / Armor":       (0.40, 0.30, 0.20, 1.0),
    "Component / Container":   (0.50, 0.40, 0.30, 1.0),
}


def _color(category):
    return CATEGORY_COLORS.get(category.strip(), (0.5, 0.5, 0.5, 1.0))


# ── Template Builders ─────────────────────────────────────────────────────────

def build_cyl(item_id, color, radius=0.04, depth=0.16, cap="none"):
    """Cylinder with optional cap. cap in {none, dome, cone, nozzle}."""
    clear_scene()
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=depth, location=(0, 0, depth / 2))
    add_material(bpy.context.active_object, color, f"{item_id}_Body_Mat")
    if cap == "dome":
        bpy.ops.mesh.primitive_uv_sphere_add(radius=radius * 0.95, location=(0, 0, depth))
        cap_obj = bpy.context.active_object
        cap_obj.scale.z = 0.5
        bpy.ops.object.transform_apply(scale=True)
        add_material(cap_obj, (color[0] * 0.6, color[1] * 0.6, color[2] * 0.6, 1.0), f"{item_id}_Cap_Mat")
    elif cap == "cone":
        bpy.ops.mesh.primitive_cone_add(radius1=radius, radius2=radius * 0.3, depth=depth * 0.25,
                                        location=(0, 0, depth + depth * 0.125))
        add_material(bpy.context.active_object, (color[0] * 0.7, color[1] * 0.7, color[2] * 0.7, 1.0), f"{item_id}_Cap_Mat")
    elif cap == "nozzle":
        bpy.ops.mesh.primitive_cylinder_add(radius=radius * 0.4, depth=depth * 0.3,
                                            location=(0, 0, depth + depth * 0.15))
        add_material(bpy.context.active_object, (0.20, 0.20, 0.22, 1.0), f"{item_id}_Nozzle_Mat")
        bpy.ops.mesh.primitive_uv_sphere_add(radius=radius * 0.2, location=(0, 0, depth + depth * 0.32))
        add_material(bpy.context.active_object, (0.10, 0.10, 0.12, 1.0), f"{item_id}_Tip_Mat")
    join_and_rename(f"SM_{item_id}")


def build_pouch(item_id, color, w=0.18, d=0.12, h=0.14):
    """Soft bag/pouch — rounded cuboid with a tied neck."""
    clear_scene()
    bpy.ops.mesh.primitive_uv_sphere_add(radius=1.0, location=(0, 0, h / 2))
    body = bpy.context.active_object
    body.scale = (w, d, h * 0.55)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, color, f"{item_id}_Body_Mat")
    # Tied neck
    bpy.ops.mesh.primitive_cylinder_add(radius=w * 0.25, depth=h * 0.25, location=(0, 0, h * 0.95))
    add_material(bpy.context.active_object, (color[0] * 0.6, color[1] * 0.6, color[2] * 0.6, 1.0), f"{item_id}_Tie_Mat")
    join_and_rename(f"SM_{item_id}")


def build_torus_roll(item_id, color, major=0.07, minor=0.02):
    """Tape roll / fiber roll — torus."""
    clear_scene()
    bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor, location=(0, 0, minor))
    add_material(bpy.context.active_object, color, f"{item_id}_Roll_Mat")
    bpy.ops.mesh.primitive_cylinder_add(radius=major * 0.55, depth=minor * 1.6, location=(0, 0, minor))
    add_material(bpy.context.active_object, (color[0] * 0.8, color[1] * 0.8, color[2] * 0.8, 1.0), f"{item_id}_Core_Mat")
    join_and_rename(f"SM_{item_id}")


def build_slab(item_id, color, w=0.14, d=0.10, h=0.012):
    """Flat slab — pad, strip, sheet, token, data fragment."""
    clear_scene()
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, h / 2))
    obj = bpy.context.active_object
    obj.scale = (w, d, h)
    bpy.ops.object.transform_apply(scale=True)
    add_material(obj, color, f"{item_id}_Slab_Mat")
    join_and_rename(f"SM_{item_id}")


def build_battery(item_id, color):
    """Boxy cell with positive terminal."""
    clear_scene()
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.07))
    body = bpy.context.active_object
    body.scale = (0.05, 0.05, 0.14)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, color, f"{item_id}_Body_Mat")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.018, depth=0.02, location=(0, 0, 0.15))
    add_material(bpy.context.active_object, (0.85, 0.85, 0.30, 1.0), f"{item_id}_Term_Mat")
    join_and_rename(f"SM_{item_id}")


def build_tool(item_id, color, head_kind):
    """Tool with handle and head. head_kind in {axe, pick, multi, sewing}."""
    clear_scene()
    # Handle
    bpy.ops.mesh.primitive_cylinder_add(radius=0.02, depth=0.55, location=(0, 0, 0.275))
    handle = bpy.context.active_object
    add_material(handle, (0.35, 0.25, 0.18, 1.0), f"{item_id}_Handle_Mat")
    if head_kind == "axe":
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0.07, 0, 0.55))
        head = bpy.context.active_object
        head.scale = (0.13, 0.04, 0.10)
        bpy.ops.object.transform_apply(scale=True)
        add_material(head, color, f"{item_id}_Head_Mat")
    elif head_kind == "pick":
        bpy.ops.mesh.primitive_cone_add(radius1=0.03, radius2=0.0, depth=0.20, location=(0.10, 0, 0.55))
        head = bpy.context.active_object
        head.rotation_euler.y = math.pi / 2
        add_material(head, color, f"{item_id}_Head_Mat")
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.55))
        sleeve = bpy.context.active_object
        sleeve.scale = (0.05, 0.04, 0.05)
        bpy.ops.object.transform_apply(scale=True)
        add_material(sleeve, color, f"{item_id}_Sleeve_Mat")
    elif head_kind == "multi":
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.55))
        head = bpy.context.active_object
        head.scale = (0.06, 0.025, 0.06)
        bpy.ops.object.transform_apply(scale=True)
        add_material(head, color, f"{item_id}_Head_Mat")
        # Pliers nose
        bpy.ops.mesh.primitive_cone_add(radius1=0.025, radius2=0.005, depth=0.06, location=(0, 0, 0.65))
        add_material(bpy.context.active_object, color, f"{item_id}_Nose_Mat")
    elif head_kind == "sewing":
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.07))
        kit = bpy.context.active_object
        kit.scale = (0.10, 0.06, 0.025)
        bpy.ops.object.transform_apply(scale=True)
        add_material(kit, color, f"{item_id}_Kit_Mat")
        # We don't want the long handle for the sewing kit — delete it
        bpy.data.objects.remove(handle, do_unlink=True)
    join_and_rename(f"SM_{item_id}")


def build_garment(item_id, color, style="vest"):
    """Folded garment silhouette. style in {vest, jacket, barding, underlayer}."""
    clear_scene()
    if style == "vest":
        # Torso
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.20))
        torso = bpy.context.active_object
        torso.scale = (0.30, 0.10, 0.32)
        bpy.ops.object.transform_apply(scale=True)
        add_material(torso, color, f"{item_id}_Torso_Mat")
        # Front plates
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0.05, 0.20))
        plate = bpy.context.active_object
        plate.scale = (0.20, 0.02, 0.25)
        bpy.ops.object.transform_apply(scale=True)
        add_material(plate, (color[0] * 0.7, color[1] * 0.7, color[2] * 0.7, 1.0), f"{item_id}_Plate_Mat")
    elif style == "jacket":
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.22))
        torso = bpy.context.active_object
        torso.scale = (0.32, 0.12, 0.36)
        bpy.ops.object.transform_apply(scale=True)
        add_material(torso, color, f"{item_id}_Torso_Mat")
        # Sleeves
        for side in [-1, 1]:
            bpy.ops.mesh.primitive_cylinder_add(radius=0.06, depth=0.30, location=(side * 0.36, 0, 0.22))
            sleeve = bpy.context.active_object
            sleeve.rotation_euler.y = math.pi / 2
            add_material(sleeve, color, f"{item_id}_Sleeve_Mat")
    elif style == "barding":
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.18))
        body = bpy.context.active_object
        body.scale = (0.40, 0.18, 0.20)
        bpy.ops.object.transform_apply(scale=True)
        add_material(body, color, f"{item_id}_Body_Mat")
        # Plate strips
        for i in range(4):
            bpy.ops.mesh.primitive_cube_add(size=1, location=((i - 1.5) * 0.18, 0.18, 0.18))
            strip = bpy.context.active_object
            strip.scale = (0.07, 0.01, 0.16)
            bpy.ops.object.transform_apply(scale=True)
            add_material(strip, (0.30, 0.30, 0.32, 1.0), f"{item_id}_Strip_Mat")
    else:  # underlayer (folded)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.05))
        fold = bpy.context.active_object
        fold.scale = (0.20, 0.14, 0.04)
        bpy.ops.object.transform_apply(scale=True)
        add_material(fold, color, f"{item_id}_Fold_Mat")
    join_and_rename(f"SM_{item_id}")


def build_respirator(item_id, color, beast=False):
    """Mask shell + dual filter cans."""
    clear_scene()
    # Mask shell
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0, 0, 0.10))
    mask = bpy.context.active_object
    mask.scale = (1.0, 0.7, 1.1) if not beast else (1.4, 0.9, 1.3)
    bpy.ops.object.transform_apply(scale=True)
    add_material(mask, color, f"{item_id}_Mask_Mat")
    # Filter cans
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.035, depth=0.06, location=(side * 0.08, 0.07, 0.08))
        can = bpy.context.active_object
        can.rotation_euler.x = math.pi / 2
        add_material(can, (color[0] * 0.6, color[1] * 0.6, color[2] * 0.6, 1.0), f"{item_id}_Filter_Mat")
    # Strap loop
    bpy.ops.mesh.primitive_torus_add(major_radius=0.10, minor_radius=0.008, location=(0, -0.04, 0.10))
    add_material(bpy.context.active_object, (0.20, 0.18, 0.15, 1.0), f"{item_id}_Strap_Mat")
    join_and_rename(f"SM_{item_id}")


def build_water_filter(item_id, color):
    """Vertical filter: cylinder + spout + cap."""
    clear_scene()
    bpy.ops.mesh.primitive_cylinder_add(radius=0.05, depth=0.30, location=(0, 0, 0.15))
    add_material(bpy.context.active_object, color, f"{item_id}_Body_Mat")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.055, depth=0.03, location=(0, 0, 0.31))
    add_material(bpy.context.active_object, (0.25, 0.25, 0.28, 1.0), f"{item_id}_Cap_Mat")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.06, location=(0.06, 0, 0.05))
    spout = bpy.context.active_object
    spout.rotation_euler.y = math.pi / 2
    add_material(spout, (0.20, 0.20, 0.22, 1.0), f"{item_id}_Spout_Mat")
    join_and_rename(f"SM_{item_id}")


def build_carrier(item_id, color, style="sled"):
    """Logistics carrier. style in {sled, travois}."""
    clear_scene()
    # Platform
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.05))
    platform = bpy.context.active_object
    platform.scale = (0.50, 0.25, 0.04)
    bpy.ops.object.transform_apply(scale=True)
    add_material(platform, color, f"{item_id}_Plat_Mat")
    if style == "sled":
        for side in [-1, 1]:
            bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.55, location=(0, side * 0.22, 0.02))
            runner = bpy.context.active_object
            runner.rotation_euler.y = math.pi / 2
            add_material(runner, (0.25, 0.20, 0.15, 1.0), f"{item_id}_Runner_Mat")
        # Pull rope
        bpy.ops.mesh.primitive_torus_add(major_radius=0.04, minor_radius=0.005, location=(0.30, 0, 0.10))
        add_material(bpy.context.active_object, (0.45, 0.32, 0.18, 1.0), f"{item_id}_Rope_Mat")
    else:  # travois — two angled poles forming a V
        for side in [-1, 1]:
            bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.7, location=(0.05, side * 0.10, 0.13))
            pole = bpy.context.active_object
            pole.rotation_euler = (0, math.pi / 2.4, side * 0.18)
            add_material(pole, (0.40, 0.30, 0.20, 1.0), f"{item_id}_Pole_Mat")
    join_and_rename(f"SM_{item_id}")


def build_loose_pile(item_id, color, count=20):
    """Pile of loose granules / strands."""
    clear_scene()
    import random
    random.seed(hash(item_id) & 0xFFFF)
    for _ in range(count):
        x = random.uniform(-0.06, 0.06)
        y = random.uniform(-0.06, 0.06)
        z = random.uniform(0.005, 0.04)
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.012, subdivisions=1, location=(x, y, z))
        add_material(bpy.context.active_object, color, f"{item_id}_Granule_Mat")
    join_and_rename(f"SM_{item_id}")


def build_part(item_id, color, kind="block"):
    """Generic mechanical part. kind in {block, barrel, casing_set, sheet, tub, kit_bundle, data_core, token}."""
    clear_scene()
    if kind == "barrel":
        bpy.ops.mesh.primitive_cylinder_add(radius=0.015, depth=0.30, location=(0, 0, 0.15))
        add_material(bpy.context.active_object, color, f"{item_id}_Bore_Mat")
        bpy.ops.mesh.primitive_cylinder_add(radius=0.025, depth=0.04, location=(0, 0, 0.30))
        add_material(bpy.context.active_object, color, f"{item_id}_Muzzle_Mat")
    elif kind == "casing_set":
        for i in range(6):
            x = (i % 3 - 1) * 0.025
            y = (i // 3 - 0.5) * 0.025
            bpy.ops.mesh.primitive_cylinder_add(radius=0.008, depth=0.04, location=(x, y, 0.02))
            add_material(bpy.context.active_object, (0.75, 0.55, 0.20, 1.0), f"{item_id}_Brass_Mat")
    elif kind == "sheet":
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.005))
        sheet = bpy.context.active_object
        sheet.scale = (0.20, 0.14, 0.008)
        bpy.ops.object.transform_apply(scale=True)
        add_material(sheet, color, f"{item_id}_Sheet_Mat")
    elif kind == "tub":
        bpy.ops.mesh.primitive_cylinder_add(radius=0.10, depth=0.10, location=(0, 0, 0.05))
        add_material(bpy.context.active_object, color, f"{item_id}_Wall_Mat")
        bpy.ops.mesh.primitive_cylinder_add(radius=0.085, depth=0.085, location=(0, 0, 0.06))
        inner = bpy.context.active_object
        add_material(inner, (color[0] * 0.5, color[1] * 0.5, color[2] * 0.5, 1.0), f"{item_id}_Inner_Mat")
    elif kind == "kit_bundle":
        # Outer wrap
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.06))
        wrap = bpy.context.active_object
        wrap.scale = (0.20, 0.10, 0.10)
        bpy.ops.object.transform_apply(scale=True)
        add_material(wrap, color, f"{item_id}_Wrap_Mat")
        # Tied straps
        for x in [-0.06, 0.06]:
            bpy.ops.mesh.primitive_torus_add(major_radius=0.10, minor_radius=0.006, location=(x, 0, 0.06))
            strap = bpy.context.active_object
            strap.rotation_euler.y = math.pi / 2
            add_material(strap, (0.20, 0.18, 0.15, 1.0), f"{item_id}_Strap_Mat")
    elif kind == "data_core":
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.025))
        body = bpy.context.active_object
        body.scale = (0.08, 0.05, 0.025)
        bpy.ops.object.transform_apply(scale=True)
        add_material(body, color, f"{item_id}_Shell_Mat")
        # Glow stripe
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.052))
        stripe = bpy.context.active_object
        stripe.scale = (0.06, 0.01, 0.003)
        bpy.ops.object.transform_apply(scale=True)
        add_material(stripe, (0.30, 0.85, 0.95, 1.0), f"{item_id}_Glow_Mat")
    elif kind == "token":
        bpy.ops.mesh.primitive_cylinder_add(radius=0.04, depth=0.006, location=(0, 0, 0.003))
        add_material(bpy.context.active_object, color, f"{item_id}_Disc_Mat")
        bpy.ops.mesh.primitive_torus_add(major_radius=0.030, minor_radius=0.003, location=(0, 0, 0.006))
        add_material(bpy.context.active_object, (color[0] * 0.7, color[1] * 0.7, color[2] * 0.7, 1.0), f"{item_id}_Ring_Mat")
    else:  # block — generic part
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.04))
        block = bpy.context.active_object
        block.scale = (0.10, 0.05, 0.07)
        bpy.ops.object.transform_apply(scale=True)
        add_material(block, color, f"{item_id}_Block_Mat")
        bpy.ops.mesh.primitive_cylinder_add(radius=0.008, depth=0.05, location=(0.05, 0, 0.04))
        pin = bpy.context.active_object
        pin.rotation_euler.y = math.pi / 2
        add_material(pin, (color[0] * 0.6, color[1] * 0.6, color[2] * 0.6, 1.0), f"{item_id}_Pin_Mat")
    join_and_rename(f"SM_{item_id}")


# ── Dispatch (token → builder + params) ───────────────────────────────────────

# First match wins. Ordered so longer / more specific tokens go before shorter ones.
DISPATCH = [
    # Substring,           builder,             kwargs (item_id + color filled in by main)
    ("HATCHET",             build_tool,          {"head_kind": "axe"}),
    ("PICKAXE",             build_tool,          {"head_kind": "pick"}),
    ("MULTI_TOOL",          build_tool,          {"head_kind": "multi"}),
    ("SEWING_KIT",          build_tool,          {"head_kind": "sewing"}),
    ("BEAST_RESPIRATOR",    build_respirator,    {"beast": True}),
    ("RESPIRATOR",          build_respirator,    {}),
    ("WATER_FILTER",        build_water_filter,  {}),
    ("DRAG_SLED",           build_carrier,       {"style": "sled"}),
    ("TRAVOIS",             build_carrier,       {"style": "travois"}),
    ("FIELD_JACKET",        build_garment,       {"style": "jacket"}),
    ("HEAVY_BARDING",       build_garment,       {"style": "barding"}),
    ("THERMAL_UNDERLAYER",  build_garment,       {"style": "underlayer"}),
    ("VEST",                build_garment,       {"style": "vest"}),
    ("BATTERY_CELL",        build_battery,       {}),
    ("FIBER_TAPE_ROLL",     build_torus_roll,    {}),
    ("SEAL_TAPE_ROLL",      build_torus_roll,    {}),
    ("ABRASIVE_GRIT_BAG",   build_pouch,         {}),
    ("FILTER_PELLETS_POUCH",build_pouch,         {}),
    ("CLOTH_WIPES_BUNDLE",  build_pouch,         {}),
    ("RESIN_POUCH",         build_pouch,         {}),
    ("GOURD_POUCH",         build_pouch,         {}),
    ("ABRASIVE_PAD",        build_slab,          {"w": 0.10, "d": 0.07, "h": 0.012}),
    ("PATCH_STRIP",         build_slab,          {"w": 0.12, "d": 0.03, "h": 0.004}),
    ("FUSE_LINK",           build_slab,          {"w": 0.06, "d": 0.015, "h": 0.008}),
    ("FILTER_INSERT",       build_cyl,           {"radius": 0.05, "depth": 0.04, "cap": "none"}),
    ("FILTER_MEMBRANE",     build_cyl,           {"radius": 0.06, "depth": 0.012, "cap": "none"}),
    ("DEGREASER_SPRAY",     build_cyl,           {"radius": 0.04, "depth": 0.18, "cap": "nozzle"}),
    ("CONTACT_CLEANER_CAN", build_cyl,           {"radius": 0.04, "depth": 0.16, "cap": "nozzle"}),
    ("SOLVENT_BOTTLE",      build_cyl,           {"radius": 0.04, "depth": 0.16, "cap": "dome"}),
    ("LUBE_TUBE",           build_cyl,           {"radius": 0.022, "depth": 0.14, "cap": "cone"}),
    ("LUBE_PACKET",         build_slab,          {"w": 0.07, "d": 0.04, "h": 0.012}),
    ("POLISH_PASTE",        build_cyl,           {"radius": 0.035, "depth": 0.06, "cap": "dome"}),
    ("THREADLOCK_VIAL",     build_cyl,           {"radius": 0.014, "depth": 0.07, "cap": "cone"}),
    ("BARREL_ASSEMBLY",     build_part,          {"kind": "barrel"}),
    ("BARREL_BLANK",        build_part,          {"kind": "barrel"}),
    ("PISTOL_CASING_SET",   build_part,          {"kind": "casing_set"}),
    ("RIFLE_CASING_SET",    build_part,          {"kind": "casing_set"}),
    ("SHOT_HULL_SET",       build_part,          {"kind": "casing_set"}),
    ("SNIPER_CASING_SET",   build_part,          {"kind": "casing_set"}),
    ("POLYMER_SHEET",       build_part,          {"kind": "sheet"}),
    ("RAW_HIDE",            build_part,          {"kind": "sheet"}),
    ("SCRAP_PLASTIC_TUB",   build_part,          {"kind": "tub"}),
    ("MILITIA",             build_part,          {"kind": "kit_bundle"}),
    ("DATA_CORE",           build_part,          {"kind": "data_core"}),
    ("INTEL_FRAGMENT",      build_part,          {"kind": "data_core"}),
    ("REMNANT_MODULE",      build_part,          {"kind": "data_core"}),
    ("DOCTRINE_SEAL",       build_part,          {"kind": "token"}),
    ("PROOF_TOKEN",         build_part,          {"kind": "token"}),
    ("SILICA_SAND",         build_loose_pile,    {"count": 30}),
    ("SINEW_STRAND",        build_loose_pile,    {"count": 16}),
    ("GLASS_TUBE",          build_cyl,           {"radius": 0.012, "depth": 0.10, "cap": "none"}),
    ("BAFFLE_CORE",         build_part,          {"kind": "block"}),
    ("BOLT_GROUP",          build_part,          {"kind": "block"}),
    ("MAG_BODY",            build_part,          {"kind": "block"}),
    ("RECEIVER_FRAME",      build_part,          {"kind": "block"}),
    ("TRIGGER_GROUP",       build_part,          {"kind": "block"}),
]


def dispatch_for(item_id):
    """Return (builder_fn, kwargs) for the first matching token, else None."""
    for token, fn, kwargs in DISPATCH:
        if token in item_id:
            return fn, dict(kwargs)
    return None


# ── Main export pipeline ───────────────────────────────────────────────────────

def main():
    print("\n=== Quiet Rift: Enigma — Handheld Items Asset Generator ===")
    csv_abs = os.path.abspath(CSV_PATH)
    if not os.path.isfile(csv_abs):
        print(f"ERROR: CSV not found at {csv_abs}")
        return

    with open(csv_abs, newline='', encoding='utf-8') as f:
        rows = [r for r in csv.DictReader(f)
                if r.get("Category", "").strip() in HANDHELD_CATEGORIES]

    print(f"Handheld rows to generate: {len(rows)}")

    unmatched = []
    for row in rows:
        item_id = row["ItemId"].strip()
        category = row["Category"].strip()
        color = _color(category)
        match = dispatch_for(item_id)
        if match is None:
            unmatched.append(item_id)
            # Fallback: generic block in category color
            build_part(item_id, color, kind="block")
        else:
            fn, kwargs = match
            kwargs.setdefault("color", color)
            print(f"\n[{item_id}] {row.get('Item', item_id)} ({category}) -> {fn.__name__}")
            fn(item_id, **kwargs)

        out_path = os.path.join(OUTPUT_DIR, f"SM_{item_id}.fbx")
        export_fbx(item_id, out_path)

    if unmatched:
        print(f"\nWARN: {len(unmatched)} items used the generic block fallback:")
        for u in unmatched:
            print(f"  - {u}")

    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Import these FBX files into UE5 via Content Browser > Import.")


if __name__ == "__main__":
    main()
