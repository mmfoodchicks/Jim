"""
Quiet Rift: Enigma — POI Prop Procedural Asset Generator (Blender 4.x)

Run this in Blender's Scripting workspace or via:
    blender --background --python qr_generate_poi_props_assets.py -- --dt_file <DT_FILE>.csv

Generates one recognizable placeholder FBX mesh for every row in the POI archetype CSV.
Each mesh is sized for UE5 import through qr_blender_common.py (1 Blender unit = 100 UE units/cm).
Export path: Content/Meshes/poi_props/ by default, or pass --output <path> via sys.argv.

Usage inside Blender:
    1. Put this file beside qr_blender_common.py in QuietRiftEnigma/Tools/BlenderScripts
    2. Open Blender > Scripting tab
    3. Open this file
    4. Set DT_FILE below or pass --dt_file <DT_FILE>.csv
    5. Press Run Script
"""

import bpy
import csv
import math
import os
import re
import sys

from qr_blender_common import clear_scene, export_fbx, add_material, join_and_rename

# ── Configuration ──────────────────────────────────────────────────────────────
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "../.."))

OUTPUT_DIR = os.path.join(PROJECT_ROOT, "Content/Meshes/poi_props")
DT_FILE = os.path.join(PROJECT_ROOT, "Content/DataTables/DT_POIArchetypes.csv")
POI_ID_FIELD = "POITypeId / Archetype"

# ── Colors ─────────────────────────────────────────────────────────────────────
METAL_DARK = (0.18, 0.19, 0.20, 1.0)
METAL_MID = (0.38, 0.40, 0.42, 1.0)
METAL_LIGHT = (0.72, 0.74, 0.76, 1.0)
RUST = (0.58, 0.24, 0.09, 1.0)
WARNING_YELLOW = (0.95, 0.70, 0.15, 1.0)
MED_RED = (0.85, 0.05, 0.05, 1.0)
GLASS_BLUE = (0.40, 0.80, 0.95, 0.55)
ICE_BLUE = (0.65, 0.88, 1.00, 0.65)
STONE = (0.33, 0.30, 0.27, 1.0)
STONE_DARK = (0.08, 0.07, 0.06, 1.0)
EMBER = (1.00, 0.28, 0.05, 1.0)
REMNANT = (0.18, 0.12, 0.28, 1.0)
REMNANT_GLOW = (0.35, 0.90, 0.95, 1.0)
SUPPLY_GREEN = (0.22, 0.42, 0.28, 1.0)
FOOD_ORANGE = (0.85, 0.42, 0.12, 1.0)

# ── Small mesh helpers ─────────────────────────────────────────────────────────

def _apply_scale(obj):
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.select_set(False)


def _add_cube(name, location, scale, color, mat_name, rotation=(0.0, 0.0, 0.0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location, rotation=rotation)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    _apply_scale(obj)
    add_material(obj, color, mat_name)
    return obj


def _add_cylinder(name, location, radius, depth, color, mat_name, vertices=24, rotation=(0.0, 0.0, 0.0)):
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices,
        radius=radius,
        depth=depth,
        location=location,
        rotation=rotation,
    )
    obj = bpy.context.active_object
    obj.name = name
    add_material(obj, color, mat_name)
    return obj


def _add_cone(name, location, radius1, depth, color, mat_name, radius2=0.0, vertices=24, rotation=(0.0, 0.0, 0.0)):
    bpy.ops.mesh.primitive_cone_add(
        vertices=vertices,
        radius1=radius1,
        radius2=radius2,
        depth=depth,
        location=location,
        rotation=rotation,
    )
    obj = bpy.context.active_object
    obj.name = name
    add_material(obj, color, mat_name)
    return obj


def _add_sphere(name, location, radius, color, mat_name, scale=(1.0, 1.0, 1.0), ico=True):
    if ico:
        bpy.ops.mesh.primitive_ico_sphere_add(radius=radius, subdivisions=2, location=location)
    else:
        bpy.ops.mesh.primitive_uv_sphere_add(radius=radius, location=location)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    _apply_scale(obj)
    add_material(obj, color, mat_name)
    return obj


def _add_torus(name, location, major_radius, minor_radius, color, mat_name, rotation=(0.0, 0.0, 0.0)):
    bpy.ops.mesh.primitive_torus_add(
        major_radius=major_radius,
        minor_radius=minor_radius,
        location=location,
        rotation=rotation,
    )
    obj = bpy.context.active_object
    obj.name = name
    add_material(obj, color, mat_name)
    return obj


def _add_label_post(prefix, color=WARNING_YELLOW):
    """Tiny survey marker so the mesh reads as a POI prop in-editor."""
    _add_cylinder(f"{prefix}_MarkerPost", (0.95, -0.65, 0.35), 0.025, 0.7, METAL_DARK, "POI_MarkerPost_Mat", vertices=10)
    _add_cube(f"{prefix}_MarkerPlate", (0.95, -0.65, 0.72), (0.18, 0.025, 0.12), color, "POI_MarkerPlate_Mat")


# ── CSV / naming helpers ───────────────────────────────────────────────────────

def _script_args():
    if "--" in sys.argv:
        return sys.argv[sys.argv.index("--") + 1:]
    return []


def _resolve_path(path):
    if not path:
        return path
    if os.path.isabs(path):
        return path
    cwd_path = os.path.abspath(path)
    script_path = os.path.abspath(os.path.join(SCRIPT_DIR, path))
    if os.path.exists(cwd_path):
        return cwd_path
    return script_path


def _parse_args():
    dt_file = DT_FILE
    output_dir = OUTPUT_DIR
    args = _script_args()

    i = 0
    while i < len(args):
        arg = args[i]
        if arg in ("--dt_file", "--dt-file", "--csv") and i + 1 < len(args):
            dt_file = args[i + 1]
            i += 2
            continue
        if arg == "--output" and i + 1 < len(args):
            output_dir = args[i + 1]
            i += 2
            continue
        if arg.lower().endswith(".csv"):
            dt_file = arg
        i += 1

    dt_file = _resolve_path(dt_file)
    output_dir = _resolve_path(output_dir)

    if not os.path.exists(dt_file):
        fallback_candidates = [
            os.path.join(SCRIPT_DIR, "DT_POIArchetypes.csv"),
            os.path.join(PROJECT_ROOT, "DT_POIArchetypes.csv"),
            os.path.join(PROJECT_ROOT, "Content/Data/DT_POIArchetypes.csv"),
            os.path.join(PROJECT_ROOT, "Content/DataTables/DT_POIArchetypes.csv"),
        ]
        for candidate in fallback_candidates:
            if os.path.exists(candidate):
                dt_file = candidate
                break

    return dt_file, output_dir


def _safe_asset_token(value):
    value = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", str(value).strip())
    value = re.sub(r"[^A-Za-z0-9]+", "_", value)
    value = re.sub(r"_+", "_", value).strip("_")
    return value.upper() or "UNNAMED_POI"


def _get_poi_id(row):
    if POI_ID_FIELD in row and row[POI_ID_FIELD].strip():
        return row[POI_ID_FIELD].strip()
    for value in row.values():
        if value and value.strip():
            return value.strip()
    return "UnnamedPOI"


def _read_rows(dt_file):
    with open(dt_file, newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        rows = []
        for row in reader:
            poi_id = _get_poi_id(row)
            if poi_id:
                rows.append(row)
        return rows


# ── Shared POI building blocks ─────────────────────────────────────────────────

def _scatter_rocks(prefix, count=8, radius=0.9, color=STONE):
    for i in range(count):
        angle = (i / count) * math.tau
        ring = radius * (0.65 + 0.08 * (i % 3))
        x = math.cos(angle) * ring
        y = math.sin(angle) * ring
        z = 0.06 + 0.025 * (i % 2)
        rock = _add_sphere(f"{prefix}_Rock_{i}", (x, y, z), 0.12 + 0.025 * (i % 3), color, "POI_Rock_Mat")
        rock.rotation_euler.z = angle


def _wreck_base(prefix):
    _add_cube(f"{prefix}_HullBase", (0, 0, 0.08), (1.20, 0.58, 0.08), METAL_DARK, "POI_Hull_Mat", rotation=(0.0, 0.0, 0.03))
    _add_cube(f"{prefix}_BrokenPanel_A", (-0.45, 0.42, 0.18), (0.46, 0.055, 0.20), METAL_MID, "POI_BrokenPanel_Mat", rotation=(0.0, 0.18, 0.42))
    _add_cube(f"{prefix}_BrokenPanel_B", (0.42, -0.37, 0.20), (0.55, 0.05, 0.18), METAL_MID, "POI_BrokenPanel_Mat", rotation=(0.0, -0.14, -0.38))
    _add_cube(f"{prefix}_RustedSkid", (0.12, 0.0, 0.20), (0.80, 0.09, 0.06), RUST, "POI_Rust_Mat", rotation=(0.0, 0.0, -0.08))
    _add_cylinder(f"{prefix}_Pipe_A", (-0.18, -0.28, 0.22), 0.055, 0.88, METAL_LIGHT, "POI_Pipe_Mat", rotation=(0.0, math.pi / 2, 0.15))
    _add_cylinder(f"{prefix}_Pipe_B", (0.28, 0.29, 0.21), 0.045, 0.70, RUST, "POI_RustPipe_Mat", rotation=(math.pi / 2, 0.0, 0.45))


def _add_crate_stack(prefix, color=SUPPLY_GREEN):
    for i in range(3):
        x = -0.30 + i * 0.24
        y = 0.12 if i % 2 == 0 else -0.10
        _add_cube(f"{prefix}_Crate_{i}", (x, y, 0.34), (0.18, 0.16, 0.14), color, "POI_Crate_Mat", rotation=(0.0, 0.0, 0.08 * i))


def _add_pod(prefix, index, location, color=GLASS_BLUE):
    x, y, z = location
    _add_cylinder(f"{prefix}_PodBody_{index}", (x, y, z), 0.13, 0.58, METAL_LIGHT, "POI_PodBody_Mat", rotation=(0.0, math.pi / 2, 0.0))
    _add_sphere(f"{prefix}_PodGlass_{index}", (x + 0.02, y, z + 0.03), 0.12, color, "POI_PodGlass_Mat", scale=(1.35, 0.85, 0.40), ico=False)


def _add_wheel(prefix, index, location):
    _add_cylinder(f"{prefix}_Wheel_{index}", location, 0.12, 0.08, METAL_DARK, "POI_Wheel_Mat", vertices=24, rotation=(math.pi / 2, 0.0, 0.0))


# ── Archetype generator functions ──────────────────────────────────────────────

def gen_wreck_prop(poi_id, asset_name):
    """Crash/wreckage POI — hull chunk with row-specific readable detail."""
    clear_scene()
    safe = _safe_asset_token(poi_id)
    _wreck_base(safe)

    if "ARMORY" in safe:
        _add_crate_stack(safe, color=(0.20, 0.32, 0.22, 1.0))
        for i in range(4):
            _add_cylinder(f"{safe}_AmmoTube_{i}", (-0.45 + i * 0.16, -0.22, 0.42), 0.035, 0.30, WARNING_YELLOW, "POI_AmmoTube_Mat", vertices=16, rotation=(0.0, math.pi / 2, 0.0))
    elif "AVIONICS" in safe:
        _add_cylinder(f"{safe}_Mast", (-0.15, 0.0, 0.62), 0.025, 0.75, METAL_LIGHT, "POI_Antenna_Mat", vertices=12)
        _add_cone(f"{safe}_Dish", (-0.15, 0.0, 1.02), 0.32, 0.12, METAL_LIGHT, "POI_Dish_Mat", radius2=0.04, vertices=32, rotation=(0.0, math.pi / 2, 0.0))
        _add_sphere(f"{safe}_SignalOrb", (-0.34, 0.0, 1.03), 0.055, REMNANT_GLOW, "POI_SignalGlow_Mat")
    elif "COMMS" in safe or "RELAY" in safe:
        _add_cylinder(f"{safe}_CollapsedTower", (0.05, 0.04, 0.48), 0.035, 1.20, METAL_LIGHT, "POI_Tower_Mat", vertices=12, rotation=(0.0, math.pi / 2.8, -0.65))
        _add_torus(f"{safe}_RelayRing", (-0.48, 0.35, 0.44), 0.20, 0.018, METAL_LIGHT, "POI_RelayRing_Mat", rotation=(math.pi / 2, 0.0, 0.25))
        _add_sphere(f"{safe}_Blinker", (-0.62, 0.42, 0.45), 0.045, REMNANT_GLOW, "POI_Blinker_Mat")
    elif "CRYO" in safe:
        for i in range(3):
            _add_pod(safe, i, (-0.32 + i * 0.32, 0.02 * (i - 1), 0.42))
    elif "ENGINEERING" in safe:
        for i in range(4):
            _add_cylinder(f"{safe}_ServicePipe_{i}", (-0.45 + i * 0.28, 0.18, 0.43), 0.035, 0.62, RUST if i % 2 else METAL_LIGHT, "POI_ServicePipe_Mat", vertices=16, rotation=(0.0, math.pi / 2, 0.1 * i))
        _add_cube(f"{safe}_ToolCabinet", (0.36, -0.18, 0.48), (0.18, 0.12, 0.28), WARNING_YELLOW, "POI_ToolCabinet_Mat")
    elif "FOOD" in safe or "GALLEY" in safe:
        _add_crate_stack(safe, color=FOOD_ORANGE)
        for i in range(3):
            _add_cylinder(f"{safe}_Canister_{i}", (0.36, -0.18 + i * 0.13, 0.36), 0.05, 0.18, METAL_LIGHT, "POI_Canister_Mat", vertices=16)
    elif "LUGGAGE" in safe:
        for i in range(5):
            _add_cube(f"{safe}_Case_{i}", (-0.45 + i * 0.22, -0.08 + 0.08 * (i % 2), 0.36 + 0.04 * (i % 3)), (0.16, 0.09, 0.12), (0.18 + 0.05 * i, 0.18, 0.25, 1.0), "POI_Luggage_Mat", rotation=(0.0, 0.0, -0.12 + i * 0.06))
    elif "MED" in safe:
        _add_cube(f"{safe}_MedLocker", (0.05, 0.0, 0.50), (0.28, 0.14, 0.30), METAL_LIGHT, "POI_MedLocker_Mat")
        _add_cube(f"{safe}_MedCrossVertical", (0.05, -0.145, 0.52), (0.035, 0.012, 0.17), MED_RED, "POI_MedCross_Mat")
        _add_cube(f"{safe}_MedCrossHorizontal", (0.05, -0.15, 0.52), (0.13, 0.012, 0.035), MED_RED, "POI_MedCross_Mat")
    elif "POWER" in safe:
        _add_cylinder(f"{safe}_ReactorCore", (0.05, 0.0, 0.52), 0.22, 0.55, METAL_DARK, "POI_ReactorCore_Mat", vertices=32)
        _add_torus(f"{safe}_ReactorRing_A", (0.05, 0.0, 0.52), 0.24, 0.018, REMNANT_GLOW, "POI_ReactorRing_Mat")
        _add_sphere(f"{safe}_CoreGlow", (0.05, 0.0, 0.54), 0.13, REMNANT_GLOW, "POI_CoreGlow_Mat")
    elif "ROVER" in safe:
        _add_cube(f"{safe}_RoverFrame", (0.05, 0.0, 0.42), (0.46, 0.28, 0.08), METAL_MID, "POI_RoverFrame_Mat")
        for i, loc in enumerate([(-0.28, -0.21, 0.28), (-0.28, 0.21, 0.28), (0.36, -0.21, 0.28), (0.36, 0.21, 0.28)]):
            _add_wheel(safe, i, loc)
    else:
        _add_crate_stack(safe)

    _add_label_post(safe)
    return join_and_rename(asset_name)


def gen_cave_or_tunnel_prop(poi_id, asset_name):
    """Cave/sinkhole/tunnel POI — dark entrance and ring of broken stone/ice."""
    clear_scene()
    safe = _safe_asset_token(poi_id)
    is_ice = "ICE" in safe
    accent = ICE_BLUE if is_ice else STONE

    _add_cylinder(f"{safe}_DarkOpening", (0, 0, 0.20), 0.52, 0.10, STONE_DARK, "POI_DarkOpening_Mat", vertices=32)
    _add_torus(f"{safe}_EntryLip", (0, 0, 0.25), 0.52, 0.08, accent, "POI_EntryLip_Mat")
    for i in range(10):
        angle = (i / 10.0) * math.tau
        x = math.cos(angle) * 0.55
        y = math.sin(angle) * 0.55
        height = 0.28 + 0.05 * (i % 4)
        _add_cone(f"{safe}_RimShard_{i}", (x, y, height / 2), 0.08 + 0.02 * (i % 3), height, accent, "POI_RimShard_Mat", vertices=7, rotation=(0.12 * (i % 2), 0.18 * (i % 3), angle))

    if "DEEP" in safe or "VEIN" in safe:
        for i in range(4):
            angle = (i / 4.0) * math.tau
            _add_sphere(f"{safe}_MineralNode_{i}", (math.cos(angle) * 0.30, math.sin(angle) * 0.30, 0.22), 0.07, REMNANT_GLOW, "POI_MineralNode_Mat")

    _add_label_post(safe, color=ICE_BLUE if is_ice else WARNING_YELLOW)
    return join_and_rename(asset_name)


def gen_meteor_field_prop(poi_id, asset_name):
    """Meteor impact POI — crater ring with a central hot fragment."""
    clear_scene()
    safe = _safe_asset_token(poi_id)
    _add_cylinder(f"{safe}_CraterFloor", (0, 0, 0.045), 0.78, 0.06, STONE_DARK, "POI_CraterFloor_Mat", vertices=48)
    _add_torus(f"{safe}_CraterRim", (0, 0, 0.10), 0.72, 0.10, STONE, "POI_CraterRim_Mat")
    _add_sphere(f"{safe}_MeteorCore", (0.10, -0.05, 0.24), 0.22, RUST, "POI_MeteorCore_Mat", scale=(1.15, 0.8, 0.75))
    _add_sphere(f"{safe}_HotFace", (0.19, -0.08, 0.27), 0.08, EMBER, "POI_HotFace_Mat")
    _scatter_rocks(safe, count=10, radius=0.95, color=STONE)
    _add_label_post(safe)
    return join_and_rename(asset_name)


def gen_razor_ridge_prop(poi_id, asset_name):
    """Razorstone ridge POI — jagged blade-rock silhouette."""
    clear_scene()
    safe = _safe_asset_token(poi_id)
    _add_cube(f"{safe}_RidgeBase", (0, 0, 0.06), (1.25, 0.26, 0.06), STONE_DARK, "POI_RidgeBase_Mat")
    for i in range(9):
        x = -0.55 + i * 0.14
        height = 0.35 + 0.12 * ((i * 3) % 4)
        blade = _add_cone(f"{safe}_Blade_{i}", (x, 0.02 * ((i % 2) * 2 - 1), height / 2.0 + 0.06), 0.08, height, STONE, "POI_RazorBlade_Mat", vertices=4, rotation=(0.0, 0.22 * ((i % 2) * 2 - 1), 0.79))
        blade.scale.x = 0.65
    _add_label_post(safe)
    return join_and_rename(asset_name)


def gen_remnant_site_prop(poi_id, asset_name):
    """Remnant site POI — non-human monolith cluster with luminous core."""
    clear_scene()
    safe = _safe_asset_token(poi_id)
    _add_cylinder(f"{safe}_Foundation", (0, 0, 0.08), 0.78, 0.12, REMNANT, "POI_RemnantFoundation_Mat", vertices=6, rotation=(0.0, 0.0, math.pi / 6))
    for i in range(5):
        angle = (i / 5.0) * math.tau
        x = math.cos(angle) * 0.42
        y = math.sin(angle) * 0.42
        height = 0.65 + 0.15 * (i % 2)
        _add_cube(f"{safe}_Monolith_{i}", (x, y, height / 2.0 + 0.12), (0.08, 0.12, height / 2.0), REMNANT, "POI_RemnantMonolith_Mat", rotation=(0.0, 0.0, angle))
    _add_torus(f"{safe}_HaloRing", (0, 0, 0.86), 0.32, 0.025, REMNANT_GLOW, "POI_RemnantHalo_Mat", rotation=(math.pi / 2, 0.0, 0.0))
    _add_sphere(f"{safe}_Core", (0, 0, 0.62), 0.13, REMNANT_GLOW, "POI_RemnantCore_Mat")
    _add_label_post(safe, color=REMNANT_GLOW)
    return join_and_rename(asset_name)


def gen_thermal_vent_prop(poi_id, asset_name):
    """Thermal vent POI — clustered black vents with ember heat markers."""
    clear_scene()
    safe = _safe_asset_token(poi_id)
    _add_cylinder(f"{safe}_ScorchedGround", (0, 0, 0.04), 0.82, 0.05, STONE_DARK, "POI_ScorchedGround_Mat", vertices=40)
    for i in range(5):
        angle = (i / 5.0) * math.tau
        x = math.cos(angle) * (0.18 + 0.08 * (i % 2))
        y = math.sin(angle) * (0.18 + 0.08 * (i % 2))
        height = 0.28 + 0.08 * (i % 3)
        _add_cone(f"{safe}_VentCone_{i}", (x, y, height / 2.0 + 0.04), 0.12, height, STONE, "POI_VentCone_Mat", radius2=0.045, vertices=18)
        _add_sphere(f"{safe}_HeatGlow_{i}", (x, y, height + 0.08), 0.065, EMBER, "POI_HeatGlow_Mat", scale=(0.8, 0.8, 1.25))
    _add_label_post(safe, color=EMBER)
    return join_and_rename(asset_name)


def gen_generic_environment_prop(poi_id, asset_name):
    """Fallback environmental POI — neutral surveyable landmark."""
    clear_scene()
    safe = _safe_asset_token(poi_id)
    _add_cylinder(f"{safe}_GroundDisc", (0, 0, 0.04), 0.62, 0.05, STONE, "POI_GroundDisc_Mat", vertices=32)
    _add_cube(f"{safe}_LandmarkBlock", (0, 0, 0.32), (0.22, 0.22, 0.28), METAL_MID, "POI_LandmarkBlock_Mat", rotation=(0.0, 0.0, math.pi / 4))
    _add_cylinder(f"{safe}_Beacon", (0, 0, 0.78), 0.05, 0.52, WARNING_YELLOW, "POI_Beacon_Mat", vertices=12)
    _add_sphere(f"{safe}_BeaconGlow", (0, 0, 1.08), 0.09, REMNANT_GLOW, "POI_BeaconGlow_Mat")
    _scatter_rocks(safe, count=6, radius=0.70, color=STONE)
    _add_label_post(safe)
    return join_and_rename(asset_name)


# ── Main dispatch ──────────────────────────────────────────────────────────────

def gen_poi_prop(row):
    poi_id = _get_poi_id(row)
    safe = _safe_asset_token(poi_id)
    asset_name = f"SM_POI_{safe}"

    if "WRECK" in safe or "DEBRIS" in safe:
        gen_wreck_prop(poi_id, asset_name)
    elif "CAVE" in safe or "SINKHOLE" in safe or "TUNNEL" in safe:
        gen_cave_or_tunnel_prop(poi_id, asset_name)
    elif "METEOR" in safe or "IMPACT" in safe:
        gen_meteor_field_prop(poi_id, asset_name)
    elif "RAZOR" in safe or "RIDGE" in safe:
        gen_razor_ridge_prop(poi_id, asset_name)
    elif "REMNANT" in safe:
        gen_remnant_site_prop(poi_id, asset_name)
    elif "THERMAL" in safe or "VENT" in safe:
        gen_thermal_vent_prop(poi_id, asset_name)
    else:
        gen_generic_environment_prop(poi_id, asset_name)

    return asset_name


def main():
    dt_file, output_dir = _parse_args()

    print("\n=== Quiet Rift: Enigma — POI Prop Asset Generator ===")
    print(f"Data table: {os.path.abspath(dt_file)}")
    print(f"Output directory: {os.path.abspath(output_dir)}")

    if not os.path.exists(dt_file):
        raise FileNotFoundError(
            f"Could not find POI archetype CSV: {dt_file}\n"
            "Pass it explicitly with: -- --dt_file <DT_FILE>.csv"
        )

    rows = _read_rows(dt_file)
    print(f"Rows found: {len(rows)}")

    for row in rows:
        poi_id = _get_poi_id(row)
        asset_name = f"SM_POI_{_safe_asset_token(poi_id)}"
        print(f"\n[{poi_id}] -> {asset_name}")
        gen_poi_prop(row)
        out_path = os.path.join(output_dir, f"{asset_name}.fbx")
        export_fbx(asset_name, out_path)

    print("\n=== Generation complete ===")
    print(f"Exported {len(rows)} POI prop placeholder mesh(es).")
    print(f"Output directory: {os.path.abspath(output_dir)}")
    print("Import these FBX files into UE5 via Content Browser > Import.")


if __name__ == "__main__":
    main()
