"""
Quiet Rift: Enigma â€” Remnant (Progenitor) Asset Generator (Blender 4.x)

Upgraded in Batch 6 of the Blender detail pass â€” every Progenitor
structure + artifact flows through qr_blender_detail.py for production
finalization (palette dedupe, smooth shading, bevels, smart UV, sockets,
UCX collision, LOD chain).

Generates placeholder meshes for every variant of AQRRemnantStructure
(EQRRemnantStructureType: SignalSpire, PowerCore, DataArchive,
ResonanceChamber) and AQRRemnantArtifact (EQRRemnantArtifactType:
DataShard, PowerCell, SignalFragment, MemoryCore). Sized for UE5
import (1 UU = 1 cm).

Per-asset sockets:
    Structures:
        SOCKET_ResearchPoint  â€” where the player stands to study
        SOCKET_ApexCrest      â€” top of the asset (for signal VFX)
    Artifacts:
        SOCKET_PickupPoint    â€” where the player grabs it

Usage inside Blender:
    1. Open Blender > Scripting tab
    2. Open this file
    3. Set OUTPUT_DIR
    4. Press Run Script
"""

import bpy
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
    palette_material,
    get_or_create_material,
    assign_material,
    add_socket,
    add_panel_seam_strip,
    add_rivet_ring,
    finalize_asset,
)

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/remnant_assets")


def _add(obj, mat):
    assign_material(obj, mat)
    return obj


def _finalize_structure(name, research_point, apex_crest, lods=(0.50, 0.20)):
    add_socket("ResearchPoint", location=research_point)
    add_socket("ApexCrest", location=apex_crest)
    finalize_asset(name,
                    bevel_width=0.005, bevel_angle_deg=30,
                    smooth_angle_deg=45, collision="convex",
                    lods=list(lods), pivot="bottom_center")


def _finalize_artifact(name, pickup):
    add_socket("PickupPoint", location=pickup)
    finalize_asset(name,
                    bevel_width=0.0015, bevel_angle_deg=30,
                    smooth_angle_deg=40, collision="convex",
                    lods=[0.50], pivot="bottom_center")


# â”€â”€ Structures â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def gen_signal_spire():
    """Tall narrow obelisk with pulsing cyan signal rings + vertical seam grooves."""
    clear_scene()
    stone_mat = palette_material("ProgenitorStone")
    dark_mat = palette_material("DarkStone")
    glow_mat = palette_material("GlowCyan")
    bpy.ops.mesh.primitive_cylinder_add(radius=1.5, depth=0.6, vertices=8, location=(0, 0, 0.3))
    _add(bpy.context.active_object, dark_mat)
    bpy.ops.mesh.primitive_cone_add(radius1=0.8, radius2=0.15, depth=12.0, vertices=6, location=(0, 0, 6.6))
    _add(bpy.context.active_object, stone_mat)
    for i, z in enumerate([3.0, 6.0, 9.0, 11.5]):
        ring_r = 0.7 - i * 0.12
        bpy.ops.mesh.primitive_torus_add(major_radius=ring_r, minor_radius=0.06, location=(0, 0, z))
        _add(bpy.context.active_object, glow_mat)
    bpy.ops.mesh.primitive_cone_add(radius1=0.10, radius2=0.0, depth=0.6, location=(0, 0, 12.9))
    _add(bpy.context.active_object, glow_mat)
    # Vertical groove seams along the obelisk faces
    for i in range(6):
        ang = (i / 6.0) * math.tau
        x = math.cos(ang) * 0.55
        y = math.sin(ang) * 0.55
        add_panel_seam_strip((x, y, 0.6), (x * 0.2, y * 0.2, 12.4),
                              width=0.025, depth=0.01, material_name="DarkStone",
                              name=f"Spire_Groove_{i}")
    # Pedestal rivet ring
    add_rivet_ring(center=(0, 0, 0.62), radius=1.40, count=12,
                    rivet_radius=0.04, depth=0.03, normal_axis='Z',
                    material_name="DarkStone")
    _finalize_structure("SM_REM_STR_SignalSpire",
                         research_point=(2.0, 0, 0), apex_crest=(0, 0, 13.2),
                         lods=(0.40, 0.15))


def gen_power_core():
    """Massive cube housing with internal glowing core + corner recess panels."""
    clear_scene()
    stone_mat = palette_material("ProgenitorStone")
    dark_mat = palette_material("DarkStone")
    glow_mat = palette_material("GlowGold")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.2))
    outer = bpy.context.active_object
    outer.scale = (3.0, 3.0, 2.4); bpy.ops.object.transform_apply(scale=True)
    _add(outer, stone_mat)
    for sx in [-1, 1]:
        for sy in [-1, 1]:
            bpy.ops.mesh.primitive_cube_add(size=1, location=(sx * 1.40, sy * 1.40, 1.2))
            cut = bpy.context.active_object
            cut.scale = (0.30, 0.30, 1.5); bpy.ops.object.transform_apply(scale=True)
            _add(cut, dark_mat)
    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.7, subdivisions=3, location=(0, 0, 1.2))
    _add(bpy.context.active_object, glow_mat)
    for axis in range(3):
        bpy.ops.mesh.primitive_torus_add(major_radius=0.95, minor_radius=0.05, location=(0, 0, 1.2))
        ring = bpy.context.active_object
        if axis == 1:
            ring.rotation_euler.x = math.pi / 2
        elif axis == 2:
            ring.rotation_euler.y = math.pi / 2
        _add(ring, dark_mat)
    # Surface panel seams across each face
    for sx in [-1, 1]:
        add_panel_seam_strip((sx * 1.51, -1.4, 1.2), (sx * 1.51, 1.4, 1.2),
                              width=0.08, depth=0.02, material_name="DarkStone",
                              name=f"PowerCore_FaceSeam_{sx}")
    _finalize_structure("SM_REM_STR_PowerCore",
                         research_point=(2.5, 0, 0), apex_crest=(0, 0, 2.5))


def gen_data_archive():
    """Crystalline cluster on a hex pedestal â€” varied crystal heights + central heart."""
    clear_scene()
    dark_mat = palette_material("DarkStone")
    crystal_mat = get_or_create_material("Remnant_DataCrystal", (0.55, 0.85, 0.95, 0.6),
                                          roughness=0.20,
                                          emissive=(0.40, 0.75, 0.90, 0.6))
    heart_mat = palette_material("GlowCyan")
    bpy.ops.mesh.primitive_cylinder_add(radius=2.0, depth=0.5, vertices=6, location=(0, 0, 0.25))
    _add(bpy.context.active_object, dark_mat)
    random.seed(11)
    for i in range(9):
        ang = (i / 9.0) * math.tau
        x = math.cos(ang) * 1.0
        y = math.sin(ang) * 1.0
        h = random.uniform(1.4, 3.6)
        bpy.ops.mesh.primitive_cone_add(radius1=0.18, radius2=0.04, depth=h, vertices=6,
                                        location=(x, y, 0.5 + h / 2))
        _add(bpy.context.active_object, crystal_mat)
    bpy.ops.mesh.primitive_cone_add(radius1=0.3, radius2=0.02, depth=4.5, vertices=6, location=(0, 0, 2.75))
    _add(bpy.context.active_object, heart_mat)
    # Pedestal rim glyph rivets
    add_rivet_ring(center=(0, 0, 0.51), radius=1.85, count=12,
                    rivet_radius=0.05, depth=0.025, normal_axis='Z',
                    material_name="GlowCyan")
    _finalize_structure("SM_REM_STR_DataArchive",
                         research_point=(2.5, 0, 0), apex_crest=(0, 0, 5.0))


def gen_resonance_chamber():
    """Buried hexagonal entry portal â€” frame walls + recessed door + violet glow."""
    clear_scene()
    stone_mat = palette_material("ProgenitorStone")
    dark_mat = palette_material("DarkStone")
    glow_mat = palette_material("GlowViolet")
    bpy.ops.mesh.primitive_cylinder_add(radius=4.0, depth=0.3, vertices=6, location=(0, 0, 0.15))
    _add(bpy.context.active_object, dark_mat)
    for i in range(6):
        ang = (i / 6.0) * math.tau
        x = math.cos(ang) * 3.5
        y = math.sin(ang) * 3.5
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 1.5))
        wall = bpy.context.active_object
        wall.scale = (0.6, 1.8, 3.0)
        wall.rotation_euler.z = ang + math.pi / 2
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(wall, stone_mat)
        # Vertical glyph seam on each wall
        add_panel_seam_strip((x, y - 0.5, 0.4), (x, y - 0.5, 2.6),
                              width=0.04, depth=0.012, material_name="DarkStone",
                              name=f"Chamber_Glyph_{i}")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.2))
    portal = bpy.context.active_object
    portal.scale = (1.8, 0.4, 2.2); bpy.ops.object.transform_apply(scale=True)
    _add(portal, glow_mat)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 2.6))
    lintel = bpy.context.active_object
    lintel.scale = (2.6, 0.6, 0.4); bpy.ops.object.transform_apply(scale=True)
    _add(lintel, stone_mat)
    _finalize_structure("SM_REM_STR_ResonanceChamber",
                         research_point=(0, -2.0, 0), apex_crest=(0, 0, 2.8))


# â”€â”€ Artifacts â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def gen_data_shard():
    """Small angular crystal sliver â€” handheld."""
    clear_scene()
    crystal_mat = get_or_create_material("Remnant_DataCrystal", (0.55, 0.85, 0.95, 0.6),
                                          roughness=0.20,
                                          emissive=(0.40, 0.75, 0.90, 0.6))
    glow_mat = palette_material("GlowCyan")
    bpy.ops.mesh.primitive_cone_add(radius1=0.025, radius2=0.0, depth=0.18, vertices=4,
                                    location=(0, 0, 0.09))
    _add(bpy.context.active_object, crystal_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.004, depth=0.14, location=(0, 0, 0.09))
    _add(bpy.context.active_object, glow_mat)
    _finalize_artifact("SM_REM_ART_DataShard", pickup=(0, 0, 0.18))


def gen_power_cell():
    """Cylinder with banded glow rings â€” handheld energy store."""
    clear_scene()
    body_mat = palette_material("DarkStone")
    band_mat = palette_material("GlowGold")
    cap_mat = palette_material("ProgenitorStone")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.04, depth=0.14, location=(0, 0, 0.07))
    _add(bpy.context.active_object, body_mat)
    for z in [0.030, 0.070, 0.110]:
        bpy.ops.mesh.primitive_torus_add(major_radius=0.043, minor_radius=0.005, location=(0, 0, z))
        _add(bpy.context.active_object, band_mat)
    for z in [0.0, 0.14]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.034, depth=0.008, location=(0, 0, z))
        _add(bpy.context.active_object, cap_mat)
    _finalize_artifact("SM_REM_ART_PowerCell", pickup=(0, 0, 0.14))


def gen_signal_fragment():
    """Broken obelisk shard â€” angular jagged piece with one ring stub."""
    clear_scene()
    stone_mat = palette_material("ProgenitorStone")
    glow_mat = palette_material("GlowCyan")
    dark_mat = palette_material("DarkStone")
    bpy.ops.mesh.primitive_cone_add(radius1=0.10, radius2=0.04, depth=0.30, vertices=6,
                                    location=(0, 0, 0.15))
    chunk = bpy.context.active_object
    chunk.rotation_euler = (0.3, 0.2, 0.0)
    _add(chunk, stone_mat)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.07, minor_radius=0.012, location=(0.04, 0, 0.18))
    ring = bpy.context.active_object
    ring.rotation_euler.y = math.pi / 2.5
    _add(ring, glow_mat)
    for i in range(3):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0.05 - i * 0.04, 0.04 * (i - 1), 0.06))
        spl = bpy.context.active_object
        spl.scale = (0.04, 0.012, 0.012)
        spl.rotation_euler.z = i * 0.6
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(spl, dark_mat)
    _finalize_artifact("SM_REM_ART_SignalFragment", pickup=(0, 0, 0.20))


def gen_memory_core():
    """Geometric clustered cube â€” many small data crystals fused together."""
    clear_scene()
    shell_mat = palette_material("DarkStone")
    crystal_mat = palette_material("GlowViolet")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.05))
    shell = bpy.context.active_object
    shell.scale = (0.10, 0.10, 0.10); bpy.ops.object.transform_apply(scale=True)
    _add(shell, shell_mat)
    for fx, fy, fz in [(0.06, 0, 0.05), (-0.06, 0, 0.05),
                        (0, 0.06, 0.05), (0, -0.06, 0.05),
                        (0, 0, 0.11)]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(fx, fy, fz))
        c = bpy.context.active_object
        c.scale = (0.025, 0.025, 0.025)
        c.rotation_euler = (0.5, 0.5, 0.5)
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(c, crystal_mat)
    _finalize_artifact("SM_REM_ART_MemoryCore", pickup=(0, 0, 0.12))


# â”€â”€ Dispatch â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

GENERATORS = {
    "REM_STR_SignalSpire":      gen_signal_spire,
    "REM_STR_PowerCore":        gen_power_core,
    "REM_STR_DataArchive":      gen_data_archive,
    "REM_STR_ResonanceChamber": gen_resonance_chamber,
    "REM_ART_DataShard":        gen_data_shard,
    "REM_ART_PowerCell":        gen_power_cell,
    "REM_ART_SignalFragment":   gen_signal_fragment,
    "REM_ART_MemoryCore":       gen_memory_core,
}


def main():
    print("\n=== Quiet Rift: Enigma â€” Remnant Asset Generator (Batch 6 detail upgrade) ===")
    for asset_id, gen in GENERATORS.items():
        print(f"\n[{asset_id}]")
        gen()
        out_path = os.path.join(OUTPUT_DIR, f"SM_{asset_id}.fbx")
        export_fbx(asset_id, out_path)
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX exports the SM_<id> mesh, UCX_ collision, LOD chain, plus")
    print("SOCKET_ResearchPoint + SOCKET_ApexCrest (structures) or SOCKET_PickupPoint (artifacts).")


if __name__ == "__main__":
    main()
