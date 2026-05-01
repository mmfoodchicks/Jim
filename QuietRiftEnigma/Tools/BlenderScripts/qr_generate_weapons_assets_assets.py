"""
Quiet Rift: Enigma — Weapons Procedural Asset Generator (Blender 4.x)

Run this in Blender's Scripting workspace or via:
    blender --background --python qr_generate_weapons_assets_assets.py

Reads every row in DT_ArmoryWeapons.csv and exports one placeholder
mesh per weapon id using the helpers in qr_blender_common.py. Each
weapon is a recognizable silhouette (pistol, SMG, carbine, DMR,
pump shotgun, bolt sniper, long-range sniper) sized for UE5 import
(1 UU = 1 cm).

Conventions:
    +X is the muzzle direction. Origin sits at the front of the grip.

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
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/weapons_assets")
CSV_PATH = os.path.join(
    os.path.dirname(__file__),
    "../../Content/QuietRift/Data/DT_ArmoryWeapons.csv",
)

# Standard part colors
STEEL    = (0.30, 0.30, 0.33, 1.0)
GUNMETAL = (0.18, 0.18, 0.20, 1.0)
POLYMER  = (0.10, 0.10, 0.10, 1.0)
WOOD     = (0.45, 0.30, 0.18, 1.0)
GLASS    = (0.20, 0.30, 0.40, 0.6)
BRASS    = (0.75, 0.55, 0.20, 1.0)


# ── Reusable part builders ────────────────────────────────────────────────────

def _barrel(length, radius, x_origin):
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=length, location=(x_origin + length / 2, 0, 0))
    b = bpy.context.active_object
    b.rotation_euler.y = math.pi / 2
    add_material(b, GUNMETAL, "Barrel_Mat")
    return b


def _receiver(length, height, depth, x_origin):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x_origin + length / 2, 0, 0))
    r = bpy.context.active_object
    r.scale = (length, depth, height)
    bpy.ops.object.transform_apply(scale=True)
    add_material(r, STEEL, "Receiver_Mat")
    return r


def _grip(x_origin, height=0.08, depth=0.025):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x_origin, 0, -height / 2 - 0.015))
    g = bpy.context.active_object
    g.scale = (0.035, depth, height)
    g.rotation_euler.y = -0.25
    bpy.ops.object.transform_apply(scale=True)
    add_material(g, POLYMER, "Grip_Mat")
    return g


def _magazine(x_origin, length=0.05, depth=0.02, drop=0.07):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x_origin, 0, -drop / 2 - 0.015))
    m = bpy.context.active_object
    m.scale = (length, depth, drop)
    bpy.ops.object.transform_apply(scale=True)
    add_material(m, GUNMETAL, "Mag_Mat")
    return m


def _stock(x_origin, length=0.18, height=0.045, depth=0.025, material=POLYMER):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x_origin - length / 2, 0, 0))
    s = bpy.context.active_object
    s.scale = (length, depth, height)
    bpy.ops.object.transform_apply(scale=True)
    add_material(s, material, "Stock_Mat")
    return s


def _scope(x_center, body_len=0.12, radius=0.018, height=0.045):
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=body_len, location=(x_center, 0, height))
    body = bpy.context.active_object
    body.rotation_euler.y = math.pi / 2
    add_material(body, GUNMETAL, "Scope_Body_Mat")
    # Objective bell
    bpy.ops.mesh.primitive_cylinder_add(radius=radius * 1.4, depth=0.025, location=(x_center + body_len / 2, 0, height))
    bell = bpy.context.active_object
    bell.rotation_euler.y = math.pi / 2
    add_material(bell, GUNMETAL, "Scope_Bell_Mat")
    # Lens
    bpy.ops.mesh.primitive_cylinder_add(radius=radius * 1.2, depth=0.005, location=(x_center + body_len / 2 + 0.013, 0, height))
    lens = bpy.context.active_object
    lens.rotation_euler.y = math.pi / 2
    add_material(lens, GLASS, "Scope_Lens_Mat")
    # Mount blocks (rear + front rails)
    for offset in (-body_len / 2 + 0.02, body_len / 2 - 0.02):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x_center + offset, 0, height - 0.025))
        mount = bpy.context.active_object
        mount.scale = (0.012, 0.018, 0.020)
        bpy.ops.object.transform_apply(scale=True)
        add_material(mount, STEEL, "Scope_Mount_Mat")


def _bipod(x_origin):
    """Folded-down bipod legs at front of weapon."""
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.005, depth=0.12, location=(x_origin, side * 0.025, -0.06))
        leg = bpy.context.active_object
        leg.rotation_euler.x = side * 0.25
        add_material(leg, STEEL, "Bipod_Leg_Mat")


# ── Generator Functions (one per WeaponId) ────────────────────────────────────

def gen_service_pistol():
    """Compact service pistol — short slide, polymer frame, single mag."""
    clear_scene()
    _receiver(length=0.16, height=0.05, depth=0.028, x_origin=0)
    # Slide on top
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.08, 0, 0.035))
    slide = bpy.context.active_object
    slide.scale = (0.16, 0.028, 0.025)
    bpy.ops.object.transform_apply(scale=True)
    add_material(slide, GUNMETAL, "Pistol_Slide_Mat")
    # Short barrel poking from slide
    _barrel(length=0.04, radius=0.006, x_origin=0.16)
    # Grip
    _grip(x_origin=0.02, height=0.10, depth=0.025)
    # Mag well in grip
    _magazine(x_origin=0.02, length=0.035, depth=0.022, drop=0.06)
    # Trigger guard
    bpy.ops.mesh.primitive_torus_add(major_radius=0.015, minor_radius=0.003, location=(0.04, 0, -0.02))
    add_material(bpy.context.active_object, GUNMETAL, "Pistol_TriggerGuard_Mat")
    join_and_rename("SM_WPN_SERVICE_PISTOL")


def gen_smg_compact():
    """Compact SMG — short barrel, vertical mag, folding stock."""
    clear_scene()
    _receiver(length=0.28, height=0.045, depth=0.030, x_origin=0)
    _barrel(length=0.16, radius=0.008, x_origin=0.28)
    _grip(x_origin=0.05, height=0.085)
    # Vertical mag (long, thin)
    _magazine(x_origin=0.10, length=0.04, depth=0.022, drop=0.13)
    # Folding stock (folded, short)
    _stock(x_origin=0, length=0.10, height=0.030, depth=0.022, material=POLYMER)
    # Top rail + sights
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.16, 0, 0.030))
    rail = bpy.context.active_object
    rail.scale = (0.20, 0.012, 0.005)
    bpy.ops.object.transform_apply(scale=True)
    add_material(rail, GUNMETAL, "SMG_Rail_Mat")
    join_and_rename("SM_WPN_SMG_COMPACT")


def gen_carbine():
    """General-purpose patrol rifle — full-length receiver, barrel, std mag."""
    clear_scene()
    _receiver(length=0.30, height=0.05, depth=0.032, x_origin=0)
    _barrel(length=0.32, radius=0.009, x_origin=0.30)
    # Handguard around front of barrel
    bpy.ops.mesh.primitive_cylinder_add(radius=0.022, depth=0.20, location=(0.40, 0, 0))
    hg = bpy.context.active_object
    hg.rotation_euler.y = math.pi / 2
    add_material(hg, POLYMER, "Carbine_Handguard_Mat")
    _grip(x_origin=0.06, height=0.085)
    _magazine(x_origin=0.12, length=0.045, depth=0.025, drop=0.10)
    _stock(x_origin=0, length=0.22, height=0.045, depth=0.028)
    # Iron sight rear
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.05, 0, 0.045))
    rsight = bpy.context.active_object
    rsight.scale = (0.012, 0.018, 0.022)
    bpy.ops.object.transform_apply(scale=True)
    add_material(rsight, GUNMETAL, "Carbine_RearSight_Mat")
    join_and_rename("SM_WPN_CARBINE")


def gen_dmr():
    """Marksman rifle — longer barrel, scope, fixed stock."""
    clear_scene()
    _receiver(length=0.36, height=0.052, depth=0.033, x_origin=0)
    _barrel(length=0.46, radius=0.010, x_origin=0.36)
    # Slim handguard
    bpy.ops.mesh.primitive_cylinder_add(radius=0.020, depth=0.32, location=(0.52, 0, 0))
    hg = bpy.context.active_object
    hg.rotation_euler.y = math.pi / 2
    add_material(hg, POLYMER, "DMR_Handguard_Mat")
    _grip(x_origin=0.07, height=0.085)
    _magazine(x_origin=0.14, length=0.045, depth=0.025, drop=0.10)
    _stock(x_origin=0, length=0.26, height=0.050, depth=0.030, material=POLYMER)
    # Cheek riser on stock
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.13, 0, 0.038))
    cheek = bpy.context.active_object
    cheek.scale = (0.16, 0.026, 0.020)
    bpy.ops.object.transform_apply(scale=True)
    add_material(cheek, POLYMER, "DMR_Cheek_Mat")
    _scope(x_center=0.18, body_len=0.18, radius=0.020, height=0.055)
    join_and_rename("SM_WPN_DMR")


def gen_pump_shotgun():
    """Pump-action shotgun — chunky receiver, pump grip, tubular magazine."""
    clear_scene()
    _receiver(length=0.30, height=0.062, depth=0.040, x_origin=0)
    # Heavy barrel (wider bore)
    _barrel(length=0.46, radius=0.013, x_origin=0.30)
    # Pump fore-end (slides on barrel — represented by chunkier ring)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.026, depth=0.16, location=(0.46, 0, -0.012))
    pump = bpy.context.active_object
    pump.rotation_euler.y = math.pi / 2
    add_material(pump, WOOD, "Pump_Foreend_Mat")
    # Tubular magazine under barrel
    bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.42, location=(0.50, 0, -0.022))
    tubmag = bpy.context.active_object
    tubmag.rotation_euler.y = math.pi / 2
    add_material(tubmag, GUNMETAL, "Pump_TubeMag_Mat")
    _grip(x_origin=0.05, height=0.090)
    # Wood stock (longer)
    _stock(x_origin=0, length=0.26, height=0.055, depth=0.030, material=WOOD)
    # Bead front sight
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.005, location=(0.76, 0, 0.018))
    add_material(bpy.context.active_object, BRASS, "Pump_Bead_Mat")
    join_and_rename("SM_WPN_PUMP_SHOTGUN")


def gen_bolt_sniper():
    """Bolt-action sniper — heavy receiver, scope, bolt handle, fixed stock."""
    clear_scene()
    _receiver(length=0.32, height=0.050, depth=0.034, x_origin=0)
    _barrel(length=0.62, radius=0.010, x_origin=0.32)
    # Bolt handle sticking out right side
    bpy.ops.mesh.primitive_cylinder_add(radius=0.005, depth=0.07, location=(0.10, 0.030, 0.020))
    bolt = bpy.context.active_object
    bolt.rotation_euler.x = math.pi / 2
    add_material(bolt, GUNMETAL, "BoltSniper_BoltHandle_Mat")
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.010, location=(0.10, 0.066, 0.020))
    add_material(bpy.context.active_object, GUNMETAL, "BoltSniper_BoltKnob_Mat")
    _grip(x_origin=0.06, height=0.085)
    _magazine(x_origin=0.13, length=0.038, depth=0.024, drop=0.06)
    _stock(x_origin=0, length=0.30, height=0.055, depth=0.034, material=WOOD)
    # Scope (large)
    _scope(x_center=0.22, body_len=0.24, radius=0.022, height=0.060)
    join_and_rename("SM_WPN_BOLT_SNIPER")


def gen_longrange_sniper():
    """Long-range sniper — extra-long heavy barrel, big scope, bipod, muzzle brake."""
    clear_scene()
    _receiver(length=0.34, height=0.054, depth=0.038, x_origin=0)
    _barrel(length=0.78, radius=0.012, x_origin=0.34)
    # Muzzle brake
    bpy.ops.mesh.primitive_cylinder_add(radius=0.020, depth=0.04, location=(1.14, 0, 0))
    mb = bpy.context.active_object
    mb.rotation_euler.y = math.pi / 2
    add_material(mb, GUNMETAL, "LRS_MuzzleBrake_Mat")
    # Brake slots
    for i in range(3):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(1.14 + (i - 1) * 0.012, 0, 0.018))
        slot = bpy.context.active_object
        slot.scale = (0.005, 0.024, 0.006)
        bpy.ops.object.transform_apply(scale=True)
        add_material(slot, POLYMER, "LRS_BrakeSlot_Mat")
    # Bolt handle
    bpy.ops.mesh.primitive_cylinder_add(radius=0.005, depth=0.07, location=(0.10, 0.030, 0.022))
    bolt = bpy.context.active_object
    bolt.rotation_euler.x = math.pi / 2
    add_material(bolt, GUNMETAL, "LRS_BoltHandle_Mat")
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.010, location=(0.10, 0.066, 0.022))
    add_material(bpy.context.active_object, GUNMETAL, "LRS_BoltKnob_Mat")
    _grip(x_origin=0.07, height=0.090)
    _magazine(x_origin=0.14, length=0.040, depth=0.026, drop=0.07)
    # Heavy chassis stock
    _stock(x_origin=0, length=0.34, height=0.060, depth=0.040, material=POLYMER)
    # Cheek riser
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.16, 0, 0.044))
    cheek = bpy.context.active_object
    cheek.scale = (0.18, 0.030, 0.022)
    bpy.ops.object.transform_apply(scale=True)
    add_material(cheek, POLYMER, "LRS_Cheek_Mat")
    # Big scope
    _scope(x_center=0.26, body_len=0.30, radius=0.026, height=0.070)
    # Folded bipod up front
    _bipod(x_origin=0.95)
    join_and_rename("SM_WPN_LONGRANGE_SNIPER")


# ── Dispatch table — every WeaponId in the CSV must map to a generator. ─────

GENERATORS = {
    "WPN_SERVICE_PISTOL":   gen_service_pistol,
    "WPN_SMG_COMPACT":      gen_smg_compact,
    "WPN_CARBINE":          gen_carbine,
    "WPN_DMR":              gen_dmr,
    "WPN_PUMP_SHOTGUN":     gen_pump_shotgun,
    "WPN_BOLT_SNIPER":      gen_bolt_sniper,
    "WPN_LONGRANGE_SNIPER": gen_longrange_sniper,
}


# ── Main export pipeline ───────────────────────────────────────────────────────

def main():
    print("\n=== Quiet Rift: Enigma — Weapons Asset Generator ===")
    csv_abs = os.path.abspath(CSV_PATH)
    if not os.path.isfile(csv_abs):
        print(f"ERROR: CSV not found at {csv_abs}")
        return

    with open(csv_abs, newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        rows = [r for r in reader if r.get("WeaponId")]

    missing = [r["WeaponId"] for r in rows if r["WeaponId"] not in GENERATORS]
    if missing:
        print(f"WARN: no generator registered for: {missing}")

    for row in rows:
        wid = row["WeaponId"]
        gen = GENERATORS.get(wid)
        if gen is None:
            continue
        print(f"\n[{wid}] {row.get('DisplayName', wid)}  (Tier {row.get('Tier','?')})")
        gen()
        out_path = os.path.join(OUTPUT_DIR, f"SM_{wid}.fbx")
        export_fbx(wid, out_path)

    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Import these FBX files into UE5 via Content Browser > Import.")


if __name__ == "__main__":
    main()
