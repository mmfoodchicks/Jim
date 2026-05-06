"""
Quiet Rift: Enigma — Station Asset Generator (Blender 4.x)

Upgraded in Batch 6 of the Blender detail pass — every crafting station
flows through qr_blender_detail.py for production finalization (palette
dedupe, smooth shading, bevels, smart UV, sockets, UCX collision, LOD
chain).

Generates placeholder meshes for every crafting / processing station
(AQRStationBase derivatives) referenced by recipes in the data tables:
camp workbench, sawbench, anvil forge, kiln furnace, kiln oven,
cookfire, chem bench, machine bench, electronics bench, armory bench,
grinder, plus the storage depot used by AQRStationBase::FindNearbyDepots.

Per-station gameplay sockets — these matter for the colony AI
(QRStationBase / QRDepotComponent already reference equivalent logic):

    SOCKET_WorkPoint   — where the assigned NPC stands while operating
    SOCKET_InputDrop   — where carriers drop input materials
    SOCKET_OutputDrop  — where finished outputs spawn
    SOCKET_DepotSlot   — only on Depot — where deposit pulls accept items

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

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/stations")


def _add(obj, mat):
    assign_material(obj, mat)
    return obj


def _slab(x, y, z, w, d, h, mat, name):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z))
    obj = bpy.context.active_object
    obj.scale = (w, d, h); bpy.ops.object.transform_apply(scale=True)
    _add(obj, mat); obj.name = name
    return obj


def _bench_top(width=2.0, depth=1.0, height=0.92, top_thickness=0.12,
                top_mat=None, leg_mat=None):
    if top_mat is None:
        top_mat = palette_material("Wood")
    if leg_mat is None:
        leg_mat = palette_material("DarkWood")
    _slab(0, 0, height - top_thickness / 2, width, depth, top_thickness, top_mat, "Bench_Top")
    leg_h = height - top_thickness
    for sx, sy in [(width / 2 - 0.06, depth / 2 - 0.06),
                   (-width / 2 + 0.06, depth / 2 - 0.06),
                   (width / 2 - 0.06, -depth / 2 + 0.06),
                   (-width / 2 + 0.06, -depth / 2 + 0.06)]:
        _slab(sx, sy, leg_h / 2, 0.08, 0.08, leg_h, leg_mat, "Bench_Leg")
    # Cross-stretcher seam under the top
    add_panel_seam_strip((-width / 2 + 0.10, 0, leg_h - 0.05),
                          (width / 2 - 0.10, 0, leg_h - 0.05),
                          width=0.04, depth=0.012, material_name="DarkWood",
                          name="Bench_Stretcher")


def _finalize_station(name, work, input_drop, output_drop, lods=(0.50,)):
    add_socket("WorkPoint", location=work)
    add_socket("InputDrop", location=input_drop)
    add_socket("OutputDrop", location=output_drop)
    finalize_asset(name,
                    bevel_width=0.0035, bevel_angle_deg=30,
                    smooth_angle_deg=50, collision="convex",
                    lods=list(lods), pivot="bottom_corner")


# ── Generators ────────────────────────────────────────────────────────────────

def gen_camp_workbench():
    """Rough wood bench with a hand-axe and rope coil — earliest station."""
    clear_scene()
    _bench_top(width=1.8, depth=0.9, height=0.90)
    _slab(-0.5, 0, 0.96, 0.16, 0.06, 0.10, palette_material("Steel"), "Axe_Head")
    bpy.ops.mesh.primitive_torus_add(major_radius=0.10, minor_radius=0.025, location=(0.5, 0.1, 0.93))
    _add(bpy.context.active_object,
          get_or_create_material("Station_Rope", (0.55, 0.42, 0.25, 1.0), roughness=0.95))
    _finalize_station("SM_ST_CAMP_WORKBENCH",
                       work=(0, -0.7, 0), input_drop=(-1.0, 0, 0), output_drop=(1.0, 0, 0))


def gen_sawbench():
    """Heavier wood bench with a circular saw blade and stack of planks."""
    clear_scene()
    _bench_top(width=2.2, depth=1.0, height=0.90)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.30, depth=0.025, location=(-0.5, 0, 1.20))
    blade = bpy.context.active_object
    blade.rotation_euler.x = math.pi / 2
    _add(blade, palette_material("Steel"))
    _slab(-0.5, 0.18, 1.20, 0.36, 0.10, 0.36, palette_material("DarkSteel"), "Sawbench_Housing")
    for i in range(3):
        _slab(0.6, 0, 0.95 + i * 0.04, 0.50, 0.18, 0.04, palette_material("Wood"), f"Sawbench_Plank_{i}")
    _finalize_station("SM_ST_SAWBENCH",
                       work=(0, -0.7, 0), input_drop=(-1.2, 0, 0), output_drop=(1.2, 0, 0))


def gen_anvil_forge():
    """Stone hearth + chimney + anvil — heavy metalwork station."""
    clear_scene()
    stone_mat = palette_material("Stone")
    coal_mat = get_or_create_material("Station_Coal", (0.18, 0.16, 0.14, 1.0), roughness=0.95)
    ember_mat = palette_material("Ember")
    anvil_mat = palette_material("Steel")
    _slab(0, 0, 0.55, 1.6, 1.2, 1.10, stone_mat, "Forge_Hearth")
    _slab(0, 0, 1.13, 1.20, 0.85, 0.10, coal_mat, "Forge_Coal")
    _slab(0, 0, 1.18, 0.80, 0.55, 0.04, ember_mat, "Forge_Ember")
    _slab(0, 0.45, 2.00, 0.6, 0.4, 1.6, stone_mat, "Forge_Chimney")
    _slab(1.30, 0, 0.45, 0.40, 0.30, 0.30, palette_material("DarkWood"), "Forge_AnvilStump")
    _slab(1.30, 0, 0.72, 0.45, 0.16, 0.16, anvil_mat, "Forge_Anvil")
    bpy.ops.mesh.primitive_cone_add(radius1=0.08, radius2=0.0, depth=0.18, location=(1.62, 0, 0.72))
    horn = bpy.context.active_object
    horn.rotation_euler.y = math.pi / 2
    _add(horn, anvil_mat)
    _finalize_station("SM_ST_ANVIL_FORGE",
                       work=(1.30, -0.6, 0), input_drop=(-1.0, 0, 0), output_drop=(1.30, 0.5, 0))


def gen_kiln_furnace():
    """Brick beehive furnace for smelting / firing."""
    clear_scene()
    stone_mat = palette_material("Stone")
    bpy.ops.mesh.primitive_cylinder_add(radius=1.1, depth=0.4, vertices=12, location=(0, 0, 0.20))
    _add(bpy.context.active_object, stone_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=1.0, location=(0, 0, 0.40))
    dome = bpy.context.active_object
    dome.scale.z = 1.2; bpy.ops.object.transform_apply(scale=True)
    _add(dome, stone_mat)
    _slab(0, -0.95, 0.55, 0.6, 0.30, 0.55,
           get_or_create_material("Station_KilnAsh", (0.18, 0.16, 0.14, 1.0), roughness=0.95),
           "Kiln_Opening")
    _slab(0, -1.05, 0.40, 0.5, 0.10, 0.20, palette_material("Ember"), "Kiln_Ember")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.15, depth=0.6, location=(0, 0, 1.85))
    _add(bpy.context.active_object, palette_material("DarkSteel"))
    _finalize_station("SM_ST_KILN_FURNACE",
                       work=(0, -1.5, 0), input_drop=(-1.5, 0, 0), output_drop=(1.5, 0, 0))


def gen_kiln_oven():
    """Lower, broader oven for ceramics / food firing — flat-topped masonry."""
    clear_scene()
    stone_mat = palette_material("Stone")
    _slab(0, 0, 0.55, 1.6, 1.2, 1.10, stone_mat, "Oven_Body")
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.7, location=(0, 0, 1.10))
    hump = bpy.context.active_object
    hump.scale = (1.0, 0.75, 0.4); bpy.ops.object.transform_apply(scale=True)
    _add(hump, stone_mat)
    _slab(0, -0.62, 0.55, 0.55, 0.05, 0.55, palette_material("DarkSteel"), "Oven_Door")
    bpy.ops.mesh.primitive_torus_add(major_radius=0.06, minor_radius=0.012, location=(0, -0.65, 0.55))
    handle = bpy.context.active_object
    handle.rotation_euler.x = math.pi / 2
    _add(handle, palette_material("Steel"))
    bpy.ops.mesh.primitive_cylinder_add(radius=0.10, depth=0.50, location=(0, 0.30, 1.55))
    _add(bpy.context.active_object, palette_material("DarkSteel"))
    _finalize_station("SM_ST_KILN_OVEN",
                       work=(0, -1.0, 0), input_drop=(-1.0, 0, 0), output_drop=(1.0, 0, 0))


def gen_cookfire():
    """Stone fire ring + spit + tripod with hanging pot."""
    clear_scene()
    stone_mat = palette_material("Stone")
    ash_mat = get_or_create_material("Station_Ash", (0.30, 0.28, 0.25, 1.0), roughness=0.95)
    log_mat = palette_material("DarkWood")
    ember_mat = palette_material("Ember")
    pot_mat = palette_material("DarkSteel")
    for i in range(8):
        ang = (i / 8.0) * math.tau
        x = math.cos(ang) * 0.55; y = math.sin(ang) * 0.55
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.14, location=(x, y, 0.07))
        st = bpy.context.active_object
        st.scale.z = 0.55; bpy.ops.object.transform_apply(scale=True)
        _add(st, stone_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.45, depth=0.04, location=(0, 0, 0.05))
    _add(bpy.context.active_object, ash_mat)
    for ang in (0, math.pi / 2):
        bpy.ops.mesh.primitive_cylinder_add(radius=0.04, depth=0.7, location=(0, 0, 0.10))
        log = bpy.context.active_object
        log.rotation_euler = (math.pi / 2, 0, ang)
        _add(log, log_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0, 0, 0.10))
    _add(bpy.context.active_object, ember_mat)
    for i in range(3):
        ang = (i / 3.0) * math.tau
        x = math.cos(ang) * 0.55; y = math.sin(ang) * 0.55
        bpy.ops.mesh.primitive_cylinder_add(radius=0.018, depth=1.4, location=(x * 0.5, y * 0.5, 0.70))
        leg = bpy.context.active_object
        leg.rotation_euler = (math.cos(ang) * 0.5, math.sin(ang) * 0.5, 0)
        _add(leg, log_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.16, location=(0, 0, 0.55))
    pot = bpy.context.active_object
    pot.scale.z = 0.7; bpy.ops.object.transform_apply(scale=True)
    _add(pot, pot_mat)
    _finalize_station("SM_ST_COOKFIRE",
                       work=(0, -0.9, 0), input_drop=(-0.8, 0, 0), output_drop=(0, 0, 0.7))


def gen_chem_bench():
    """Steel bench with glassware: flask, beaker, condenser tube."""
    clear_scene()
    _bench_top(width=2.0, depth=0.9, height=0.92,
                top_mat=palette_material("Steel"),
                leg_mat=palette_material("DarkSteel"))
    glass_mat = palette_material("Glass")
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.12, location=(-0.4, 0.05, 1.06))
    _add(bpy.context.active_object, glass_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.025, depth=0.10, location=(-0.4, 0.05, 1.20))
    _add(bpy.context.active_object, glass_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.06, depth=0.20, location=(0.05, 0.05, 1.04))
    _add(bpy.context.active_object, glass_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.018, depth=0.55, location=(0.45, 0.05, 1.14))
    cond = bpy.context.active_object
    cond.rotation_euler.y = math.pi / 4
    _add(cond, palette_material("Copper"))
    bpy.ops.mesh.primitive_cylinder_add(radius=0.02, depth=0.45, location=(0.65, 0.05, 1.16))
    _add(bpy.context.active_object, palette_material("DarkSteel"))
    _finalize_station("SM_ST_CHEM_BENCH",
                       work=(0, -0.7, 0), input_drop=(-1.1, 0, 0), output_drop=(1.1, 0, 0))


def gen_machine_bench():
    """Heavy steel bench with vise + lathe spindle."""
    clear_scene()
    _bench_top(width=2.2, depth=1.0, height=0.92,
                top_mat=palette_material("Steel"),
                leg_mat=palette_material("DarkSteel"))
    dark_steel = palette_material("DarkSteel")
    steel_mat = palette_material("Steel")
    _slab(-0.6, -0.25, 1.02, 0.20, 0.16, 0.10, dark_steel, "Vise_Base")
    for sx in [0.05, -0.05]:
        _slab(-0.6 + sx, -0.25, 1.10, 0.04, 0.16, 0.10, steel_mat, "Vise_Jaw")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.018, depth=0.30, location=(-0.6, -0.25, 1.10))
    screw = bpy.context.active_object
    screw.rotation_euler.y = math.pi / 2
    _add(screw, steel_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.10, depth=0.18, location=(0.6, 0, 1.10))
    spindle = bpy.context.active_object
    spindle.rotation_euler.y = math.pi / 2
    _add(spindle, dark_steel)
    _slab(0.95, 0, 1.06, 0.18, 0.20, 0.14, dark_steel, "Lathe_Tailstock")
    for sy in [0.10, -0.10]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.55, location=(0.78, sy, 1.04))
        bed = bpy.context.active_object
        bed.rotation_euler.y = math.pi / 2
        _add(bed, steel_mat)
    add_rivet_grid(origin=(-0.45, 0.40, 1.00), spacing=(0.20, 0.0),
                    rows=1, cols=5, rivet_radius=0.015, depth=0.008,
                    normal_axis='Z', material_name="DarkSteel")
    _finalize_station("SM_ST_MACHINE_BENCH",
                       work=(0, -0.7, 0), input_drop=(-1.2, 0, 0), output_drop=(1.2, 0, 0))


def gen_electronics_bench():
    """Wood bench with soldering iron stand + scope chassis."""
    clear_scene()
    _bench_top(width=2.0, depth=0.9, height=0.92)
    dark_steel = palette_material("DarkSteel")
    glass_mat = palette_material("GlassDark")
    _slab(-0.55, 0.10, 1.18, 0.40, 0.28, 0.30, dark_steel, "Elec_Chassis")
    _slab(-0.55, -0.05, 1.18, 0.26, 0.005, 0.20, glass_mat, "Elec_Screen")
    bpy.ops.mesh.primitive_cone_add(radius1=0.07, radius2=0.04, depth=0.04, location=(0.20, 0.05, 0.96))
    _add(bpy.context.active_object, dark_steel)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.25, location=(0.20, 0.05, 1.10))
    iron = bpy.context.active_object
    iron.rotation_euler = (0.7, 0, 0)
    _add(iron, palette_material("Copper"))
    bpy.ops.mesh.primitive_torus_add(major_radius=0.10, minor_radius=0.018, location=(0.55, 0, 0.95))
    _add(bpy.context.active_object, palette_material("Copper"))
    _slab(0.55, 0.25, 0.95, 0.30, 0.20, 0.02, palette_material("Steel"), "Elec_Tray")
    _finalize_station("SM_ST_ELECTRONICS_BENCH",
                       work=(0, -0.7, 0), input_drop=(-1.1, 0, 0), output_drop=(1.1, 0, 0))


def gen_armory_bench():
    """Wood-and-steel bench with weapon cradle + ammo box + pegboard."""
    clear_scene()
    _bench_top(width=2.4, depth=1.0, height=0.92)
    dark_steel = palette_material("DarkSteel")
    steel_mat = palette_material("Steel")
    for sx in [-0.5, 0.3]:
        _slab(sx, 0, 1.04, 0.10, 0.30, 0.08, dark_steel, "Armory_Cradle")
    _slab(-0.10, 0, 1.10, 1.10, 0.06, 0.05, steel_mat, "Armory_Weapon")
    _slab(0.85, 0.25, 1.02, 0.30, 0.22, 0.18,
           get_or_create_material("Station_AmmoBox", (0.35, 0.40, 0.25, 1.0), roughness=0.85),
           "Armory_AmmoBox")
    _slab(0, 0.40, 1.40, 2.0, 0.04, 0.50, palette_material("DarkWood"), "Armory_Pegboard")
    add_rivet_grid(origin=(-0.85, 0.43, 1.20), spacing=(0.20, 0.20),
                    rows=2, cols=10, rivet_radius=0.012, depth=0.008,
                    normal_axis='Y', material_name="DarkSteel")
    _finalize_station("SM_ST_ARMORY_BENCH",
                       work=(0, -0.7, 0), input_drop=(-1.3, 0, 0), output_drop=(1.3, 0, 0))


def gen_grinder():
    """Pedal-driven grinder wheel on a wood frame."""
    clear_scene()
    dark_wood = palette_material("DarkWood")
    stone_mat = palette_material("Stone")
    steel_mat = palette_material("Steel")
    for sy in [0.18, -0.18]:
        _slab(0, sy, 0.45, 0.10, 0.10, 0.90, dark_wood, "Grinder_Post")
    _slab(0, 0, 0.90, 0.10, 0.50, 0.10, dark_wood, "Grinder_Beam")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.30, depth=0.08, location=(0, 0, 0.55))
    wheel = bpy.context.active_object
    wheel.rotation_euler.x = math.pi / 2
    _add(wheel, stone_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.020, depth=0.50, location=(0, 0, 0.55))
    axle = bpy.context.active_object
    axle.rotation_euler.x = math.pi / 2
    _add(axle, steel_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.18, location=(0, 0.34, 0.45))
    _add(bpy.context.active_object, dark_wood)
    _slab(0, 0.30, 0.55, 0.018, 0.04, 0.20, dark_wood, "Grinder_Arm")
    _slab(0.30, 0, 0.20, 0.30, 0.40, 0.06, steel_mat, "Grinder_Tray")
    _finalize_station("SM_ST_GRINDER",
                       work=(0, -0.5, 0), input_drop=(-0.6, 0, 0.5), output_drop=(0.30, 0, 0.20))


def gen_depot():
    """Storage depot — wood-banded crate stack with placard.
       Used by AQRStationBase::FindNearbyDepots — every camp needs one."""
    clear_scene()
    wood_mat = palette_material("Wood")
    band_mat = palette_material("DarkSteel")
    placard_mat = get_or_create_material("Station_Placard", (0.85, 0.80, 0.55, 1.0),
                                          roughness=0.85)
    _slab(0, 0, 0.45, 1.4, 1.0, 0.90, wood_mat, "Depot_LowerCrate")
    _slab(0, 0, 1.20, 1.0, 0.8, 0.60, wood_mat, "Depot_UpperCrate")
    for z in (0.20, 0.70, 1.00, 1.40):
        scale = (1.45, 1.05, 0.025) if z < 1.0 else (1.05, 0.85, 0.025)
        _slab(0, 0, z, *scale, band_mat, f"Depot_Band_{int(z*100)}")
    _slab(0, -0.55, 0.55, 0.40, 0.02, 0.25, placard_mat, "Depot_Placard")
    add_socket("WorkPoint", location=(0, -1.0, 0))
    add_socket("InputDrop", location=(0, -0.8, 0))
    add_socket("OutputDrop", location=(0, 0.8, 0))
    add_socket("DepotSlot", location=(0, 0, 1.50))
    finalize_asset("SM_ST_DEPOT",
                    bevel_width=0.0035, bevel_angle_deg=30,
                    smooth_angle_deg=50, collision="convex",
                    lods=[0.50], pivot="bottom_corner")


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
    print("\n=== Quiet Rift: Enigma — Station Asset Generator (Batch 6 detail upgrade) ===")
    for asset_id, gen in GENERATORS.items():
        print(f"\n[{asset_id}]")
        gen()
        out_path = os.path.join(OUTPUT_DIR, f"SM_{asset_id}.fbx")
        export_fbx(asset_id, out_path)
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX exports SM_<id> + UCX_ collision + LOD0.50 +")
    print("SOCKET_WorkPoint / SOCKET_InputDrop / SOCKET_OutputDrop empties.")
    print("Depot also exposes SOCKET_DepotSlot.")


if __name__ == "__main__":
    main()
