"""
Quiet Rift: Enigma â€” Cosmetic Clothing Asset Generator (Blender 4.x)

Cosmetic-only clothing items added in Batch 4 of the Blender detail
pass. These are explicitly cosmetic â€” they have NO gameplay effect.
EQRItemCategory::Cosmetic and Item.Category.Cosmetic gameplay tag mark
them as such in the type system.

Reads every row in DT_Items_Master.csv whose Category is Cosmetic and
exports a placeholder FBX per item. Each item is built as a folded /
laid-flat garment silhouette so it reads correctly as an inventory icon
or stowed clothing item. Categories covered:
    Footwear:      hiking boots, tactical boots, tennis shoes,
                    running shoes
    Pants:         cargo pants, jeans, athletic shorts, cargo shorts
    Shirts:        t-shirt, long-sleeve, sleeveless tank, button-up
    Outerwear:     hoodie, denim jacket, bomber jacket
    Undergarments: sports bra, briefs, boxer shorts, panties

Per-item sockets:
    Footwear:      SOCKET_AnkleAttach (where the foot bone slots in)
    Pants/shorts:  SOCKET_WaistAttach
    Shirts/outer:  SOCKET_NeckAttach
    Undergarments: SOCKET_AnchorPoint

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

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/cosmetic_clothing")
CSV_PATH = os.path.join(
    os.path.dirname(__file__),
    "../../Content/QuietRift/Data/DT_Items_Master.csv",
)


def _mat(slot, color, roughness=0.90, emissive=None):
    return get_or_create_material(slot, color, roughness=roughness,
                                   metallic=0.0, emissive=emissive)


def _add(obj, mat):
    assign_material(obj, mat)
    return obj


def _slab(x, y, z, w, d, h, mat, name):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z))
    obj = bpy.context.active_object
    obj.scale = (w, d, h); bpy.ops.object.transform_apply(scale=True)
    _add(obj, mat); obj.name = name
    return obj


def _finalize_cosmetic(name, *, attach_kind="WaistAttach", attach_loc=(0, 0, 0)):
    add_socket(attach_kind, location=attach_loc)
    finalize_asset(name,
                    bevel_width=0.0015, bevel_angle_deg=30,
                    smooth_angle_deg=50, collision="convex",
                    lods=[0.50], pivot="bottom_center")


# â”€â”€ Footwear (paired) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def _shoe_pair(item_id, sole_mat, upper_mat, accent_mat=None,
                ankle_height=0.05, length=0.28, width=0.10, ankle_collar=False):
    """Generic shoe: two shoes side-by-side. Returns nothing."""
    for side, sx in enumerate([-0.06, 0.06]):
        # Sole
        _slab(sx, 0, 0.012, length, width, 0.024, sole_mat, f"{item_id}_Sole_{side}")
        # Upper
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.5, location=(sx, 0, 0.04))
        upper = bpy.context.active_object
        upper.scale = (length / 2, width / 2, 0.05)
        bpy.ops.object.transform_apply(scale=True)
        _add(upper, upper_mat); upper.name = f"{item_id}_Upper_{side}"
        # Toe cap (slightly darker)
        _slab(sx + length * 0.35, 0, 0.025, length * 0.18, width * 0.85, 0.025,
                accent_mat or upper_mat, f"{item_id}_ToeCap_{side}")
        # Heel
        _slab(sx - length * 0.35, 0, 0.030, length * 0.20, width * 0.85, 0.040,
                upper_mat, f"{item_id}_Heel_{side}")
        if ankle_collar:
            # Tall ankle collar around heel
            bpy.ops.mesh.primitive_cylinder_add(radius=width * 0.40, depth=ankle_height,
                                                 location=(sx - length * 0.30, 0, 0.04 + ankle_height / 2))
            collar = bpy.context.active_object
            collar.scale = (1.0, 0.9, 1.0); bpy.ops.object.transform_apply(scale=True)
            _add(collar, upper_mat); collar.name = f"{item_id}_AnkleCollar_{side}"
        # Lacing: 4-row rivet pattern across the tongue
        for i in range(4):
            for sgn in [-1, 1]:
                bpy.ops.mesh.primitive_cylinder_add(radius=0.0035, depth=0.005,
                                                     location=(sx + 0.02 + i * 0.02,
                                                                sgn * width * 0.18, 0.06))
                eyelet = bpy.context.active_object
                _add(eyelet, palette_material("DarkSteel"))
                eyelet.name = f"{item_id}_Eyelet_{side}_{i}_{sgn}"


def gen_boots_hiking():
    clear_scene()
    sole = _mat("Cosmetic_BootSole", (0.20, 0.18, 0.15, 1.0), roughness=0.95)
    upper = _mat("Cosmetic_BootUpper_Hiking", (0.45, 0.30, 0.18, 1.0))
    accent = _mat("Cosmetic_BootToeCap_Hiking", (0.30, 0.20, 0.12, 1.0))
    _shoe_pair("Boots_Hiking", sole, upper, accent, ankle_height=0.10,
                length=0.28, width=0.10, ankle_collar=True)
    _finalize_cosmetic("SM_COS_BOOTS_HIKING", attach_kind="AnkleAttach", attach_loc=(0, 0, 0.10))


def gen_boots_tactical():
    clear_scene()
    sole = _mat("Cosmetic_BootSole", (0.10, 0.10, 0.10, 1.0), roughness=0.95)
    upper = _mat("Cosmetic_BootUpper_Tactical", (0.08, 0.08, 0.08, 1.0))
    _shoe_pair("Boots_Tactical", sole, upper,  ankle_height=0.14,
                length=0.30, width=0.10, ankle_collar=True)
    _finalize_cosmetic("SM_COS_BOOTS_TACTICAL", attach_kind="AnkleAttach", attach_loc=(0, 0, 0.14))


def gen_shoes_tennis():
    clear_scene()
    sole = _mat("Cosmetic_TennisShoe_Sole", (0.95, 0.92, 0.85, 1.0), roughness=0.85)
    upper = _mat("Cosmetic_TennisShoe_Upper", (0.92, 0.92, 0.92, 1.0))
    stripe = _mat("Cosmetic_TennisShoe_Stripe", (0.30, 0.50, 0.85, 1.0))
    _shoe_pair("Shoes_Tennis", sole, upper,  ankle_height=0.04,
                length=0.26, width=0.09, ankle_collar=False)
    _finalize_cosmetic("SM_COS_SHOES_TENNIS", attach_kind="AnkleAttach", attach_loc=(0, 0, 0.06))


def gen_shoes_running():
    clear_scene()
    sole = _mat("Cosmetic_RunningShoe_Sole", (0.18, 0.18, 0.20, 1.0), roughness=0.90)
    upper = _mat("Cosmetic_RunningShoe_Upper", (0.22, 0.50, 0.85, 1.0))
    accent = _mat("Cosmetic_RunningShoe_Accent", (0.95, 0.90, 0.20, 1.0))
    _shoe_pair("Shoes_Running", sole, upper,  ankle_height=0.04,
                length=0.27, width=0.09, ankle_collar=False)
    _finalize_cosmetic("SM_COS_SHOES_RUNNING", attach_kind="AnkleAttach", attach_loc=(0, 0, 0.06))


# â”€â”€ Pants / shorts â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def _pants(item_id, body_mat, accent_mat=None, length=0.55, waist_w=0.30,
            leg_separation=0.05, has_pockets=False):
    """Folded-flat pants silhouette with two leg tubes."""
    # Waistband
    _slab(0, 0, length + 0.025, waist_w, 0.025, 0.04, accent_mat or body_mat,
           f"{item_id}_Waistband")
    # Two leg tubes
    for sx in [-leg_separation, leg_separation]:
        _slab(sx, 0, length / 2, waist_w * 0.40, 0.04, length, body_mat,
               f"{item_id}_Leg_{sx}")
        # Knee crease seam
        _slab(sx, 0.022, length * 0.55, waist_w * 0.38, 0.001, 0.005,
                accent_mat or body_mat, f"{item_id}_Crease_{sx}")
    # Belt loops
    for sx in [-waist_w * 0.35, 0.0, waist_w * 0.35]:
        _slab(sx, 0.01, length + 0.025, 0.012, 0.004, 0.025, accent_mat or body_mat,
               f"{item_id}_Loop_{sx}")
    if has_pockets:
        for sx in [-waist_w * 0.30, waist_w * 0.30]:
            _slab(sx, 0.022, length * 0.78, 0.07, 0.002, 0.06, accent_mat or body_mat,
                   f"{item_id}_Pocket_{sx}")


def gen_pants_cargo():
    clear_scene()
    body = _mat("Cosmetic_PantsCargo_Body", (0.30, 0.34, 0.20, 1.0))
    accent = _mat("Cosmetic_PantsCargo_Accent", (0.20, 0.22, 0.13, 1.0))
    _pants("PantsCargo", body, accent_mat=accent, length=0.58, waist_w=0.30,
            leg_separation=0.06, has_pockets=True)
    # Side cargo pockets (large)
    for sx in [-0.18, 0.18]:
        _slab(sx, 0.025, 0.32, 0.08, 0.003, 0.10, accent, f"PantsCargo_SidePocket_{sx}")
    _finalize_cosmetic("SM_COS_PANTS_CARGO", attach_loc=(0, 0, 0.62))


def gen_pants_jeans():
    clear_scene()
    body = _mat("Cosmetic_PantsJeans_Body", (0.30, 0.40, 0.65, 1.0))
    accent = _mat("Cosmetic_PantsJeans_Stitch", (0.92, 0.85, 0.40, 1.0))
    _pants("PantsJeans", body, accent_mat=accent, length=0.60, waist_w=0.30,
            leg_separation=0.06, has_pockets=True)
    _finalize_cosmetic("SM_COS_PANTS_JEANS", attach_loc=(0, 0, 0.64))


def gen_shorts_athletic():
    clear_scene()
    body = _mat("Cosmetic_ShortsAthletic_Body", (0.20, 0.22, 0.25, 1.0))
    accent = _mat("Cosmetic_ShortsAthletic_Stripe", (0.92, 0.92, 0.92, 1.0))
    _pants("ShortsAthletic", body, accent_mat=accent, length=0.22, waist_w=0.30,
            leg_separation=0.06, has_pockets=False)
    # Side stripe
    for sx in [-0.18, 0.18]:
        _slab(sx, 0.022, 0.12, 0.005, 0.002, 0.20, accent, f"ShortsAthletic_Stripe_{sx}")
    _finalize_cosmetic("SM_COS_SHORTS_ATHLETIC", attach_loc=(0, 0, 0.26))


def gen_shorts_cargo():
    clear_scene()
    body = _mat("Cosmetic_ShortsCargo_Body", (0.45, 0.42, 0.30, 1.0))
    accent = _mat("Cosmetic_ShortsCargo_Accent", (0.30, 0.28, 0.20, 1.0))
    _pants("ShortsCargo", body, accent_mat=accent, length=0.26, waist_w=0.30,
            leg_separation=0.06, has_pockets=True)
    # Cargo side pockets
    for sx in [-0.18, 0.18]:
        _slab(sx, 0.025, 0.14, 0.07, 0.003, 0.08, accent, f"ShortsCargo_SidePocket_{sx}")
    _finalize_cosmetic("SM_COS_SHORTS_CARGO", attach_loc=(0, 0, 0.30))


# â”€â”€ Shirts â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def _shirt(item_id, body_mat, accent_mat=None, sleeve_kind="short",
            torso_h=0.40, waist_w=0.32, has_collar=False, has_buttons=False):
    """Folded-flat shirt: torso + sleeves + optional collar/buttons."""
    # Torso
    _slab(0, 0, torso_h / 2, waist_w, 0.04, torso_h, body_mat, f"{item_id}_Torso")
    # Neck hole (negative-space hint via darker patch)
    _slab(0, 0.022, torso_h - 0.02, waist_w * 0.20, 0.002, 0.015,
           accent_mat or body_mat, f"{item_id}_NeckHole")
    # Sleeves
    if sleeve_kind == "long":
        sleeve_len = 0.35
        sleeve_z = torso_h - 0.04
    elif sleeve_kind == "short":
        sleeve_len = 0.12
        sleeve_z = torso_h - 0.04
    elif sleeve_kind == "sleeveless":
        sleeve_len = 0.04
        sleeve_z = torso_h - 0.06
    else:
        sleeve_len = 0.0
        sleeve_z = torso_h - 0.04
    if sleeve_len > 0:
        for side in [-1, 1]:
            sx = side * (waist_w * 0.5 + sleeve_len * 0.5)
            bpy.ops.mesh.primitive_cylinder_add(radius=0.05, depth=sleeve_len,
                                                 location=(sx, 0, sleeve_z))
            sleeve = bpy.context.active_object
            sleeve.rotation_euler.y = math.pi / 2
            _add(sleeve, body_mat); sleeve.name = f"{item_id}_Sleeve_{side}"
    if has_collar:
        # Open collar
        for side in [-1, 1]:
            _slab(side * 0.04, 0.022, torso_h - 0.02, 0.04, 0.005, 0.04,
                    accent_mat or body_mat, f"{item_id}_Collar_{side}")
    if has_buttons:
        for i in range(5):
            z = torso_h - 0.04 - i * 0.06
            bpy.ops.mesh.primitive_cylinder_add(radius=0.006, depth=0.003, location=(0, 0.022, z))
            btn = bpy.context.active_object
            _add(btn, accent_mat or body_mat); btn.name = f"{item_id}_Button_{i}"


def gen_shirt_tshirt():
    clear_scene()
    body = _mat("Cosmetic_TShirt_Body", (0.85, 0.85, 0.85, 1.0))
    _shirt("TShirt", body, sleeve_kind="short", torso_h=0.40, waist_w=0.32)
    _finalize_cosmetic("SM_COS_SHIRT_TSHIRT", attach_kind="NeckAttach", attach_loc=(0, 0, 0.40))


def gen_shirt_longsleeve():
    clear_scene()
    body = _mat("Cosmetic_LongSleeve_Body", (0.55, 0.30, 0.25, 1.0))
    _shirt("LongSleeve", body, sleeve_kind="long", torso_h=0.40, waist_w=0.32)
    _finalize_cosmetic("SM_COS_SHIRT_LONGSLEEVE", attach_kind="NeckAttach", attach_loc=(0, 0, 0.40))


def gen_shirt_sleeveless():
    clear_scene()
    body = _mat("Cosmetic_Tank_Body", (0.20, 0.40, 0.60, 1.0))
    _shirt("Tank", body, sleeve_kind="sleeveless", torso_h=0.40, waist_w=0.30)
    _finalize_cosmetic("SM_COS_SHIRT_SLEEVELESS", attach_kind="NeckAttach", attach_loc=(0, 0, 0.40))


def gen_shirt_buttonup():
    clear_scene()
    body = _mat("Cosmetic_ButtonUp_Body", (0.82, 0.78, 0.55, 1.0))
    accent = _mat("Cosmetic_ButtonUp_Button", (0.20, 0.18, 0.15, 1.0))
    _shirt("ButtonUp", body, accent_mat=accent, sleeve_kind="long",
            torso_h=0.42, waist_w=0.32, has_collar=True, has_buttons=True)
    # Chest pocket
    _slab(-0.10, 0.022, 0.30, 0.07, 0.002, 0.08, accent, "ButtonUp_ChestPocket")
    _finalize_cosmetic("SM_COS_SHIRT_BUTTONUP", attach_kind="NeckAttach", attach_loc=(0, 0, 0.42))


# â”€â”€ Outerwear â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def gen_hoodie():
    clear_scene()
    body = _mat("Cosmetic_Hoodie_Body", (0.30, 0.32, 0.38, 1.0))
    accent = _mat("Cosmetic_Hoodie_Cuff", (0.20, 0.22, 0.27, 1.0))
    _shirt("Hoodie", body, accent_mat=accent, sleeve_kind="long",
            torso_h=0.50, waist_w=0.36)
    # Hood (loose half-sphere behind neck)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.16, location=(0, -0.03, 0.50))
    hood = bpy.context.active_object
    hood.scale = (0.9, 0.6, 0.7); bpy.ops.object.transform_apply(scale=True)
    _add(hood, body); hood.name = "Hoodie_Hood"
    # Drawstrings
    for sx in [-0.04, 0.04]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.005, depth=0.10, location=(sx, 0.025, 0.43))
        _add(bpy.context.active_object, accent)
    # Kangaroo pocket
    _slab(0, 0.024, 0.20, 0.20, 0.002, 0.10, accent, "Hoodie_KangarooPocket")
    # Ribbed cuff at waist
    _slab(0, 0, 0.04, 0.36, 0.045, 0.025, accent, "Hoodie_Cuff_Waist")
    _finalize_cosmetic("SM_COS_HOODIE", attach_kind="NeckAttach", attach_loc=(0, 0, 0.50))


def gen_jacket_denim():
    clear_scene()
    body = _mat("Cosmetic_DenimJacket_Body", (0.25, 0.35, 0.55, 1.0))
    accent = _mat("Cosmetic_DenimJacket_Stitch", (0.92, 0.85, 0.40, 1.0))
    button = _mat("Cosmetic_DenimJacket_Button", (0.65, 0.55, 0.20, 1.0), roughness=0.55, emissive=None)
    _shirt("DenimJacket", body, accent_mat=accent, sleeve_kind="long",
            torso_h=0.45, waist_w=0.36, has_collar=True)
    # Two chest pockets
    for sx in [-0.10, 0.10]:
        _slab(sx, 0.024, 0.34, 0.08, 0.002, 0.08, accent, f"DenimJacket_ChestPocket_{sx}")
    # Buttons down center (smaller than shirt buttons)
    for i in range(5):
        z = 0.40 - i * 0.07
        bpy.ops.mesh.primitive_cylinder_add(radius=0.008, depth=0.004, location=(0, 0.024, z))
        _add(bpy.context.active_object, button)
    _finalize_cosmetic("SM_COS_JACKET_DENIM", attach_kind="NeckAttach", attach_loc=(0, 0, 0.45))


def gen_jacket_bomber():
    clear_scene()
    body = _mat("Cosmetic_Bomber_Body", (0.22, 0.20, 0.18, 1.0))
    accent = _mat("Cosmetic_Bomber_Ribbed", (0.40, 0.30, 0.10, 1.0))
    _shirt("Bomber", body, accent_mat=accent, sleeve_kind="long",
            torso_h=0.42, waist_w=0.36)
    # Ribbed cuffs at sleeve ends
    for side in [-1, 1]:
        sx = side * 0.355
        bpy.ops.mesh.primitive_cylinder_add(radius=0.052, depth=0.04,
                                             location=(sx, 0, 0.38))
        cuff = bpy.context.active_object
        cuff.rotation_euler.y = math.pi / 2
        _add(cuff, accent); cuff.name = f"Bomber_SleeveCuff_{side}"
    # Ribbed waistband
    _slab(0, 0, 0.03, 0.36, 0.045, 0.04, accent, "Bomber_WaistRib")
    # Front zipper line
    _slab(0, 0.022, 0.21, 0.005, 0.002, 0.32, accent, "Bomber_Zipper")
    _finalize_cosmetic("SM_COS_JACKET_BOMBER", attach_kind="NeckAttach", attach_loc=(0, 0, 0.42))


# â”€â”€ Undergarments â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def gen_bra_sports():
    clear_scene()
    body = _mat("Cosmetic_SportsBra_Body", (0.20, 0.20, 0.20, 1.0))
    accent = _mat("Cosmetic_SportsBra_Trim", (0.85, 0.20, 0.30, 1.0))
    # Main band
    bpy.ops.mesh.primitive_cylinder_add(radius=0.12, depth=0.10, location=(0, 0, 0.05))
    band = bpy.context.active_object
    band.scale = (1.0, 0.6, 1.0); bpy.ops.object.transform_apply(scale=True)
    _add(band, body); band.name = "SportsBra_Band"
    # Top trim
    bpy.ops.mesh.primitive_torus_add(major_radius=0.12, minor_radius=0.005, location=(0, 0, 0.10))
    trim = bpy.context.active_object
    trim.scale = (1.0, 0.6, 1.0); bpy.ops.object.transform_apply(scale=True)
    _add(trim, accent); trim.name = "SportsBra_TopTrim"
    # Shoulder straps
    for sx in [-0.05, 0.05]:
        _slab(sx, 0.0, 0.18, 0.020, 0.04, 0.12, body, f"SportsBra_Strap_{sx}")
    _finalize_cosmetic("SM_COS_BRA_SPORTS", attach_kind="AnchorPoint", attach_loc=(0, 0, 0.10))


def gen_underwear_briefs():
    clear_scene()
    body = _mat("Cosmetic_Briefs_Body", (0.85, 0.85, 0.85, 1.0))
    waist = _mat("Cosmetic_Briefs_Waistband", (0.20, 0.22, 0.25, 1.0))
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.05))
    main = bpy.context.active_object
    main.scale = (0.20, 0.07, 0.10); bpy.ops.object.transform_apply(scale=True)
    _add(main, body); main.name = "Briefs_Main"
    _slab(0, 0, 0.10, 0.21, 0.07, 0.018, waist, "Briefs_Waistband")
    _finalize_cosmetic("SM_COS_UNDERWEAR_BRIEFS", attach_kind="AnchorPoint", attach_loc=(0, 0, 0.11))


def gen_underwear_boxers():
    clear_scene()
    body = _mat("Cosmetic_Boxers_Body", (0.30, 0.40, 0.65, 1.0))
    pattern = _mat("Cosmetic_Boxers_Pattern", (0.20, 0.30, 0.50, 1.0))
    waist = _mat("Cosmetic_Boxers_Waistband", (0.20, 0.22, 0.25, 1.0))
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.07))
    main = bpy.context.active_object
    main.scale = (0.20, 0.08, 0.14); bpy.ops.object.transform_apply(scale=True)
    _add(main, body); main.name = "Boxers_Main"
    # Plaid stripe pattern
    for i in range(4):
        _slab(0, 0.043, 0.04 + i * 0.04, 0.20, 0.001, 0.005, pattern,
                f"Boxers_PatternStripe_{i}")
    _slab(0, 0, 0.14, 0.21, 0.08, 0.020, waist, "Boxers_Waistband")
    _finalize_cosmetic("SM_COS_UNDERWEAR_BOXERS", attach_kind="AnchorPoint", attach_loc=(0, 0, 0.15))


def gen_underwear_panties():
    clear_scene()
    body = _mat("Cosmetic_Panties_Body", (0.85, 0.85, 0.85, 1.0))
    trim = _mat("Cosmetic_Panties_Trim", (0.92, 0.55, 0.65, 1.0))
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.04))
    main = bpy.context.active_object
    main.scale = (0.18, 0.06, 0.08); bpy.ops.object.transform_apply(scale=True)
    _add(main, body); main.name = "Panties_Main"
    bpy.ops.mesh.primitive_torus_add(major_radius=0.09, minor_radius=0.003, location=(0, 0, 0.075))
    waistband = bpy.context.active_object
    waistband.scale = (1.0, 0.7, 1.0); bpy.ops.object.transform_apply(scale=True)
    _add(waistband, trim); waistband.name = "Panties_Waistband"
    _finalize_cosmetic("SM_COS_UNDERWEAR_PANTIES", attach_kind="AnchorPoint", attach_loc=(0, 0, 0.08))


# â”€â”€ Dispatch â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

GENERATORS = {
    "COS_BOOTS_HIKING":        gen_boots_hiking,
    "COS_BOOTS_TACTICAL":      gen_boots_tactical,
    "COS_SHOES_TENNIS":        gen_shoes_tennis,
    "COS_SHOES_RUNNING":       gen_shoes_running,
    "COS_PANTS_CARGO":         gen_pants_cargo,
    "COS_PANTS_JEANS":         gen_pants_jeans,
    "COS_SHORTS_ATHLETIC":     gen_shorts_athletic,
    "COS_SHORTS_CARGO":        gen_shorts_cargo,
    "COS_SHIRT_TSHIRT":        gen_shirt_tshirt,
    "COS_SHIRT_LONGSLEEVE":    gen_shirt_longsleeve,
    "COS_SHIRT_SLEEVELESS":    gen_shirt_sleeveless,
    "COS_SHIRT_BUTTONUP":      gen_shirt_buttonup,
    "COS_HOODIE":              gen_hoodie,
    "COS_JACKET_DENIM":        gen_jacket_denim,
    "COS_JACKET_BOMBER":       gen_jacket_bomber,
    "COS_BRA_SPORTS":          gen_bra_sports,
    "COS_UNDERWEAR_BRIEFS":    gen_underwear_briefs,
    "COS_UNDERWEAR_BOXERS":    gen_underwear_boxers,
    "COS_UNDERWEAR_PANTIES":   gen_underwear_panties,
}


def main():
    print("\n=== Quiet Rift: Enigma â€” Cosmetic Clothing Asset Generator ===")
    csv_abs = os.path.abspath(CSV_PATH)
    if not os.path.isfile(csv_abs):
        print(f"ERROR: CSV not found at {csv_abs}")
        return
    with open(csv_abs, newline='', encoding='utf-8') as f:
        rows = [r for r in csv.DictReader(f)
                if r.get("Category", "").strip() == "Cosmetic"]
    print(f"Cosmetic rows to generate: {len(rows)}")
    missing = [r["ItemId"] for r in rows if r["ItemId"] not in GENERATORS]
    if missing:
        print(f"WARN: no generator registered for: {missing}")
    for row in rows:
        rid = row["ItemId"].strip()
        gen = GENERATORS.get(rid)
        if gen is None:
            continue
        print(f"\n[{rid}] {row.get('Item', rid)}")
        gen()
        out_path = os.path.join(OUTPUT_DIR, f"SM_{rid}.fbx")
        export_fbx(rid, out_path)
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("These items are explicitly cosmetic-only â€” they have no gameplay effect.")


if __name__ == "__main__":
    main()

