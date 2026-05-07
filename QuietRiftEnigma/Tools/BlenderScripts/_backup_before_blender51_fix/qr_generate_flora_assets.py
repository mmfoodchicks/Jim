"""
Quiet Rift: Enigma — Flora Procedural Asset Generator (Blender 4.x)

Upgraded in Batch 1 of the Blender detail pass to use qr_blender_detail.py
for production-grade finalization (bevels, smooth shading, UV unwrap,
convex collision, LODs, sockets) and the canonical palette / dedup material
helpers. The four legacy fauna generators that lived in this file
(Shardback Grazer, Ironstag Stalker, Shellmaw Ambusher, Fogleech Swarm)
have been moved to qr_generate_wildlife_assets.py.

Run this in Blender's Scripting workspace or via:
    blender --background --python qr_generate_flora_assets.py

Per-asset detail pipeline (see finalize_asset):
    - Per-species named materials (deduped via get_or_create_material)
    - Smooth shading with 25 degree auto-smooth (organic, very smooth)
    - 2mm bevel limited by edge angle so panel-equivalent ridges stay crisp
    - Smart UV project so meshes import texture-ready
    - HarvestPoint socket at the gameplay pickup location
    - Convex hull collision (UCX_) and one LOD at 0.40 ratio
    - Pivot set to bottom_center so the asset stands on the ground

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
    get_or_create_material,
    assign_material,
    add_socket,
    add_panel_seam_strip,
    finalize_asset,
)

# ── Configuration ──────────────────────────────────────────────────────────────
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/Flora")

# ── Local material helper ─────────────────────────────────────────────────────

def _mat(slot_name, color_rgba, roughness=0.85, emissive=None):
    """Per-species named material that dedupes across calls."""
    return get_or_create_material(slot_name, color_rgba, roughness=roughness,
                                  metallic=0.0, emissive=emissive)


def _add(obj, mat):
    assign_material(obj, mat)
    return obj


# ── Reusable detail helpers ───────────────────────────────────────────────────

def _root_tuft(center, count=5, radius=0.10, root_radius=0.012, mat=None):
    """A small cluster of short angled cones suggesting roots fanning out from `center`."""
    cx, cy, cz = center
    for i in range(count):
        ang = (i / count) * math.tau
        x = cx + math.cos(ang) * radius * 0.6
        y = cy + math.sin(ang) * radius * 0.6
        bpy.ops.mesh.primitive_cone_add(radius1=root_radius, radius2=0.0,
                                        depth=radius, location=(x, y, cz))
        root = bpy.context.active_object
        # Splay outward and slightly down.
        root.rotation_euler = (math.cos(ang) * 0.6, math.sin(ang) * 0.6,
                                ang + math.pi / 2)
        if mat is not None:
            assign_material(root, mat)


def _surface_bumps(center, ring_radius, count=6, bump_radius=0.025, mat=None):
    """Small ico-spheres around `center` to add tertiary surface texture."""
    cx, cy, cz = center
    for i in range(count):
        ang = (i / count) * math.tau
        x = cx + math.cos(ang) * ring_radius
        y = cy + math.sin(ang) * ring_radius
        bpy.ops.mesh.primitive_ico_sphere_add(radius=bump_radius, subdivisions=1,
                                               location=(x, y, cz))
        b = bpy.context.active_object
        if mat is not None:
            assign_material(b, mat)


# ── Generators ────────────────────────────────────────────────────────────────

def gen_lattice_bulb():
    """Hollow polygon bulb cluster — primary body + secondary lobes + tertiary roots."""
    clear_scene()
    body_mat = _mat("Flora_LatticeBulb_Body", (0.85, 0.88, 0.90, 1.0), roughness=0.65)
    veins_mat = _mat("Flora_LatticeBulb_Veins", (0.55, 0.62, 0.55, 1.0))
    root_mat = _mat("Flora_LatticeBulb_Root", (0.25, 0.18, 0.12, 1.0))

    # Primary body — central low dome
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, location=(0, 0, 0.10))
    core = bpy.context.active_object
    core.scale = (1.0, 1.0, 0.6)
    bpy.ops.object.transform_apply(scale=True)
    _add(core, body_mat)

    # Secondary structure — 6 lobed bulbs around the core
    for i in range(6):
        ang = (i / 6.0) * math.tau
        x = math.cos(ang) * 0.22
        y = math.sin(ang) * 0.22
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.12, subdivisions=2,
                                               location=(x, y, 0.13))
        lobe = bpy.context.active_object
        lobe.scale = (1.0, 1.0, 0.7)
        bpy.ops.object.transform_apply(scale=True)
        _add(lobe, body_mat)

    # Tertiary detail — vein nubs, root tuft
    _surface_bumps((0, 0, 0.20), ring_radius=0.18, count=8, bump_radius=0.018, mat=veins_mat)
    _root_tuft((0, 0, 0.0), count=6, radius=0.12, root_radius=0.010, mat=root_mat)

    add_socket("HarvestPoint", location=(0, 0, 0.20))
    finalize_asset("SM_PLT_LATTICE_BULB",
                   bevel_width=0.002, bevel_angle_deg=35,
                   smooth_angle_deg=25, collision="convex",
                   lods=[0.40], pivot="bottom_center")


def gen_spiral_reed(count=8):
    """Corkscrew reed patch — bundle of tall helix stalks with luminous tips and a tussock base."""
    clear_scene()
    stalk_mat = _mat("Flora_SpiralReed_Stalk", (0.72, 0.68, 0.42, 1.0))
    tussock_mat = _mat("Flora_SpiralReed_Tussock", (0.45, 0.38, 0.22, 1.0))
    tip_mat = _mat("Flora_SpiralReed_Tip", (0.30, 0.90, 0.75, 1.0),
                    roughness=0.20, emissive=(0.30, 0.90, 0.75, 1.0))

    # Primary body — base tussock (low dome of dirt-fiber)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.32, location=(0, 0, 0.04))
    tussock = bpy.context.active_object
    tussock.scale = (1.0, 1.0, 0.18)
    bpy.ops.object.transform_apply(scale=True)
    _add(tussock, tussock_mat)

    # Secondary structure — `count` reed stalks
    for i in range(count):
        ang = (i / count) * math.tau
        x = math.cos(ang) * 0.35
        y = math.sin(ang) * 0.35
        bpy.ops.mesh.primitive_cylinder_add(radius=0.022, depth=1.20,
                                             location=(x, y, 0.62))
        stalk = bpy.context.active_object
        stalk.rotation_euler.z = ang * 2
        stalk.rotation_euler.x = math.cos(ang) * 0.10
        stalk.rotation_euler.y = math.sin(ang) * 0.10
        _add(stalk, stalk_mat)
        # Tertiary detail — luminous tip
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.045,
                                              location=(x * 1.05, y * 1.05, 1.25))
        tip = bpy.context.active_object
        _add(tip, tip_mat)

    # Tertiary — short ground sprigs around the tussock
    _surface_bumps((0, 0, 0.06), ring_radius=0.30, count=10, bump_radius=0.018,
                   mat=tussock_mat)

    add_socket("HarvestPoint", location=(0, 0, 0.50))
    finalize_asset("SM_PLT_SPIRAL_REED",
                   bevel_width=0.002, bevel_angle_deg=35,
                   smooth_angle_deg=25, collision="convex",
                   lods=[0.40], pivot="bottom_center")


def gen_mawcap_bloom():
    """Heavy stalk fungus with lipped cap aperture, gill ridges, and ridged stem."""
    clear_scene()
    stalk_mat = _mat("Flora_Mawcap_Stalk", (0.22, 0.32, 0.18, 1.0))
    cap_mat = _mat("Flora_Mawcap_Cap", (0.45, 0.60, 0.30, 1.0))
    gill_mat = _mat("Flora_Mawcap_Gill", (0.30, 0.42, 0.20, 1.0))
    lip_mat = _mat("Flora_Mawcap_Lip", (0.60, 0.42, 0.55, 1.0))

    # Primary body — stalk
    bpy.ops.mesh.primitive_cylinder_add(radius=0.085, depth=0.65, location=(0, 0, 0.32))
    stalk = bpy.context.active_object
    _add(stalk, stalk_mat)

    # Secondary structure — cap base
    bpy.ops.mesh.primitive_cone_add(radius1=0.50, radius2=0.05, depth=0.20,
                                    location=(0, 0, 0.70))
    cap = bpy.context.active_object
    _add(cap, cap_mat)

    # Tertiary detail — gill ridges under cap (radial thin slabs)
    for i in range(12):
        ang = (i / 12.0) * math.tau
        x = math.cos(ang) * 0.25
        y = math.sin(ang) * 0.25
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.62))
        gill = bpy.context.active_object
        gill.scale = (0.18, 0.005, 0.04)
        gill.rotation_euler.z = ang
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(gill, gill_mat)

    # Lip aperture (torus) — secondary
    bpy.ops.mesh.primitive_torus_add(major_radius=0.30, minor_radius=0.05,
                                      location=(0, 0, 0.74))
    lip = bpy.context.active_object
    _add(lip, lip_mat)

    # Stem ribbing as raised panel seams
    for i in range(4):
        ang = (i / 4.0) * math.tau
        x = math.cos(ang) * 0.087
        y = math.sin(ang) * 0.087
        add_panel_seam_strip((x, y, 0.04), (x, y, 0.60),
                              width=0.004, depth=0.003,
                              material_name="DarkWood",
                              name=f"Mawcap_Rib_{i}")

    add_socket("HarvestPoint", location=(0, 0, 0.74))
    finalize_asset("SM_PLT_MAWCAP_BLOOM",
                   bevel_width=0.0025, bevel_angle_deg=30,
                   smooth_angle_deg=25, collision="convex",
                   lods=[0.40], pivot="bottom_center")


def gen_cinder_thorn():
    """Low black shrub with ember-red thorn clusters and ash pods."""
    clear_scene()
    body_mat = _mat("Flora_CinderThorn_Body", (0.10, 0.08, 0.08, 1.0))
    thorn_mat = _mat("Flora_CinderThorn_Thorn", (0.90, 0.25, 0.05, 1.0),
                      roughness=0.40, emissive=(0.85, 0.20, 0.04, 1.0))
    ash_mat = _mat("Flora_CinderThorn_Ash", (0.45, 0.40, 0.35, 1.0))

    # Primary body — main shrub
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.35, location=(0, 0, 0.30))
    shrub = bpy.context.active_object
    shrub.scale = (1.0, 1.0, 0.6)
    bpy.ops.object.transform_apply(scale=True)
    _add(shrub, body_mat)

    # Secondary structure — 12 ember thorns radiating outward + up
    for i in range(12):
        ang = (i / 12.0) * math.tau
        x = math.cos(ang) * 0.32
        y = math.sin(ang) * 0.32
        bpy.ops.mesh.primitive_cone_add(radius1=0.03, radius2=0.005, depth=0.20,
                                         location=(x, y, 0.30 + (i % 2) * 0.05))
        thorn = bpy.context.active_object
        thorn.rotation_euler.x = math.pi / 2
        thorn.rotation_euler.z = ang
        _add(thorn, thorn_mat)

    # Tertiary detail — ash-colored bumps on top
    _surface_bumps((0, 0, 0.45), ring_radius=0.18, count=6, bump_radius=0.022,
                   mat=ash_mat)
    # Cracked-base hint via dark seams radiating
    for i in range(4):
        ang = (i / 4.0) * math.tau
        x = math.cos(ang) * 0.30
        y = math.sin(ang) * 0.30
        add_panel_seam_strip((0, 0, 0.05), (x, y, 0.05),
                              width=0.005, depth=0.002,
                              material_name="DarkRock",
                              name=f"CinderThorn_Crack_{i}")

    add_socket("HarvestPoint", location=(0, 0, 0.40))
    finalize_asset("SM_PLT_CINDER_THORN",
                   bevel_width=0.002, bevel_angle_deg=30,
                   smooth_angle_deg=30, collision="convex",
                   lods=[0.40], pivot="bottom_center")


def gen_ironbrine_cups():
    """Cup-shaped saline growths collecting rust-colored brine, with crystalline salt rims."""
    clear_scene()
    cup_mat = _mat("Flora_Ironbrine_Cup", (0.55, 0.20, 0.10, 1.0))
    brine_mat = _mat("Flora_Ironbrine_Brine", (0.35, 0.55, 0.28, 1.0),
                      roughness=0.10)
    salt_mat = _mat("Flora_Ironbrine_Salt", (0.92, 0.88, 0.80, 1.0),
                     roughness=0.60)

    # Primary body — base mound
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, location=(0, 0, 0.06))
    base = bpy.context.active_object
    base.scale = (1.0, 1.0, 0.35)
    bpy.ops.object.transform_apply(scale=True)
    _add(base, cup_mat)

    # Secondary structure — 4 cups
    for i in range(4):
        ang = (i / 4.0) * math.tau
        x = math.cos(ang) * 0.30
        y = math.sin(ang) * 0.30
        bpy.ops.mesh.primitive_cone_add(radius1=0.18, radius2=0.08, depth=0.15,
                                         location=(x, y, 0.15))
        cup = bpy.context.active_object
        _add(cup, cup_mat)
        # Brine pool inside
        bpy.ops.mesh.primitive_cylinder_add(radius=0.07, depth=0.02,
                                             location=(x, y, 0.23))
        brine = bpy.context.active_object
        _add(brine, brine_mat)
        # Tertiary — salt-crystal rim (small ico spheres around cup edge)
        _surface_bumps((x, y, 0.22), ring_radius=0.085, count=6,
                       bump_radius=0.012, mat=salt_mat)

    add_socket("HarvestPoint", location=(0, 0, 0.22))
    finalize_asset("SM_PLT_IRONBRINE_CUPS",
                   bevel_width=0.002, bevel_angle_deg=35,
                   smooth_angle_deg=25, collision="convex",
                   lods=[0.40], pivot="bottom_center")


def gen_glassbark_tree():
    """Pale translucent tree with visible internal rib structure, root flare, and branch nubs."""
    clear_scene()
    trunk_mat = _mat("Flora_Glassbark_Trunk", (0.88, 0.92, 0.95, 0.85), roughness=0.30)
    rib_mat = _mat("Flora_Glassbark_Rib", (0.60, 0.75, 0.85, 0.65), roughness=0.20)
    canopy_mat = _mat("Flora_Glassbark_Canopy", (0.80, 0.88, 0.92, 0.75), roughness=0.30)
    branch_mat = _mat("Flora_Glassbark_Branch", (0.70, 0.80, 0.88, 1.0), roughness=0.40)

    # Primary body — root flare base (cone widening down)
    bpy.ops.mesh.primitive_cone_add(radius1=0.30, radius2=0.15, depth=0.40,
                                    location=(0, 0, 0.20))
    flare = bpy.context.active_object
    _add(flare, trunk_mat)

    # Trunk
    bpy.ops.mesh.primitive_cylinder_add(radius=0.15, depth=2.80, location=(0, 0, 1.80))
    trunk = bpy.context.active_object
    _add(trunk, trunk_mat)

    # Secondary structure — vertical ribs
    for i in range(8):
        ang = (i / 8.0) * math.tau
        x = math.cos(ang) * 0.155
        y = math.sin(ang) * 0.155
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 1.80))
        rib = bpy.context.active_object
        rib.scale = (0.018, 0.018, 1.50)
        bpy.ops.object.transform_apply(scale=True)
        _add(rib, rib_mat)

    # Tertiary — branch nubs partway up trunk
    for i, z in enumerate([1.20, 1.80, 2.40, 2.90]):
        for ang_rel in [0.0, math.pi]:
            ang = (i * 0.7) + ang_rel
            x = math.cos(ang) * 0.22
            y = math.sin(ang) * 0.22
            bpy.ops.mesh.primitive_cone_add(radius1=0.04, radius2=0.0, depth=0.20,
                                             location=(x, y, z))
            nub = bpy.context.active_object
            nub.rotation_euler = (math.cos(ang) * math.pi / 2.5,
                                   math.sin(ang) * math.pi / 2.5, 0)
            _add(nub, branch_mat)

    # Canopy
    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.95, subdivisions=3, location=(0, 0, 3.50))
    canopy = bpy.context.active_object
    canopy.scale = (1.0, 1.0, 0.7)
    bpy.ops.object.transform_apply(scale=True)
    _add(canopy, canopy_mat)

    add_socket("HarvestPoint", location=(0, 0, 0.40))
    finalize_asset("SM_TRE_GLASSBARK",
                   bevel_width=0.003, bevel_angle_deg=30,
                   smooth_angle_deg=30, collision="convex",
                   lods=[0.50, 0.20], pivot="bottom_center")


def gen_slagroot_tree():
    """Squat heat-scarred tree with fused root pedestal, fissured bark seams, and ember pods."""
    clear_scene()
    pedestal_mat = _mat("Flora_Slagroot_Pedestal", (0.18, 0.10, 0.06, 1.0))
    trunk_mat = _mat("Flora_Slagroot_Trunk", (0.15, 0.08, 0.05, 1.0))
    crack_mat = _mat("Flora_Slagroot_Crack", (0.85, 0.30, 0.05, 1.0),
                      roughness=0.40, emissive=(0.85, 0.30, 0.05, 1.0))
    canopy_mat = _mat("Flora_Slagroot_Canopy", (0.12, 0.07, 0.04, 1.0))

    # Primary body — root pedestal
    bpy.ops.mesh.primitive_cylinder_add(radius=0.65, depth=0.45,
                                         location=(0, 0, 0.225))
    pedestal = bpy.context.active_object
    _add(pedestal, pedestal_mat)

    # Trunk (squat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.22, depth=2.0, location=(0, 0, 1.40))
    trunk = bpy.context.active_object
    _add(trunk, trunk_mat)

    # Secondary structure — 4 fused root buttresses
    for i in range(4):
        ang = (i / 4.0) * math.tau
        x = math.cos(ang) * 0.45
        y = math.sin(ang) * 0.45
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.30))
        buttress = bpy.context.active_object
        buttress.scale = (0.12, 0.18, 0.50)
        buttress.rotation_euler.z = ang
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(buttress, pedestal_mat)

    # Tertiary detail — fissured bark seams glowing ember
    for i in range(6):
        ang = (i / 6.0) * math.tau
        x = math.cos(ang) * 0.225
        y = math.sin(ang) * 0.225
        seam = add_panel_seam_strip((x, y, 0.50), (x, y, 2.30),
                                     width=0.012, depth=0.003,
                                     material_name="GlowRed",
                                     name=f"Slagroot_Fissure_{i}")
        if seam is not None:
            assign_material(seam, crack_mat)

    # Squat canopy
    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.75, subdivisions=2,
                                           location=(0, 0, 2.70))
    canopy = bpy.context.active_object
    canopy.scale = (1.0, 1.0, 0.55)
    bpy.ops.object.transform_apply(scale=True)
    _add(canopy, canopy_mat)

    # Ember nodes hanging in canopy
    for ang in [0.0, math.tau / 3, 2 * math.tau / 3]:
        x = math.cos(ang) * 0.45
        y = math.sin(ang) * 0.45
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.05, location=(x, y, 2.55))
        ember = bpy.context.active_object
        _add(ember, crack_mat)

    add_socket("HarvestPoint", location=(0, 0, 0.50))
    finalize_asset("SM_TRE_SLAGROOT",
                   bevel_width=0.003, bevel_angle_deg=30,
                   smooth_angle_deg=30, collision="convex",
                   lods=[0.50, 0.20], pivot="bottom_center")


def gen_asterbark_tree():
    """Dark hardwood with star-like mineral fleck sparkles, branch system, and root nubs."""
    clear_scene()
    trunk_mat = _mat("Flora_Asterbark_Trunk", (0.10, 0.07, 0.06, 1.0))
    fleck_mat = _mat("Flora_Asterbark_Fleck", (0.85, 0.88, 0.90, 1.0),
                      roughness=0.30, emissive=(0.55, 0.62, 0.75, 1.0))
    canopy_mat = _mat("Flora_Asterbark_Canopy", (0.08, 0.06, 0.05, 1.0))
    branch_mat = _mat("Flora_Asterbark_Branch", (0.12, 0.08, 0.06, 1.0))

    # Primary body — trunk
    bpy.ops.mesh.primitive_cylinder_add(radius=0.20, depth=4.0, location=(0, 0, 2.0))
    trunk = bpy.context.active_object
    _add(trunk, trunk_mat)

    # Root nubs at base (small flare)
    for i in range(6):
        ang = (i / 6.0) * math.tau
        x = math.cos(ang) * 0.25
        y = math.sin(ang) * 0.25
        bpy.ops.mesh.primitive_cone_add(radius1=0.05, radius2=0.0, depth=0.20,
                                         location=(x, y, 0.10))
        root = bpy.context.active_object
        root.rotation_euler = (math.cos(ang) * 1.0, math.sin(ang) * 1.0, 0)
        _add(root, trunk_mat)

    # Secondary structure — branches at upper trunk
    for i, (ang, length) in enumerate([(0.0, 0.7), (math.tau / 3, 0.6),
                                          (2 * math.tau / 3, 0.65)]):
        for z, scale in [(2.8, 0.7), (3.4, 1.0)]:
            x = math.cos(ang) * (0.20 + length * scale * 0.5)
            y = math.sin(ang) * (0.20 + length * scale * 0.5)
            bpy.ops.mesh.primitive_cylinder_add(radius=0.04, depth=length * scale,
                                                 location=(x, y, z))
            branch = bpy.context.active_object
            branch.rotation_euler = (math.sin(ang) * 1.2, -math.cos(ang) * 1.2, 0)
            _add(branch, branch_mat)

    # Tertiary detail — mineral flecks scattered on trunk
    for i in range(28):
        ang = (i / 28.0) * math.tau
        z = (i / 28.0) * 3.5 + 0.3
        x = math.cos(ang) * 0.22
        y = math.sin(ang) * 0.22
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.025, subdivisions=1,
                                               location=(x, y, z))
        fleck = bpy.context.active_object
        _add(fleck, fleck_mat)

    # Canopy
    bpy.ops.mesh.primitive_ico_sphere_add(radius=1.10, subdivisions=3,
                                           location=(0, 0, 4.50))
    canopy = bpy.context.active_object
    canopy.scale = (1.0, 1.0, 0.75)
    bpy.ops.object.transform_apply(scale=True)
    _add(canopy, canopy_mat)

    add_socket("HarvestPoint", location=(0, 0, 0.30))
    finalize_asset("SM_TRE_ASTERBARK",
                   bevel_width=0.003, bevel_angle_deg=30,
                   smooth_angle_deg=30, collision="convex",
                   lods=[0.50, 0.20], pivot="bottom_center")


# ── Dispatch ──────────────────────────────────────────────────────────────────

GENERATORS = {
    # Flora — herbaceous
    "PLT_LATTICE_BULB":   (gen_lattice_bulb,    "Flora"),
    "PLT_SPIRAL_REED":    (gen_spiral_reed,     "Flora"),
    "PLT_MAWCAP_BLOOM":   (gen_mawcap_bloom,    "Flora"),
    "PLT_CINDER_THORN":   (gen_cinder_thorn,    "Flora"),
    "PLT_IRONBRINE_CUPS": (gen_ironbrine_cups,  "Flora"),
    # Trees
    "TRE_GLASSBARK":      (gen_glassbark_tree,  "Trees"),
    "TRE_SLAGROOT":       (gen_slagroot_tree,   "Trees"),
    "TRE_ASTERBARK":      (gen_asterbark_tree,  "Trees"),
}


def main():
    print("\n=== Quiet Rift: Enigma — Flora Asset Generator (Batch 1 detail upgrade) ===")
    for entity_id, (gen_fn, subfolder) in GENERATORS.items():
        print(f"\n[{entity_id}]")
        gen_fn()
        out_path = os.path.join(OUTPUT_DIR, subfolder, f"SM_{entity_id}.fbx")
        export_fbx(entity_id, out_path)

    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX contains the SM_<id> mesh, a UCX_ convex collision hull,")
    print("LOD chain meshes, and a SOCKET_HarvestPoint empty for gameplay pickup.")


if __name__ == "__main__":
    main()
