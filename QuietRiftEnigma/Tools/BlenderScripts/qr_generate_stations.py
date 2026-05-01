"""
Quiet Rift: Enigma — Station Asset Generator (Blender 4.x)

Run this in Blender's Scripting workspace or via:
    blender --background --python qr_generate_stations.py

Generates placeholder meshes for every crafting / processing station
(AQRStationBase derivatives) referenced by recipes in the data tables:
camp workbench, sawbench, anvil forge, kiln furnace, kiln oven,
cookfire, chem bench, machine bench, electronics bench, armory bench,
grinder, plus the storage depot used by AQRStationBase::FindNearbyDepots.
Sized for UE5 import (1 UU = 1 cm).

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

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/stations")

# Station palette
WOOD       = (0.45, 0.30, 0.18, 1.0)
DARK_WOOD  = (0.28, 0.18, 0.10, 1.0)
STEEL      = (0.30, 0.32, 0.36, 1.0)
DARK_STEEL = (0.18, 0.20, 0.24, 1.0)
STONE      = (0.42, 0.42, 0.40, 1.0)
ASH        = (0.18, 0.16, 0.14, 1.0)
EMBER      = (0.95, 0.42, 0.05, 1.0)
COPPER_PIPE = (0.65, 0.42, 0.22, 1.0)
GLASS_BLUE = (0.20, 0.40, 0.55, 0.6)


def _bench_top(width=2.0, depth=1.0, height=0.92, top_thickness=0.12, top_color=WOOD, leg_color=DARK_WOOD):
    """Standard four-legged workbench. Returns nothing — caller joins after."""
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, height - top_thickness / 2))
    top = bpy.context.active_object
    top.scale = (width, depth, top_thickness)
    bpy.ops.object.transform_apply(scale=True)
    add_material(top, top_color, "Bench_Top_Mat")
    leg_h = height - top_thickness
    for sx, sy in [(width / 2 - 0.06, depth / 2 - 0.06),
                   (-width / 2 + 0.06, depth / 2 - 0.06),
                   (width / 2 - 0.06, -depth / 2 + 0.06),
                   (-width / 2 + 0.06, -depth / 2 + 0.06)]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(sx, sy, leg_h / 2))
        leg = bpy.context.active_object
        leg.scale = (0.08, 0.08, leg_h)
        bpy.ops.object.transform_apply(scale=True)
        add_material(leg, leg_color, "Bench_Leg_Mat")


# ── Generators ────────────────────────────────────────────────────────────────

def gen_camp_workbench():
    """Rough wood bench with a hand-axe and rope coil — earliest station."""
    clear_scene()
    _bench_top(width=1.8, depth=0.9, height=0.90)
    # Axe head on top
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.5, 0, 0.96))
    head = bpy.context.active_object
    head.scale = (0.16, 0.06, 0.10)
    bpy.ops.object.transform_apply(scale=True)
    add_material(head, STEEL, "CampBench_Axe_Mat")
    # Rope coil
    bpy.ops.mesh.primitive_torus_add(major_radius=0.10, minor_radius=0.025, location=(0.5, 0.1, 0.93))
    add_material(bpy.context.active_object, (0.55, 0.42, 0.25, 1.0), "CampBench_Rope_Mat")
    join_and_rename("SM_ST_CAMP_WORKBENCH")


def gen_sawbench():
    """Heavier wood bench with a circular saw blade and stack of planks."""
    clear_scene()
    _bench_top(width=2.2, depth=1.0, height=0.90)
    # Saw blade (disc on edge)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.30, depth=0.025, location=(-0.5, 0, 1.20))
    blade = bpy.context.active_object
    blade.rotation_euler.x = math.pi / 2
    add_material(blade, STEEL, "Sawbench_Blade_Mat")
    # Blade housing
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.5, 0.18, 1.20))
    housing = bpy.context.active_object
    housing.scale = (0.36, 0.10, 0.36)
    bpy.ops.object.transform_apply(scale=True)
    add_material(housing, DARK_STEEL, "Sawbench_Housing_Mat")
    # Stack of planks on the right
    for i in range(3):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0.6, 0, 0.95 + i * 0.04))
        plank = bpy.context.active_object
        plank.scale = (0.50, 0.18, 0.04)
        bpy.ops.object.transform_apply(scale=True)
        add_material(plank, WOOD, f"Sawbench_Plank_{i}_Mat")
    join_and_rename("SM_ST_SAWBENCH")


def gen_anvil_forge():
    """Stone hearth + chimney + anvil. Heavy metalwork station."""
    clear_scene()
    # Hearth base (stone block)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.55))
    hearth = bpy.context.active_object
    hearth.scale = (1.6, 1.2, 1.10)
    bpy.ops.object.transform_apply(scale=True)
    add_material(hearth, STONE, "Forge_Hearth_Mat")
    # Coal bed (top recess)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.13))
    coal = bpy.context.active_object
    coal.scale = (1.20, 0.85, 0.10)
    bpy.ops.object.transform_apply(scale=True)
    add_material(coal, ASH, "Forge_Coal_Mat")
    # Embers
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.18))
    emb = bpy.context.active_object
    emb.scale = (0.80, 0.55, 0.04)
    bpy.ops.object.transform_apply(scale=True)
    add_material(emb, EMBER, "Forge_Ember_Mat")
    # Chimney
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0.45, 2.00))
    chim = bpy.context.active_object
    chim.scale = (0.6, 0.4, 1.6)
    bpy.ops.object.transform_apply(scale=True)
    add_material(chim, STONE, "Forge_Chimney_Mat")
    # Anvil to the side
    bpy.ops.mesh.primitive_cube_add(size=1, location=(1.30, 0, 0.45))
    anvil_base = bpy.context.active_object
    anvil_base.scale = (0.40, 0.30, 0.30)
    bpy.ops.object.transform_apply(scale=True)
    add_material(anvil_base, DARK_WOOD, "Forge_AnvilStump_Mat")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(1.30, 0, 0.72))
    anvil = bpy.context.active_object
    anvil.scale = (0.45, 0.16, 0.16)
    bpy.ops.object.transform_apply(scale=True)
    add_material(anvil, STEEL, "Forge_Anvil_Mat")
    # Anvil horn
    bpy.ops.mesh.primitive_cone_add(radius1=0.08, radius2=0.0, depth=0.18, location=(1.62, 0, 0.72))
    horn = bpy.context.active_object
    horn.rotation_euler.y = math.pi / 2
    add_material(horn, STEEL, "Forge_AnvilHorn_Mat")
    join_and_rename("SM_ST_ANVIL_FORGE")


def gen_kiln_furnace():
    """Brick beehive furnace for smelting / firing — large dome with vent."""
    clear_scene()
    # Stone base
    bpy.ops.mesh.primitive_cylinder_add(radius=1.1, depth=0.4, vertices=12, location=(0, 0, 0.20))
    add_material(bpy.context.active_object, STONE, "Kiln_Base_Mat")
    # Beehive dome
    bpy.ops.mesh.primitive_uv_sphere_add(radius=1.0, location=(0, 0, 0.40))
    dome = bpy.context.active_object
    dome.scale.z = 1.2
    bpy.ops.object.transform_apply(scale=True)
    add_material(dome, STONE, "Kiln_Dome_Mat")
    # Front opening
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, -0.95, 0.55))
    opening = bpy.context.active_object
    opening.scale = (0.6, 0.30, 0.55)
    bpy.ops.object.transform_apply(scale=True)
    add_material(opening, ASH, "Kiln_Opening_Mat")
    # Embers in opening
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, -1.05, 0.40))
    add_material(bpy.context.active_object, EMBER, "Kiln_Ember_Mat")
    # Vent stack on top
    bpy.ops.mesh.primitive_cylinder_add(radius=0.15, depth=0.6, location=(0, 0, 1.85))
    add_material(bpy.context.active_object, DARK_STEEL, "Kiln_Stack_Mat")
    join_and_rename("SM_ST_KILN_FURNACE")


def gen_kiln_oven():
    """Lower, broader oven for ceramics / food firing — flat-topped masonry."""
    clear_scene()
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.55))
    body = bpy.context.active_object
    body.scale = (1.6, 1.2, 1.10)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, STONE, "Oven_Body_Mat")
    # Domed top hump
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.7, location=(0, 0, 1.10))
    hump = bpy.context.active_object
    hump.scale = (1.0, 0.75, 0.4)
    bpy.ops.object.transform_apply(scale=True)
    add_material(hump, STONE, "Oven_Hump_Mat")
    # Iron door
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, -0.62, 0.55))
    door = bpy.context.active_object
    door.scale = (0.55, 0.05, 0.55)
    bpy.ops.object.transform_apply(scale=True)
    add_material(door, DARK_STEEL, "Oven_Door_Mat")
    # Door handle
    bpy.ops.mesh.primitive_torus_add(major_radius=0.06, minor_radius=0.012, location=(0, -0.65, 0.55))
    handle = bpy.context.active_object
    handle.rotation_euler.x = math.pi / 2
    add_material(handle, STEEL, "Oven_Handle_Mat")
    # Small chimney
    bpy.ops.mesh.primitive_cylinder_add(radius=0.10, depth=0.50, location=(0, 0.30, 1.55))
    add_material(bpy.context.active_object, DARK_STEEL, "Oven_Chimney_Mat")
    join_and_rename("SM_ST_KILN_OVEN")


def gen_cookfire():
    """Stone fire ring + spit + tripod with hanging pot."""
    clear_scene()
    # Stone ring (8 stones)
    for i in range(8):
        angle = (i / 8.0) * math.tau
        x = math.cos(angle) * 0.55
        y = math.sin(angle) * 0.55
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.14, location=(x, y, 0.07))
        st = bpy.context.active_object
        st.scale.z = 0.55
        bpy.ops.object.transform_apply(scale=True)
        add_material(st, STONE, "Cookfire_Stone_Mat")
    # Ash bed
    bpy.ops.mesh.primitive_cylinder_add(radius=0.45, depth=0.04, location=(0, 0, 0.05))
    add_material(bpy.context.active_object, ASH, "Cookfire_Ash_Mat")
    # Crossed logs
    for ang in (0, math.pi / 2):
        bpy.ops.mesh.primitive_cylinder_add(radius=0.04, depth=0.7, location=(0, 0, 0.10))
        log = bpy.context.active_object
        log.rotation_euler = (math.pi / 2, 0, ang)
        add_material(log, DARK_WOOD, "Cookfire_Log_Mat")
    # Embers
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0, 0, 0.10))
    add_material(bpy.context.active_object, EMBER, "Cookfire_Ember_Mat")
    # Tripod legs
    for i in range(3):
        angle = (i / 3.0) * math.tau
        x = math.cos(angle) * 0.55
        y = math.sin(angle) * 0.55
        bpy.ops.mesh.primitive_cylinder_add(radius=0.018, depth=1.4, location=(x * 0.5, y * 0.5, 0.70))
        leg = bpy.context.active_object
        leg.rotation_euler = (math.cos(angle) * 0.5, math.sin(angle) * 0.5, 0)
        add_material(leg, DARK_WOOD, "Cookfire_Tripod_Mat")
    # Hanging pot
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.16, location=(0, 0, 0.55))
    pot = bpy.context.active_object
    pot.scale.z = 0.7
    bpy.ops.object.transform_apply(scale=True)
    add_material(pot, DARK_STEEL, "Cookfire_Pot_Mat")
    join_and_rename("SM_ST_COOKFIRE")


def gen_chem_bench():
    """Steel bench with glassware: flask, beaker, condenser tube."""
    clear_scene()
    _bench_top(width=2.0, depth=0.9, height=0.92, top_color=STEEL, leg_color=DARK_STEEL)
    # Round flask
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.12, location=(-0.4, 0.05, 1.06))
    flask = bpy.context.active_object
    add_material(flask, GLASS_BLUE, "Chem_Flask_Mat")
    # Flask neck
    bpy.ops.mesh.primitive_cylinder_add(radius=0.025, depth=0.10, location=(-0.4, 0.05, 1.20))
    add_material(bpy.context.active_object, GLASS_BLUE, "Chem_FlaskNeck_Mat")
    # Tall beaker
    bpy.ops.mesh.primitive_cylinder_add(radius=0.06, depth=0.20, location=(0.05, 0.05, 1.04))
    add_material(bpy.context.active_object, GLASS_BLUE, "Chem_Beaker_Mat")
    # Condenser tube (slanted)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.018, depth=0.55, location=(0.45, 0.05, 1.14))
    cond = bpy.context.active_object
    cond.rotation_euler.y = math.pi / 4
    add_material(cond, COPPER_PIPE, "Chem_Condenser_Mat")
    # Stand for the condenser
    bpy.ops.mesh.primitive_cylinder_add(radius=0.02, depth=0.45, location=(0.65, 0.05, 1.16))
    add_material(bpy.context.active_object, DARK_STEEL, "Chem_Stand_Mat")
    join_and_rename("SM_ST_CHEM_BENCH")


def gen_machine_bench():
    """Heavy steel bench with a mounted vise and a small lathe spindle."""
    clear_scene()
    _bench_top(width=2.2, depth=1.0, height=0.92, top_color=STEEL, leg_color=DARK_STEEL)
    # Vise base
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.6, -0.25, 1.02))
    vbase = bpy.context.active_object
    vbase.scale = (0.20, 0.16, 0.10)
    bpy.ops.object.transform_apply(scale=True)
    add_material(vbase, DARK_STEEL, "Machine_ViseBase_Mat")
    # Vise jaws
    for sx in [0.05, -0.05]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.6 + sx, -0.25, 1.10))
        jaw = bpy.context.active_object
        jaw.scale = (0.04, 0.16, 0.10)
        bpy.ops.object.transform_apply(scale=True)
        add_material(jaw, STEEL, "Machine_Jaw_Mat")
    # Vise screw
    bpy.ops.mesh.primitive_cylinder_add(radius=0.018, depth=0.30, location=(-0.6, -0.25, 1.10))
    screw = bpy.context.active_object
    screw.rotation_euler.y = math.pi / 2
    add_material(screw, STEEL, "Machine_Screw_Mat")
    # Lathe spindle on right
    bpy.ops.mesh.primitive_cylinder_add(radius=0.10, depth=0.18, location=(0.6, 0, 1.10))
    spind = bpy.context.active_object
    spind.rotation_euler.y = math.pi / 2
    add_material(spind, DARK_STEEL, "Machine_Spindle_Mat")
    # Tail stock
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.95, 0, 1.06))
    tail = bpy.context.active_object
    tail.scale = (0.18, 0.20, 0.14)
    bpy.ops.object.transform_apply(scale=True)
    add_material(tail, DARK_STEEL, "Machine_Tailstock_Mat")
    # Bed rails
    for sy in [0.10, -0.10]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.55, location=(0.78, sy, 1.04))
        bed = bpy.context.active_object
        bed.rotation_euler.y = math.pi / 2
        add_material(bed, STEEL, "Machine_Bed_Mat")
    join_and_rename("SM_ST_MACHINE_BENCH")


def gen_electronics_bench():
    """Wood bench with a soldering iron stand and a stacked test scope box."""
    clear_scene()
    _bench_top(width=2.0, depth=0.9, height=0.92)
    # Scope chassis (rectangular box with screen)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.55, 0.10, 1.18))
    chassis = bpy.context.active_object
    chassis.scale = (0.40, 0.28, 0.30)
    bpy.ops.object.transform_apply(scale=True)
    add_material(chassis, DARK_STEEL, "Elec_Chassis_Mat")
    # Screen
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.55, -0.05, 1.18))
    scr = bpy.context.active_object
    scr.scale = (0.26, 0.005, 0.20)
    bpy.ops.object.transform_apply(scale=True)
    add_material(scr, GLASS_BLUE, "Elec_Screen_Mat")
    # Soldering iron stand (cone base + iron)
    bpy.ops.mesh.primitive_cone_add(radius1=0.07, radius2=0.04, depth=0.04, location=(0.20, 0.05, 0.96))
    add_material(bpy.context.active_object, DARK_STEEL, "Elec_StandBase_Mat")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.25, location=(0.20, 0.05, 1.10))
    iron = bpy.context.active_object
    iron.rotation_euler = (0.7, 0, 0)
    add_material(iron, COPPER_PIPE, "Elec_Iron_Mat")
    # Coil of wire
    bpy.ops.mesh.primitive_torus_add(major_radius=0.10, minor_radius=0.018, location=(0.55, 0, 0.95))
    add_material(bpy.context.active_object, COPPER_PIPE, "Elec_Coil_Mat")
    # Component tray (small slab)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.55, 0.25, 0.95))
    tray = bpy.context.active_object
    tray.scale = (0.30, 0.20, 0.02)
    bpy.ops.object.transform_apply(scale=True)
    add_material(tray, STEEL, "Elec_Tray_Mat")
    join_and_rename("SM_ST_ELECTRONICS_BENCH")


def gen_armory_bench():
    """Wood-and-steel bench with a weapon cradle and an ammo box."""
    clear_scene()
    _bench_top(width=2.4, depth=1.0, height=0.92)
    # Weapon cradle (two padded V-blocks)
    for sx in [-0.5, 0.3]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(sx, 0, 1.04))
        v = bpy.context.active_object
        v.scale = (0.10, 0.30, 0.08)
        bpy.ops.object.transform_apply(scale=True)
        add_material(v, DARK_STEEL, "Armory_Cradle_Mat")
    # Stand-in weapon body (rectangular bar lying in cradles)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.10, 0, 1.10))
    wpn = bpy.context.active_object
    wpn.scale = (1.10, 0.06, 0.05)
    bpy.ops.object.transform_apply(scale=True)
    add_material(wpn, STEEL, "Armory_Weapon_Mat")
    # Ammo box
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.85, 0.25, 1.02))
    box = bpy.context.active_object
    box.scale = (0.30, 0.22, 0.18)
    bpy.ops.object.transform_apply(scale=True)
    add_material(box, (0.35, 0.40, 0.25, 1.0), "Armory_AmmoBox_Mat")
    # Pegboard backstop
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0.40, 1.40))
    peg = bpy.context.active_object
    peg.scale = (2.0, 0.04, 0.50)
    bpy.ops.object.transform_apply(scale=True)
    add_material(peg, DARK_WOOD, "Armory_Pegboard_Mat")
    join_and_rename("SM_ST_ARMORY_BENCH")


def gen_grinder():
    """Pedal-driven grinder wheel on a wood frame."""
    clear_scene()
    # Frame base (two side posts)
    for sy in [0.18, -0.18]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, sy, 0.45))
        post = bpy.context.active_object
        post.scale = (0.10, 0.10, 0.90)
        bpy.ops.object.transform_apply(scale=True)
        add_material(post, DARK_WOOD, "Grinder_Post_Mat")
    # Cross beam top
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.90))
    beam = bpy.context.active_object
    beam.scale = (0.10, 0.50, 0.10)
    bpy.ops.object.transform_apply(scale=True)
    add_material(beam, DARK_WOOD, "Grinder_Beam_Mat")
    # Stone wheel
    bpy.ops.mesh.primitive_cylinder_add(radius=0.30, depth=0.08, location=(0, 0, 0.55))
    wheel = bpy.context.active_object
    wheel.rotation_euler.x = math.pi / 2
    add_material(wheel, STONE, "Grinder_Wheel_Mat")
    # Axle
    bpy.ops.mesh.primitive_cylinder_add(radius=0.020, depth=0.50, location=(0, 0, 0.55))
    axle = bpy.context.active_object
    axle.rotation_euler.x = math.pi / 2
    add_material(axle, STEEL, "Grinder_Axle_Mat")
    # Crank handle
    bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.18, location=(0, 0.34, 0.45))
    crank = bpy.context.active_object
    add_material(crank, DARK_WOOD, "Grinder_Crank_Mat")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0.30, 0.55))
    arm = bpy.context.active_object
    arm.scale = (0.018, 0.04, 0.20)
    bpy.ops.object.transform_apply(scale=True)
    add_material(arm, DARK_WOOD, "Grinder_Arm_Mat")
    # Catch tray (sparks/swarf)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.30, 0, 0.20))
    tray = bpy.context.active_object
    tray.scale = (0.30, 0.40, 0.06)
    bpy.ops.object.transform_apply(scale=True)
    add_material(tray, STEEL, "Grinder_Tray_Mat")
    join_and_rename("SM_ST_GRINDER")


def gen_depot():
    """Storage depot — wood-banded crate stack with a category placard.
       Used by AQRStationBase::FindNearbyDepots — every camp needs one."""
    clear_scene()
    # Lower crate
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.45))
    lo = bpy.context.active_object
    lo.scale = (1.4, 1.0, 0.90)
    bpy.ops.object.transform_apply(scale=True)
    add_material(lo, WOOD, "Depot_LowerCrate_Mat")
    # Upper crate (smaller, offset)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.20))
    up = bpy.context.active_object
    up.scale = (1.0, 0.8, 0.60)
    bpy.ops.object.transform_apply(scale=True)
    add_material(up, WOOD, "Depot_UpperCrate_Mat")
    # Steel banding rings
    for z in (0.20, 0.70, 1.00, 1.40):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, z))
        band = bpy.context.active_object
        band.scale = (1.45, 1.05, 0.025) if z < 1.0 else (1.05, 0.85, 0.025)
        bpy.ops.object.transform_apply(scale=True)
        add_material(band, DARK_STEEL, "Depot_Band_Mat")
    # Placard sign on front
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, -0.55, 0.55))
    placard = bpy.context.active_object
    placard.scale = (0.40, 0.02, 0.25)
    bpy.ops.object.transform_apply(scale=True)
    add_material(placard, (0.85, 0.80, 0.55, 1.0), "Depot_Placard_Mat")
    join_and_rename("SM_ST_DEPOT")


# ── Dispatch ──────────────────────────────────────────────────────────────────

GENERATORS = {
    "ST_CAMP_WORKBENCH":   gen_camp_workbench,
    "ST_SAWBENCH":         gen_sawbench,
    "ST_ANVIL_FORGE":      gen_anvil_forge,
    "ST_KILN_FURNACE":     gen_kiln_furnace,
    "ST_KILN_OVEN":        gen_kiln_oven,
    "ST_COOKFIRE":         gen_cookfire,
    "ST_CHEM_BENCH":       gen_chem_bench,
    "ST_MACHINE_BENCH":    gen_machine_bench,
    "ST_ELECTRONICS_BENCH":gen_electronics_bench,
    "ST_ARMORY_BENCH":     gen_armory_bench,
    "ST_GRINDER":          gen_grinder,
    "ST_DEPOT":            gen_depot,
}


def main():
    print("\n=== Quiet Rift: Enigma — Station Asset Generator ===")
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