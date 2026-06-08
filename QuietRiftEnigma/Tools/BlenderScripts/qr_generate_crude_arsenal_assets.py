r"""
qr_generate_crude_arsenal_assets.py -- Blender generators for the crude
survival arsenal: daggers, swords, axes, spears, picks, clubs, bows,
crossbow, sling, and the shield ladder. One FBX per item id, exported
to Content/Meshes/weapons_assets/ so qr_seed_crude_arsenal.py's item
defs pick them up automatically.

Convention (matches qr_generate_weapons_assets_assets.py):
    +X = "muzzle" direction (blade tip / arrow nose).
    Origin at the front of the grip (where the hand sits).

Run from Blender CLI:
    cd D:\QuietRiftEnigma\Jim\QuietRiftEnigma\Tools\BlenderScripts
    blender --background --python qr_generate_crude_arsenal_assets.py

Or from inside Blender's Scripting tab: open + Run.
"""

import bpy
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from qr_blender_common import clear_scene, export_fbx  # noqa: E402
from qr_blender_detail import (  # noqa: E402
    palette_material,
    assign_material,
    add_socket,
    finalize_asset,
)


OUTPUT_DIR = os.path.join(os.path.dirname(__file__),
                          "../../Content/Meshes/weapons_assets")

METALS = ["FERRIC", "BLACKGLASS", "MAGNET", "SUNWIRE",
          "SPARKSTONE", "FROSTSPARK", "REMNANT"]
METAL_PALETTE = {
    "FERRIC":     "Steel",
    "BLACKGLASS": "DarkSteel",
    "MAGNET":     "Gunmetal",
    "SUNWIRE":    "Brass",
    "SPARKSTONE": "Brass",
    "FROSTSPARK": "Glass",
    "REMNANT":    "Gunmetal",
}


def _add(obj, mat_name):
    """Assign a palette material (creating it if necessary)."""
    assign_material(obj, palette_material(mat_name))
    return obj


def _cube(name, location, scale, mat_name):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    o = bpy.context.active_object
    o.name = name
    o.scale = scale
    bpy.ops.object.transform_apply(scale=True)
    _add(o, mat_name)
    return o


def _cyl(name, radius, depth, location, mat_name, axis_y_rot=0.0):
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=depth, location=location)
    o = bpy.context.active_object
    o.name = name
    o.rotation_euler.y = axis_y_rot
    bpy.ops.object.transform_apply(rotation=True)
    _add(o, mat_name)
    return o


# ─── Blade-shape helpers ─────────────────────────────────────────────

def _blade(length, half_w, thickness, x_origin, mat="Steel"):
    """Flat blade as a cube: tip at +X. Origin at the cross-guard root."""
    _cube("Blade",
          location=(x_origin + length / 2, 0, 0),
          scale=(length, half_w * 2, thickness),
          mat_name=mat)


def _guard(width, depth, x_origin, mat="Steel"):
    _cube("Guard",
          location=(x_origin, 0, 0),
          scale=(0.03, width, depth),
          mat_name=mat)


def _haft(length, radius, x_origin, mat="Wood"):
    _cyl("Haft", radius=radius, depth=length,
         location=(x_origin - length / 2, 0, 0),
         mat_name=mat, axis_y_rot=math.pi / 2)


def _pommel(x_origin, radius=0.025, mat="Steel"):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=radius, location=(x_origin, 0, 0))
    o = bpy.context.active_object
    o.name = "Pommel"
    _add(o, mat)
    return o


# ─── Crude / generic ─────────────────────────────────────────────────

def gen_dagger(metal=None):
    clear_scene()
    mat = METAL_PALETTE.get(metal, "Steel")
    _haft(0.10, 0.012, 0.0, mat="Wood")
    _guard(0.06, 0.012, 0.005, mat=mat)
    _blade(0.20, 0.012, 0.010, 0.005, mat=mat)
    _pommel(-0.10)


def gen_sword(metal=None):
    clear_scene()
    mat = METAL_PALETTE.get(metal, "Steel")
    _haft(0.13, 0.014, 0.0, mat="Wood")
    _guard(0.10, 0.014, 0.008, mat=mat)
    _blade(0.55, 0.020, 0.010, 0.008, mat=mat)
    _pommel(-0.13)


def gen_machete():
    clear_scene()
    _haft(0.12, 0.014, 0.0, mat="Wood")
    _guard(0.06, 0.012, 0.006, mat="Steel")
    # Wide curved-ish blade (cube approximation)
    _cube("Blade", location=(0.20, 0, 0), scale=(0.40, 0.04, 0.008), mat_name="Steel")


def gen_hatchet():
    clear_scene()
    _haft(0.32, 0.012, 0.0, mat="Wood")
    # Head: a wedge in front of the grip
    _cube("Head", location=(0.04, 0, 0.025), scale=(0.10, 0.030, 0.090), mat_name="Steel")
    _cube("Edge", location=(0.10, 0, 0.025), scale=(0.04, 0.026, 0.080), mat_name="DarkSteel")


def gen_stone_axe():
    clear_scene()
    _haft(0.36, 0.013, 0.0, mat="Wood")
    _cube("Stone", location=(0.05, 0, 0.025), scale=(0.10, 0.040, 0.080), mat_name="Stone")


def gen_axe(metal=None):
    """Two-handed metal axe."""
    clear_scene()
    mat = METAL_PALETTE.get(metal, "Steel")
    _haft(0.55, 0.014, 0.0, mat="Wood")
    _cube("Head", location=(0.06, 0, 0.030), scale=(0.14, 0.034, 0.120), mat_name=mat)
    _cube("Edge", location=(0.14, 0, 0.030), scale=(0.04, 0.030, 0.110), mat_name="DarkSteel")


def gen_pickaxe():
    clear_scene()
    _haft(0.55, 0.014, 0.0, mat="Wood")
    # Crossed pick + spike
    _cube("Head", location=(0.02, 0, 0.020), scale=(0.05, 0.05, 0.020), mat_name="Steel")
    _cube("Pick", location=(0.13, 0, 0.020), scale=(0.18, 0.020, 0.020), mat_name="Steel")
    _cube("Spike", location=(-0.10, 0, 0.020), scale=(0.13, 0.020, 0.020), mat_name="Steel")


def gen_spear(metal=None, length=2.0):
    clear_scene()
    mat = METAL_PALETTE.get(metal, "Steel")
    _haft(length, 0.012, 0.0, mat="Wood")
    # Head at the tip
    _cube("Head", location=(0.08, 0, 0), scale=(0.18, 0.025, 0.010), mat_name=mat)


def gen_stone_spear():
    gen_spear(metal=None, length=2.0)
    # Override head material to stone
    head = bpy.data.objects.get("Head")
    if head:
        assign_material(head, palette_material("Stone"))


def gen_wood_club():
    clear_scene()
    _haft(0.45, 0.020, 0.0, mat="Wood")
    _cube("Knob", location=(-0.45, 0, 0), scale=(0.10, 0.060, 0.060), mat_name="Wood")


def gen_knuckle_duster():
    clear_scene()
    _cube("Ring", location=(0.0, 0, 0), scale=(0.025, 0.10, 0.030), mat_name="Steel")
    for x in (-0.022, -0.007, 0.008, 0.023):
        _cube("Stud_{:+}".format(int(x * 1000)),
              location=(0.020, x, 0.012),
              scale=(0.012, 0.010, 0.012), mat_name="DarkSteel")


def gen_bone_knife():
    clear_scene()
    _haft(0.10, 0.012, 0.0, mat="Bone")
    _blade(0.18, 0.012, 0.008, 0.0, mat="Bone")


# ─── Bows ────────────────────────────────────────────────────────────

def gen_shortbow():
    clear_scene()
    # Limbs (two arcs approximated as long thin cubes)
    _cube("UpperLimb", location=(0, 0, 0.40), scale=(0.020, 0.025, 0.45), mat_name="Wood")
    _cube("LowerLimb", location=(0, 0, -0.40), scale=(0.020, 0.025, 0.45), mat_name="Wood")
    _cube("Grip",      location=(0, 0, 0),    scale=(0.040, 0.04, 0.10),  mat_name="Leather")
    # String
    _cube("String",    location=(-0.04, 0, 0), scale=(0.002, 0.004, 1.30), mat_name="Cord")


def gen_recurve_bow():
    gen_shortbow()
    # Slightly larger curved tips: add cap blocks
    _cube("TipUp",   location=(0.02, 0, 0.86),  scale=(0.020, 0.020, 0.030), mat_name="Wood")
    _cube("TipDown", location=(0.02, 0, -0.86), scale=(0.020, 0.020, 0.030), mat_name="Wood")


def gen_crossbow():
    clear_scene()
    # Stock
    _cube("Stock", location=(-0.10, 0, 0),  scale=(0.50, 0.04, 0.06), mat_name="Wood")
    _cube("Grip",  location=(-0.30, 0, -0.04), scale=(0.04, 0.03, 0.10), mat_name="Wood")
    # Prod (limbs across the front)
    _cube("Prod",  location=(0.16, 0, 0.03), scale=(0.025, 0.50, 0.025), mat_name="Steel")
    # String + bolt groove
    _cube("Groove", location=(0.05, 0, 0.05), scale=(0.30, 0.02, 0.012), mat_name="DarkSteel")


def gen_sling():
    clear_scene()
    # Two cords + a leather pouch
    _cube("PouchA", location=(0, -0.05, 0), scale=(0.001, 0.10, 0.40), mat_name="Cord")
    _cube("PouchB", location=(0,  0.05, 0), scale=(0.001, 0.10, 0.40), mat_name="Cord")
    _cube("Pouch",  location=(0, 0, -0.20), scale=(0.10, 0.04, 0.04),  mat_name="Leather")


# ─── Shields ─────────────────────────────────────────────────────────

def gen_wood_shield():
    clear_scene()
    _cube("Board",  location=(0.05, 0, 0), scale=(0.10, 0.55, 0.75), mat_name="Wood")
    _cube("Boss",   location=(0.11, 0, 0), scale=(0.04, 0.12, 0.12), mat_name="Steel")
    _cube("StrapL", location=(-0.01, 0.10, 0), scale=(0.04, 0.04, 0.20), mat_name="Leather")
    _cube("StrapR", location=(-0.01,-0.10, 0), scale=(0.04, 0.04, 0.20), mat_name="Leather")


def gen_scrap_shield():
    clear_scene()
    _cube("Plate1", location=(0.05, -0.18, 0.20), scale=(0.08, 0.20, 0.30), mat_name="Steel")
    _cube("Plate2", location=(0.05,  0.18, 0.20), scale=(0.08, 0.20, 0.30), mat_name="DarkSteel")
    _cube("Plate3", location=(0.05, 0.0, -0.20), scale=(0.08, 0.50, 0.30), mat_name="Steel")
    _cube("Boss",   location=(0.11, 0, 0),       scale=(0.04, 0.12, 0.12), mat_name="Gunmetal")


def gen_riot_shield():
    clear_scene()
    # Transparent body + a steel frame
    _cube("Body",   location=(0.04, 0, 0), scale=(0.05, 0.55, 0.85), mat_name="Glass")
    _cube("FrameT", location=(0.06, 0, 0.42), scale=(0.08, 0.55, 0.04), mat_name="DarkSteel")
    _cube("FrameB", location=(0.06, 0, -0.42), scale=(0.08, 0.55, 0.04), mat_name="DarkSteel")
    _cube("FrameL", location=(0.06,  0.26, 0), scale=(0.08, 0.04, 0.85), mat_name="DarkSteel")
    _cube("FrameR", location=(0.06, -0.26, 0), scale=(0.08, 0.04, 0.85), mat_name="DarkSteel")


def gen_plasma_shield():
    """Halo-style emitter on a forearm bracer; the actual energy field is
    a runtime FX, so the mesh is just the bracer + projector ring."""
    clear_scene()
    _cyl("Bracer", radius=0.06, depth=0.20, location=(-0.05, 0, 0),
         mat_name="Gunmetal", axis_y_rot=math.pi / 2)
    _cyl("Emitter", radius=0.10, depth=0.020, location=(0.08, 0, 0),
         mat_name="Brass", axis_y_rot=math.pi / 2)
    # A glowing ring (use Glass for a translucent stand-in)
    _cyl("FieldRing", radius=0.18, depth=0.010, location=(0.10, 0, 0),
         mat_name="Glass", axis_y_rot=math.pi / 2)


# ─── Generator registry ──────────────────────────────────────────────

CRUDE_GENERATORS = {
    "WPN_DAGGER":         lambda: gen_dagger(),
    "WPN_BONE_KNIFE":     gen_bone_knife,
    "WPN_MACHETE":        gen_machete,
    "WPN_HATCHET":        gen_hatchet,
    "WPN_STONE_AXE":      gen_stone_axe,
    "WPN_WOOD_CLUB":      gen_wood_club,
    "WPN_STONE_SPEAR":    gen_stone_spear,
    "WPN_PICKAXE":        gen_pickaxe,
    "WPN_KNUCKLE_DUSTER": gen_knuckle_duster,
}

BOW_GENERATORS = {
    "WPN_SHORTBOW":    gen_shortbow,
    "WPN_RECURVE_BOW": gen_recurve_bow,
    "WPN_CROSSBOW":    gen_crossbow,
    "WPN_SLING":       gen_sling,
}

SHIELD_GENERATORS = {
    "WPN_WOOD_SHIELD":   gen_wood_shield,
    "WPN_SCRAP_SHIELD":  gen_scrap_shield,
    "WPN_RIOT_SHIELD":   gen_riot_shield,
    "WPN_PLASMA_SHIELD": gen_plasma_shield,
}


def _finalize_and_export(name):
    add_socket("Grip", location=(0, 0, 0))
    finalize_asset(name,
                   bevel_width=0.0025, bevel_angle_deg=30,
                   smooth_angle_deg=55, collision="convex",
                   lods=[0.50], pivot="geometry_center")
    out_path = os.path.join(OUTPUT_DIR, "{}.fbx".format(name))
    export_fbx(name, out_path)
    print("  -> {}".format(out_path))


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    total = 0

    print("=== Crude melee ===")
    for wid, gen in CRUDE_GENERATORS.items():
        print("[{}]".format(wid))
        gen()
        _finalize_and_export("SM_" + wid)
        total += 1

    print("\n=== Metal-tier blades ===")
    for metal in METALS:
        for kind, fn in (("SWORD", gen_sword),
                         ("AXE",   gen_axe),
                         ("SPEAR", gen_spear)):
            wid = "WPN_{}_{}".format(metal, kind)
            print("[{}]".format(wid))
            fn(metal=metal) if fn.__code__.co_argcount > 0 else fn()
            _finalize_and_export("SM_" + wid)
            total += 1

    print("\n=== Bows ===")
    for wid, gen in BOW_GENERATORS.items():
        print("[{}]".format(wid))
        gen()
        _finalize_and_export("SM_" + wid)
        total += 1

    print("\n=== Shields ===")
    for wid, gen in SHIELD_GENERATORS.items():
        print("[{}]".format(wid))
        gen()
        _finalize_and_export("SM_" + wid)
        total += 1

    print("\n=== Done: {} FBX exported to {}".format(
        total, os.path.abspath(OUTPUT_DIR)))


if __name__ == "__main__":
    main()
