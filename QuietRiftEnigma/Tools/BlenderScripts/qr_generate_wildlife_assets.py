"""
Quiet Rift: Enigma — Wildlife Procedural Asset Generator (Blender 4.x)

Upgraded in Batch 2 of the Blender detail pass — every generator now flows
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
      (e.g. "Wildlife_RidgebackGrazer_Body") → automatic dedupe
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
    """Tiny emissive sphere — shared eye-glint helper for predators / hazards."""
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


# ── CSV-driven Generators (12 species) ────────────────────────────────────────

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


# ── Legacy fauna (relocated from flora script in Batch 1, upgraded in Batch 2) ─

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
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.07, subdivisions=2, location=(x, y, z))
        leech = bpy.context.active_object
        leech.scale.z = 2.0; bpy.ops.object.transform_apply(scale=True)
        _add(leech, swarm_mat)
    _finalize_creature("SM_ANM_FogleechSwarm",
                        mouth_socket=(0, 0, 0.7), spine_socket=(0, 0, 0.7),
                        lods=(0.30,))


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

EXTRAS = {
    "ANM_ShardbackGrazer":   gen_shardback_grazer_legacy,
    "ANM_IronstagStalker":   gen_ironstag_stalker_legacy,
    "ANM_ShellmawAmbusher":  gen_shellmaw_ambusher_legacy,
    "ANM_FogleechSwarm":     gen_fogleech_swarm_legacy,
}


def main():
    print("\n=== Quiet Rift: Enigma — Wildlife Asset Generator (Batch 2 detail upgrade) ===")
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
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX contains the SM_<id> mesh, UCX_ convex collision,")
    print("LOD chain, and MouthSocket / SpineSocket empties for AI / VFX attach.")


if __name__ == "__main__":
    main()
