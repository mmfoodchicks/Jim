"""
Quiet Rift: Enigma — POI Archetype Set-Dressing Generator (Blender 4.x)

Run this in Blender's Scripting workspace or via:
    blender --background --python qr_generate_poi_props.py

Reads every row in DT_POIArchetypes.csv and exports one placeholder
"hero prop cluster" FBX per archetype using the helpers in
qr_blender_common.py. Each cluster is a small composed scene
(typically 4–8 sub-meshes joined into one) representing the visual
identity of that POI type — wreckage debris, cave mouth, rover bay,
remnant site, etc. Sized for UE5 import (1 UU = 1 cm).

Wreck-type archetypes share the _wreck_floor helper (scorched twisted
deck plate) so the family reads consistently. World-feature archetypes
(cave, ridge, vent field, sinkhole, meteor field) use bespoke
geometry.

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

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/poi_props")
CSV_PATH = os.path.join(
    os.path.dirname(__file__),
    "../../Content/QuietRift/Data/DT_POIArchetypes.csv",
)

# Palette
HULL       = (0.45, 0.42, 0.40, 1.0)
SCORCHED   = (0.18, 0.16, 0.14, 1.0)
STEEL      = (0.30, 0.32, 0.36, 1.0)
DARK_STEEL = (0.18, 0.20, 0.24, 1.0)
COPPER     = (0.65, 0.42, 0.22, 1.0)
ROCK       = (0.45, 0.42, 0.38, 1.0)
DARK_ROCK  = (0.25, 0.22, 0.20, 1.0)
ORE        = (0.55, 0.45, 0.30, 1.0)
ICE_BLUE   = (0.55, 0.75, 0.88, 0.7)
WATER      = (0.20, 0.35, 0.50, 0.6)
STEAM      = (0.92, 0.92, 0.95, 0.4)
GLOW_RED   = (0.85, 0.20, 0.15, 1.0)
GLOW_CYAN  = (0.30, 0.85, 0.95, 1.0)
CLOTH      = (0.55, 0.42, 0.30, 1.0)
LUGGAGE    = (0.30, 0.20, 0.15, 1.0)


def _slab(x, y, z, w, d, h, color, name):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z))
    obj = bpy.context.active_object
    obj.scale = (w, d, h)
    bpy.ops.object.transform_apply(scale=True)
    add_material(obj, color, name)
    return obj


def _wreck_floor(width=4.0, depth=3.0, tilt=0.12):
    """Twisted scorched deck plate — base for any wreck POI."""
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.05))
    floor = bpy.context.active_object
    floor.scale = (width, depth, 0.10)
    floor.rotation_euler = (tilt, -tilt * 0.6, 0)
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    add_material(floor, SCORCHED, "Wreck_Floor_Mat")
    # Tear along one edge — small jagged splinters
    for i in range(5):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(width / 2 - 0.1 - i * 0.1, depth / 2 - 0.05, 0.06 + i * 0.04))
        sh = bpy.context.active_object
        sh.scale = (0.10, 0.05, 0.08 + i * 0.02)
        sh.rotation_euler.z = i * 0.4
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        add_material(sh, HULL, f"Wreck_Tear_{i}_Mat")


# ── Wreck Archetypes ─────────────────────────────────────────────────────────

def gen_armory_wreck():
    """Tilted gun rack + ammo crate stack on scorched deck."""
    clear_scene()
    _wreck_floor()
    # Gun rack frame (vertical)
    _slab(-1.0, 0.6, 0.95, 0.10, 0.30, 1.80, DARK_STEEL, "Armory_RackFrame")
    # Rack horizontal bars
    for z in [0.40, 1.00, 1.60]:
        _slab(-1.0, 0.5, z, 0.06, 0.50, 0.04, STEEL, f"Armory_RackBar_{int(z*100)}")
    # Three weapon stand-ins on rack
    for i in range(3):
        z = 0.42 + i * 0.6
        _slab(-1.0, 0.7, z, 0.10, 0.05, 0.50, STEEL, f"Armory_Weapon_{i}")
    # Ammo crate stack (right side)
    _slab(0.8, -0.4, 0.30, 0.50, 0.40, 0.60, (0.35, 0.40, 0.25, 1.0), "Armory_AmmoCrate_Lo")
    _slab(0.6, -0.3, 0.85, 0.45, 0.35, 0.50, (0.35, 0.40, 0.25, 1.0), "Armory_AmmoCrate_Hi")
    # Spilled brass casings
    for _ in range(6):
        x = random.uniform(-0.5, 0.5); y = random.uniform(-1.0, -0.4)
        bpy.ops.mesh.primitive_cylinder_add(radius=0.02, depth=0.04, location=(x, y, 0.12))
        c = bpy.context.active_object
        c.rotation_euler = (random.random(), 0, random.random() * math.tau)
        add_material(c, COPPER, "Armory_Brass_Mat")
    join_and_rename("SM_POI_ArmoryWreck")


def gen_avionics_wreck():
    """Tilted console bank + cracked displays + bent antenna."""
    clear_scene()
    _wreck_floor()
    # Console bank
    _slab(-0.6, 0.5, 0.55, 1.6, 0.6, 1.10, DARK_STEEL, "Avi_Console_Body")
    # Three display panels
    for i in range(3):
        x = -1.1 + i * 0.55
        _slab(x, 0.81, 0.95, 0.42, 0.02, 0.30, GLOW_CYAN, f"Avi_Display_{i}")
    # Bent antenna mast
    bpy.ops.mesh.primitive_cylinder_add(radius=0.025, depth=2.0, location=(0.9, 0.4, 1.05))
    mast = bpy.context.active_object
    mast.rotation_euler = (0.4, 0, 0)
    add_material(mast, DARK_STEEL, "Avi_Antenna_Mat")
    # Antenna dish at top
    bpy.ops.mesh.primitive_cone_add(radius1=0.30, radius2=0.0, depth=0.10, location=(0.9, 1.2, 1.95))
    dish = bpy.context.active_object
    dish.rotation_euler = (math.pi / 2, 0, 0)
    add_material(dish, STEEL, "Avi_Dish_Mat")
    join_and_rename("SM_POI_AvionicsWreck")


def gen_engineering_wreck():
    """Cylindrical machinery hulk + scattered tools + spilled fluid."""
    clear_scene()
    _wreck_floor()
    # Reactor / generator cylinder lying on side
    bpy.ops.mesh.primitive_cylinder_add(radius=0.55, depth=2.0, location=(-0.4, 0, 0.65))
    cyl = bpy.context.active_object
    cyl.rotation_euler.y = math.pi / 2
    cyl.rotation_euler.z = 0.2
    add_material(cyl, STEEL, "Eng_Cylinder_Mat")
    # End cap with bolts
    bpy.ops.mesh.primitive_cylinder_add(radius=0.50, depth=0.06, location=(0.6, 0, 0.65))
    cap = bpy.context.active_object
    cap.rotation_euler.y = math.pi / 2
    add_material(cap, DARK_STEEL, "Eng_Cap_Mat")
    # Bent pipe extending out
    bpy.ops.mesh.primitive_cylinder_add(radius=0.06, depth=0.8, location=(0.9, 0.3, 0.65))
    pipe = bpy.context.active_object
    pipe.rotation_euler = (0, 0.6, 0.4)
    add_material(pipe, COPPER, "Eng_Pipe_Mat")
    # Scattered tool boxes
    for sx, sy in [(0.6, -0.8), (-0.9, 0.8)]:
        _slab(sx, sy, 0.18, 0.30, 0.20, 0.20, (0.55, 0.20, 0.10, 1.0), "Eng_Toolbox")
    join_and_rename("SM_POI_EngineeringWreck")


def gen_food_wreck():
    """Broken pantry shelving + spilled grain + dented food crates."""
    clear_scene()
    _wreck_floor(width=3.5, depth=3.0)
    # Tilted shelf unit
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.4, 0.7, 0.85))
    shelf = bpy.context.active_object
    shelf.scale = (1.4, 0.30, 1.50)
    shelf.rotation_euler.z = 0.3
    shelf.rotation_euler.x = 0.2
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    add_material(shelf, (0.45, 0.32, 0.20, 1.0), "Food_Shelf_Mat")
    # Shelf horizontal boards visible
    for z in [0.4, 0.85, 1.30]:
        _slab(-0.4, 0.7, z, 1.30, 0.32, 0.04, (0.30, 0.20, 0.12, 1.0), f"Food_Board_{int(z*100)}")
    # Crates on the floor
    _slab(0.8, -0.5, 0.25, 0.55, 0.45, 0.50, (0.50, 0.35, 0.20, 1.0), "Food_Crate_1")
    _slab(0.4, -0.9, 0.20, 0.45, 0.40, 0.40, (0.50, 0.35, 0.20, 1.0), "Food_Crate_2")
    # Spilled grain pile
    for _ in range(15):
        x = random.uniform(0.2, 0.9); y = random.uniform(-0.6, -0.1); z = random.uniform(0.10, 0.18)
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.04, subdivisions=1, location=(x, y, z))
        add_material(bpy.context.active_object, (0.85, 0.72, 0.40, 1.0), "Food_Grain_Mat")
    join_and_rename("SM_POI_FoodWreck")


def gen_galley_wreck():
    """Toppled galley oven + broken prep table + scattered cookware."""
    clear_scene()
    _wreck_floor()
    # Toppled oven (cube on its side)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.6, 0.4, 0.40))
    oven = bpy.context.active_object
    oven.scale = (0.80, 0.70, 0.80)
    oven.rotation_euler = (0, 0, 0.4)
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    add_material(oven, STEEL, "Galley_Oven_Mat")
    # Oven door (darker recess)
    _slab(-0.20, 0.45, 0.40, 0.10, 0.55, 0.55, DARK_STEEL, "Galley_OvenDoor")
    # Broken prep table
    _slab(0.7, -0.2, 0.42, 1.20, 0.6, 0.04, STEEL, "Galley_TableTop")
    # One leg standing, three broken
    _slab(1.20, 0.05, 0.20, 0.06, 0.06, 0.40, DARK_STEEL, "Galley_Leg")
    # Pots / pans (spheres + cylinders flat on ground)
    for sx, sy, r in [(0.3, 0.7, 0.18), (-0.2, -0.6, 0.14)]:
        bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=(sx, sy, 0.10))
        pot = bpy.context.active_object
        pot.scale = (1, 1, 0.55)
        bpy.ops.object.transform_apply(scale=True)
        add_material(pot, DARK_STEEL, "Galley_Pot_Mat")
    join_and_rename("SM_POI_GalleyWreck")


def gen_luggage_wreck():
    """Pile of personal-effect cases of varying sizes + spilled cloth."""
    clear_scene()
    _wreck_floor(width=3.0, depth=3.0)
    # 6 luggage cases of varying sizes, slightly randomized
    bag_sizes = [(0.5, 0.3, 0.5), (0.4, 0.25, 0.4), (0.6, 0.35, 0.45),
                 (0.35, 0.22, 0.35), (0.55, 0.30, 0.50), (0.45, 0.28, 0.40)]
    positions = [(-0.5, 0.5), (0.3, 0.7), (-0.7, -0.3), (0.6, -0.5), (0.0, 0.0), (-0.2, -0.8)]
    heights   = [0.25, 0.20, 0.42, 0.18, 0.55, 0.20]
    for (w, d, h), (x, y), z in zip(bag_sizes, positions, heights):
        _slab(x, y, z, w, d, h, LUGGAGE, "Luggage_Case")
        # Strap band
        _slab(x, y + d / 2 + 0.005, z, w + 0.02, 0.01, h * 0.18, DARK_STEEL, "Luggage_Strap")
    # Spilled cloth pile
    for _ in range(4):
        x = random.uniform(-0.8, 0.8); y = random.uniform(-1.0, 1.0)
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, location=(x, y, 0.13))
        cloth = bpy.context.active_object
        cloth.scale = (1.4, 1.0, 0.25)
        bpy.ops.object.transform_apply(scale=True)
        add_material(cloth, CLOTH, "Luggage_Cloth_Mat")
    join_and_rename("SM_POI_LuggageWreck")


def gen_medbay_wreck():
    """Bed frame + IV stand + broken cabinet."""
    clear_scene()
    _wreck_floor()
    # Bed frame
    _slab(-0.4, 0.5, 0.40, 1.80, 0.80, 0.08, STEEL, "Med_BedTop")
    for sx, sy in [(0.5, 0.85), (-1.3, 0.85), (0.5, 0.15), (-1.3, 0.15)]:
        _slab(sx, sy, 0.20, 0.06, 0.06, 0.40, DARK_STEEL, "Med_BedLeg")
    # Mattress pad
    _slab(-0.4, 0.5, 0.46, 1.70, 0.70, 0.06, (0.85, 0.85, 0.82, 1.0), "Med_Mattress")
    # IV stand
    _slab(0.7, 0.9, 0.85, 0.04, 0.04, 1.70, STEEL, "Med_IVPole")
    bpy.ops.mesh.primitive_torus_add(major_radius=0.20, minor_radius=0.015, location=(0.7, 0.9, 0.04))
    add_material(bpy.context.active_object, STEEL, "Med_IVBase_Mat")
    # IV bag
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0.7, 0.85, 1.55))
    bag = bpy.context.active_object
    bag.scale = (0.7, 0.7, 1.4)
    bpy.ops.object.transform_apply(scale=True)
    add_material(bag, (0.95, 0.85, 0.55, 0.7), "Med_IVBag_Mat")
    # Broken cabinet
    _slab(0.9, -0.6, 0.55, 0.50, 0.40, 1.10, STEEL, "Med_Cabinet")
    # Broken door hanging open
    _slab(0.65, -0.45, 0.55, 0.04, 0.40, 1.00, DARK_STEEL, "Med_CabDoor")
    join_and_rename("SM_POI_MedBayWreck")


def gen_powermodule_wreck():
    """Broken reactor cylinder + bent conduit + sparking glow node."""
    clear_scene()
    _wreck_floor()
    # Reactor torus (broken open ring)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.65, minor_radius=0.18, location=(-0.4, 0.2, 0.50))
    torus = bpy.context.active_object
    torus.rotation_euler = (math.pi / 2, 0, 0.3)
    add_material(torus, DARK_STEEL, "Power_Torus_Mat")
    # Inner core (glowing)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, location=(-0.4, 0.2, 0.50))
    add_material(bpy.context.active_object, GLOW_RED, "Power_Core_Mat")
    # Bent conduit pipes
    for sx, ang in [(0.3, 0.6), (0.7, -0.4)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.06, depth=1.4, location=(sx, -0.3, 0.55))
        pipe = bpy.context.active_object
        pipe.rotation_euler = (ang, 0, 0.3)
        add_material(pipe, COPPER, "Power_Conduit_Mat")
    # Sparking node (bright dot)
    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.05, location=(0.5, -0.5, 0.85))
    add_material(bpy.context.active_object, (0.95, 0.85, 0.30, 1.0), "Power_Spark_Mat")
    join_and_rename("SM_POI_PowerModuleWreck")


def gen_roverbay_wreck():
    """Broken rover chassis + access ramp + tire pile."""
    clear_scene()
    _wreck_floor(width=4.5, depth=3.5)
    # Rover chassis (rectangular hull)
    _slab(-0.4, 0.4, 0.55, 1.80, 1.00, 0.50, HULL, "Rover_Chassis")
    # Cabin module on top
    _slab(-0.7, 0.4, 1.00, 1.00, 0.85, 0.40, DARK_STEEL, "Rover_Cabin")
    # Cabin window
    _slab(-0.20, 0.85, 1.05, 0.50, 0.02, 0.20, GLOW_CYAN, "Rover_Window")
    # Wheels (3 attached, 1 missing)
    for sx, sy in [(0.4, 1.0), (-1.2, 1.0), (-1.2, -0.2)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.30, depth=0.20, location=(sx, sy, 0.30))
        w = bpy.context.active_object
        w.rotation_euler.x = math.pi / 2
        add_material(w, SCORCHED, "Rover_Wheel_Mat")
    # Detached tire on the ground
    bpy.ops.mesh.primitive_cylinder_add(radius=0.30, depth=0.20, location=(1.6, -0.6, 0.30))
    tire = bpy.context.active_object
    tire.rotation_euler.x = math.pi / 2
    add_material(tire, SCORCHED, "Rover_Tire_Mat")
    # Drop ramp at rear
    bpy.ops.mesh.primitive_cube_add(size=1, location=(1.0, 0.4, 0.20))
    ramp = bpy.context.active_object
    ramp.scale = (1.0, 1.0, 0.06)
    ramp.rotation_euler.y = -0.4
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    add_material(ramp, STEEL, "Rover_Ramp_Mat")
    join_and_rename("SM_POI_RoverBayWreck")


# ── World-Feature Archetypes ─────────────────────────────────────────────────

def gen_cave_mouth_deepvein():
    """Rock arch over a cave mouth with an exposed ore vein."""
    clear_scene()
    # Arch — two side pillars + lintel mound
    for sy in [-1.5, 1.5]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.7, depth=3.0, vertices=8, location=(0, sy, 1.5))
        p = bpy.context.active_object
        p.scale = (1.2, 1.0, 1.0)
        bpy.ops.object.transform_apply(scale=True)
        add_material(p, ROCK, "Cave_Pillar_Mat")
    # Lintel
    bpy.ops.mesh.primitive_uv_sphere_add(radius=1.0, location=(0, 0, 3.0))
    lintel = bpy.context.active_object
    lintel.scale = (1.5, 2.5, 0.8)
    bpy.ops.object.transform_apply(scale=True)
    add_material(lintel, ROCK, "Cave_Lintel_Mat")
    # Dark interior (gives "mouth" silhouette)
    _slab(0.6, 0, 1.2, 0.4, 2.4, 2.4, (0.05, 0.05, 0.06, 1.0), "Cave_Interior")
    # Ore vein (zigzag of ore chunks on the right pillar)
    for i in range(5):
        z = 0.5 + i * 0.5
        sy = -1.4 if i % 2 else -1.6
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.10, subdivisions=1, location=(-0.6, sy, z))
        add_material(bpy.context.active_object, ORE, "Cave_Ore_Mat")
    join_and_rename("SM_POI_CaveMouthDeepVein")


def gen_commsrelay_debris():
    """Toppled comms tower + bent dish array."""
    clear_scene()
    # Tower base (still upright)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.30, depth=2.0, vertices=4, location=(-1.5, 0, 1.0))
    add_material(bpy.context.active_object, DARK_STEEL, "Comms_TowerBase_Mat")
    # Tower top section (toppled, lying)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.25, depth=4.0, vertices=4, location=(0.8, 0.3, 0.30))
    top = bpy.context.active_object
    top.rotation_euler = (0, math.pi / 2 - 0.2, 0.4)
    add_material(top, DARK_STEEL, "Comms_TowerTop_Mat")
    # Lattice cross bracing on the toppled section
    for i in range(4):
        x = -0.5 + i * 0.7
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 0.3, 0.30))
        brace = bpy.context.active_object
        brace.scale = (0.30, 0.04, 0.04)
        brace.rotation_euler.z = (i % 2) * math.pi / 4 - math.pi / 8
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        add_material(brace, STEEL, "Comms_Brace_Mat")
    # Broken dish lying on ground
    bpy.ops.mesh.primitive_cone_add(radius1=0.9, radius2=0.0, depth=0.18, location=(2.1, -0.2, 0.20))
    dish = bpy.context.active_object
    dish.rotation_euler = (math.pi / 2 - 0.5, 0, 0.4)
    add_material(dish, STEEL, "Comms_Dish_Mat")
    # Cable spool
    bpy.ops.mesh.primitive_torus_add(major_radius=0.25, minor_radius=0.06, location=(-0.5, -1.0, 0.10))
    add_material(bpy.context.active_object, DARK_STEEL, "Comms_Cable_Mat")
    join_and_rename("SM_POI_CommsRelayDebris")


def gen_cryopod_cluster():
    """4 cryo pods arranged in a cluster, one cracked open."""
    clear_scene()
    positions = [(-1.0, 0.5, 0.0), (0.4, 0.7, 0.3), (-0.6, -0.7, 0.0), (0.8, -0.5, 0.6)]
    rotations = [0.0, 0.3, -0.2, 1.2]   # last pod tilted (cracked open)
    for i, ((x, y, rot_z), tilt) in enumerate(zip(positions, rotations)):
        # Pod body (capsule = cylinder + two hemispheres)
        bpy.ops.mesh.primitive_cylinder_add(radius=0.35, depth=1.40, location=(x, y, 0.45))
        body = bpy.context.active_object
        body.rotation_euler = (math.pi / 2, 0, tilt)
        add_material(body, STEEL, f"Cryo_Body_{i}_Mat")
        # End caps
        for end in [-0.7, 0.7]:
            ex = x + math.cos(tilt) * end
            ey = y + math.sin(tilt) * end
            bpy.ops.mesh.primitive_uv_sphere_add(radius=0.35, location=(ex, ey, 0.45))
            add_material(bpy.context.active_object, STEEL, f"Cryo_Cap_{i}_Mat")
        # Window on top
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.78))
        win = bpy.context.active_object
        win.scale = (0.80, 0.20, 0.02)
        win.rotation_euler.z = tilt
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        add_material(win, ICE_BLUE if i < 3 else (0.20, 0.20, 0.25, 0.5), f"Cryo_Window_{i}_Mat")
    # Frost crystals around the cracked pod
    for _ in range(8):
        x = 0.8 + random.uniform(-0.6, 0.6); y = -0.5 + random.uniform(-0.5, 0.5)
        bpy.ops.mesh.primitive_cone_add(radius1=0.05, radius2=0.0, depth=0.12, location=(x, y, 0.06))
        add_material(bpy.context.active_object, ICE_BLUE, "Cryo_Frost_Mat")
    join_and_rename("SM_POI_CryoPodCluster")


def gen_flooded_sinkhole():
    """Circular pit with water surface + jagged rim debris (also ice tunnel variant)."""
    clear_scene()
    # Crater rim (8 boulders in a ring)
    for i in range(8):
        angle = (i / 8.0) * math.tau
        x = math.cos(angle) * 1.8
        y = math.sin(angle) * 1.8
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.45 + (i % 2) * 0.15, location=(x, y, 0.20))
        b = bpy.context.active_object
        b.scale = (1.0, 1.0, 0.6)
        bpy.ops.object.transform_apply(scale=True)
        add_material(b, ROCK, "Sinkhole_Rim_Mat")
    # Water surface
    bpy.ops.mesh.primitive_cylinder_add(radius=1.4, depth=0.04, vertices=24, location=(0, 0, 0.05))
    add_material(bpy.context.active_object, WATER, "Sinkhole_Water_Mat")
    # Ice shelf (one frozen segment for the IceTunnel variant)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(1.0, 0.3, 0.10))
    ice = bpy.context.active_object
    ice.scale = (0.8, 1.0, 0.10)
    ice.rotation_euler.z = 0.4
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    add_material(ice, ICE_BLUE, "Sinkhole_Ice_Mat")
    join_and_rename("SM_POI_FloodedSinkhole_IceTunnel")


def gen_meteor_impact_field():
    """Crater rim + 4 charred meteor chunks of decreasing size."""
    clear_scene()
    # Crater bowl (negative — represented by a darker disc)
    bpy.ops.mesh.primitive_cylinder_add(radius=1.8, depth=0.10, vertices=20, location=(0, 0, 0.05))
    add_material(bpy.context.active_object, SCORCHED, "Meteor_Crater_Mat")
    # Crater rim (low circular wall of small rocks)
    for i in range(12):
        angle = (i / 12.0) * math.tau
        x = math.cos(angle) * 1.8
        y = math.sin(angle) * 1.8
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, location=(x, y, 0.16))
        s = bpy.context.active_object
        s.scale = (1.4, 1.0, 0.7)
        bpy.ops.object.transform_apply(scale=True)
        add_material(s, DARK_ROCK, "Meteor_Rim_Mat")
    # 4 meteor chunks (decreasing size, scattered)
    chunks = [(0.0, 0.0, 0.50), (1.2, -0.6, 0.35), (-0.8, 0.9, 0.25), (0.4, 0.4, 0.15)]
    for x, y, r in chunks:
        bpy.ops.mesh.primitive_ico_sphere_add(radius=r, subdivisions=2, location=(x, y, r * 0.7))
        chunk = bpy.context.active_object
        chunk.rotation_euler = (random.random(), random.random(), random.random())
        add_material(chunk, SCORCHED, "Meteor_Chunk_Mat")
        # Glowing fissures (small bright cracks)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, r * 0.7 + r * 0.4))
        fis = bpy.context.active_object
        fis.scale = (r * 0.6, r * 0.05, r * 0.05)
        bpy.ops.object.transform_apply(scale=True)
        add_material(fis, GLOW_RED, "Meteor_Fissure_Mat")
    join_and_rename("SM_POI_MeteorImpactField")


def gen_razorstone_ridge():
    """Jagged ridgeline of sharp blade-like stones."""
    clear_scene()
    # Ridge base
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.30))
    base = bpy.context.active_object
    base.scale = (4.0, 0.8, 0.60)
    bpy.ops.object.transform_apply(scale=True)
    add_material(base, ROCK, "Razor_Base_Mat")
    # 7 jagged blade stones along the ridge
    for i in range(7):
        x = -1.7 + i * 0.55
        h = 1.2 + (i % 3) * 0.6
        bpy.ops.mesh.primitive_cone_add(radius1=0.28, radius2=0.04, depth=h, vertices=4,
                                        location=(x, 0, 0.6 + h / 2))
        blade = bpy.context.active_object
        blade.rotation_euler = ((i % 2 - 0.5) * 0.3, 0, math.pi / 4 + i * 0.1)
        add_material(blade, DARK_ROCK, f"Razor_Blade_{i}_Mat")
    join_and_rename("SM_POI_RazorstoneRidge")


def gen_remnant_site():
    """Half-buried Progenitor obelisk fragment + two short standing pillars + glow."""
    clear_scene()
    # Sunken disc / dais
    bpy.ops.mesh.primitive_cylinder_add(radius=2.0, depth=0.15, vertices=8, location=(0, 0, 0.075))
    add_material(bpy.context.active_object, (0.30, 0.32, 0.36, 1.0), "Remnant_Dais_Mat")
    # Tilted broken obelisk fragment
    bpy.ops.mesh.primitive_cone_add(radius1=0.40, radius2=0.10, depth=3.0, vertices=6, location=(-0.6, 0.3, 1.4))
    obel = bpy.context.active_object
    obel.rotation_euler = (0.5, 0.3, 0)
    add_material(obel, (0.62, 0.66, 0.72, 1.0), "Remnant_Obelisk_Mat")
    # Two short standing pillars
    for sx in [0.8, 1.4]:
        bpy.ops.mesh.primitive_cone_add(radius1=0.20, radius2=0.08, depth=1.2, vertices=6, location=(sx, -0.4, 0.75))
        add_material(bpy.context.active_object, (0.62, 0.66, 0.72, 1.0), "Remnant_Pillar_Mat")
    # Cyan glow ring on the dais
    bpy.ops.mesh.primitive_torus_add(major_radius=0.6, minor_radius=0.04, location=(0, 0, 0.16))
    add_material(bpy.context.active_object, GLOW_CYAN, "Remnant_GlowRing_Mat")
    # Apex shard on the broken obelisk
    bpy.ops.mesh.primitive_cone_add(radius1=0.06, radius2=0.0, depth=0.3, location=(-1.1, 0.7, 2.6))
    apex = bpy.context.active_object
    apex.rotation_euler = (0.5, 0.3, 0)
    add_material(apex, GLOW_CYAN, "Remnant_Apex_Mat")
    join_and_rename("SM_POI_RemnantSite")


def gen_thermal_vent_field():
    """Cracked rock floor + 4 vent stacks with steam puffs."""
    clear_scene()
    # Cracked rock floor
    bpy.ops.mesh.primitive_cylinder_add(radius=2.5, depth=0.10, vertices=16, location=(0, 0, 0.05))
    add_material(bpy.context.active_object, DARK_ROCK, "Vent_Floor_Mat")
    # Crack lines (slim glowing slabs)
    for i in range(4):
        angle = (i / 4.0) * math.tau
        bpy.ops.mesh.primitive_cube_add(size=1, location=(math.cos(angle) * 0.9, math.sin(angle) * 0.9, 0.105))
        crack = bpy.context.active_object
        crack.scale = (1.5, 0.04, 0.005)
        crack.rotation_euler.z = angle
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        add_material(crack, GLOW_RED, "Vent_Crack_Mat")
    # 4 vent stacks
    for sx, sy in [(0.8, 0.8), (-0.9, 0.6), (0.3, -1.0), (-0.7, -0.7)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.18, depth=0.40, location=(sx, sy, 0.30))
        add_material(bpy.context.active_object, ROCK, "Vent_Stack_Mat")
        bpy.ops.mesh.primitive_torus_add(major_radius=0.16, minor_radius=0.025, location=(sx, sy, 0.50))
        add_material(bpy.context.active_object, DARK_ROCK, "Vent_Rim_Mat")
        # Steam puffs (3 spheres of decreasing radius)
        for j in range(3):
            z = 0.65 + j * 0.30
            r = 0.16 + j * 0.06
            bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=(sx + j * 0.05, sy, z))
            add_material(bpy.context.active_object, STEAM, "Vent_Steam_Mat")
    join_and_rename("SM_POI_ThermalVentField")


# ── Dispatch ──────────────────────────────────────────────────────────────────

# CSV uses display names with mixed punctuation; map them to generators.
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
    print("\n=== Quiet Rift: Enigma — POI Props Asset Generator ===")
    csv_abs = os.path.abspath(CSV_PATH)
    if not os.path.isfile(csv_abs):
        print(f"ERROR: CSV not found at {csv_abs}")
        return

    with open(csv_abs, newline='', encoding='utf-8') as f:
        rows = [r for r in csv.DictReader(f) if r.get("POITypeId / Archetype")]

    # Seed the random generator deterministically per archetype so output is stable.
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
    print("Import these FBX files into UE5 via Content Browser > Import.")


if __name__ == "__main__":
    main()
