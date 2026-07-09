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


# ── Photoreal layer (2026-07-09 overhaul) ────────────────────────────────────
# Trees are rebuilt the way real games build them: bark-displaced trunk
# geometry + alpha-card canopies using a baked leaf-cluster texture.
# The procedural PBR shading is baked to PNG maps and embedded in the
# FBX, so UE imports fully textured materials. Verified by rendering
# in-container before commit.

from qr_blender_photoreal import (  # noqa: E402
    pbr_material, refine, bake_pbr, _n,
)


def _h(i):
    """Deterministic 0..1 hash -- same asset every regeneration."""
    return (math.sin(i * 12.9898) * 43758.5453) % 1.0


TEX_DIR = os.path.join(OUTPUT_DIR, "Textures")


def _leaf_card_texture(name, dark, mid, bright, blades=False, seed=0):
    """Bake a flat-albedo leaf/blade cluster card with alpha to
    TEX_DIR/T_<name>_CARD.png and return the path. Rendered with an
    emission shader so the PNG is pure albedo (no lighting/specular)."""
    os.makedirs(TEX_DIR, exist_ok=True)
    out = os.path.join(TEX_DIR, "T_{}_CARD.png".format(name))

    clear_scene()
    mat = bpy.data.materials.new("QR_CardCapture_" + name)
    mat.use_nodes = True
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    outn = nt.nodes.new("ShaderNodeOutputMaterial")
    em = nt.nodes.new("ShaderNodeEmission")
    coord = nt.nodes.new("ShaderNodeTexCoord")
    grad = nt.nodes.new("ShaderNodeTexGradient")
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    e = ramp.color_ramp.elements
    e[0].position, e[0].color = 0.0, dark + (1.0,)
    e[1].position, e[1].color = 1.0, bright + (1.0,)
    mid_e = ramp.color_ramp.elements.new(0.5)
    mid_e.color = mid + (1.0,)
    nt.links.new(coord.outputs["Object"], grad.inputs["Vector"])
    nt.links.new(grad.outputs["Fac"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], em.inputs["Color"])
    em.inputs["Strength"].default_value = 1.0
    nt.links.new(em.outputs["Emission"], outn.inputs["Surface"])

    count = 34 if blades else 30
    for i in range(count):
        a = (i / float(count)) * 2 * math.pi
        r = 0.10 + 0.42 * _h(i * 7 + seed + 1)
        L = (0.55 if blades else 0.30) + 0.26 * _h(i * 11 + seed + 2)
        bpy.ops.mesh.primitive_cone_add(
            radius1=0.018 if blades else 0.05, radius2=0.004,
            depth=L, vertices=4,
            location=(math.cos(a) * r * 0.8, math.sin(a) * r * 0.8, 0.02))
        leaf = bpy.context.active_object
        leaf.scale = (1.0, 0.22 if blades else 0.26, 1.0)
        tilt = (0.55 if blades else 0.85) + 0.25 * _h(i + seed + 3)
        leaf.rotation_euler = (math.pi / 2 * tilt, 0, a)
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        leaf.data.materials.append(mat)

    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 16
    scene.render.film_transparent = True
    scene.render.resolution_x = scene.render.resolution_y = 512
    scene.render.image_settings.color_mode = 'RGBA'
    try:
        scene.view_settings.view_transform = 'Standard'
    except Exception:
        pass
    cd = bpy.data.cameras.new("CardCam")
    cd.type = 'ORTHO'
    cd.ortho_scale = 2.6 if blades else 2.4
    cam = bpy.data.objects.new("CardCam", cd)
    bpy.context.collection.objects.link(cam)
    cam.location = (0, 0, 3)
    scene.camera = cam
    scene.render.filepath = out
    bpy.ops.render.render(write_still=True)
    scene.render.film_transparent = False
    return out


def _card_material(name, png_path, emissive_strength=0.15):
    mat = bpy.data.materials.get(name)
    if mat:
        return mat
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    img = bpy.data.images.load(png_path, check_existing=True)
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = img
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
    bsdf.inputs["Roughness"].default_value = 0.45
    if "Emission Color" in bsdf.inputs and emissive_strength > 0:
        nt.links.new(tex.outputs["Color"], bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = emissive_strength
    mat.blend_method = 'CLIP'
    return mat


def _cards_at(cx, cy, cz, n, size, seed, mat):
    for k in range(n):
        bpy.ops.mesh.primitive_plane_add(
            size=size,
            location=(cx + (_h(seed + k * 3) - 0.5) * size * 0.55,
                      cy + (_h(seed + k * 5 + 1) - 0.5) * size * 0.55,
                      cz + (_h(seed + k * 7 + 2) - 0.5) * size * 0.55))
        card = bpy.context.active_object
        card.rotation_euler = (1.3 * (_h(seed + k * 11) - 0.5),
                               1.3 * (_h(seed + k * 13 + 1) - 0.5),
                               _h(seed + k) * math.pi)
        bpy.ops.object.transform_apply(rotation=True)
        card.data.materials.append(mat)


def _photoreal_tree(entity_id, card_name, leaf_dark, leaf_mid, leaf_bright,
                    bark_tint, bark_tint2, trunk_h=4.2, trunk_r=0.40,
                    branches=8, leaf_glow=0.15, seed=0):
    """Shared photoreal tree builder: bark trunk + flare + branches
    (joined + baked to one atlas), alpha-card canopy, trunk-only UCX."""
    card_png = _leaf_card_texture(card_name, leaf_dark, leaf_mid,
                                  leaf_bright, seed=seed)
    clear_scene()

    bark = pbr_material("QR_Bark_" + entity_id, "bark",
                        tint=bark_tint, tint2=bark_tint2, scale=2.2)
    nt = bark.node_tree
    wave = next((n for n in nt.nodes if n.bl_idname == "ShaderNodeTexWave"), None)
    coord = next((n for n in nt.nodes if n.bl_idname == "ShaderNodeTexCoord"), None)
    if wave is not None and coord is not None and             not any(n.bl_idname == "ShaderNodeMapping" for n in nt.nodes):
        wave.bands_direction = 'X'
        mapping = _n(nt, "ShaderNodeMapping", -1050, 100)
        mapping.inputs["Scale"].default_value = (1.0, 1.0, 0.10)
        nt.links.new(coord.outputs["Object"], mapping.inputs["Vector"])
        nt.links.new(mapping.outputs["Vector"], wave.inputs["Vector"])

    bpy.ops.mesh.primitive_cone_add(radius1=trunk_r, radius2=trunk_r * 0.38,
                                    depth=trunk_h, vertices=12,
                                    location=(0, 0, trunk_h * 0.5))
    trunk = bpy.context.active_object
    trunk.data.materials.append(bark)
    bpy.ops.mesh.primitive_cone_add(radius1=trunk_r * 2.6, radius2=trunk_r * 0.9,
                                    depth=0.7, vertices=12, location=(0, 0, 0.18))
    flare = bpy.context.active_object
    flare.data.materials.append(bark)
    for t in (trunk, flare):
        refine(t, subdiv=2,
               displace=[("fine", 0.3, 0.045), ("angular", 2.0, 0.035)],
               smooth_angle=45)

    tips = []
    for i in range(branches):
        a = 2 * math.pi * i / branches + 0.5 * _h(i + seed)
        z0 = trunk_h * 0.50 + trunk_h * 0.45 * _h(i * 5 + seed + 1)
        L = trunk_h * 0.26 + trunk_h * 0.21 * _h(i * 7 + seed + 2)
        droop = 0.55 + 0.7 * _h(i * 13 + seed + 3)
        bpy.ops.mesh.primitive_cone_add(
            radius1=trunk_r * 0.22, radius2=trunk_r * 0.06, depth=L, vertices=7,
            location=(math.cos(a) * (0.22 + L * 0.30),
                      math.sin(a) * (0.22 + L * 0.30), z0 + L * 0.30))
        br = bpy.context.active_object
        br.rotation_euler = (math.sin(a) * droop, -math.cos(a) * droop, 0)
        bpy.ops.object.transform_apply(rotation=True)
        br.data.materials.append(bark)
        tips.append((math.cos(a) * (0.30 + L * 0.55),
                     math.sin(a) * (0.30 + L * 0.55),
                     z0 + L * 0.50, (i + 1) * 31 + seed))

    # Join bark parts and bake to one atlas.
    for o in bpy.context.selected_objects:
        o.select_set(False)
    barks = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    for o in barks:
        o.select_set(True)
    bpy.context.view_layer.objects.active = barks[0]
    if len(barks) > 1:
        bpy.ops.object.join()
    wood = bpy.context.view_layer.objects.active
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
    bpy.ops.object.mode_set(mode='OBJECT')
    bake_pbr(wood, entity_id + "_Bark", TEX_DIR, size=1024, samples=8)

    # Canopy cards.
    card_mat = _card_material("QR_Leaves_" + entity_id, card_png, leaf_glow)
    csize = trunk_h * 0.43
    for tx, ty, tz, s in tips:
        _cards_at(tx, ty, tz, n=5, size=csize * 0.8, seed=s, mat=card_mat)
    _cards_at(0, 0, trunk_h * 1.05, n=7, size=csize, seed=seed + 777, mat=card_mat)
    _cards_at(0, 0, trunk_h * 1.21, n=5, size=csize * 0.8, seed=seed + 555, mat=card_mat)

    # Trunk-only collision so the canopy doesn't block players.
    bpy.ops.mesh.primitive_cylinder_add(radius=trunk_r * 1.15, depth=trunk_h,
                                        vertices=8, location=(0, 0, trunk_h * 0.5))
    bpy.context.active_object.name = "UCX_{}_00".format("SM_" + entity_id)

    add_socket("HarvestPoint", location=(0, 0, 1.2))
    finalize_asset("SM_" + entity_id, bevel_width=0.0, uv_unwrap=False,
                   smooth_angle_deg=40, collision="none",
                   lods=[0.50], pivot="bottom_center")


def gen_glassbark_tree():
    _photoreal_tree("TRE_GLASSBARK", "GLASSBARK",
                    (0.10, 0.18, 0.20), (0.30, 0.48, 0.52), (0.62, 0.80, 0.82),
                    bark_tint=(0.55, 0.55, 0.52), bark_tint2=(0.28, 0.28, 0.27),
                    trunk_h=4.6, trunk_r=0.36, leaf_glow=0.30, seed=11)


def gen_slagroot_tree():
    _photoreal_tree("TRE_SLAGROOT", "SLAGROOT",
                    (0.05, 0.03, 0.02), (0.28, 0.10, 0.04), (0.65, 0.28, 0.08),
                    bark_tint=(0.12, 0.10, 0.09), bark_tint2=(0.05, 0.04, 0.04),
                    trunk_h=3.6, trunk_r=0.46, branches=6, leaf_glow=0.55, seed=23)


def gen_asterbark_tree():
    _photoreal_tree("TRE_ASTERBARK", "ASTERBARK",
                    (0.16, 0.10, 0.22), (0.38, 0.26, 0.44), (0.78, 0.62, 0.38),
                    bark_tint=(0.38, 0.34, 0.30), bark_tint2=(0.16, 0.14, 0.13),
                    trunk_h=4.0, trunk_r=0.38, leaf_glow=0.25, seed=37)


def gen_prismleaf_tree():
    _photoreal_tree("TRE_PRISMLEAF", "PRISMLEAF",
                    (0.045, 0.16, 0.13), (0.07, 0.26, 0.20), (0.10, 0.34, 0.24),
                    bark_tint=(0.30, 0.26, 0.22), bark_tint2=(0.10, 0.085, 0.075),
                    trunk_h=4.2, trunk_r=0.40, leaf_glow=0.18, seed=51)


# ── Ground cover: card-based shard grass ─────────────────────────────────────

def _shard_grass(variant, tint_dark, tint_mid, tint_bright, size, seed):
    card = _leaf_card_texture("GRASS_" + variant, tint_dark, tint_mid,
                              tint_bright, blades=True, seed=seed)
    clear_scene()
    mat = _card_material("QR_Grass_" + variant, card, emissive_strength=0.12)
    for k in range(3):
        bpy.ops.mesh.primitive_plane_add(size=size, location=(0, 0, size * 0.32))
        c = bpy.context.active_object
        c.rotation_euler = (math.pi / 2, 0, k * math.pi / 3)
        bpy.ops.object.transform_apply(rotation=True)
        c.data.materials.append(mat)
    finalize_asset("SM_PLT_SHARD_GRASS_{}".format(variant),
                   bevel_width=0.0, uv_unwrap=False, smooth_angle_deg=60,
                   collision="none", lods=None, pivot="bottom_center")


def gen_shard_grass_a():
    _shard_grass("A", (0.06, 0.16, 0.10), (0.14, 0.34, 0.20),
                 (0.30, 0.52, 0.30), size=0.8, seed=101)


def gen_shard_grass_b():
    _shard_grass("B", (0.05, 0.14, 0.13), (0.11, 0.30, 0.27),
                 (0.24, 0.48, 0.42), size=0.55, seed=202)


def gen_shard_grass_c():
    _shard_grass("C", (0.09, 0.15, 0.07), (0.20, 0.32, 0.14),
                 (0.40, 0.52, 0.24), size=1.0, seed=303)


# ── Rocks & crystal spurs (photoreal bake) ───────────────────────────────────

def _boulder(variant, radius, squash, dark=False):
    clear_scene()
    tint = (0.24, 0.22, 0.20) if dark else (0.36, 0.33, 0.29)
    tint2 = (0.09, 0.085, 0.08) if dark else (0.14, 0.125, 0.11)
    rock_mat = pbr_material("QR_Boulder_{}".format(variant), "rock",
                            tint=tint, tint2=tint2)
    bpy.ops.mesh.primitive_ico_sphere_add(radius=radius, subdivisions=3,
                                          location=(0, 0, radius * squash * 0.75))
    rock = bpy.context.active_object
    rock.scale = (1.0 + 0.35 * _h(variant), 0.85 + 0.3 * _h(variant + 5), squash)
    bpy.ops.object.transform_apply(scale=True)
    rock.data.materials.append(rock_mat)
    refine(rock, subdiv=1,
           displace=[("angular", radius * 1.0, radius * 0.28),
                     ("fine", radius * 0.18, radius * 0.045)],
           smooth_angle=30)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
    bpy.ops.object.mode_set(mode='OBJECT')
    name = "RCK_BOULDER_{}".format("ABC"[variant])
    bake_pbr(rock, name, TEX_DIR, size=1024, samples=8)
    finalize_asset("SM_" + name, bevel_width=0.0, uv_unwrap=False,
                   smooth_angle_deg=30, collision="convex",
                   lods=[0.40], pivot="bottom_center")


def gen_boulder_a():
    _boulder(0, radius=0.55, squash=0.75)


def gen_boulder_b():
    _boulder(1, radius=1.10, squash=0.65, dark=True)


def gen_boulder_c():
    _boulder(2, radius=1.80, squash=0.55)


def gen_crystal_spur():
    """Mineral crystal cluster. Opaque crystal shading (transmission
    bakes black) with vein emissive baked into the albedo."""
    clear_scene()
    spar = pbr_material("QR_CrystalSpur", "leaf_alien",
                        tint=(0.55, 0.74, 0.80), tint2=(0.22, 0.38, 0.44),
                        emissive=(0.25, 0.55, 0.60), emissive_strength=2.0)
    base = pbr_material("QR_SpurBase", "rock",
                        tint=(0.24, 0.22, 0.20), tint2=(0.09, 0.085, 0.08))
    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.35, subdivisions=2,
                                          location=(0, 0, 0.12))
    b = bpy.context.active_object
    b.scale = (1.3, 1.1, 0.4)
    bpy.ops.object.transform_apply(scale=True)
    b.data.materials.append(base)
    for i in range(5):
        a = 2 * math.pi * _h(i * 9 + 1)
        r = 0.16 * _h(i * 5 + 2)
        hgt = 0.5 + 0.8 * _h(i * 7 + 3)
        bpy.ops.mesh.primitive_cone_add(radius1=0.09 + 0.05 * _h(i + 4),
                                        radius2=0.015, depth=hgt, vertices=6,
                                        location=(math.cos(a) * r,
                                                  math.sin(a) * r, hgt * 0.45))
        c = bpy.context.active_object
        c.rotation_euler = (math.sin(a) * 0.35, -math.cos(a) * 0.35, a)
        bpy.ops.object.transform_apply(rotation=True)
        c.data.materials.append(spar)

    for o in bpy.context.selected_objects:
        o.select_set(False)
    parts = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    for o in parts:
        o.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    if len(parts) > 1:
        bpy.ops.object.join()
    spur = bpy.context.view_layer.objects.active
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
    bpy.ops.object.mode_set(mode='OBJECT')
    bake_pbr(spur, "RCK_CRYSTAL_SPUR", TEX_DIR, size=1024, samples=8)
    finalize_asset("SM_RCK_CRYSTAL_SPUR", bevel_width=0.0, uv_unwrap=False,
                   smooth_angle_deg=20, collision="convex",
                   lods=[0.40], pivot="bottom_center")


# ── Dispatch ──────────────────────────────────────────────────────────────────

GENERATORS = {
    # Flora — herbaceous
    "PLT_LATTICE_BULB":   (gen_lattice_bulb,    "Flora"),
    "PLT_SPIRAL_REED":    (gen_spiral_reed,     "Flora"),
    "PLT_MAWCAP_BLOOM":   (gen_mawcap_bloom,    "Flora"),
    "PLT_CINDER_THORN":   (gen_cinder_thorn,    "Flora"),
    "PLT_IRONBRINE_CUPS": (gen_ironbrine_cups,  "Flora"),
    # Ground cover (no collision -- pure dressing)
    "PLT_SHARD_GRASS_A":  (gen_shard_grass_a,   "Flora"),
    "PLT_SHARD_GRASS_B":  (gen_shard_grass_b,   "Flora"),
    "PLT_SHARD_GRASS_C":  (gen_shard_grass_c,   "Flora"),
    # Trees
    "TRE_GLASSBARK":      (gen_glassbark_tree,  "Trees"),
    "TRE_SLAGROOT":       (gen_slagroot_tree,   "Trees"),
    "TRE_ASTERBARK":      (gen_asterbark_tree,  "Trees"),
    "TRE_PRISMLEAF":      (gen_prismleaf_tree,  "Trees"),
    # Rocks
    "RCK_BOULDER_A":      (gen_boulder_a,       "Rocks"),
    "RCK_BOULDER_B":      (gen_boulder_b,       "Rocks"),
    "RCK_BOULDER_C":      (gen_boulder_c,       "Rocks"),
    "RCK_CRYSTAL_SPUR":   (gen_crystal_spur,    "Rocks"),
}


def main():
    print("\n=== Quiet Rift: Enigma — Flora Asset Generator (Batch 1 detail upgrade) ===")
    for entity_id, (gen_fn, subfolder) in GENERATORS.items():
        print(f"\n[{entity_id}]")
        gen_fn()
        out_path = os.path.join(OUTPUT_DIR, subfolder, f"SM_{entity_id}.fbx")
        export_fbx(entity_id, out_path, embed_textures=True)

    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX contains the SM_<id> mesh, a UCX_ convex collision hull,")
    print("LOD chain meshes, and a SOCKET_HarvestPoint empty for gameplay pickup.")


if __name__ == "__main__":
    main()
