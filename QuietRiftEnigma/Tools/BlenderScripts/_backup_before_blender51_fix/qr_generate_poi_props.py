"""
Quiet Rift: Enigma â€” POI Archetype Set-Dressing Generator (Blender 4.x)

Upgraded in Batch 8 of the Blender detail pass â€” every POI archetype
flows through qr_blender_detail.py for production finalization (palette
dedupe, smooth shading, bevels, smart UV, sockets, UCX collision, LOD
chain).

Reads every row in DT_POIArchetypes.csv and exports one composed-scene
FBX per archetype. Wreck-type archetypes share a _wreck_floor helper
for visual consistency; world-feature archetypes use bespoke geometry.

Per-archetype sockets (gameplay-critical):
    Wreck archetypes:
        SOCKET_LootSpawnA / SOCKET_LootSpawnB â€” where loot containers spawn
        SOCKET_PlayerEntry                     â€” natural player approach point
    Cave / Sinkhole / Vent:
        SOCKET_Entrance, SOCKET_Interior, plus archetype-specific points
    Comms Relay:
        SOCKET_DishCenter, SOCKET_TowerBase
    Cryo Pod Cluster:
        SOCKET_PodA-D
    Meteor Impact:
        SOCKET_CraterCenter, SOCKET_ChunkA-C
    Remnant Site:
        SOCKET_StudyPoint, SOCKET_ApexCrest

Random scatter is seeded per-archetype so output is stable.
Filenames sanitize the slash in FloodedSinkhole/IceTunnel.

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

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/poi_props")
CSV_PATH = os.path.join(
    os.path.dirname(__file__),
    "../../Content/QuietRift/Data/DT_POIArchetypes.csv",
)


def _add(obj, mat):
    assign_material(obj, mat)
    return obj


def _slab(x, y, z, w, d, h, mat, name):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z))
    obj = bpy.context.active_object
    obj.scale = (w, d, h); bpy.ops.object.transform_apply(scale=True)
    _add(obj, mat); obj.name = name
    return obj


# Bespoke palette materials shared across POI archetypes.
def _hull():     return get_or_create_material("POI_Hull",     (0.45, 0.42, 0.40, 1.0), roughness=0.85)
def _scorched(): return get_or_create_material("POI_Scorched", (0.18, 0.16, 0.14, 1.0), roughness=0.95)
def _ore():      return get_or_create_material("POI_Ore",      (0.55, 0.45, 0.30, 1.0),
                                                 roughness=0.55, emissive=(0.40, 0.30, 0.10, 0.6))
def _ice():      return get_or_create_material("POI_Ice",      (0.55, 0.75, 0.88, 0.7), roughness=0.20)
def _water():    return get_or_create_material("POI_Water",    (0.20, 0.35, 0.50, 0.6), roughness=0.05)
def _steam():    return get_or_create_material("POI_Steam",    (0.92, 0.92, 0.95, 0.4),
                                                 roughness=0.20, emissive=(0.85, 0.85, 0.90, 0.5))
def _ammo_box(): return get_or_create_material("POI_AmmoBox",  (0.35, 0.40, 0.25, 1.0))
def _toolbox(): return get_or_create_material("POI_Toolbox",  (0.55, 0.20, 0.10, 1.0))
def _crate():   return get_or_create_material("POI_FoodCrate",(0.50, 0.35, 0.20, 1.0))
def _shelf():   return get_or_create_material("POI_Shelf",    (0.45, 0.32, 0.20, 1.0))
def _luggage(): return get_or_create_material("POI_Luggage",  (0.30, 0.20, 0.15, 1.0))
def _cloth():   return get_or_create_material("POI_Cloth",    (0.55, 0.42, 0.30, 1.0))
def _grain():   return get_or_create_material("POI_Grain",    (0.85, 0.72, 0.40, 1.0))
def _frost():   return get_or_create_material("POI_Frost",    (0.85, 0.92, 0.95, 1.0),
                                                 roughness=0.30, emissive=(0.55, 0.75, 0.88, 0.4))


def _wreck_floor(width=4.0, depth=3.0, tilt=0.12):
    """Twisted scorched deck plate with seam lines + bolt pattern."""
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.05))
    floor = bpy.context.active_object
    floor.scale = (width, depth, 0.10)
    floor.rotation_euler = (tilt, -tilt * 0.6, 0)
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    _add(floor, _scorched()); floor.name = "Wreck_Floor"
    for i in range(5):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(width / 2 - 0.1 - i * 0.1, depth / 2 - 0.05, 0.06 + i * 0.04))
        sh = bpy.context.active_object
        sh.scale = (0.10, 0.05, 0.08 + i * 0.02)
        sh.rotation_euler.z = i * 0.4
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(sh, _hull()); sh.name = f"Wreck_Tear_{i}"
    # Decking panel seams running across the floor
    for sx in [-width / 4, width / 4]:
        add_panel_seam_strip((sx, -depth / 2 + 0.1, 0.105), (sx, depth / 2 - 0.1, 0.105),
                              width=0.04, depth=0.012, material_name="DarkSteel",
                              name=f"Wreck_FloorSeam_{int(sx*10)}")
    # Rivet rows along one edge
    add_rivet_grid(origin=(-width / 2 + 0.2, -depth / 2 + 0.15, 0.11),
                    spacing=(0.4, 0.0), rows=1, cols=int(width / 0.4) - 1,
                    rivet_radius=0.02, depth=0.012, normal_axis='Z',
                    material_name="DarkSteel")


def _finalize_poi(name, sockets, lods=(0.40, 0.15), pivot="bottom_center", collision="convex"):
    for s in sockets:
        add_socket(s["name"], location=s.get("loc", (0, 0, 0)),
                    rotation=s.get("rot", (0, 0, 0)))
    finalize_asset(name,
                    bevel_width=0.004, bevel_angle_deg=30,
                    smooth_angle_deg=50, collision=collision,
                    lods=list(lods), pivot=pivot)


# â”€â”€ Wreck Archetypes â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def gen_armory_wreck():
    """Tilted gun rack + ammo crate stack on scorched deck."""
    clear_scene()
    _wreck_floor()
    rack_mat = palette_material("DarkSteel")
    bar_mat = palette_material("Steel")
    _slab(-1.0, 0.6, 0.95, 0.10, 0.30, 1.80, rack_mat, "Armory_RackFrame")
    for z in [0.40, 1.00, 1.60]:
        _slab(-1.0, 0.5, z, 0.06, 0.50, 0.04, bar_mat, f"Armory_RackBar_{int(z*100)}")
    for i in range(3):
        z = 0.42 + i * 0.6
        _slab(-1.0, 0.7, z, 0.10, 0.05, 0.50, bar_mat, f"Armory_Weapon_{i}")
    _slab(0.8, -0.4, 0.30, 0.50, 0.40, 0.60, _ammo_box(), "Armory_AmmoCrate_Lo")
    _slab(0.6, -0.3, 0.85, 0.45, 0.35, 0.50, _ammo_box(), "Armory_AmmoCrate_Hi")
    brass_mat = palette_material("Brass")
    for _ in range(6):
        x = random.uniform(-0.5, 0.5); y = random.uniform(-1.0, -0.4)
        bpy.ops.mesh.primitive_cylinder_add(radius=0.02, depth=0.04, location=(x, y, 0.12))
        c = bpy.context.active_object
        c.rotation_euler = (random.random(), 0, random.random() * math.tau)
        _add(c, brass_mat); c.name = "Armory_Brass"
    _finalize_poi("SM_POI_ArmoryWreck", sockets=[
        {"name": "LootSpawnA", "loc": (-1.0, 0.7, 0.95)},
        {"name": "LootSpawnB", "loc": (0.7, -0.35, 0.85)},
        {"name": "PlayerEntry", "loc": (2.0, 0, 0)},
    ])


def gen_avionics_wreck():
    """Tilted console bank + cracked displays + bent antenna."""
    clear_scene()
    _wreck_floor()
    console_mat = palette_material("DarkSteel")
    display_mat = palette_material("GlowCyan")
    mast_mat = palette_material("DarkSteel")
    dish_mat = palette_material("Steel")
    _slab(-0.6, 0.5, 0.55, 1.6, 0.6, 1.10, console_mat, "Avi_Console_Body")
    for i in range(3):
        x = -1.1 + i * 0.55
        _slab(x, 0.81, 0.95, 0.42, 0.02, 0.30, display_mat, f"Avi_Display_{i}")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.025, depth=2.0, location=(0.9, 0.4, 1.05))
    mast = bpy.context.active_object
    mast.rotation_euler = (0.4, 0, 0)
    _add(mast, mast_mat); mast.name = "Avi_Antenna"
    bpy.ops.mesh.primitive_cone_add(radius1=0.30, radius2=0.0, depth=0.10, location=(0.9, 1.2, 1.95))
    dish = bpy.context.active_object
    dish.rotation_euler = (math.pi / 2, 0, 0)
    _add(dish, dish_mat); dish.name = "Avi_Dish"
    _finalize_poi("SM_POI_AvionicsWreck", sockets=[
        {"name": "LootSpawnA", "loc": (-0.6, 0.5, 1.10)},
        {"name": "LootSpawnB", "loc": (0.9, 1.2, 1.95)},
        {"name": "PlayerEntry", "loc": (-2.0, 0, 0)},
    ])


def gen_engineering_wreck():
    """Cylindrical machinery hulk + scattered tools + bent pipe."""
    clear_scene()
    _wreck_floor()
    cyl_mat = palette_material("Steel")
    cap_mat = palette_material("DarkSteel")
    pipe_mat = palette_material("Copper")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.55, depth=2.0, location=(-0.4, 0, 0.65))
    cyl = bpy.context.active_object
    cyl.rotation_euler.y = math.pi / 2; cyl.rotation_euler.z = 0.2
    _add(cyl, cyl_mat); cyl.name = "Eng_Cylinder"
    bpy.ops.mesh.primitive_cylinder_add(radius=0.50, depth=0.06, location=(0.6, 0, 0.65))
    cap = bpy.context.active_object
    cap.rotation_euler.y = math.pi / 2
    _add(cap, cap_mat); cap.name = "Eng_Cap"
    bpy.ops.mesh.primitive_cylinder_add(radius=0.06, depth=0.8, location=(0.9, 0.3, 0.65))
    pipe = bpy.context.active_object
    pipe.rotation_euler = (0, 0.6, 0.4)
    _add(pipe, pipe_mat); pipe.name = "Eng_Pipe"
    for sx, sy in [(0.6, -0.8), (-0.9, 0.8)]:
        _slab(sx, sy, 0.18, 0.30, 0.20, 0.20, _toolbox(), "Eng_Toolbox")
    add_rivet_grid(origin=(0.55, -0.45, 0.66), spacing=(0.0, 0.18),
                    rows=6, cols=1, rivet_radius=0.018, depth=0.01,
                    normal_axis='X', material_name="DarkSteel")
    _finalize_poi("SM_POI_EngineeringWreck", sockets=[
        {"name": "LootSpawnA", "loc": (-0.4, 0, 1.10)},
        {"name": "LootSpawnB", "loc": (0.6, -0.8, 0.30)},
        {"name": "PlayerEntry", "loc": (2.0, 0, 0)},
    ])


def gen_food_wreck():
    """Broken pantry shelving + spilled grain + dented food crates."""
    clear_scene()
    _wreck_floor(width=3.5, depth=3.0)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.4, 0.7, 0.85))
    shelf = bpy.context.active_object
    shelf.scale = (1.4, 0.30, 1.50); shelf.rotation_euler.z = 0.3; shelf.rotation_euler.x = 0.2
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    _add(shelf, _shelf()); shelf.name = "Food_Shelf"
    board_mat = get_or_create_material("POI_FoodBoard", (0.30, 0.20, 0.12, 1.0), roughness=0.95)
    for z in [0.4, 0.85, 1.30]:
        _slab(-0.4, 0.7, z, 1.30, 0.32, 0.04, board_mat, f"Food_Board_{int(z*100)}")
    _slab(0.8, -0.5, 0.25, 0.55, 0.45, 0.50, _crate(), "Food_Crate_1")
    _slab(0.4, -0.9, 0.20, 0.45, 0.40, 0.40, _crate(), "Food_Crate_2")
    grain_mat = _grain()
    for _ in range(15):
        x = random.uniform(0.2, 0.9); y = random.uniform(-0.6, -0.1); z = random.uniform(0.10, 0.18)
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.04, subdivisions=1, location=(x, y, z))
        _add(bpy.context.active_object, grain_mat)
    _finalize_poi("SM_POI_FoodWreck", sockets=[
        {"name": "LootSpawnA", "loc": (-0.4, 0.7, 0.85)},
        {"name": "LootSpawnB", "loc": (0.7, -0.7, 0.50)},
        {"name": "PlayerEntry", "loc": (-2.0, 0, 0)},
    ])


def gen_galley_wreck():
    """Toppled galley oven + broken prep table + scattered cookware."""
    clear_scene()
    _wreck_floor()
    oven_mat = palette_material("Steel")
    door_mat = palette_material("DarkSteel")
    pot_mat = palette_material("DarkSteel")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.6, 0.4, 0.40))
    oven = bpy.context.active_object
    oven.scale = (0.80, 0.70, 0.80); oven.rotation_euler = (0, 0, 0.4)
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    _add(oven, oven_mat); oven.name = "Galley_Oven"
    _slab(-0.20, 0.45, 0.40, 0.10, 0.55, 0.55, door_mat, "Galley_OvenDoor")
    _slab(0.7, -0.2, 0.42, 1.20, 0.6, 0.04, oven_mat, "Galley_TableTop")
    _slab(1.20, 0.05, 0.20, 0.06, 0.06, 0.40, door_mat, "Galley_Leg")
    for sx, sy, r in [(0.3, 0.7, 0.18), (-0.2, -0.6, 0.14)]:
        bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=(sx, sy, 0.10))
        pot = bpy.context.active_object
        pot.scale = (1, 1, 0.55); bpy.ops.object.transform_apply(scale=True)
        _add(pot, pot_mat); pot.name = "Galley_Pot"
    _finalize_poi("SM_POI_GalleyWreck", sockets=[
        {"name": "LootSpawnA", "loc": (-0.6, 0.4, 0.80)},
        {"name": "LootSpawnB", "loc": (0.7, -0.2, 0.55)},
        {"name": "PlayerEntry", "loc": (2.0, 0, 0)},
    ])


def gen_luggage_wreck():
    """Pile of personal-effect cases of varying sizes + spilled cloth."""
    clear_scene()
    _wreck_floor(width=3.0, depth=3.0)
    luggage_mat = _luggage()
    strap_mat = palette_material("DarkSteel")
    bag_sizes = [(0.5, 0.3, 0.5), (0.4, 0.25, 0.4), (0.6, 0.35, 0.45),
                 (0.35, 0.22, 0.35), (0.55, 0.30, 0.50), (0.45, 0.28, 0.40)]
    positions = [(-0.5, 0.5), (0.3, 0.7), (-0.7, -0.3), (0.6, -0.5), (0.0, 0.0), (-0.2, -0.8)]
    heights = [0.25, 0.20, 0.42, 0.18, 0.55, 0.20]
    for (w, d, h), (x, y), z in zip(bag_sizes, positions, heights):
        _slab(x, y, z, w, d, h, luggage_mat, "Luggage_Case")
        _slab(x, y + d / 2 + 0.005, z, w + 0.02, 0.01, h * 0.18, strap_mat, "Luggage_Strap")
    cloth_mat = _cloth()
    for _ in range(4):
        x = random.uniform(-0.8, 0.8); y = random.uniform(-1.0, 1.0)
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, location=(x, y, 0.13))
        cloth = bpy.context.active_object
        cloth.scale = (1.4, 1.0, 0.25); bpy.ops.object.transform_apply(scale=True)
        _add(cloth, cloth_mat); cloth.name = "Luggage_Cloth"
    _finalize_poi("SM_POI_LuggageWreck", sockets=[
        {"name": "LootSpawnA", "loc": (-0.5, 0.5, 0.50)},
        {"name": "LootSpawnB", "loc": (0.6, -0.5, 0.40)},
        {"name": "LootSpawnC", "loc": (0.0, 0.0, 0.55)},
        {"name": "PlayerEntry", "loc": (-1.6, 0, 0)},
    ])


def gen_medbay_wreck():
    """Bed frame + IV stand + broken cabinet."""
    clear_scene()
    _wreck_floor()
    steel_mat = palette_material("Steel")
    dark_steel = palette_material("DarkSteel")
    mattress_mat = get_or_create_material("POI_Mattress", (0.85, 0.85, 0.82, 1.0), roughness=0.95)
    iv_bag_mat = get_or_create_material("POI_IVBag", (0.95, 0.85, 0.55, 0.7), roughness=0.20)
    _slab(-0.4, 0.5, 0.40, 1.80, 0.80, 0.08, steel_mat, "Med_BedTop")
    for sx, sy in [(0.5, 0.85), (-1.3, 0.85), (0.5, 0.15), (-1.3, 0.15)]:
        _slab(sx, sy, 0.20, 0.06, 0.06, 0.40, dark_steel, "Med_BedLeg")
    _slab(-0.4, 0.5, 0.46, 1.70, 0.70, 0.06, mattress_mat, "Med_Mattress")
    _slab(0.7, 0.9, 0.85, 0.04, 0.04, 1.70, steel_mat, "Med_IVPole")
    bpy.ops.mesh.primitive_torus_add(major_radius=0.20, minor_radius=0.015, location=(0.7, 0.9, 0.04))
    _add(bpy.context.active_object, steel_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0.7, 0.85, 1.55))
    bag = bpy.context.active_object
    bag.scale = (0.7, 0.7, 1.4); bpy.ops.object.transform_apply(scale=True)
    _add(bag, iv_bag_mat); bag.name = "Med_IVBag"
    _slab(0.9, -0.6, 0.55, 0.50, 0.40, 1.10, steel_mat, "Med_Cabinet")
    _slab(0.65, -0.45, 0.55, 0.04, 0.40, 1.00, dark_steel, "Med_CabDoor")
    _finalize_poi("SM_POI_MedBayWreck", sockets=[
        {"name": "LootSpawnA", "loc": (0.9, -0.6, 1.10)},
        {"name": "LootSpawnB", "loc": (-0.4, 0.5, 0.50)},
        {"name": "PlayerEntry", "loc": (-2.0, 0, 0)},
    ])


def gen_powermodule_wreck():
    """Broken reactor torus + bent conduit + sparking glow node."""
    clear_scene()
    _wreck_floor()
    torus_mat = palette_material("DarkSteel")
    core_mat = palette_material("GlowRed")
    conduit_mat = palette_material("Copper")
    spark_mat = palette_material("GlowGold")
    bpy.ops.mesh.primitive_torus_add(major_radius=0.65, minor_radius=0.18, location=(-0.4, 0.2, 0.50))
    torus = bpy.context.active_object
    torus.rotation_euler = (math.pi / 2, 0, 0.3)
    _add(torus, torus_mat); torus.name = "Power_Torus"
    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.18, subdivisions=3, location=(-0.4, 0.2, 0.50))
    _add(bpy.context.active_object, core_mat)
    for sx, ang in [(0.3, 0.6), (0.7, -0.4)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.06, depth=1.4, location=(sx, -0.3, 0.55))
        pipe = bpy.context.active_object
        pipe.rotation_euler = (ang, 0, 0.3)
        _add(pipe, conduit_mat); pipe.name = "Power_Conduit"
    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.05, subdivisions=2, location=(0.5, -0.5, 0.85))
    _add(bpy.context.active_object, spark_mat)
    _finalize_poi("SM_POI_PowerModuleWreck", sockets=[
        {"name": "LootSpawnA", "loc": (-0.4, 0.2, 1.10)},
        {"name": "GlowEffect", "loc": (-0.4, 0.2, 0.50)},
        {"name": "PlayerEntry", "loc": (2.0, 0, 0)},
    ])


def gen_roverbay_wreck():
    """Broken rover chassis + access ramp + tire pile."""
    clear_scene()
    _wreck_floor(width=4.5, depth=3.5)
    hull_mat = _hull()
    cabin_mat = palette_material("DarkSteel")
    window_mat = palette_material("GlowCyan")
    wheel_mat = _scorched()
    ramp_mat = palette_material("Steel")
    _slab(-0.4, 0.4, 0.55, 1.80, 1.00, 0.50, hull_mat, "Rover_Chassis")
    _slab(-0.7, 0.4, 1.00, 1.00, 0.85, 0.40, cabin_mat, "Rover_Cabin")
    _slab(-0.20, 0.85, 1.05, 0.50, 0.02, 0.20, window_mat, "Rover_Window")
    for sx, sy in [(0.4, 1.0), (-1.2, 1.0), (-1.2, -0.2)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.30, depth=0.20, location=(sx, sy, 0.30))
        w = bpy.context.active_object
        w.rotation_euler.x = math.pi / 2
        _add(w, wheel_mat); w.name = "Rover_Wheel"
    bpy.ops.mesh.primitive_cylinder_add(radius=0.30, depth=0.20, location=(1.6, -0.6, 0.30))
    tire = bpy.context.active_object
    tire.rotation_euler.x = math.pi / 2
    _add(tire, wheel_mat); tire.name = "Rover_Tire"
    bpy.ops.mesh.primitive_cube_add(size=1, location=(1.0, 0.4, 0.20))
    ramp = bpy.context.active_object
    ramp.scale = (1.0, 1.0, 0.06); ramp.rotation_euler.y = -0.4
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    _add(ramp, ramp_mat); ramp.name = "Rover_Ramp"
    _finalize_poi("SM_POI_RoverBayWreck", sockets=[
        {"name": "LootSpawnA", "loc": (-0.7, 0.4, 1.30)},
        {"name": "LootSpawnB", "loc": (1.6, -0.6, 0.40)},
        {"name": "PlayerEntry", "loc": (2.5, 0, 0)},
    ])


# â”€â”€ World-Feature Archetypes â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def gen_cave_mouth_deepvein():
    """Rock arch over a cave mouth with an exposed ore vein."""
    clear_scene()
    rock_mat = palette_material("Rock")
    interior_mat = get_or_create_material("POI_CaveInterior", (0.05, 0.05, 0.06, 1.0), roughness=0.95)
    ore_mat = _ore()
    for sy in [-1.5, 1.5]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.7, depth=3.0, vertices=8, location=(0, sy, 1.5))
        p = bpy.context.active_object
        p.scale = (1.2, 1.0, 1.0); bpy.ops.object.transform_apply(scale=True)
        _add(p, rock_mat); p.name = "Cave_Pillar"
    bpy.ops.mesh.primitive_ico_sphere_add(radius=1.0, subdivisions=3, location=(0, 0, 3.0))
    lintel = bpy.context.active_object
    lintel.scale = (1.5, 2.5, 0.8); bpy.ops.object.transform_apply(scale=True)
    _add(lintel, rock_mat); lintel.name = "Cave_Lintel"
    _slab(0.6, 0, 1.2, 0.4, 2.4, 2.4, interior_mat, "Cave_Interior")
    for i in range(5):
        z = 0.5 + i * 0.5
        sy = -1.4 if i % 2 else -1.6
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.10, subdivisions=2, location=(-0.6, sy, z))
        _add(bpy.context.active_object, ore_mat)
    _finalize_poi("SM_POI_CaveMouthDeepVein", sockets=[
        {"name": "Entrance",  "loc": (0.4, 0, 1.0)},
        {"name": "Interior",  "loc": (1.0, 0, 1.2)},
        {"name": "OreVein",   "loc": (-0.6, -1.5, 1.5)},
    ])


def gen_commsrelay_debris():
    """Toppled comms tower + bent dish array + cable spool."""
    clear_scene()
    dark_steel = palette_material("DarkSteel")
    steel_mat = palette_material("Steel")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.30, depth=2.0, vertices=4, location=(-1.5, 0, 1.0))
    _add(bpy.context.active_object, dark_steel)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.25, depth=4.0, vertices=4, location=(0.8, 0.3, 0.30))
    top = bpy.context.active_object
    top.rotation_euler = (0, math.pi / 2 - 0.2, 0.4)
    _add(top, dark_steel); top.name = "Comms_TowerTop"
    for i in range(4):
        x = -0.5 + i * 0.7
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 0.3, 0.30))
        brace = bpy.context.active_object
        brace.scale = (0.30, 0.04, 0.04)
        brace.rotation_euler.z = (i % 2) * math.pi / 4 - math.pi / 8
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(brace, steel_mat); brace.name = "Comms_Brace"
    bpy.ops.mesh.primitive_cone_add(radius1=0.9, radius2=0.0, depth=0.18, location=(2.1, -0.2, 0.20))
    dish = bpy.context.active_object
    dish.rotation_euler = (math.pi / 2 - 0.5, 0, 0.4)
    _add(dish, steel_mat); dish.name = "Comms_Dish"
    bpy.ops.mesh.primitive_torus_add(major_radius=0.25, minor_radius=0.06, location=(-0.5, -1.0, 0.10))
    _add(bpy.context.active_object, dark_steel)
    _finalize_poi("SM_POI_CommsRelayDebris", sockets=[
        {"name": "DishCenter", "loc": (2.1, -0.2, 0.30)},
        {"name": "TowerBase", "loc": (-1.5, 0, 0)},
        {"name": "PlayerEntry", "loc": (3.0, 0, 0)},
    ])


def gen_cryopod_cluster():
    """4 cryo pods arranged in a cluster, one cracked open + frost."""
    clear_scene()
    pod_mat = palette_material("Steel")
    window_mat = _ice()
    cracked_mat = get_or_create_material("POI_CryoPodCracked", (0.20, 0.20, 0.25, 0.5),
                                           roughness=0.60)
    frost_mat = _frost()
    positions = [(-1.0, 0.5, 0.0), (0.4, 0.7, 0.3), (-0.6, -0.7, 0.0), (0.8, -0.5, 0.6)]
    for i, (x, y, tilt) in enumerate(positions):
        bpy.ops.mesh.primitive_cylinder_add(radius=0.35, depth=1.40, location=(x, y, 0.45))
        body = bpy.context.active_object
        body.rotation_euler = (math.pi / 2, 0, tilt)
        _add(body, pod_mat); body.name = f"Cryo_Body_{i}"
        for end in [-0.7, 0.7]:
            ex = x + math.cos(tilt) * end
            ey = y + math.sin(tilt) * end
            bpy.ops.mesh.primitive_uv_sphere_add(radius=0.35, location=(ex, ey, 0.45))
            _add(bpy.context.active_object, pod_mat)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.78))
        win = bpy.context.active_object
        win.scale = (0.80, 0.20, 0.02); win.rotation_euler.z = tilt
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(win, window_mat if i < 3 else cracked_mat); win.name = f"Cryo_Window_{i}"
    for _ in range(8):
        x = 0.8 + random.uniform(-0.6, 0.6); y = -0.5 + random.uniform(-0.5, 0.5)
        bpy.ops.mesh.primitive_cone_add(radius1=0.05, radius2=0.0, depth=0.12, location=(x, y, 0.06))
        _add(bpy.context.active_object, frost_mat)
    _finalize_poi("SM_POI_CryoPodCluster", sockets=[
        {"name": "PodA", "loc": (-1.0, 0.5, 0.45)},
        {"name": "PodB", "loc": (0.4, 0.7, 0.45)},
        {"name": "PodC", "loc": (-0.6, -0.7, 0.45)},
        {"name": "PodD", "loc": (0.8, -0.5, 0.45)},
    ])


def gen_flooded_sinkhole():
    """Circular pit with water surface + jagged rim debris + ice shelf."""
    clear_scene()
    rock_mat = palette_material("Rock")
    water_mat = _water()
    ice_mat = _ice()
    for i in range(8):
        ang = (i / 8.0) * math.tau
        x = math.cos(ang) * 1.8; y = math.sin(ang) * 1.8
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.45 + (i % 2) * 0.15, location=(x, y, 0.20))
        b = bpy.context.active_object
        b.scale = (1.0, 1.0, 0.6); bpy.ops.object.transform_apply(scale=True)
        _add(b, rock_mat); b.name = "Sinkhole_Rim"
    bpy.ops.mesh.primitive_cylinder_add(radius=1.4, depth=0.04, vertices=24, location=(0, 0, 0.05))
    _add(bpy.context.active_object, water_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(1.0, 0.3, 0.10))
    ice = bpy.context.active_object
    ice.scale = (0.8, 1.0, 0.10); ice.rotation_euler.z = 0.4
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    _add(ice, ice_mat); ice.name = "Sinkhole_Ice"
    _finalize_poi("SM_POI_FloodedSinkhole_IceTunnel", sockets=[
        {"name": "Entrance",  "loc": (0, -2.0, 0)},
        {"name": "Interior",  "loc": (0, 0, 0.05)},
        {"name": "IceShelf",  "loc": (1.0, 0.3, 0.15)},
    ])


def gen_meteor_impact_field():
    """Crater rim + 4 charred meteor chunks of decreasing size + glowing fissures."""
    clear_scene()
    crater_mat = _scorched()
    rim_mat = palette_material("DarkRock")
    chunk_mat = _scorched()
    fissure_mat = palette_material("GlowRed")
    bpy.ops.mesh.primitive_cylinder_add(radius=1.8, depth=0.10, vertices=20, location=(0, 0, 0.05))
    _add(bpy.context.active_object, crater_mat)
    for i in range(12):
        ang = (i / 12.0) * math.tau
        x = math.cos(ang) * 1.8; y = math.sin(ang) * 1.8
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.18, location=(x, y, 0.16))
        s = bpy.context.active_object
        s.scale = (1.4, 1.0, 0.7); bpy.ops.object.transform_apply(scale=True)
        _add(s, rim_mat); s.name = "Meteor_Rim"
    chunks = [(0.0, 0.0, 0.50), (1.2, -0.6, 0.35), (-0.8, 0.9, 0.25), (0.4, 0.4, 0.15)]
    for x, y, r in chunks:
        bpy.ops.mesh.primitive_ico_sphere_add(radius=r, subdivisions=2, location=(x, y, r * 0.7))
        chunk = bpy.context.active_object
        chunk.rotation_euler = (random.random(), random.random(), random.random())
        _add(chunk, chunk_mat); chunk.name = "Meteor_Chunk"
        _slab(x, y, r * 0.7 + r * 0.4, r * 0.6, r * 0.05, r * 0.05, fissure_mat, "Meteor_Fissure")
    _finalize_poi("SM_POI_MeteorImpactField", sockets=[
        {"name": "CraterCenter", "loc": (0.0, 0.0, 0.10)},
        {"name": "ChunkA", "loc": (0.0, 0.0, 0.50)},
        {"name": "ChunkB", "loc": (1.2, -0.6, 0.35)},
        {"name": "ChunkC", "loc": (-0.8, 0.9, 0.25)},
    ])


def gen_razorstone_ridge():
    """Jagged ridgeline of sharp blade-like stones."""
    clear_scene()
    rock_mat = palette_material("Rock")
    blade_mat = palette_material("DarkRock")
    _slab(0, 0, 0.30, 4.0, 0.8, 0.60, rock_mat, "Razor_Base")
    for i in range(7):
        x = -1.7 + i * 0.55
        h = 1.2 + (i % 3) * 0.6
        bpy.ops.mesh.primitive_cone_add(radius1=0.28, radius2=0.04, depth=h, vertices=4, location=(x, 0, 0.6 + h / 2))
        blade = bpy.context.active_object
        blade.rotation_euler = ((i % 2 - 0.5) * 0.3, 0, math.pi / 4 + i * 0.1)
        _add(blade, blade_mat); blade.name = f"Razor_Blade_{i}"
    _finalize_poi("SM_POI_RazorstoneRidge", sockets=[
        {"name": "RidgeA", "loc": (-1.5, 0, 1.5)},
        {"name": "RidgeB", "loc": (1.5, 0, 1.5)},
        {"name": "PlayerEntry", "loc": (0, -2.0, 0)},
    ])


def gen_remnant_site():
    """Half-buried Progenitor obelisk fragment + standing pillars + glow ring."""
    clear_scene()
    dais_mat = palette_material("DarkStone")
    obelisk_mat = palette_material("ProgenitorStone")
    glow_mat = palette_material("GlowCyan")
    bpy.ops.mesh.primitive_cylinder_add(radius=2.0, depth=0.15, vertices=8, location=(0, 0, 0.075))
    _add(bpy.context.active_object, dais_mat)
    bpy.ops.mesh.primitive_cone_add(radius1=0.40, radius2=0.10, depth=3.0, vertices=6, location=(-0.6, 0.3, 1.4))
    obel = bpy.context.active_object
    obel.rotation_euler = (0.5, 0.3, 0)
    _add(obel, obelisk_mat); obel.name = "Remnant_Obelisk"
    for sx in [0.8, 1.4]:
        bpy.ops.mesh.primitive_cone_add(radius1=0.20, radius2=0.08, depth=1.2, vertices=6, location=(sx, -0.4, 0.75))
        _add(bpy.context.active_object, obelisk_mat)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.6, minor_radius=0.04, location=(0, 0, 0.16))
    _add(bpy.context.active_object, glow_mat)
    bpy.ops.mesh.primitive_cone_add(radius1=0.06, radius2=0.0, depth=0.3, location=(-1.1, 0.7, 2.6))
    apex = bpy.context.active_object
    apex.rotation_euler = (0.5, 0.3, 0)
    _add(apex, glow_mat); apex.name = "Remnant_Apex"
    _finalize_poi("SM_POI_RemnantSite", sockets=[
        {"name": "StudyPoint", "loc": (0, 0, 0.20)},
        {"name": "ApexCrest", "loc": (-1.1, 0.7, 2.7)},
        {"name": "GlowRing", "loc": (0, 0, 0.16)},
    ])


def gen_thermal_vent_field():
    """Cracked rock floor + 4 vent stacks with steam puffs + glow cracks."""
    clear_scene()
    floor_mat = palette_material("DarkRock")
    crack_mat = palette_material("GlowRed")
    vent_mat = palette_material("Rock")
    rim_mat = palette_material("DarkRock")
    steam_mat = _steam()
    bpy.ops.mesh.primitive_cylinder_add(radius=2.5, depth=0.10, vertices=16, location=(0, 0, 0.05))
    _add(bpy.context.active_object, floor_mat)
    for i in range(4):
        ang = (i / 4.0) * math.tau
        bpy.ops.mesh.primitive_cube_add(size=1, location=(math.cos(ang) * 0.9, math.sin(ang) * 0.9, 0.105))
        crack = bpy.context.active_object
        crack.scale = (1.5, 0.04, 0.005); crack.rotation_euler.z = ang
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(crack, crack_mat); crack.name = "Vent_Crack"
    for sx, sy in [(0.8, 0.8), (-0.9, 0.6), (0.3, -1.0), (-0.7, -0.7)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.18, depth=0.40, location=(sx, sy, 0.30))
        _add(bpy.context.active_object, vent_mat)
        bpy.ops.mesh.primitive_torus_add(major_radius=0.16, minor_radius=0.025, location=(sx, sy, 0.50))
        _add(bpy.context.active_object, rim_mat)
        for j in range(3):
            z = 0.65 + j * 0.30
            r = 0.16 + j * 0.06
            bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=(sx + j * 0.05, sy, z))
            _add(bpy.context.active_object, steam_mat)
    _finalize_poi("SM_POI_ThermalVentField", sockets=[
        {"name": "VentA", "loc": (0.8, 0.8, 0.50)},
        {"name": "VentB", "loc": (-0.9, 0.6, 0.50)},
        {"name": "VentC", "loc": (0.3, -1.0, 0.50)},
        {"name": "VentD", "loc": (-0.7, -0.7, 0.50)},
        {"name": "CenterCrack", "loc": (0, 0, 0.10)},
    ])


# â”€â”€ Dispatch â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

GENERATORS = {
    "ArmoryWreck":              gen_armory_wreck,
    "AvionicsWreck":            gen_avionics_wreck,
    "CaveMouthDeepVein":        gen_cave_mouth_deepvein,
    "CommsRelayDebris":         gen_commsrelay_debris,
    "CryoPodCluster":           gen_cryopod_cluster,
    "EngineeringWreck":         gen_engineering_wreck,
    "FloodedSinkhole/IceTunnel":gen_flooded_sinkhole,
    "FoodWreck":                gen_food_wreck,
    "GalleyWreck":              gen_galley_wreck,
    "LuggageWreck":             gen_luggage_wreck,
    "MedBayWreck":              gen_medbay_wreck,
    "MeteorImpactField":        gen_meteor_impact_field,
    "PowerModuleWreck":         gen_powermodule_wreck,
    "RazorstoneRidge":          gen_razorstone_ridge,
    "RemnantSite":              gen_remnant_site,
    "RoverBayWreck":            gen_roverbay_wreck,
    "ThermalVentField":         gen_thermal_vent_field,
}


def _safe_filename(s):
    return s.replace("/", "_")


def main():
    print("\n=== Quiet Rift: Enigma â€” POI Props Asset Generator (Batch 8 detail upgrade) ===")
    csv_abs = os.path.abspath(CSV_PATH)
    if not os.path.isfile(csv_abs):
        print(f"ERROR: CSV not found at {csv_abs}")
        return
    with open(csv_abs, newline='', encoding='utf-8') as f:
        rows = [r for r in csv.DictReader(f) if r.get("POITypeId / Archetype")]
    missing = []
    for row in rows:
        pid = row["POITypeId / Archetype"].strip()
        gen = GENERATORS.get(pid)
        if gen is None:
            missing.append(pid)
            continue
        random.seed(hash(pid) & 0xFFFFFFFF)
        print(f"\n[{pid}]")
        gen()
        out_path = os.path.join(OUTPUT_DIR, f"SM_POI_{_safe_filename(pid)}.fbx")
        export_fbx(pid, out_path)
    if missing:
        print(f"\nWARN: no generator registered for: {missing}")
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX exports SM_POI_<id> + UCX_ collision + LOD chain (0.40, 0.15) +")
    print("per-archetype gameplay sockets (LootSpawn / Entrance / Interior / VentA-D / etc).")


if __name__ == "__main__":
    main()
