"""
Quiet Rift: Enigma — Handheld Items Procedural Asset Generator (Blender 4.x)

Upgraded in Batch 3 of the Blender detail pass — every handheld item flows
through qr_blender_detail.py for production finalization (palette dedupe,
smooth shading, bevels, smart UV, sockets where they matter, UCX collision).

Reads every "handheld" row in DT_Items_Master.csv (Equipment, Consumable,
Item, Tech, Leadership, Weapon Kit, Equipment/Logistics, Equipment/Armor,
Component/Container) — 63 rows total — and routes each ItemId through
12 parameterized template builders dispatched by substring match. Each
builder pulls its colors from qr_blender_detail's canonical palette so
material zones dedupe across the whole asset library.

Per-item sockets where meaningful:
    Tools (hatchet, pickaxe, multi-tool):  SOCKET_Grip, SOCKET_BusinessEnd
    Sewing kit:                            SOCKET_Lid
    Tubes / bottles / sprays / vials:      SOCKET_Cap
    Pouches / bags / bundles:              SOCKET_TiePoint
    Battery cell:                          SOCKET_Terminal
    Garments (vest / jacket / barding):    SOCKET_WaistAttach
    Respirator:                            SOCKET_FaceAttach
    Water filter:                          SOCKET_Spout
    Drag sled / travois:                   SOCKET_Hitch
    Casing sets / kit bundles:             SOCKET_Center

Other items (slabs, pads, loose piles, generic parts) skip sockets —
nothing meaningful to attach.

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

# Map each Category to a primary palette material name. Builders look up these
# via palette_material() so a Consumable item stays the same Polymer color
# everywhere it appears.
CATEGORY_PALETTE = {
    "Consumable":              "Polymer",
    "Equipment":               "DarkCanvas",
    "Item":                    "Gunmetal",
    "Tech":                    "DarkSteel",
    "Leadership":              "Brass",
    "Weapon Kit":              "Olive",
    "Equipment / Logistics":   "Wood",
    "Equipment / Armor":       "Leather",
    "Component / Container":   "DarkLeather",
}


def _category_mat(category):
    return palette_material(CATEGORY_PALETTE.get(category.strip(), "Steel"))


def _add(obj, mat):
    assign_material(obj, mat)
    return obj


def _finalize_handheld(name, sockets=None, lods=None):
    """Standard finalize for handheld items: subtle bevel, smooth shading,
    convex collision, optional sockets + LODs."""
    if sockets:
        for s in sockets:
            add_socket(s["name"], location=s.get("loc", (0, 0, 0)),
                        rotation=s.get("rot", (0, 0, 0)))
    finalize_asset(name,
                    bevel_width=0.0015, bevel_angle_deg=30,
                    smooth_angle_deg=50, collision="convex",
                    lods=list(lods) if lods else None,
                    pivot="geometry_center")


# ── Template Builders ─────────────────────────────────────────────────────────

def build_cyl(item_id, body_mat, radius=0.04, depth=0.16, cap="none"):
    """Cylinder with optional cap. cap in {none, dome, cone, nozzle}."""
    clear_scene()
    cap_mat = get_or_create_material(f"Cyl_Cap_{item_id}",
                                      (body_mat.diffuse_color[0] * 0.65,
                                       body_mat.diffuse_color[1] * 0.65,
                                       body_mat.diffuse_color[2] * 0.65, 1.0),
                                      roughness=0.55)
    nozzle_mat = palette_material("DarkSteel")
    tip_mat = palette_material("Polymer")
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=depth,
                                         location=(0, 0, depth / 2))
    _add(bpy.context.active_object, body_mat)
    sockets = []
    if cap == "dome":
        bpy.ops.mesh.primitive_uv_sphere_add(radius=radius * 0.95, location=(0, 0, depth))
        c = bpy.context.active_object
        c.scale.z = 0.5; bpy.ops.object.transform_apply(scale=True)
        _add(c, cap_mat)
        sockets.append({"name": "Cap", "loc": (0, 0, depth + radius * 0.5)})
    elif cap == "cone":
        bpy.ops.mesh.primitive_cone_add(radius1=radius, radius2=radius * 0.3,
                                        depth=depth * 0.25,
                                        location=(0, 0, depth + depth * 0.125))
        _add(bpy.context.active_object, cap_mat)
        sockets.append({"name": "Cap", "loc": (0, 0, depth + depth * 0.25)})
    elif cap == "nozzle":
        bpy.ops.mesh.primitive_cylinder_add(radius=radius * 0.4, depth=depth * 0.3,
                                             location=(0, 0, depth + depth * 0.15))
        _add(bpy.context.active_object, nozzle_mat)
        bpy.ops.mesh.primitive_uv_sphere_add(radius=radius * 0.2,
                                              location=(0, 0, depth + depth * 0.32))
        _add(bpy.context.active_object, tip_mat)
        sockets.append({"name": "Cap", "loc": (0, 0, depth + depth * 0.4)})
    _finalize_handheld(f"SM_{item_id}", sockets=sockets)


def build_pouch(item_id, body_mat, w=0.18, d=0.12, h=0.14):
    """Soft bag/pouch — rounded cuboid with a tied neck."""
    clear_scene()
    tie_mat = get_or_create_material(f"Pouch_Tie_{item_id}",
                                      (body_mat.diffuse_color[0] * 0.55,
                                       body_mat.diffuse_color[1] * 0.55,
                                       body_mat.diffuse_color[2] * 0.55, 1.0),
                                      roughness=0.85)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=1.0, location=(0, 0, h / 2))
    body = bpy.context.active_object
    body.scale = (w, d, h * 0.55); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=w * 0.25, depth=h * 0.25, location=(0, 0, h * 0.95))
    _add(bpy.context.active_object, tie_mat)
    _finalize_handheld(f"SM_{item_id}",
                        sockets=[{"name": "TiePoint", "loc": (0, 0, h * 1.05)}])


def build_torus_roll(item_id, body_mat, major=0.07, minor=0.02):
    """Tape roll / fiber roll — torus with inner core."""
    clear_scene()
    core_mat = get_or_create_material(f"Roll_Core_{item_id}",
                                       (body_mat.diffuse_color[0] * 0.75,
                                        body_mat.diffuse_color[1] * 0.75,
                                        body_mat.diffuse_color[2] * 0.75, 1.0),
                                       roughness=0.85)
    bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor, location=(0, 0, minor))
    _add(bpy.context.active_object, body_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=major * 0.55, depth=minor * 1.6, location=(0, 0, minor))
    _add(bpy.context.active_object, core_mat)
    _finalize_handheld(f"SM_{item_id}")


def build_slab(item_id, body_mat, w=0.14, d=0.10, h=0.012):
    """Flat slab — pad, strip, sheet, token, data fragment."""
    clear_scene()
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, h / 2))
    obj = bpy.context.active_object
    obj.scale = (w, d, h); bpy.ops.object.transform_apply(scale=True)
    _add(obj, body_mat)
    _finalize_handheld(f"SM_{item_id}")


def build_battery(item_id, body_mat):
    """Boxy cell with positive terminal."""
    clear_scene()
    term_mat = palette_material("Brass")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.07))
    body = bpy.context.active_object
    body.scale = (0.05, 0.05, 0.14); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.018, depth=0.02, location=(0, 0, 0.15))
    _add(bpy.context.active_object, term_mat)
    _finalize_handheld(f"SM_{item_id}",
                        sockets=[{"name": "Terminal", "loc": (0, 0, 0.16)}])


def build_tool(item_id, body_mat, head_kind):
    """Tool with handle and head. head_kind in {axe, pick, multi, sewing}."""
    clear_scene()
    handle_mat = palette_material("Wood")
    steel_mat = palette_material("Steel")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.02, depth=0.55, location=(0, 0, 0.275))
    handle = bpy.context.active_object
    _add(handle, handle_mat)

    sockets = [{"name": "Grip", "loc": (0, 0, 0.05)}]

    if head_kind == "axe":
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0.07, 0, 0.55))
        head = bpy.context.active_object
        head.scale = (0.13, 0.04, 0.10); bpy.ops.object.transform_apply(scale=True)
        _add(head, steel_mat)
        sockets.append({"name": "BusinessEnd", "loc": (0.13, 0, 0.55)})
    elif head_kind == "pick":
        bpy.ops.mesh.primitive_cone_add(radius1=0.03, radius2=0.0, depth=0.20, location=(0.10, 0, 0.55))
        h = bpy.context.active_object
        h.rotation_euler.y = math.pi / 2
        _add(h, steel_mat)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.55))
        sleeve = bpy.context.active_object
        sleeve.scale = (0.05, 0.04, 0.05); bpy.ops.object.transform_apply(scale=True)
        _add(sleeve, steel_mat)
        sockets.append({"name": "BusinessEnd", "loc": (0.20, 0, 0.55)})
    elif head_kind == "multi":
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.55))
        head = bpy.context.active_object
        head.scale = (0.06, 0.025, 0.06); bpy.ops.object.transform_apply(scale=True)
        _add(head, steel_mat)
        bpy.ops.mesh.primitive_cone_add(radius1=0.025, radius2=0.005, depth=0.06, location=(0, 0, 0.65))
        _add(bpy.context.active_object, steel_mat)
        sockets.append({"name": "BusinessEnd", "loc": (0, 0, 0.68)})
    elif head_kind == "sewing":
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.07))
        kit = bpy.context.active_object
        kit.scale = (0.10, 0.06, 0.025); bpy.ops.object.transform_apply(scale=True)
        _add(kit, body_mat)
        # Sewing kit doesn't need the long handle.
        bpy.data.objects.remove(handle, do_unlink=True)
        sockets = [{"name": "Lid", "loc": (0, 0, 0.085)}]
    _finalize_handheld(f"SM_{item_id}", sockets=sockets)


def build_garment(item_id, body_mat, style="vest"):
    """Folded garment silhouette. style in {vest, jacket, barding, underlayer}."""
    clear_scene()
    plate_mat = get_or_create_material(f"Garment_Plate_{item_id}",
                                        (body_mat.diffuse_color[0] * 0.7,
                                         body_mat.diffuse_color[1] * 0.7,
                                         body_mat.diffuse_color[2] * 0.7, 1.0),
                                        roughness=0.85)
    strip_mat = palette_material("DarkSteel")
    if style == "vest":
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.20))
        torso = bpy.context.active_object
        torso.scale = (0.30, 0.10, 0.32); bpy.ops.object.transform_apply(scale=True)
        _add(torso, body_mat)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0.05, 0.20))
        plate = bpy.context.active_object
        plate.scale = (0.20, 0.02, 0.25); bpy.ops.object.transform_apply(scale=True)
        _add(plate, plate_mat)
    elif style == "jacket":
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.22))
        torso = bpy.context.active_object
        torso.scale = (0.32, 0.12, 0.36); bpy.ops.object.transform_apply(scale=True)
        _add(torso, body_mat)
        for side in [-1, 1]:
            bpy.ops.mesh.primitive_cylinder_add(radius=0.06, depth=0.30, location=(side * 0.36, 0, 0.22))
            sleeve = bpy.context.active_object
            sleeve.rotation_euler.y = math.pi / 2
            _add(sleeve, body_mat)
    elif style == "barding":
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.18))
        body = bpy.context.active_object
        body.scale = (0.40, 0.18, 0.20); bpy.ops.object.transform_apply(scale=True)
        _add(body, body_mat)
        for i in range(4):
            bpy.ops.mesh.primitive_cube_add(size=1, location=((i - 1.5) * 0.18, 0.18, 0.18))
            strip = bpy.context.active_object
            strip.scale = (0.07, 0.01, 0.16); bpy.ops.object.transform_apply(scale=True)
            _add(strip, strip_mat)
    else:  # underlayer (folded)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.05))
        fold = bpy.context.active_object
        fold.scale = (0.20, 0.14, 0.04); bpy.ops.object.transform_apply(scale=True)
        _add(fold, body_mat)
    _finalize_handheld(f"SM_{item_id}",
                        sockets=[{"name": "WaistAttach", "loc": (0, 0, 0.05)}])


def build_respirator(item_id, body_mat, beast=False):
    """Mask shell + dual filter cans."""
    clear_scene()
    filter_mat = get_or_create_material(f"Respirator_Filter_{item_id}",
                                         (body_mat.diffuse_color[0] * 0.55,
                                          body_mat.diffuse_color[1] * 0.55,
                                          body_mat.diffuse_color[2] * 0.55, 1.0),
                                         roughness=0.55)
    strap_mat = palette_material("Leather")
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0, 0, 0.10))
    mask = bpy.context.active_object
    mask.scale = (1.0, 0.7, 1.1) if not beast else (1.4, 0.9, 1.3)
    bpy.ops.object.transform_apply(scale=True)
    _add(mask, body_mat)
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.035, depth=0.06, location=(side * 0.08, 0.07, 0.08))
        can = bpy.context.active_object
        can.rotation_euler.x = math.pi / 2
        _add(can, filter_mat)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.10, minor_radius=0.008, location=(0, -0.04, 0.10))
    _add(bpy.context.active_object, strap_mat)
    _finalize_handheld(f"SM_{item_id}",
                        sockets=[{"name": "FaceAttach", "loc": (0, -0.05, 0.10)}])


def build_water_filter(item_id, body_mat):
    """Vertical filter: cylinder + spout + cap."""
    clear_scene()
    cap_mat = palette_material("DarkSteel")
    spout_mat = palette_material("Gunmetal")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.05, depth=0.30, location=(0, 0, 0.15))
    _add(bpy.context.active_object, body_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.055, depth=0.03, location=(0, 0, 0.31))
    _add(bpy.context.active_object, cap_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.06, location=(0.06, 0, 0.05))
    spout = bpy.context.active_object
    spout.rotation_euler.y = math.pi / 2
    _add(spout, spout_mat)
    _finalize_handheld(f"SM_{item_id}",
                        sockets=[{"name": "Spout", "loc": (0.10, 0, 0.05)}])


def build_carrier(item_id, body_mat, style="sled"):
    """Logistics carrier. style in {sled, travois}."""
    clear_scene()
    runner_mat = palette_material("DarkWood")
    rope_mat = palette_material("Leather")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.05))
    platform = bpy.context.active_object
    platform.scale = (0.50, 0.25, 0.04); bpy.ops.object.transform_apply(scale=True)
    _add(platform, body_mat)
    if style == "sled":
        for side in [-1, 1]:
            bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.55, location=(0, side * 0.22, 0.02))
            runner = bpy.context.active_object
            runner.rotation_euler.y = math.pi / 2
            _add(runner, runner_mat)
        bpy.ops.mesh.primitive_torus_add(major_radius=0.04, minor_radius=0.005, location=(0.30, 0, 0.10))
        _add(bpy.context.active_object, rope_mat)
        hitch_loc = (0.32, 0, 0.10)
    else:
        for side in [-1, 1]:
            bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.7, location=(0.05, side * 0.10, 0.13))
            pole = bpy.context.active_object
            pole.rotation_euler = (0, math.pi / 2.4, side * 0.18)
            _add(pole, runner_mat)
        hitch_loc = (0.45, 0, 0.20)
    _finalize_handheld(f"SM_{item_id}",
                        sockets=[{"name": "Hitch", "loc": hitch_loc}])


def build_loose_pile(item_id, body_mat, count=20):
    """Pile of loose granules / strands."""
    clear_scene()
    import random
    random.seed(hash(item_id) & 0xFFFF)
    for _ in range(count):
        x = random.uniform(-0.06, 0.06)
        y = random.uniform(-0.06, 0.06)
        z = random.uniform(0.005, 0.04)
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.012, subdivisions=1, location=(x, y, z))
        _add(bpy.context.active_object, body_mat)
    _finalize_handheld(f"SM_{item_id}")


def build_part(item_id, body_mat, kind="block"):
    """Generic mechanical part. kind in {block, barrel, casing_set, sheet, tub,
    kit_bundle, data_core, token}."""
    clear_scene()
    sockets = []
    if kind == "barrel":
        steel_mat = palette_material("Gunmetal")
        bpy.ops.mesh.primitive_cylinder_add(radius=0.015, depth=0.30, location=(0, 0, 0.15))
        _add(bpy.context.active_object, steel_mat)
        bpy.ops.mesh.primitive_cylinder_add(radius=0.025, depth=0.04, location=(0, 0, 0.30))
        _add(bpy.context.active_object, steel_mat)
    elif kind == "casing_set":
        brass = palette_material("Brass")
        for i in range(6):
            x = (i % 3 - 1) * 0.025
            y = (i // 3 - 0.5) * 0.025
            bpy.ops.mesh.primitive_cylinder_add(radius=0.008, depth=0.04, location=(x, y, 0.02))
            _add(bpy.context.active_object, brass)
        sockets.append({"name": "Center", "loc": (0, 0, 0.04)})
    elif kind == "sheet":
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.005))
        sheet = bpy.context.active_object
        sheet.scale = (0.20, 0.14, 0.008); bpy.ops.object.transform_apply(scale=True)
        _add(sheet, body_mat)
    elif kind == "tub":
        inner_mat = get_or_create_material(f"Tub_Inner_{item_id}",
                                            (body_mat.diffuse_color[0] * 0.5,
                                             body_mat.diffuse_color[1] * 0.5,
                                             body_mat.diffuse_color[2] * 0.5, 1.0),
                                            roughness=0.85)
        bpy.ops.mesh.primitive_cylinder_add(radius=0.10, depth=0.10, location=(0, 0, 0.05))
        _add(bpy.context.active_object, body_mat)
        bpy.ops.mesh.primitive_cylinder_add(radius=0.085, depth=0.085, location=(0, 0, 0.06))
        _add(bpy.context.active_object, inner_mat)
    elif kind == "kit_bundle":
        strap_mat = palette_material("DarkLeather")
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.06))
        wrap = bpy.context.active_object
        wrap.scale = (0.20, 0.10, 0.10); bpy.ops.object.transform_apply(scale=True)
        _add(wrap, body_mat)
        for x in [-0.06, 0.06]:
            bpy.ops.mesh.primitive_torus_add(major_radius=0.10, minor_radius=0.006, location=(x, 0, 0.06))
            strap = bpy.context.active_object
            strap.rotation_euler.y = math.pi / 2
            _add(strap, strap_mat)
        sockets.append({"name": "Center", "loc": (0, 0, 0.12)})
    elif kind == "data_core":
        glow_mat = get_or_create_material(f"DataCore_Glow_{item_id}",
                                           (0.30, 0.85, 0.95, 1.0), roughness=0.20,
                                           emissive=(0.30, 0.85, 0.95, 1.0))
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.025))
        b = bpy.context.active_object
        b.scale = (0.08, 0.05, 0.025); bpy.ops.object.transform_apply(scale=True)
        _add(b, body_mat)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.052))
        stripe = bpy.context.active_object
        stripe.scale = (0.06, 0.01, 0.003); bpy.ops.object.transform_apply(scale=True)
        _add(stripe, glow_mat)
    elif kind == "token":
        ring_mat = get_or_create_material(f"Token_Ring_{item_id}",
                                           (body_mat.diffuse_color[0] * 0.7,
                                            body_mat.diffuse_color[1] * 0.7,
                                            body_mat.diffuse_color[2] * 0.7, 1.0),
                                           roughness=0.55, metallic=0.85)
        bpy.ops.mesh.primitive_cylinder_add(radius=0.04, depth=0.006, location=(0, 0, 0.003))
        _add(bpy.context.active_object, body_mat)
        bpy.ops.mesh.primitive_torus_add(major_radius=0.030, minor_radius=0.003, location=(0, 0, 0.006))
        _add(bpy.context.active_object, ring_mat)
    else:  # block — generic part
        pin_mat = get_or_create_material(f"Block_Pin_{item_id}",
                                          (body_mat.diffuse_color[0] * 0.6,
                                           body_mat.diffuse_color[1] * 0.6,
                                           body_mat.diffuse_color[2] * 0.6, 1.0),
                                          roughness=0.55)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.04))
        block = bpy.context.active_object
        block.scale = (0.10, 0.05, 0.07); bpy.ops.object.transform_apply(scale=True)
        _add(block, body_mat)
        bpy.ops.mesh.primitive_cylinder_add(radius=0.008, depth=0.05, location=(0.05, 0, 0.04))
        pin = bpy.context.active_object
        pin.rotation_euler.y = math.pi / 2
        _add(pin, pin_mat)
    _finalize_handheld(f"SM_{item_id}", sockets=sockets)


# ── Dispatch (token → builder + params) ───────────────────────────────────────

# First match wins. Ordered so longer / more specific tokens go before shorter ones.
DISPATCH = [
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
    for token, fn, kwargs in DISPATCH:
        if token in item_id:
            return fn, dict(kwargs)
    return None


# ── Main export pipeline ───────────────────────────────────────────────────────

def main():
    print("\n=== Quiet Rift: Enigma — Handheld Items Asset Generator (Batch 3 detail upgrade) ===")
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
        body_mat = _category_mat(category)
        match = dispatch_for(item_id)
        if match is None:
            unmatched.append(item_id)
            build_part(item_id, body_mat, kind="block")
            continue
        fn, kwargs = match
        kwargs.setdefault("body_mat", body_mat)
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
    print("Each FBX exports SM_<id> + UCX_ collision; tools / pouches / containers /")
    print("tubes / batteries / garments / respirator / water filter / carriers / kit")
    print("bundles also export their meaningful SOCKET_* attach points.")


if __name__ == "__main__":
    main()
