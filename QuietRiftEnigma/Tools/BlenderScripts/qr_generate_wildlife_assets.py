"""
Quiet Rift: Enigma — Wildlife Procedural Asset Generator (Blender 4.x)

Run this in Blender's Scripting workspace or via:
    blender --background --python qr_generate_wildlife_assets.py

Reads every row in DT_Species_Wildlife.csv and exports one placeholder mesh
per species. Each mesh is a recognizable silhouette sized for UE5 import
(1 UU = 1 cm). Shared helpers live in qr_blender_common.py.

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

# Make sibling helper module importable when run via `blender --python`.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from qr_blender_common import (  # noqa: E402
    SCALE,
    clear_scene,
    export_fbx,
    add_material,
    join_and_rename,
)

# ── Configuration ──────────────────────────────────────────────────────────────
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/wildlife")
CSV_PATH = os.path.join(
    os.path.dirname(__file__),
    "../../Content/QuietRift/Data/DT_Species_Wildlife.csv",
)

# Color palette keyed off BehaviorRole so silhouettes read at a glance in editor.
ROLE_COLORS = {
    "Prey":      (0.65, 0.55, 0.40, 1.0),
    "Predator":  (0.20, 0.10, 0.10, 1.0),
    "Scavenger": (0.45, 0.35, 0.25, 1.0),
    "Hazard":    (0.30, 0.55, 0.30, 1.0),
    "Ambient":   (0.55, 0.55, 0.50, 1.0),
}


def _role_color(role):
    return ROLE_COLORS.get(role, (0.6, 0.6, 0.6, 1.0))


# ── Generator Functions (one per species in DT_Species_Wildlife.csv) ──────────

def gen_ridgeback_grazer(role):
    """Large herd quadruped with dorsal plate ridge."""
    clear_scene()
    color = _role_color(role)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.7, location=(0, 0, 0.9))
    body = bpy.context.active_object
    body.scale = (1.6, 0.85, 0.7)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, color, "Ridgeback_Body_Mat")
    # Dorsal ridge plates
    for i in range(6):
        bpy.ops.mesh.primitive_cube_add(size=1, location=((i - 2.5) * 0.28, 0, 1.45))
        plate = bpy.context.active_object
        plate.scale = (0.18, 0.45, 0.10)
        bpy.ops.object.transform_apply(scale=True)
        add_material(plate, (0.55, 0.45, 0.32, 1.0), "Ridgeback_Plate_Mat")
    # Four legs
    for sx, sy in [(0.55, 0.45), (0.55, -0.45), (-0.55, 0.45), (-0.55, -0.45)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.09, depth=0.9, location=(sx, sy, 0.45))
        add_material(bpy.context.active_object, color, "Ridgeback_Leg_Mat")
    # Head
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.32, location=(1.3, 0, 1.0))
    add_material(bpy.context.active_object, color, "Ridgeback_Head_Mat")
    join_and_rename("SM_ANM_RidgebackGrazer")


def gen_glasshorn_runner(role):
    """Small fast quadruped with a single forward-curving glass horn."""
    clear_scene()
    color = _role_color(role)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.35, location=(0, 0, 0.55))
    body = bpy.context.active_object
    body.scale = (1.5, 0.6, 0.55)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, color, "Glasshorn_Body_Mat")
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, location=(0.7, 0, 0.7))
    add_material(bpy.context.active_object, color, "Glasshorn_Head_Mat")
    # Horn
    bpy.ops.mesh.primitive_cone_add(radius1=0.04, radius2=0.0, depth=0.35, location=(0.85, 0, 0.95))
    horn = bpy.context.active_object
    horn.rotation_euler.y = math.pi / 3
    add_material(horn, (0.85, 0.92, 0.95, 1.0), "Glasshorn_Horn_Mat")
    # Slim legs
    for sx, sy in [(0.35, 0.22), (0.35, -0.22), (-0.35, 0.22), (-0.35, -0.22)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.04, depth=0.6, location=(sx, sy, 0.25))
        add_material(bpy.context.active_object, color, "Glasshorn_Leg_Mat")
    join_and_rename("SM_ANM_GlasshornRunner")


def gen_ashback_boar(role):
    """Squat boar with dorsal ash-streak and tusks."""
    clear_scene()
    color = _role_color(role)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.5, location=(0, 0, 0.55))
    body = bpy.context.active_object
    body.scale = (1.3, 0.75, 0.65)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, color, "AshBoar_Body_Mat")
    # Dorsal ash stripe
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.95))
    stripe = bpy.context.active_object
    stripe.scale = (0.55, 0.10, 0.04)
    bpy.ops.object.transform_apply(scale=True)
    add_material(stripe, (0.20, 0.18, 0.18, 1.0), "AshBoar_Stripe_Mat")
    # Snout
    bpy.ops.mesh.primitive_cylinder_add(radius=0.16, depth=0.3, location=(0.75, 0, 0.55))
    snout = bpy.context.active_object
    snout.rotation_euler.y = math.pi / 2
    add_material(snout, color, "AshBoar_Snout_Mat")
    # Tusks
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cone_add(radius1=0.03, radius2=0.0, depth=0.18, location=(0.85, side * 0.08, 0.5))
        tusk = bpy.context.active_object
        tusk.rotation_euler.y = -math.pi / 2.5
        add_material(tusk, (0.92, 0.88, 0.78, 1.0), "AshBoar_Tusk_Mat")
    # Stout legs
    for sx, sy in [(0.4, 0.3), (0.4, -0.3), (-0.4, 0.3), (-0.4, -0.3)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.07, depth=0.5, location=(sx, sy, 0.25))
        add_material(bpy.context.active_object, color, "AshBoar_Leg_Mat")
    join_and_rename("SM_ANM_AshbackBoar")


def gen_hookjaw_stalker(role):
    """Sleek silent predator with elongated hook jaw."""
    clear_scene()
    color = _role_color(role)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.55, location=(0, 0, 0.85))
    body = bpy.context.active_object
    body.scale = (1.7, 0.7, 0.6)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, color, "Hookjaw_Body_Mat")
    # Head
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.25, location=(1.0, 0, 0.95))
    add_material(bpy.context.active_object, color, "Hookjaw_Head_Mat")
    # Hook jaw — curved cone
    bpy.ops.mesh.primitive_cone_add(radius1=0.06, radius2=0.0, depth=0.35, location=(1.25, 0, 0.78))
    jaw = bpy.context.active_object
    jaw.rotation_euler.y = math.pi / 1.7
    add_material(jaw, (0.10, 0.05, 0.05, 1.0), "Hookjaw_Jaw_Mat")
    # Lean legs
    for sx, sy in [(0.55, 0.32), (0.55, -0.32), (-0.55, 0.32), (-0.55, -0.32)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.05, depth=0.85, location=(sx, sy, 0.43))
        add_material(bpy.context.active_object, color, "Hookjaw_Leg_Mat")
    join_and_rename("SM_ANM_HookjawStalker")


def gen_thornhide_dray(role):
    """Small pack scavenger with bristling thorn spines."""
    clear_scene()
    color = _role_color(role)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.3, location=(0, 0, 0.45))
    body = bpy.context.active_object
    body.scale = (1.2, 0.65, 0.6)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, color, "ThornDray_Body_Mat")
    # Thorn spines
    for i in range(10):
        angle = (i / 10.0) * math.tau
        x = math.cos(angle) * 0.25
        y = math.sin(angle) * 0.18
        bpy.ops.mesh.primitive_cone_add(radius1=0.02, radius2=0.0, depth=0.18, location=(x, y, 0.65))
        spine = bpy.context.active_object
        spine.rotation_euler = (math.pi / 6 * math.sin(angle), math.pi / 6 * math.cos(angle), 0)
        add_material(spine, (0.20, 0.15, 0.10, 1.0), "ThornDray_Spine_Mat")
    # Head
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.16, location=(0.45, 0, 0.5))
    add_material(bpy.context.active_object, color, "ThornDray_Head_Mat")
    # Legs
    for sx, sy in [(0.25, 0.2), (0.25, -0.2), (-0.25, 0.2), (-0.25, -0.2)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.04, depth=0.4, location=(sx, sy, 0.2))
        add_material(bpy.context.active_object, color, "ThornDray_Leg_Mat")
    join_and_rename("SM_ANM_ThornhideDray")


def gen_mire_ox(role):
    """Massive heavy-bodied quadruped with broad horns."""
    clear_scene()
    color = _role_color(role)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.95, location=(0, 0, 1.1))
    body = bpy.context.active_object
    body.scale = (1.8, 1.0, 0.85)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, color, "MireOx_Body_Mat")
    # Head
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.4, location=(1.6, 0, 1.05))
    add_material(bpy.context.active_object, color, "MireOx_Head_Mat")
    # Horns
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.05, depth=0.7, location=(1.55, side * 0.4, 1.25))
        horn = bpy.context.active_object
        horn.rotation_euler.x = side * math.pi / 2.2
        add_material(horn, (0.18, 0.14, 0.10, 1.0), "MireOx_Horn_Mat")
    # Stout legs
    for sx, sy in [(0.7, 0.55), (0.7, -0.55), (-0.7, 0.55), (-0.7, -0.55)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.13, depth=1.0, location=(sx, sy, 0.5))
        add_material(bpy.context.active_object, color, "MireOx_Leg_Mat")
    join_and_rename("SM_ANM_MireOx")


def gen_lantern_mite_swarm(role):
    """Loose cloud of small glowing mite bodies."""
    clear_scene()
    import random
    random.seed(7)
    base_color = _role_color(role)
    for _ in range(60):
        x = random.uniform(-0.7, 0.7)
        y = random.uniform(-0.7, 0.7)
        z = random.uniform(0.2, 1.2)
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.05, subdivisions=1, location=(x, y, z))
        mite = bpy.context.active_object
        glow = random.uniform(0.6, 1.0)
        add_material(mite, (base_color[0] * 0.4, base_color[1] * glow, base_color[2] * 0.4, 1.0), "LanternMite_Mat")
    join_and_rename("SM_ANM_LanternMiteSwarm")


def gen_burrow_eel(role):
    """Long undulating worm-eel emerging from the ground."""
    clear_scene()
    color = _role_color(role)
    # Segmented body
    for i in range(6):
        z = 0.1 + i * 0.18
        radius = 0.12 - i * 0.012
        bpy.ops.mesh.primitive_uv_sphere_add(radius=radius, location=(math.sin(i * 0.7) * 0.1, math.cos(i * 0.5) * 0.1, z))
        seg = bpy.context.active_object
        seg.scale.z = 1.4
        bpy.ops.object.transform_apply(scale=True)
        add_material(seg, color, "BurrowEel_Seg_Mat")
    # Mouth
    bpy.ops.mesh.primitive_cone_add(radius1=0.1, radius2=0.05, depth=0.1, location=(0.2, 0.2, 1.25))
    add_material(bpy.context.active_object, (0.45, 0.15, 0.15, 1.0), "BurrowEel_Mouth_Mat")
    join_and_rename("SM_ANM_BurrowEel")


def gen_ironmantle_beetle(role):
    """Low armored beetle with overlapping mantle plates."""
    clear_scene()
    color = _role_color(role)
    # Body underside
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.35, location=(0, 0, 0.18))
    body = bpy.context.active_object
    body.scale = (1.5, 0.9, 0.4)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, color, "Ironmantle_Body_Mat")
    # Mantle plates (overlapping shells)
    for i in range(4):
        bpy.ops.mesh.primitive_cube_add(size=1, location=((i - 1.5) * 0.22, 0, 0.32))
        plate = bpy.context.active_object
        plate.scale = (0.13, 0.32, 0.06)
        plate.rotation_euler.y = -0.15
        bpy.ops.object.transform_apply(scale=True)
        add_material(plate, (0.30, 0.30, 0.35, 1.0), "Ironmantle_Plate_Mat")
    # Six little legs
    for i in range(6):
        side = 1 if i < 3 else -1
        row = (i % 3) - 1
        bpy.ops.mesh.primitive_cylinder_add(radius=0.025, depth=0.18, location=(row * 0.3, side * 0.32, 0.09))
        add_material(bpy.context.active_object, (0.18, 0.18, 0.20, 1.0), "Ironmantle_Leg_Mat")
    join_and_rename("SM_ANM_IronmantleBeetle")


def gen_pale_rafter(role):
    """Cave-dwelling vertical predator: pale body with batlike wing flaps."""
    clear_scene()
    color = _role_color(role)
    # Body
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.32, location=(0, 0, 1.1))
    body = bpy.context.active_object
    body.scale = (0.7, 0.6, 1.4)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, (0.85, 0.85, 0.80, 1.0), "PaleRafter_Body_Mat")
    # Head
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.16, location=(0, 0, 1.55))
    add_material(bpy.context.active_object, (0.80, 0.80, 0.75, 1.0), "PaleRafter_Head_Mat")
    # Wing flaps
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, side * 0.55, 1.1))
        wing = bpy.context.active_object
        wing.scale = (0.05, 0.5, 0.6)
        bpy.ops.object.transform_apply(scale=True)
        add_material(wing, color, "PaleRafter_Wing_Mat")
    # Hanging claws
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cone_add(radius1=0.03, radius2=0.0, depth=0.15, location=(0, side * 0.1, 0.55))
        claw = bpy.context.active_object
        claw.rotation_euler.x = math.pi
        add_material(claw, (0.30, 0.20, 0.20, 1.0), "PaleRafter_Claw_Mat")
    join_and_rename("SM_ANM_PaleRafter")


def gen_stonebelly_tortoise(role):
    """Heavy domed shell creature with stubby legs."""
    clear_scene()
    color = _role_color(role)
    # Shell dome
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.7, location=(0, 0, 0.5))
    shell = bpy.context.active_object
    shell.scale.z = 0.55
    bpy.ops.object.transform_apply(scale=True)
    add_material(shell, (0.40, 0.35, 0.30, 1.0), "Stonebelly_Shell_Mat")
    # Belly
    bpy.ops.mesh.primitive_cylinder_add(radius=0.62, depth=0.18, location=(0, 0, 0.18))
    add_material(bpy.context.active_object, color, "Stonebelly_Belly_Mat")
    # Head poking out
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, location=(0.75, 0, 0.3))
    add_material(bpy.context.active_object, color, "Stonebelly_Head_Mat")
    # Stubby legs
    for sx, sy in [(0.4, 0.45), (0.4, -0.45), (-0.4, 0.45), (-0.4, -0.45)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.09, depth=0.18, location=(sx, sy, 0.12))
        add_material(bpy.context.active_object, color, "Stonebelly_Leg_Mat")
    join_and_rename("SM_ANM_StonebellyTortoise")


def gen_embermane_alpha(role):
    """Boss-tier predator with flame-colored mane and oversized fore-claws."""
    clear_scene()
    color = _role_color(role)
    # Body
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.85, location=(0, 0, 1.1))
    body = bpy.context.active_object
    body.scale = (1.7, 0.85, 0.85)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, color, "Embermane_Body_Mat")
    # Head
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.4, location=(1.4, 0, 1.25))
    add_material(bpy.context.active_object, color, "Embermane_Head_Mat")
    # Mane — ring of cones around the neck
    for i in range(14):
        angle = (i / 14.0) * math.tau
        x = 1.05 + math.cos(angle) * 0.05
        y = math.sin(angle) * 0.45
        z = 1.25 + math.sin(angle) * 0.45
        bpy.ops.mesh.primitive_cone_add(radius1=0.05, radius2=0.0, depth=0.35, location=(x, y, z))
        spike = bpy.context.active_object
        spike.rotation_euler = (math.cos(angle) * math.pi / 3, 0, angle)
        add_material(spike, (0.85, 0.30, 0.05, 1.0), "Embermane_Mane_Mat")
    # Fore-claws (oversized cones at front legs)
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cone_add(radius1=0.07, radius2=0.0, depth=0.3, location=(0.85, side * 0.55, 0.25))
        claw = bpy.context.active_object
        claw.rotation_euler.x = math.pi
        add_material(claw, (0.18, 0.10, 0.10, 1.0), "Embermane_Claw_Mat")
    # Legs
    for sx, sy in [(0.75, 0.5), (0.75, -0.5), (-0.75, 0.5), (-0.75, -0.5)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.11, depth=1.0, location=(sx, sy, 0.5))
        add_material(bpy.context.active_object, color, "Embermane_Leg_Mat")
    join_and_rename("SM_ANM_EmbermaneAlpha")


# ── Dispatch table — every SpeciesId in the CSV must map to a generator. ─────

GENERATORS = {
    "ANM_RidgebackGrazer":    gen_ridgeback_grazer,
    "ANM_GlasshornRunner":    gen_glasshorn_runner,
    "ANM_AshbackBoar":        gen_ashback_boar,
    "ANM_HookjawStalker":     gen_hookjaw_stalker,
    "ANM_ThornhideDray":      gen_thornhide_dray,
    "ANM_MireOx":             gen_mire_ox,
    "ANM_LanternMiteSwarm":   gen_lantern_mite_swarm,
    "ANM_BurrowEel":          gen_burrow_eel,
    "ANM_IronmantleBeetle":   gen_ironmantle_beetle,
    "ANM_PaleRafter":         gen_pale_rafter,
    "ANM_StonebellyTortoise": gen_stonebelly_tortoise,
    "ANM_EmbermaneAlpha":     gen_embermane_alpha,
}


# ── Main export pipeline ───────────────────────────────────────────────────────

def main():
    print("\n=== Quiet Rift: Enigma — Wildlife Asset Generator ===")
    csv_abs = os.path.abspath(CSV_PATH)
    if not os.path.isfile(csv_abs):
        print(f"ERROR: CSV not found at {csv_abs}")
        return

    with open(csv_abs, newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        rows = [r for r in reader if r.get("SpeciesId")]

    missing = [r["SpeciesId"] for r in rows if r["SpeciesId"] not in GENERATORS]
    if missing:
        print(f"WARN: no generator registered for: {missing}")

    for row in rows:
        sid = row["SpeciesId"]
        gen = GENERATORS.get(sid)
        if gen is None:
            continue
        print(f"\n[{sid}] {row.get('DisplayName', sid)} ({row.get('BehaviorRole', '?')})")
        gen(row.get("BehaviorRole", "Ambient"))
        out_path = os.path.join(OUTPUT_DIR, f"SM_{sid}.fbx")
        export_fbx(sid, out_path)

    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Import these FBX files into UE5 via Content Browser > Import.")


if __name__ == "__main__":
    main()
