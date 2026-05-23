"""
Quiet Rift: Enigma â€” Wildlife Procedural Asset Generator (Blender 4.x)

Upgraded in Batch 2 of the Blender detail pass â€” every generator now flows
through qr_blender_detail.py for production finalization (deduped per-
species materials, smooth shading + auto-smooth, angle-limited bevels,
smart UV unwrap, MouthSocket / SpineSocket sockets, convex collision,
and LOD chain).

Reads every row in DT_Species_Wildlife.csv and exports one placeholder
mesh per species using the helpers in qr_blender_common.py +
qr_blender_detail.py. Four legacy fauna relocated from the old flora
script (Shardback / Ironstag / Shellmaw / Fogleech) export from the
EXTRAS dict alongside the CSV-driven set until they're canonicalized.

Per-species detail pipeline:
    - Per-species named materials via get_or_create_material
      (e.g. "Wildlife_RidgebackGrazer_Body") â†’ automatic dedupe
    - Body / dorsal armor / appendage / accent material zones
    - Smooth shading at 30 deg auto-smooth (organic, soft)
    - 3 mm bevel limited by edge angle
    - Smart UV project so meshes import texture-ready
    - MouthSocket on the head, SpineSocket on the back
    - Convex hull collision (UCX_) per asset
    - LOD1 at 0.40 ratio
    - bottom_center pivot

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
    get_or_create_material,
    assign_material,
    add_socket,
    finalize_asset,
)

# â”€â”€ Configuration â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/wildlife")
CSV_PATH = os.path.join(
    os.path.dirname(__file__),
    "../../Content/QuietRift/Data/DT_Species_Wildlife.csv",
)
# v15 canonical species table -- ANI_* + PRD_* entity IDs, source-of-truth.
V15_CSV_PATH = os.path.join(
    os.path.dirname(__file__),
    "../../Content/Data/DT_Species_v15.csv",
)

# Color palette keyed off BehaviorRole so silhouettes read at a glance in editor.
ROLE_COLORS = {
    "Prey":      (0.65, 0.55, 0.40, 1.0),
    "Predator":  (0.20, 0.10, 0.10, 1.0),
    "Scavenger": (0.45, 0.35, 0.25, 1.0),
    "Hazard":    (0.30, 0.55, 0.30, 1.0),
    "Ambient":   (0.55, 0.55, 0.50, 1.0),
}
EYE_GLINT = (0.95, 0.85, 0.30, 1.0)


def _role_color(role):
    return ROLE_COLORS.get(role, (0.6, 0.6, 0.6, 1.0))


def _mat(slot, color, roughness=0.85, emissive=None):
    return get_or_create_material(slot, color, roughness=roughness,
                                  metallic=0.0, emissive=emissive)


def _add(obj, mat):
    assign_material(obj, mat)
    return obj


def _eye_glint(x, y, z, mat=None):
    """Tiny emissive sphere â€” shared eye-glint helper for predators / hazards."""
    if mat is None:
        mat = _mat("Wildlife_EyeGlint", EYE_GLINT, roughness=0.10, emissive=EYE_GLINT)
    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.012, subdivisions=1, location=(x, y, z))
    _add(bpy.context.active_object, mat)


def _quad_legs(positions, leg_radius, leg_depth, leg_color_mat):
    for sx, sy in positions:
        bpy.ops.mesh.primitive_cylinder_add(radius=leg_radius, depth=leg_depth,
                                             location=(sx, sy, leg_depth / 2))
        _add(bpy.context.active_object, leg_color_mat)


def _finalize_creature(name, mouth_socket=None, spine_socket=None, lods=(0.40,)):
    """Standard finalize for wildlife: smooth, beveled, UV'd, with MouthSocket
    and SpineSocket parented to the joined mesh."""
    if mouth_socket is not None:
        add_socket("MouthSocket", location=mouth_socket)
    if spine_socket is not None:
        add_socket("SpineSocket", location=spine_socket)
    finalize_asset(name,
                   bevel_width=0.003, bevel_angle_deg=30,
                   smooth_angle_deg=30, collision="convex",
                   lods=list(lods), pivot="bottom_center")


# â”€â”€ CSV-driven Generators (12 species) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def gen_ridgeback_grazer(role):
    """Large herd quadruped with dorsal plate ridge."""
    clear_scene()
    body_mat = _mat("Wildlife_Ridgeback_Body", _role_color(role))
    plate_mat = _mat("Wildlife_Ridgeback_Plate", (0.55, 0.45, 0.32, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.7, location=(0, 0, 0.9))
    body = bpy.context.active_object
    body.scale = (1.6, 0.85, 0.7); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    for i in range(6):
        bpy.ops.mesh.primitive_cube_add(size=1, location=((i - 2.5) * 0.28, 0, 1.45))
        plate = bpy.context.active_object
        plate.scale = (0.18, 0.45, 0.10); bpy.ops.object.transform_apply(scale=True)
        _add(plate, plate_mat)
    _quad_legs([(0.55, 0.45), (0.55, -0.45), (-0.55, 0.45), (-0.55, -0.45)],
                leg_radius=0.09, leg_depth=0.9, leg_color_mat=body_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.32, location=(1.3, 0, 1.0))
    _add(bpy.context.active_object, body_mat)
    _eye_glint(1.55, 0.18, 1.10); _eye_glint(1.55, -0.18, 1.10)
    _finalize_creature("SM_ANM_RidgebackGrazer",
                        mouth_socket=(1.55, 0, 0.95), spine_socket=(0, 0, 1.55))


def gen_glasshorn_runner(role):
    """Small fast quadruped with a single forward-curving glass horn."""
    clear_scene()
    body_mat = _mat("Wildlife_Glasshorn_Body", _role_color(role))
    horn_mat = _mat("Wildlife_Glasshorn_Horn", (0.85, 0.92, 0.95, 1.0),
                     roughness=0.20, emissive=(0.50, 0.85, 0.95, 0.6))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.35, location=(0, 0, 0.55))
    body = bpy.context.active_object
    body.scale = (1.5, 0.6, 0.55); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, location=(0.7, 0, 0.7))
    _add(bpy.context.active_object, body_mat)
    bpy.ops.mesh.primitive_cone_add(radius1=0.04, radius2=0.0, depth=0.35, location=(0.85, 0, 0.95))
    horn = bpy.context.active_object
    horn.rotation_euler.y = math.pi / 3
    _add(horn, horn_mat)
    _quad_legs([(0.35, 0.22), (0.35, -0.22), (-0.35, 0.22), (-0.35, -0.22)],
                leg_radius=0.04, leg_depth=0.6, leg_color_mat=body_mat)
    _eye_glint(0.85, 0.10, 0.74); _eye_glint(0.85, -0.10, 0.74)
    _finalize_creature("SM_ANM_GlasshornRunner",
                        mouth_socket=(0.88, 0, 0.62), spine_socket=(0, 0, 0.85))


def gen_ashback_boar(role):
    """Squat boar with dorsal ash-streak and tusks."""
    clear_scene()
    body_mat = _mat("Wildlife_AshBoar_Body", _role_color(role))
    stripe_mat = _mat("Wildlife_AshBoar_Stripe", (0.20, 0.18, 0.18, 1.0))
    tusk_mat = _mat("Wildlife_Tusk", (0.92, 0.88, 0.78, 1.0), roughness=0.40)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.5, location=(0, 0, 0.55))
    body = bpy.context.active_object
    body.scale = (1.3, 0.75, 0.65); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.95))
    stripe = bpy.context.active_object
    stripe.scale = (0.55, 0.10, 0.04); bpy.ops.object.transform_apply(scale=True)
    _add(stripe, stripe_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.16, depth=0.3, location=(0.75, 0, 0.55))
    snout = bpy.context.active_object
    snout.rotation_euler.y = math.pi / 2
    _add(snout, body_mat)
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cone_add(radius1=0.03, radius2=0.0, depth=0.18, location=(0.85, side * 0.08, 0.5))
        tusk = bpy.context.active_object
        tusk.rotation_euler.y = -math.pi / 2.5
        _add(tusk, tusk_mat)
    _quad_legs([(0.4, 0.3), (0.4, -0.3), (-0.4, 0.3), (-0.4, -0.3)],
                leg_radius=0.07, leg_depth=0.5, leg_color_mat=body_mat)
    _eye_glint(0.7, 0.16, 0.72); _eye_glint(0.7, -0.16, 0.72)
    _finalize_creature("SM_ANM_AshbackBoar",
                        mouth_socket=(0.92, 0, 0.55), spine_socket=(0, 0, 0.95))


def gen_hookjaw_stalker(role):
    """Sleek silent predator with elongated hook jaw."""
    clear_scene()
    body_mat = _mat("Wildlife_Hookjaw_Body", _role_color(role))
    jaw_mat = _mat("Wildlife_Hookjaw_Jaw", (0.10, 0.05, 0.05, 1.0), roughness=0.40)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.55, location=(0, 0, 0.85))
    body = bpy.context.active_object
    body.scale = (1.7, 0.7, 0.6); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.25, location=(1.0, 0, 0.95))
    _add(bpy.context.active_object, body_mat)
    bpy.ops.mesh.primitive_cone_add(radius1=0.06, radius2=0.0, depth=0.35, location=(1.25, 0, 0.78))
    jaw = bpy.context.active_object
    jaw.rotation_euler.y = math.pi / 1.7
    _add(jaw, jaw_mat)
    _quad_legs([(0.55, 0.32), (0.55, -0.32), (-0.55, 0.32), (-0.55, -0.32)],
                leg_radius=0.05, leg_depth=0.85, leg_color_mat=body_mat)
    _eye_glint(1.10, 0.13, 1.00); _eye_glint(1.10, -0.13, 1.00)
    _finalize_creature("SM_ANM_HookjawStalker",
                        mouth_socket=(1.40, 0, 0.70), spine_socket=(0, 0, 1.20))


def gen_thornhide_dray(role):
    """Small pack scavenger with bristling thorn spines."""
    clear_scene()
    body_mat = _mat("Wildlife_ThornDray_Body", _role_color(role))
    spine_mat = _mat("Wildlife_ThornDray_Spine", (0.20, 0.15, 0.10, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.3, location=(0, 0, 0.45))
    body = bpy.context.active_object
    body.scale = (1.2, 0.65, 0.6); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    for i in range(10):
        angle = (i / 10.0) * math.tau
        x = math.cos(angle) * 0.25
        y = math.sin(angle) * 0.18
        bpy.ops.mesh.primitive_cone_add(radius1=0.02, radius2=0.0, depth=0.18, location=(x, y, 0.65))
        spine = bpy.context.active_object
        spine.rotation_euler = (math.pi / 6 * math.sin(angle), math.pi / 6 * math.cos(angle), 0)
        _add(spine, spine_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.16, location=(0.45, 0, 0.5))
    _add(bpy.context.active_object, body_mat)
    _quad_legs([(0.25, 0.2), (0.25, -0.2), (-0.25, 0.2), (-0.25, -0.2)],
                leg_radius=0.04, leg_depth=0.4, leg_color_mat=body_mat)
    _eye_glint(0.55, 0.10, 0.55); _eye_glint(0.55, -0.10, 0.55)
    _finalize_creature("SM_ANM_ThornhideDray",
                        mouth_socket=(0.62, 0, 0.45), spine_socket=(0, 0, 0.78))


def gen_mire_ox(role):
    """Massive heavy-bodied quadruped with broad horns."""
    clear_scene()
    body_mat = _mat("Wildlife_MireOx_Body", _role_color(role))
    horn_mat = _mat("Wildlife_MireOx_Horn", (0.18, 0.14, 0.10, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.95, location=(0, 0, 1.1))
    body = bpy.context.active_object
    body.scale = (1.8, 1.0, 0.85); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.4, location=(1.6, 0, 1.05))
    _add(bpy.context.active_object, body_mat)
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.05, depth=0.7, location=(1.55, side * 0.4, 1.25))
        horn = bpy.context.active_object
        horn.rotation_euler.x = side * math.pi / 2.2
        _add(horn, horn_mat)
    _quad_legs([(0.7, 0.55), (0.7, -0.55), (-0.7, 0.55), (-0.7, -0.55)],
                leg_radius=0.13, leg_depth=1.0, leg_color_mat=body_mat)
    _eye_glint(1.85, 0.22, 1.20); _eye_glint(1.85, -0.22, 1.20)
    _finalize_creature("SM_ANM_MireOx",
                        mouth_socket=(1.95, 0, 0.95), spine_socket=(0, 0, 1.85))


def gen_lantern_mite_swarm(role):
    """Loose cloud of small glowing mite bodies."""
    clear_scene()
    base_color = _role_color(role)
    swarm_mat = _mat("Wildlife_LanternMite", (base_color[0] * 0.4, base_color[1] * 0.85,
                                                base_color[2] * 0.4, 1.0),
                      roughness=0.30,
                      emissive=(base_color[0] * 0.4, base_color[1] * 0.85, base_color[2] * 0.4, 1.0))
    random.seed(7)
    for _ in range(60):
        x = random.uniform(-0.7, 0.7)
        y = random.uniform(-0.7, 0.7)
        z = random.uniform(0.2, 1.2)
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.05, subdivisions=1, location=(x, y, z))
        _add(bpy.context.active_object, swarm_mat)
    # Swarm has no head/spine; use centroid sockets so AI can target.
    _finalize_creature("SM_ANM_LanternMiteSwarm",
                        mouth_socket=(0, 0, 0.7), spine_socket=(0, 0, 0.7),
                        lods=(0.30,))


def gen_burrow_eel(role):
    """Long undulating worm-eel emerging from the ground."""
    clear_scene()
    body_mat = _mat("Wildlife_BurrowEel_Body", _role_color(role))
    mouth_mat = _mat("Wildlife_BurrowEel_Mouth", (0.45, 0.15, 0.15, 1.0))
    for i in range(6):
        z = 0.1 + i * 0.18
        radius = 0.12 - i * 0.012
        bpy.ops.mesh.primitive_uv_sphere_add(radius=radius,
                                              location=(math.sin(i * 0.7) * 0.1, math.cos(i * 0.5) * 0.1, z))
        seg = bpy.context.active_object
        seg.scale.z = 1.4; bpy.ops.object.transform_apply(scale=True)
        _add(seg, body_mat)
    bpy.ops.mesh.primitive_cone_add(radius1=0.1, radius2=0.05, depth=0.1, location=(0.2, 0.2, 1.25))
    _add(bpy.context.active_object, mouth_mat)
    _finalize_creature("SM_ANM_BurrowEel",
                        mouth_socket=(0.2, 0.2, 1.30), spine_socket=(0, 0, 0.6))


def gen_ironmantle_beetle(role):
    """Low armored beetle with overlapping mantle plates."""
    clear_scene()
    body_mat = _mat("Wildlife_Ironmantle_Body", _role_color(role))
    plate_mat = _mat("Wildlife_Ironmantle_Plate", (0.30, 0.30, 0.35, 1.0), roughness=0.55)
    leg_mat = _mat("Wildlife_Ironmantle_Leg", (0.18, 0.18, 0.20, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.35, location=(0, 0, 0.18))
    body = bpy.context.active_object
    body.scale = (1.5, 0.9, 0.4); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    for i in range(4):
        bpy.ops.mesh.primitive_cube_add(size=1, location=((i - 1.5) * 0.22, 0, 0.32))
        plate = bpy.context.active_object
        plate.scale = (0.13, 0.32, 0.06)
        plate.rotation_euler.y = -0.15
        bpy.ops.object.transform_apply(scale=True)
        _add(plate, plate_mat)
    for i in range(6):
        side = 1 if i < 3 else -1
        row = (i % 3) - 1
        bpy.ops.mesh.primitive_cylinder_add(radius=0.025, depth=0.18, location=(row * 0.3, side * 0.32, 0.09))
        _add(bpy.context.active_object, leg_mat)
    _finalize_creature("SM_ANM_IronmantleBeetle",
                        mouth_socket=(0.35, 0, 0.18), spine_socket=(0, 0, 0.40))


def gen_pale_rafter(role):
    """Cave-dwelling vertical predator: pale body with batlike wing flaps."""
    clear_scene()
    body_mat = _mat("Wildlife_PaleRafter_Body", (0.85, 0.85, 0.80, 1.0))
    head_mat = _mat("Wildlife_PaleRafter_Head", (0.80, 0.80, 0.75, 1.0))
    wing_mat = _mat("Wildlife_PaleRafter_Wing", _role_color(role))
    claw_mat = _mat("Wildlife_PaleRafter_Claw", (0.30, 0.20, 0.20, 1.0), roughness=0.40)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.32, location=(0, 0, 1.1))
    body = bpy.context.active_object
    body.scale = (0.7, 0.6, 1.4); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.16, location=(0, 0, 1.55))
    _add(bpy.context.active_object, head_mat)
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, side * 0.55, 1.1))
        wing = bpy.context.active_object
        wing.scale = (0.05, 0.5, 0.6); bpy.ops.object.transform_apply(scale=True)
        _add(wing, wing_mat)
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cone_add(radius1=0.03, radius2=0.0, depth=0.15, location=(0, side * 0.1, 0.55))
        claw = bpy.context.active_object
        claw.rotation_euler.x = math.pi
        _add(claw, claw_mat)
    _eye_glint(0.10, 0.06, 1.58); _eye_glint(0.10, -0.06, 1.58)
    _finalize_creature("SM_ANM_PaleRafter",
                        mouth_socket=(0.12, 0, 1.50), spine_socket=(0, 0, 1.30))


def gen_stonebelly_tortoise(role):
    """Heavy domed shell creature with stubby legs."""
    clear_scene()
    shell_mat = _mat("Wildlife_Stonebelly_Shell", (0.40, 0.35, 0.30, 1.0))
    body_mat = _mat("Wildlife_Stonebelly_Body", _role_color(role))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.7, location=(0, 0, 0.5))
    shell = bpy.context.active_object
    shell.scale.z = 0.55; bpy.ops.object.transform_apply(scale=True)
    _add(shell, shell_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.62, depth=0.18, location=(0, 0, 0.18))
    _add(bpy.context.active_object, body_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, location=(0.75, 0, 0.3))
    _add(bpy.context.active_object, body_mat)
    for sx, sy in [(0.4, 0.45), (0.4, -0.45), (-0.4, 0.45), (-0.4, -0.45)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.09, depth=0.18, location=(sx, sy, 0.12))
        _add(bpy.context.active_object, body_mat)
    _finalize_creature("SM_ANM_StonebellyTortoise",
                        mouth_socket=(0.92, 0, 0.30), spine_socket=(0, 0, 0.78))


def gen_embermane_alpha(role):
    """Boss-tier predator with flame-colored mane and oversized fore-claws."""
    clear_scene()
    body_mat = _mat("Wildlife_Embermane_Body", _role_color(role))
    mane_mat = _mat("Wildlife_Embermane_Mane", (0.85, 0.30, 0.05, 1.0),
                     roughness=0.40, emissive=(0.85, 0.30, 0.05, 1.0))
    claw_mat = _mat("Wildlife_Embermane_Claw", (0.18, 0.10, 0.10, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.85, location=(0, 0, 1.1))
    body = bpy.context.active_object
    body.scale = (1.7, 0.85, 0.85); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.4, location=(1.4, 0, 1.25))
    _add(bpy.context.active_object, body_mat)
    for i in range(14):
        angle = (i / 14.0) * math.tau
        x = 1.05 + math.cos(angle) * 0.05
        y = math.sin(angle) * 0.45
        z = 1.25 + math.sin(angle) * 0.45
        bpy.ops.mesh.primitive_cone_add(radius1=0.05, radius2=0.0, depth=0.35, location=(x, y, z))
        spike = bpy.context.active_object
        spike.rotation_euler = (math.cos(angle) * math.pi / 3, 0, angle)
        _add(spike, mane_mat)
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cone_add(radius1=0.07, radius2=0.0, depth=0.3, location=(0.85, side * 0.55, 0.25))
        claw = bpy.context.active_object
        claw.rotation_euler.x = math.pi
        _add(claw, claw_mat)
    _quad_legs([(0.75, 0.5), (0.75, -0.5), (-0.75, 0.5), (-0.75, -0.5)],
                leg_radius=0.11, leg_depth=1.0, leg_color_mat=body_mat)
    _eye_glint(1.55, 0.20, 1.40); _eye_glint(1.55, -0.20, 1.40)
    _finalize_creature("SM_ANM_EmbermaneAlpha",
                        mouth_socket=(1.75, 0, 1.15), spine_socket=(0, 0, 1.85))


# â”€â”€ Legacy fauna (relocated from flora script in Batch 1, upgraded in Batch 2) â”€

def gen_shardback_grazer_legacy():
    """Low hexapod with ceramic back plates."""
    clear_scene()
    body_mat = _mat("Wildlife_Shardback_Body", (0.78, 0.72, 0.60, 1.0))
    plate_mat = _mat("Wildlife_Shardback_Plate", (0.88, 0.85, 0.78, 1.0))
    leg_mat = _mat("Wildlife_Shardback_Leg", (0.60, 0.55, 0.45, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.6, location=(0, 0, 0.5))
    body = bpy.context.active_object
    body.scale = (1.4, 0.8, 0.6); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    for i in range(5):
        bpy.ops.mesh.primitive_cube_add(size=1, location=((i - 2) * 0.22, 0, 0.72))
        plate = bpy.context.active_object
        plate.scale = (0.20, 0.50, 0.06); bpy.ops.object.transform_apply(scale=True)
        _add(plate, plate_mat)
    for i in range(6):
        side = 1 if i < 3 else -1
        row = (i % 3) - 1
        bpy.ops.mesh.primitive_cylinder_add(radius=0.05, depth=0.45,
                                             location=(row * 0.35, side * 0.65, 0.25))
        leg = bpy.context.active_object
        leg.rotation_euler.x = math.pi / 6
        _add(leg, leg_mat)
    _eye_glint(0.78, 0.18, 0.65); _eye_glint(0.78, -0.18, 0.65)
    _finalize_creature("SM_ANM_ShardbackGrazer",
                        mouth_socket=(0.95, 0, 0.55), spine_socket=(0, 0, 0.85))


def gen_ironstag_stalker_legacy():
    """Tall territorial predator with ferric blade antlers."""
    clear_scene()
    body_mat = _mat("Wildlife_Ironstag_Body", (0.15, 0.10, 0.10, 1.0))
    antler_mat = _mat("Wildlife_Ironstag_Antler", (0.65, 0.22, 0.08, 1.0),
                       roughness=0.40, emissive=(0.45, 0.10, 0.05, 1.0))
    plate_mat = _mat("Wildlife_Ironstag_ChestPlate", (0.30, 0.28, 0.35, 1.0), roughness=0.55)
    leg_mat = _mat("Wildlife_Ironstag_Leg", (0.13, 0.09, 0.09, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.7, location=(0, 0, 1.2))
    body = bpy.context.active_object
    body.scale = (1.6, 0.9, 0.9); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.2, depth=0.6, location=(0.7, 0, 1.6))
    neck = bpy.context.active_object
    neck.rotation_euler.y = math.pi / 4
    _add(neck, body_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.3, location=(1.1, 0, 1.9))
    _add(bpy.context.active_object, body_mat)
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(1.0, side * 0.25, 2.3))
        antler = bpy.context.active_object
        antler.scale = (0.05, 0.08, 0.55)
        antler.rotation_euler.z = math.pi / 8 * side
        bpy.ops.object.transform_apply(scale=True)
        _add(antler, antler_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.0))
    plate = bpy.context.active_object
    plate.scale = (0.35, 0.55, 0.28); bpy.ops.object.transform_apply(scale=True)
    _add(plate, plate_mat)
    for i in range(4):
        row = 1 if i < 2 else -1
        side = 1 if i % 2 == 0 else -1
        bpy.ops.mesh.primitive_cylinder_add(radius=0.08, depth=1.0,
                                             location=(row * 0.5, side * 0.4, 0.5))
        _add(bpy.context.active_object, leg_mat)
    _eye_glint(1.25, 0.15, 1.95); _eye_glint(1.25, -0.15, 1.95)
    _finalize_creature("SM_ANM_IronstagStalker",
                        mouth_socket=(1.40, 0, 1.85), spine_socket=(0, 0, 1.95))


def gen_shellmaw_ambusher_legacy():
    """Broad shell-backed beast that hides as a mineral hump until opening trapdoor maw."""
    clear_scene()
    shell_mat = _mat("Wildlife_Shellmaw_Shell", (0.38, 0.28, 0.20, 1.0))
    crust_mat = _mat("Wildlife_Shellmaw_Crust", (0.42, 0.35, 0.25, 0.6))
    maw_mat = _mat("Wildlife_Shellmaw_Maw", (0.65, 0.48, 0.40, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.9, location=(0, 0, 0.4))
    shell = bpy.context.active_object
    shell.scale.z = 0.45; bpy.ops.object.transform_apply(scale=True)
    _add(shell, shell_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.95, location=(0, 0, 0.38))
    crust = bpy.context.active_object
    crust.scale.z = 0.43; bpy.ops.object.transform_apply(scale=True)
    _add(crust, crust_mat)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.45, minor_radius=0.12, location=(0.65, 0, 0.25))
    maw = bpy.context.active_object
    maw.rotation_euler.y = math.pi / 2
    _add(maw, maw_mat)
    _finalize_creature("SM_ANM_ShellmawAmbusher",
                        mouth_socket=(0.90, 0, 0.25), spine_socket=(0, 0, 0.85))


def gen_fogleech_swarm_legacy():
    """Swarm cloud: many small leech bodies grouped together."""
    clear_scene()
    swarm_mat = _mat("Wildlife_Fogleech", (0.12, 0.22, 0.12, 1.0),
                      roughness=0.50, emissive=(0.08, 0.18, 0.08, 1.0))
    random.seed(42)
    for _ in range(40):
        x = random.uniform(-0.8, 0.8)
        y = random.uniform(-0.8, 0.8)
        z = random.uniform(0.2, 1.2)
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.07, subdivisions=2, location=(x, y, z))
        leech = bpy.context.active_object
        leech.scale.z = 2.0; bpy.ops.object.transform_apply(scale=True)
        _add(leech, swarm_mat)
    _finalize_creature("SM_ANM_FogleechSwarm",
                        mouth_socket=(0, 0, 0.7), spine_socket=(0, 0, 0.7),
                        lods=(0.30,))


# â”€â”€ Dispatch table â€” every SpeciesId in the CSV must map to a generator. â”€â”€â”€â”€â”€

# v15 canonical species (DT_Species_v15.csv) --------------------------------
# Author at GDD VisualSummary scale: Blender metres -> UE cm 1:1 after the
# scale=1.0 export fix. Colossal megafauna (Pillarback ~10m, Vaultback ~5m)
# are intentionally large; the dropped-item clamp must be raised to display
# them at full authored size.

def gen_v15_pebble_skitter(role):
    """Palm-sized radial body on ten stilt-legs."""
    clear_scene()
    body_mat = _mat("Wildlife_Pebble_Body", (0.40, 0.40, 0.40, 1.0))
    under_mat = _mat("Wildlife_Pebble_Under", (0.85, 0.78, 0.70, 1.0))
    leg_mat = _mat("Wildlife_Pebble_Leg", (0.30, 0.30, 0.30, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.08, location=(0, 0, 0.06))
    body = bpy.context.active_object
    body.scale = (1.0, 1.0, 0.35); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.06, depth=0.012, location=(0, 0, 0.03))
    _add(bpy.context.active_object, under_mat)
    for i in range(10):
        angle = (i / 10.0) * math.tau
        x = math.cos(angle) * 0.07
        y = math.sin(angle) * 0.07
        bpy.ops.mesh.primitive_cylinder_add(radius=0.004, depth=0.06, location=(x, y, 0.03))
        _add(bpy.context.active_object, leg_mat)
    _finalize_creature("SM_ANI_PebbleSkitter",
                       mouth_socket=(0, 0, 0.08), spine_socket=(0, 0, 0.10))


def gen_v15_gleam_larver(role):
    """Segmented tube with reflective plates and three hook anchors."""
    clear_scene()
    body_mat = _mat("Wildlife_Larver_Body", (0.85, 0.82, 0.75, 1.0))
    plate_mat = _mat("Wildlife_Larver_Plate", (0.72, 0.74, 0.78, 1.0), roughness=0.20)
    hook_mat = _mat("Wildlife_Larver_Hook", (0.20, 0.18, 0.15, 1.0))
    for i in range(8):
        x = i * 0.10 - 0.35
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.08, location=(x, 0, 0.08))
        seg = bpy.context.active_object
        seg.scale = (0.9, 1.2, 0.9); bpy.ops.object.transform_apply(scale=True)
        _add(seg, body_mat)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 0, 0.16))
        plate = bpy.context.active_object
        plate.scale = (0.05, 0.10, 0.015); bpy.ops.object.transform_apply(scale=True)
        _add(plate, plate_mat)
    for hx, hy in [(0.0, 0.10), (0.05, -0.08), (-0.05, -0.05)]:
        bpy.ops.mesh.primitive_cone_add(radius1=0.015, radius2=0.0, depth=0.06,
                                         location=(0.45 + hx, hy, 0.04))
        _add(bpy.context.active_object, hook_mat)
    _finalize_creature("SM_ANI_GleamLarver",
                       mouth_socket=(0.50, 0, 0.06), spine_socket=(0, 0, 0.18))


def gen_v15_silt_strider(role):
    """Three spring legs plus stabilizer fins and buoyant belly sac for marsh travel."""
    clear_scene()
    body_mat = _mat("Wildlife_Strider_Body", (0.40, 0.30, 0.20, 1.0))
    sac_mat = _mat("Wildlife_Strider_Sac", (0.20, 0.55, 0.55, 1.0))
    fin_mat = _mat("Wildlife_Strider_Fin", (0.75, 0.72, 0.65, 1.0))
    leg_mat = _mat("Wildlife_Strider_Leg", (0.30, 0.25, 0.18, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.45, location=(0, 0, 0.85))
    body = bpy.context.active_object
    body.scale = (1.1, 0.85, 0.55); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.40, location=(0, 0, 0.60))
    sac = bpy.context.active_object
    sac.scale = (0.9, 0.9, 0.55); bpy.ops.object.transform_apply(scale=True)
    _add(sac, sac_mat)
    for angle in [0, math.tau / 3, 2 * math.tau / 3]:
        x = math.cos(angle) * 0.35
        y = math.sin(angle) * 0.35
        bpy.ops.mesh.primitive_cylinder_add(radius=0.05, depth=0.85, location=(x, y, 0.42))
        leg = bpy.context.active_object
        leg.rotation_euler = (math.cos(angle) * 0.18, math.sin(angle) * 0.18, 0)
        _add(leg, leg_mat)
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, side * 0.45, 0.85))
        fin = bpy.context.active_object
        fin.scale = (0.30, 0.04, 0.18); bpy.ops.object.transform_apply(scale=True)
        _add(fin, fin_mat)
    _eye_glint(0.40, 0.12, 0.95); _eye_glint(0.40, -0.12, 0.95)
    _finalize_creature("SM_ANI_SiltStrider",
                       mouth_socket=(0.50, 0, 0.85), spine_socket=(0, 0, 1.15))


def gen_v15_pillarback_hauler(role):
    """COLOSSAL megafauna: tall quadruped with stacked column vertebrae.
    Authored at ~10m tall as a 'living landmark' per Master GDD v1.5."""
    clear_scene()
    body_mat = _mat("Wildlife_Pillarback_Body", (0.12, 0.10, 0.08, 1.0))
    column_mat = _mat("Wildlife_Pillarback_Column", (0.18, 0.15, 0.12, 1.0), roughness=0.65)
    mineral_mat = _mat("Wildlife_Pillarback_Mineral", (0.55, 0.48, 0.30, 1.0),
                       roughness=0.30, emissive=(0.20, 0.15, 0.05, 1.0))
    leg_mat = _mat("Wildlife_Pillarback_Leg", (0.15, 0.12, 0.10, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=2.0, location=(0, 0, 5.0))
    body = bpy.context.active_object
    body.scale = (1.8, 1.2, 1.0); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    for i in range(7):
        z = 6.0 + i * 0.55
        radius = 0.55 - i * 0.04
        bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=0.50, location=(-1.5, 0, z))
        _add(bpy.context.active_object, column_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.65, location=(-1.5, 0, 9.7))
    _add(bpy.context.active_object, body_mat)
    for i in range(4):
        x = (i - 1.5) * 0.85
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.45, location=(x, 0, 6.4))
        cav = bpy.context.active_object
        cav.scale = (0.7, 1.1, 0.4); bpy.ops.object.transform_apply(scale=True)
        _add(cav, mineral_mat)
    for sx, sy in [(1.4, 1.0), (1.4, -1.0), (-1.4, 1.0), (-1.4, -1.0)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.35, depth=3.2, location=(sx, sy, 1.6))
        _add(bpy.context.active_object, leg_mat)
    _eye_glint(-1.85, 0.30, 9.85); _eye_glint(-1.85, -0.30, 9.85)
    _finalize_creature("SM_ANI_PillarbackHauler",
                       mouth_socket=(-2.05, 0, 9.55), spine_socket=(0, 0, 7.5))


def gen_v15_basin_treader(role):
    """Wide disc body on eight piston legs with cilia-lined underside for grazing."""
    clear_scene()
    body_mat = _mat("Wildlife_Treader_Body", (0.45, 0.45, 0.48, 1.0))
    cilia_mat = _mat("Wildlife_Treader_Cilia", (0.80, 0.78, 0.72, 1.0))
    moss_mat = _mat("Wildlife_Treader_Moss", (0.40, 0.50, 0.30, 1.0))
    leg_mat = _mat("Wildlife_Treader_Leg", (0.35, 0.32, 0.30, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=1.2, location=(0, 0, 1.3))
    body = bpy.context.active_object
    body.scale = (1.0, 1.0, 0.4); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=1.18, location=(0, 0, 1.45))
    moss = bpy.context.active_object
    moss.scale = (0.95, 0.95, 0.15); bpy.ops.object.transform_apply(scale=True)
    _add(moss, moss_mat)
    for i in range(20):
        angle = (i / 20.0) * math.tau
        x = math.cos(angle) * 0.6
        y = math.sin(angle) * 0.6
        bpy.ops.mesh.primitive_cylinder_add(radius=0.02, depth=0.20, location=(x, y, 1.05))
        _add(bpy.context.active_object, cilia_mat)
    for i in range(8):
        angle = (i / 8.0) * math.tau
        x = math.cos(angle) * 0.95
        y = math.sin(angle) * 0.95
        bpy.ops.mesh.primitive_cylinder_add(radius=0.10, depth=1.1, location=(x, y, 0.55))
        _add(bpy.context.active_object, leg_mat)
    _finalize_creature("SM_ANI_BasinTreader",
                       mouth_socket=(0, 0, 1.05), spine_socket=(0, 0, 1.55))


def gen_v15_nestweaver_drifter(role):
    """Soft gliding creature with four membrane fins and gas-burst sacs."""
    clear_scene()
    body_mat = _mat("Wildlife_Nestweaver_Body", (0.55, 0.50, 0.45, 1.0))
    membrane_mat = _mat("Wildlife_Nestweaver_Membrane", (0.92, 0.88, 0.80, 1.0), roughness=0.25)
    sac_mat = _mat("Wildlife_Nestweaver_Sac", (0.80, 0.65, 0.55, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.35, location=(0, 0, 1.2))
    body = bpy.context.active_object
    body.scale = (1.4, 0.7, 0.6); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    for side in [-1, 1]:
        for front in [-1, 1]:
            bpy.ops.mesh.primitive_cube_add(size=1, location=(front * 0.30, side * 0.55, 1.2))
            fin = bpy.context.active_object
            fin.scale = (0.45, 0.55, 0.015); bpy.ops.object.transform_apply(scale=True)
            fin.rotation_euler.y = front * 0.12
            _add(fin, membrane_mat)
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.10, location=(0, side * 0.20, 1.10))
        _add(bpy.context.active_object, sac_mat)
    _eye_glint(0.40, 0.10, 1.28); _eye_glint(0.40, -0.10, 1.28)
    _finalize_creature("SM_ANI_NestweaverDrifter",
                       mouth_socket=(0.50, 0, 1.18), spine_socket=(0, 0, 1.30))


def gen_v15_milkbladder_herdling(role):
    """Six-limbed crawler with visible ventral milk sac and blunt grazing face."""
    clear_scene()
    body_mat = _mat("Wildlife_Milkbladder_Body", (0.65, 0.55, 0.42, 1.0))
    sac_mat = _mat("Wildlife_Milkbladder_Sac", (0.95, 0.92, 0.85, 1.0))
    stain_mat = _mat("Wildlife_Milkbladder_Stain", (0.50, 0.62, 0.40, 1.0))
    leg_mat = _mat("Wildlife_Milkbladder_Leg", (0.55, 0.45, 0.35, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.45, location=(0, 0, 0.45))
    body = bpy.context.active_object
    body.scale = (1.4, 0.85, 0.65); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.32, location=(0, 0, 0.20))
    sac = bpy.context.active_object
    sac.scale = (1.1, 0.85, 0.55); bpy.ops.object.transform_apply(scale=True)
    _add(sac, sac_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.70))
    stripe = bpy.context.active_object
    stripe.scale = (0.55, 0.08, 0.03); bpy.ops.object.transform_apply(scale=True)
    _add(stripe, stain_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.22, location=(0.62, 0, 0.50))
    _add(bpy.context.active_object, body_mat)
    for sx in [0.42, 0.0, -0.42]:
        for sy in [0.38, -0.38]:
            bpy.ops.mesh.primitive_cylinder_add(radius=0.05, depth=0.40, location=(sx, sy, 0.20))
            _add(bpy.context.active_object, leg_mat)
    _eye_glint(0.78, 0.12, 0.60); _eye_glint(0.78, -0.12, 0.60)
    _finalize_creature("SM_ANI_MilkbladderHerdling",
                       mouth_socket=(0.82, 0, 0.45), spine_socket=(0, 0, 0.75))


def gen_v15_bone_lantern(role):
    """Floating gas bladder with trailing tendrils and cold blue bioluminescence."""
    clear_scene()
    bladder_mat = _mat("Wildlife_BoneLantern_Body", (0.92, 0.90, 0.85, 1.0),
                       roughness=0.30, emissive=(0.20, 0.45, 0.65, 1.0))
    tendril_mat = _mat("Wildlife_BoneLantern_Tendril", (0.85, 0.82, 0.78, 1.0),
                       roughness=0.40, emissive=(0.15, 0.30, 0.50, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.30, location=(0, 0, 1.4))
    bladder = bpy.context.active_object
    bladder.scale = (1.0, 1.0, 1.3); bpy.ops.object.transform_apply(scale=True)
    _add(bladder, bladder_mat)
    random.seed(11)
    for _ in range(7):
        ox = random.uniform(-0.18, 0.18)
        oy = random.uniform(-0.18, 0.18)
        bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.9, location=(ox, oy, 0.55))
        _add(bpy.context.active_object, tendril_mat)
    _finalize_creature("SM_ANI_BoneLantern",
                       mouth_socket=(0, 0, 1.20), spine_socket=(0, 0, 1.55))


def gen_v15_latchfin_mite(role):
    """Fingernail-sized puck organism with adhesive fins."""
    clear_scene()
    body_mat = _mat("Wildlife_Latchfin_Body", (0.20, 0.15, 0.10, 1.0))
    fin_mat = _mat("Wildlife_Latchfin_Fin", (0.70, 0.65, 0.55, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.015, location=(0, 0, 0.010))
    body = bpy.context.active_object
    body.scale = (1.2, 1.0, 0.4); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, side * 0.012, 0.008))
        fin = bpy.context.active_object
        fin.scale = (0.014, 0.006, 0.002); bpy.ops.object.transform_apply(scale=True)
        _add(fin, fin_mat)
    _finalize_creature("SM_ANI_LatchfinMite",
                       mouth_socket=(0.018, 0, 0.010), spine_socket=(0, 0, 0.015))


def gen_v15_crackrunner(role):
    """Rigid spine-wheel creature that curls into a rolling hoop."""
    clear_scene()
    body_mat = _mat("Wildlife_Crackrunner_Body", (0.10, 0.08, 0.08, 1.0))
    rust_mat = _mat("Wildlife_Crackrunner_Rust", (0.55, 0.25, 0.10, 1.0))
    bpy.ops.mesh.primitive_torus_add(major_radius=0.30, minor_radius=0.06, location=(0, 0, 0.30))
    hoop = bpy.context.active_object
    hoop.rotation_euler.x = math.pi / 2
    _add(hoop, body_mat)
    for i in range(12):
        angle = (i / 12.0) * math.tau
        x = math.cos(angle) * 0.30
        z = 0.30 + math.sin(angle) * 0.30
        bpy.ops.mesh.primitive_cone_add(radius1=0.025, radius2=0.0, depth=0.08, location=(x, 0, z))
        spine = bpy.context.active_object
        spine.rotation_euler.y = angle + math.pi / 2
        _add(spine, rust_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.08, location=(0.30, 0, 0.30))
    _add(bpy.context.active_object, body_mat)
    _eye_glint(0.36, 0.04, 0.32); _eye_glint(0.36, -0.04, 0.32)
    _finalize_creature("SM_ANI_Crackrunner",
                       mouth_socket=(0.38, 0, 0.30), spine_socket=(0, 0, 0.60))


def gen_v15_ridge_courser(role):
    """Long-bodied six-limbed runner with spring forelegs and vane tail. Primary fast mount."""
    clear_scene()
    body_mat = _mat("Wildlife_Courser_Body", (0.55, 0.45, 0.32, 1.0))
    accent_mat = _mat("Wildlife_Courser_Accent", (0.15, 0.13, 0.10, 1.0))
    face_mat = _mat("Wildlife_Courser_Face", (0.78, 0.72, 0.65, 1.0))
    leg_mat = _mat("Wildlife_Courser_Leg", (0.40, 0.32, 0.22, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.60, location=(0, 0, 1.10))
    body = bpy.context.active_object
    body.scale = (1.9, 0.75, 0.65); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.40))
    stripe = bpy.context.active_object
    stripe.scale = (1.3, 0.08, 0.04); bpy.ops.object.transform_apply(scale=True)
    _add(stripe, accent_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.28, location=(1.20, 0, 1.30))
    _add(bpy.context.active_object, body_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(1.42, 0, 1.30))
    face = bpy.context.active_object
    face.scale = (0.10, 0.30, 0.20); bpy.ops.object.transform_apply(scale=True)
    _add(face, face_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-1.20, 0, 1.35))
    vane = bpy.context.active_object
    vane.scale = (0.55, 0.04, 0.22); bpy.ops.object.transform_apply(scale=True)
    _add(vane, accent_mat)
    for sx, sy in [(0.85, 0.40), (0.85, -0.40), (-0.65, 0.42), (-0.65, -0.42),
                    (0.10, 0.38), (0.10, -0.38)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.07, depth=1.05, location=(sx, sy, 0.52))
        _add(bpy.context.active_object, leg_mat)
    _eye_glint(1.45, 0.16, 1.38); _eye_glint(1.45, -0.16, 1.38)
    _finalize_creature("SM_ANI_RidgeCourser",
                       mouth_socket=(1.52, 0, 1.22), spine_socket=(0, 0, 1.55))


def gen_v15_vaultback_dray(role):
    """COLOSSAL load-bearing beast with dorsal vault depression and natural harness ribs.
    Authored at ~5m long x ~3m tall per Master GDD v1.5 'Primary heavy transport mount'."""
    clear_scene()
    body_mat = _mat("Wildlife_Vaultback_Body", (0.35, 0.30, 0.25, 1.0))
    tread_mat = _mat("Wildlife_Vaultback_Tread", (0.10, 0.08, 0.08, 1.0), roughness=0.60)
    iron_mat = _mat("Wildlife_Vaultback_Iron", (0.45, 0.40, 0.35, 1.0),
                    roughness=0.40, emissive=(0.10, 0.08, 0.05, 1.0))
    leg_mat = _mat("Wildlife_Vaultback_Leg", (0.25, 0.22, 0.18, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=1.5, location=(0, 0, 2.0))
    body = bpy.context.active_object
    body.scale = (1.6, 1.1, 0.85); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.95, depth=0.30, location=(0, 0, 2.85))
    _add(bpy.context.active_object, tread_mat)
    for i in range(5):
        x = (i - 2) * 0.55
        bpy.ops.mesh.primitive_torus_add(major_radius=1.0, minor_radius=0.08, location=(x, 0, 2.60))
        rib = bpy.context.active_object
        rib.rotation_euler.y = math.pi / 2
        _add(rib, iron_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.45, depth=1.1, location=(2.40, 0, 2.20))
    neck = bpy.context.active_object
    neck.rotation_euler.y = math.pi / 2.4
    _add(neck, body_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.55, location=(3.10, 0, 1.95))
    _add(bpy.context.active_object, body_mat)
    for sx, sy in [(1.20, 0.85), (1.20, -0.85), (-1.20, 0.85), (-1.20, -0.85)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.32, depth=1.95, location=(sx, sy, 1.0))
        _add(bpy.context.active_object, leg_mat)
    _eye_glint(3.40, 0.22, 2.10); _eye_glint(3.40, -0.22, 2.10)
    _finalize_creature("SM_ANI_VaultbackDray",
                       mouth_socket=(3.55, 0, 1.85), spine_socket=(0, 0, 2.90))


def gen_v15_tetherback_packgrazer(role):
    """Broad-backed grazer with stable dorsal anchor ridges. Calmer than the Dray."""
    clear_scene()
    body_mat = _mat("Wildlife_Tetherback_Body", (0.50, 0.45, 0.38, 1.0))
    tan_mat = _mat("Wildlife_Tetherback_Tan", (0.72, 0.62, 0.48, 1.0))
    ridge_mat = _mat("Wildlife_Tetherback_Ridge", (0.38, 0.32, 0.25, 1.0))
    leg_mat = _mat("Wildlife_Tetherback_Leg", (0.42, 0.36, 0.28, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.95, location=(0, 0, 1.20))
    body = bpy.context.active_object
    body.scale = (1.6, 1.1, 0.70); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.85))
    pad = bpy.context.active_object
    pad.scale = (2.6, 1.6, 0.10); bpy.ops.object.transform_apply(scale=True)
    _add(pad, tan_mat)
    for i in range(4):
        x = (i - 1.5) * 0.55
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 0, 1.98))
        rid = bpy.context.active_object
        rid.scale = (0.10, 1.30, 0.12); bpy.ops.object.transform_apply(scale=True)
        _add(rid, ridge_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.42, location=(1.65, 0, 1.20))
    _add(bpy.context.active_object, body_mat)
    for sx, sy in [(0.85, 0.65), (0.85, -0.65), (-0.85, 0.65), (-0.85, -0.65)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.16, depth=1.20, location=(sx, sy, 0.60))
        _add(bpy.context.active_object, leg_mat)
    _eye_glint(1.92, 0.18, 1.32); _eye_glint(1.92, -0.18, 1.32)
    _finalize_creature("SM_ANI_TetherbackPackgrazer",
                       mouth_socket=(2.05, 0, 1.05), spine_socket=(0, 0, 2.05))


def gen_v15_suture_wisp(role):
    """Tall ribbon-bodied hunter with oil-sheen and filament tail."""
    clear_scene()
    body_mat = _mat("Wildlife_Wisp_Body", (0.05, 0.04, 0.06, 1.0),
                    roughness=0.20, emissive=(0.20, 0.05, 0.40, 1.0))
    edge_mat = _mat("Wildlife_Wisp_Edge", (0.55, 0.20, 0.75, 1.0),
                    roughness=0.30, emissive=(0.55, 0.20, 0.75, 1.0))
    filament_mat = _mat("Wildlife_Wisp_Filament", (0.10, 0.08, 0.10, 1.0))
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.30))
    ribbon = bpy.context.active_object
    ribbon.scale = (0.30, 0.06, 2.40); bpy.ops.object.transform_apply(scale=True)
    _add(ribbon, body_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.30))
    edge = bpy.context.active_object
    edge.scale = (0.32, 0.015, 2.45); bpy.ops.object.transform_apply(scale=True)
    _add(edge, edge_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, location=(0, 0, 2.40))
    _add(bpy.context.active_object, body_mat)
    for i in range(5):
        ox = math.sin(i) * 0.08
        oy = math.cos(i) * 0.08
        bpy.ops.mesh.primitive_cylinder_add(radius=0.006, depth=0.95,
                                             location=(ox, oy, -0.20))
        _add(bpy.context.active_object, filament_mat)
    _eye_glint(0.05, 0.06, 2.42); _eye_glint(0.05, -0.06, 2.42)
    _finalize_creature("SM_PRD_SutureWisp",
                       mouth_socket=(0.10, 0, 2.35), spine_socket=(0, 0, 1.40))


def gen_v15_needle_maw(role):
    """Buried cone organism with needle plates. Erupts upward on vibration."""
    clear_scene()
    cone_mat = _mat("Wildlife_NeedleMaw_Body", (0.78, 0.72, 0.62, 1.0))
    needle_mat = _mat("Wildlife_NeedleMaw_Needle", (0.92, 0.88, 0.78, 1.0))
    interior_mat = _mat("Wildlife_NeedleMaw_Interior", (0.55, 0.10, 0.10, 1.0))
    bpy.ops.mesh.primitive_cone_add(radius1=0.95, radius2=0.0, depth=1.30, location=(0, 0, 0.65))
    _add(bpy.context.active_object, cone_mat)
    bpy.ops.mesh.primitive_cone_add(radius1=0.55, radius2=0.0, depth=0.80, location=(0, 0, 0.40))
    _add(bpy.context.active_object, interior_mat)
    for i in range(16):
        angle = (i / 16.0) * math.tau
        x = math.cos(angle) * 0.60
        y = math.sin(angle) * 0.60
        bpy.ops.mesh.primitive_cone_add(radius1=0.04, radius2=0.0, depth=0.35,
                                         location=(x, y, 0.55))
        needle = bpy.context.active_object
        needle.rotation_euler = (math.cos(angle) * 0.4, math.sin(angle) * 0.4, 0)
        _add(needle, needle_mat)
    _finalize_creature("SM_PRD_NeedleMaw",
                       mouth_socket=(0, 0, 0.30), spine_socket=(0, 0, 1.10))


def gen_v15_drift_stalker(role):
    """Low sled-like body with vent vanes that skim the ground on gusts."""
    clear_scene()
    body_mat = _mat("Wildlife_DriftStalker_Body", (0.40, 0.38, 0.35, 1.0))
    glass_mat = _mat("Wildlife_DriftStalker_Glass", (0.20, 0.18, 0.20, 1.0), roughness=0.20)
    vent_mat = _mat("Wildlife_DriftStalker_Vent", (0.80, 0.50, 0.20, 1.0),
                    roughness=0.30, emissive=(0.80, 0.40, 0.10, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.55, location=(0, 0, 0.50))
    body = bpy.context.active_object
    body.scale = (2.2, 0.85, 0.40); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.85))
    canopy = bpy.context.active_object
    canopy.scale = (1.20, 0.45, 0.18); bpy.ops.object.transform_apply(scale=True)
    _add(canopy, glass_mat)
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, side * 0.55, 0.45))
        vane = bpy.context.active_object
        vane.scale = (1.5, 0.05, 0.28); bpy.ops.object.transform_apply(scale=True)
        _add(vane, body_mat)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(-0.80, side * 0.55, 0.45))
        vent = bpy.context.active_object
        vent.scale = (0.20, 0.06, 0.15); bpy.ops.object.transform_apply(scale=True)
        _add(vent, vent_mat)
    bpy.ops.mesh.primitive_cone_add(radius1=0.35, radius2=0.10, depth=0.6, location=(1.20, 0, 0.50))
    snout = bpy.context.active_object
    snout.rotation_euler.y = math.pi / 2
    _add(snout, body_mat)
    _eye_glint(1.35, 0.12, 0.65); _eye_glint(1.35, -0.12, 0.65)
    _finalize_creature("SM_PRD_DriftStalker",
                       mouth_socket=(1.55, 0, 0.50), spine_socket=(0, 0, 0.95))


def gen_v15_vane_rippers(role):
    """Knife-thin pack hunters with dorsal vanes and flat sensor faces."""
    clear_scene()
    body_mat = _mat("Wildlife_VaneRipper_Body", (0.18, 0.16, 0.18, 1.0))
    face_mat = _mat("Wildlife_VaneRipper_Face", (0.75, 0.72, 0.65, 1.0))
    vane_mat = _mat("Wildlife_VaneRipper_Vane", (0.45, 0.42, 0.38, 1.0))
    leg_mat = _mat("Wildlife_VaneRipper_Leg", (0.20, 0.18, 0.18, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.30, location=(0, 0, 0.55))
    body = bpy.context.active_object
    body.scale = (2.2, 0.40, 0.55); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    for i in range(4):
        x = (i - 1.5) * 0.45
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 0, 0.95))
        vane = bpy.context.active_object
        vane.scale = (0.08, 0.04, 0.28); bpy.ops.object.transform_apply(scale=True)
        _add(vane, vane_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, location=(0.85, 0, 0.55))
    _add(bpy.context.active_object, body_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(1.00, 0, 0.55))
    face = bpy.context.active_object
    face.scale = (0.06, 0.28, 0.18); bpy.ops.object.transform_apply(scale=True)
    _add(face, face_mat)
    for sx, sy in [(0.45, 0.20), (0.45, -0.20), (-0.45, 0.20), (-0.45, -0.20)]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.04, depth=0.55, location=(sx, sy, 0.28))
        _add(bpy.context.active_object, leg_mat)
    _eye_glint(1.05, 0.10, 0.62); _eye_glint(1.05, -0.10, 0.62)
    _finalize_creature("SM_PRD_VaneRippers",
                       mouth_socket=(1.12, 0, 0.50), spine_socket=(0, 0, 1.05))


def gen_v15_glassjaw_cluster(role):
    """Many small hinged jaw-pods linked by tendon cords."""
    clear_scene()
    pod_mat = _mat("Wildlife_Glassjaw_Pod", (0.75, 0.70, 0.55, 1.0),
                   roughness=0.30, emissive=(0.25, 0.20, 0.15, 1.0))
    tendon_mat = _mat("Wildlife_Glassjaw_Tendon", (0.85, 0.55, 0.60, 1.0))
    random.seed(23)
    positions = []
    for _ in range(9):
        x = random.uniform(-0.40, 0.40)
        y = random.uniform(-0.40, 0.40)
        z = random.uniform(0.06, 0.20)
        positions.append((x, y, z))
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.07, location=(x, y, z))
        pod = bpy.context.active_object
        pod.scale = (1.2, 0.8, 0.7); bpy.ops.object.transform_apply(scale=True)
        _add(pod, pod_mat)
    for i in range(len(positions) - 1):
        ax, ay, az = positions[i]
        bx, by, bz = positions[i + 1]
        mx, my, mz = (ax + bx) / 2, (ay + by) / 2, (az + bz) / 2
        length = math.sqrt((bx - ax) ** 2 + (by - ay) ** 2 + (bz - az) ** 2)
        bpy.ops.mesh.primitive_cylinder_add(radius=0.008, depth=length, location=(mx, my, mz))
        cord = bpy.context.active_object
        cord.rotation_euler = (math.atan2(by - ay, bz - az), 0, math.atan2(by - ay, bx - ax))
        _add(cord, tendon_mat)
    _finalize_creature("SM_PRD_GlassjawCluster",
                       mouth_socket=(0, 0, 0.18), spine_socket=(0, 0, 0.22),
                       lods=(0.30,))


def gen_v15_silt_hounds(role):
    """Flat-bodied waterline ambushers with retractable stabbing limbs."""
    clear_scene()
    body_mat = _mat("Wildlife_SiltHound_Body", (0.10, 0.08, 0.06, 1.0))
    sheen_mat = _mat("Wildlife_SiltHound_Sheen", (0.20, 0.50, 0.50, 1.0), roughness=0.30)
    limb_mat = _mat("Wildlife_SiltHound_Limb", (0.30, 0.25, 0.20, 1.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.40, location=(0, 0, 0.20))
    body = bpy.context.active_object
    body.scale = (1.6, 0.95, 0.30); bpy.ops.object.transform_apply(scale=True)
    _add(body, body_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.38, location=(0, 0, 0.28))
    sheen = bpy.context.active_object
    sheen.scale = (1.5, 0.85, 0.12); bpy.ops.object.transform_apply(scale=True)
    _add(sheen, sheen_mat)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.20, location=(0.60, 0, 0.22))
    _add(bpy.context.active_object, body_mat)
    for sx, sy in [(0.30, 0.40), (0.30, -0.40), (-0.30, 0.40), (-0.30, -0.40)]:
        bpy.ops.mesh.primitive_cone_add(radius1=0.04, radius2=0.0, depth=0.40,
                                         location=(sx, sy, 0.10))
        limb = bpy.context.active_object
        limb.rotation_euler.x = math.pi / 2 if sy > 0 else -math.pi / 2
        _add(limb, limb_mat)
    _eye_glint(0.72, 0.10, 0.26); _eye_glint(0.72, -0.10, 0.26)
    _finalize_creature("SM_PRD_SiltHounds",
                       mouth_socket=(0.82, 0, 0.20), spine_socket=(0, 0, 0.35))


def gen_v15_trench_diggers(role):
    """Segmented drill-bodied predators with rotating mineral tooth collars."""
    clear_scene()
    body_mat = _mat("Wildlife_TrenchDigger_Body", (0.10, 0.08, 0.08, 1.0))
    tooth_mat = _mat("Wildlife_TrenchDigger_Tooth", (0.82, 0.78, 0.65, 1.0))
    iron_mat = _mat("Wildlife_TrenchDigger_Iron", (0.45, 0.35, 0.25, 1.0))
    for i in range(5):
        x = i * 0.32 - 0.80
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.32, location=(x, 0, 0.32))
        seg = bpy.context.active_object
        seg.scale = (1.0, 1.2, 1.0); bpy.ops.object.transform_apply(scale=True)
        _add(seg, body_mat)
        bpy.ops.mesh.primitive_torus_add(major_radius=0.34, minor_radius=0.05, location=(x, 0, 0.32))
        ring = bpy.context.active_object
        ring.rotation_euler.y = math.pi / 2
        _add(ring, iron_mat)
    bpy.ops.mesh.primitive_cone_add(radius1=0.38, radius2=0.0, depth=0.65, location=(1.10, 0, 0.32))
    drill = bpy.context.active_object
    drill.rotation_euler.y = math.pi / 2
    _add(drill, body_mat)
    for i in range(10):
        angle = (i / 10.0) * math.tau
        ty = math.cos(angle) * 0.35
        tz = 0.32 + math.sin(angle) * 0.35
        bpy.ops.mesh.primitive_cone_add(radius1=0.05, radius2=0.0, depth=0.18, location=(0.95, ty, tz))
        tooth = bpy.context.active_object
        tooth.rotation_euler.x = angle
        _add(tooth, tooth_mat)
    _finalize_creature("SM_PRD_TrenchDiggers",
                       mouth_socket=(1.40, 0, 0.32), spine_socket=(0, 0, 0.65))


def gen_v15_carrion_choir(role):
    """Hovering bladder flock emitting layered subsonic calls."""
    clear_scene()
    bladder_mat = _mat("Wildlife_CarrionChoir_Body", (0.78, 0.75, 0.65, 1.0))
    sac_mat = _mat("Wildlife_CarrionChoir_Sac", (0.30, 0.55, 0.70, 1.0),
                   roughness=0.30, emissive=(0.10, 0.30, 0.50, 1.0))
    random.seed(31)
    for _ in range(8):
        x = random.uniform(-0.50, 0.50)
        y = random.uniform(-0.50, 0.50)
        z = random.uniform(0.80, 1.50)
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.14, location=(x, y, z))
        bladder = bpy.context.active_object
        bladder.scale.z = 1.4; bpy.ops.object.transform_apply(scale=True)
        _add(bladder, bladder_mat)
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.05, location=(x, y, z - 0.12))
        _add(bpy.context.active_object, sac_mat)
    _finalize_creature("SM_PRD_CarrionChoir",
                       mouth_socket=(0, 0, 1.20), spine_socket=(0, 0, 1.20),
                       lods=(0.30,))


# v15 dispatch -- maps DT_Species_v15.csv EntityId to generator.
# Legacy generators are reused where the species already existed.

V15_GENERATORS = {
    "ANI_PEBBLE_SKITTER":         gen_v15_pebble_skitter,
    "ANI_GLEAM_LARVER":           gen_v15_gleam_larver,
    "ANI_SHARDBACK_GRAZER":       lambda role: gen_shardback_grazer_legacy(),
    "ANI_SILT_STRIDER":           gen_v15_silt_strider,
    "ANI_PILLARBACK_HAULER":      gen_v15_pillarback_hauler,
    "ANI_BASIN_TREADER":          gen_v15_basin_treader,
    "ANI_NESTWEAVER_DRIFTER":     gen_v15_nestweaver_drifter,
    "ANI_MILKBLADDER_HERDLING":   gen_v15_milkbladder_herdling,
    "ANI_BONE_LANTERN":           gen_v15_bone_lantern,
    "ANI_LATCHFIN_MITE":          gen_v15_latchfin_mite,
    "ANI_CRACKRUNNER":            gen_v15_crackrunner,
    "ANI_RIDGE_COURSER":          gen_v15_ridge_courser,
    "ANI_VAULTBACK_DRAY":         gen_v15_vaultback_dray,
    "ANI_TETHERBACK_PACKGRAZER":  gen_v15_tetherback_packgrazer,
    "PRD_SUTURE_WISP":            gen_v15_suture_wisp,
    "PRD_NEEDLE_MAW":             gen_v15_needle_maw,
    "PRD_DRIFT_STALKER":          gen_v15_drift_stalker,
    "PRD_VANE_RIPPERS":           gen_v15_vane_rippers,
    "PRD_GLASSJAW_CLUSTER":       gen_v15_glassjaw_cluster,
    "PRD_SILT_HOUNDS":            gen_v15_silt_hounds,
    "PRD_TRENCH_DIGGERS":         gen_v15_trench_diggers,
    "PRD_CARRION_CHOIR":          gen_v15_carrion_choir,
    "PRD_FOGLEECH_SWARM":         lambda role: gen_fogleech_swarm_legacy(),
    "PRD_IRONSTAG_STALKER":       lambda role: gen_ironstag_stalker_legacy(),
    "PRD_SHELLMAW_AMBUSHER":      lambda role: gen_shellmaw_ambusher_legacy(),
}


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

EXTRAS = {
    "ANM_ShardbackGrazer":   gen_shardback_grazer_legacy,
    "ANM_IronstagStalker":   gen_ironstag_stalker_legacy,
    "ANM_ShellmawAmbusher":  gen_shellmaw_ambusher_legacy,
    "ANM_FogleechSwarm":     gen_fogleech_swarm_legacy,
}


def main():
    print("\n=== Quiet Rift: Enigma â€” Wildlife Asset Generator (Batch 2 detail upgrade) ===")
    csv_abs = os.path.abspath(CSV_PATH)
    if not os.path.isfile(csv_abs):
        print(f"ERROR: CSV not found at {csv_abs}")
        return
    with open(csv_abs, newline='', encoding='utf-8') as f:
        rows = [r for r in csv.DictReader(f) if r.get("SpeciesId")]
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
    for sid, gen in EXTRAS.items():
        print(f"\n[EXTRA {sid}] (not yet in DT_Species_Wildlife.csv)")
        gen()
        out_path = os.path.join(OUTPUT_DIR, f"SM_{sid}.fbx")
        export_fbx(sid, out_path)

    # v15 canonical species pass -- iterates DT_Species_v15.csv and dispatches
    # by EntityId. Filenames use the EntityId verbatim (SM_ANI_*.fbx /
    # SM_PRD_*.fbx) so they line up with the GDD-canonical IDs.
    v15_abs = os.path.abspath(V15_CSV_PATH)
    if os.path.isfile(v15_abs):
        print(f"\n--- v15 canonical species pass ({v15_abs}) ---")
        with open(v15_abs, newline='', encoding='utf-8') as f:
            v15_rows = [r for r in csv.DictReader(f) if r.get("EntityId")]
        v15_missing = [r["EntityId"] for r in v15_rows if r["EntityId"] not in V15_GENERATORS]
        if v15_missing:
            print(f"WARN: no v15 generator registered for: {v15_missing}")
        for row in v15_rows:
            eid = row["EntityId"]
            gen = V15_GENERATORS.get(eid)
            if gen is None:
                continue
            print(f"\n[v15 {eid}] {row.get('DisplayName', eid)} ({row.get('Role', '?')})")
            gen(row.get("Role", "Ambient"))
            out_path = os.path.join(OUTPUT_DIR, f"SM_{eid}.fbx")
            export_fbx(eid, out_path)
    else:
        print(f"\nNOTE: v15 CSV not found at {v15_abs} -- skipping v15 pass.")

    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX contains the SM_<id> mesh, UCX_ convex collision,")
    print("LOD chain, and MouthSocket / SpineSocket empties for AI / VFX attach.")


if __name__ == "__main__":
    main()
