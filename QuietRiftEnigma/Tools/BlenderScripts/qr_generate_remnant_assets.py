"""
Quiet Rift: Enigma — Remnant (Progenitor) Asset Generator (Blender 4.x)

Run this in Blender's Scripting workspace or via:
    blender --background --python qr_generate_remnant_assets.py

Generates placeholder meshes for every variant of AQRRemnantStructure
(EQRRemnantStructureType) and AQRRemnantArtifact (EQRRemnantArtifactType)
using the helpers in qr_blender_common.py. Sized for UE5 import
(1 UU = 1 cm).

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

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/remnant_assets")

# Progenitor palette — pale alien stone + cyan signal glow
STONE      = (0.62, 0.66, 0.72, 1.0)
DARKSTONE  = (0.30, 0.34, 0.40, 1.0)
GLOW_CYAN  = (0.30, 0.85, 0.95, 1.0)
GLOW_GOLD  = (0.85, 0.75, 0.30, 1.0)
GLOW_VIOLET = (0.55, 0.35, 0.85, 1.0)
CRYSTAL    = (0.55, 0.85, 0.95, 0.6)


# ── Structures ─────────────────────────────────────────────────────────────────

def gen_signal_spire():
    """Tall narrow obelisk with pulsing cyan signal rings."""
    clear_scene()
    # Base pedestal
    bpy.ops.mesh.primitive_cylinder_add(radius=1.5, depth=0.6, vertices=8, location=(0, 0, 0.3))
    add_material(bpy.context.active_object, DARKSTONE, "Spire_Base_Mat")
    # Spire shaft (tall narrow tapered hexagonal column)
    bpy.ops.mesh.primitive_cone_add(radius1=0.8, radius2=0.15, depth=12.0, vertices=6, location=(0, 0, 6.6))
    add_material(bpy.context.active_object, STONE, "Spire_Shaft_Mat")
    # Signal rings at altitude
    for i, z in enumerate([3.0, 6.0, 9.0, 11.5]):
        ring_r = 0.7 - i * 0.12
        bpy.ops.mesh.primitive_torus_add(major_radius=ring_r, minor_radius=0.06, location=(0, 0, z))
        add_material(bpy.context.active_object, GLOW_CYAN, f"Spire_Ring_{i}_Mat")
    # Apex shard
    bpy.ops.mesh.primitive_cone_add(radius1=0.10, radius2=0.0, depth=0.6, location=(0, 0, 12.9))
    add_material(bpy.context.active_object, GLOW_CYAN, "Spire_Apex_Mat")
    join_and_rename("SM_REM_STR_SignalSpire")


def gen_power_core():
    """Massive cube housing with internal glowing core."""
    clear_scene()
    # Outer plinth
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.2))
    outer = bpy.context.active_object
    outer.scale = (3.0, 3.0, 2.4)
    bpy.ops.object.transform_apply(scale=True)
    add_material(outer, STONE, "PowerCore_Outer_Mat")
    # Recessed corner cuts (8 dark panels)
    for sx in [-1, 1]:
        for sy in [-1, 1]:
            bpy.ops.mesh.primitive_cube_add(size=1, location=(sx * 1.40, sy * 1.40, 1.2))
            cut = bpy.context.active_object
            cut.scale = (0.30, 0.30, 1.5)
            bpy.ops.object.transform_apply(scale=True)
            add_material(cut, DARKSTONE, "PowerCore_Recess_Mat")
    # Inner glowing sphere
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.7, location=(0, 0, 1.2))
    add_material(bpy.context.active_object, GLOW_GOLD, "PowerCore_Glow_Mat")
    # Conduit rings around it
    for axis in range(3):
        bpy.ops.mesh.primitive_torus_add(major_radius=0.95, minor_radius=0.05, location=(0, 0, 1.2))
        ring = bpy.context.active_object
        if axis == 1:
            ring.rotation_euler.x = math.pi / 2
        elif axis == 2:
            ring.rotation_euler.y = math.pi / 2
        add_material(ring, DARKSTONE, f"PowerCore_Conduit_{axis}_Mat")
    join_and_rename("SM_REM_STR_PowerCore")


def gen_data_archive():
    """Crystalline cluster on a hex pedestal."""
    clear_scene()
    # Hex pedestal
    bpy.ops.mesh.primitive_cylinder_add(radius=2.0, depth=0.5, vertices=6, location=(0, 0, 0.25))
    add_material(bpy.context.active_object, DARKSTONE, "Archive_Pedestal_Mat")
    # Crystal columns of varying heights
    import random
    random.seed(11)
    for i in range(9):
        angle = (i / 9.0) * math.tau
        x = math.cos(angle) * 1.0
        y = math.sin(angle) * 1.0
        h = random.uniform(1.4, 3.6)
        bpy.ops.mesh.primitive_cone_add(radius1=0.18, radius2=0.04, depth=h, vertices=6,
                                        location=(x, y, 0.5 + h / 2))
        add_material(bpy.context.active_object, CRYSTAL, f"Archive_Crystal_{i}_Mat")
    # Central tallest crystal
    bpy.ops.mesh.primitive_cone_add(radius1=0.3, radius2=0.02, depth=4.5, vertices=6, location=(0, 0, 2.75))
    add_material(bpy.context.active_object, GLOW_CYAN, "Archive_Heart_Mat")
    join_and_rename("SM_REM_STR_DataArchive")


def gen_resonance_chamber():
    """Buried hexagonal entry portal — frame + recessed door + violet glow."""
    clear_scene()
    # Sunken floor pad
    bpy.ops.mesh.primitive_cylinder_add(radius=4.0, depth=0.3, vertices=6, location=(0, 0, 0.15))
    add_material(bpy.context.active_object, DARKSTONE, "Chamber_Pad_Mat")
    # Frame walls forming a hex around an opening
    for i in range(6):
        angle = (i / 6.0) * math.tau
        x = math.cos(angle) * 3.5
        y = math.sin(angle) * 3.5
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 1.5))
        wall = bpy.context.active_object
        wall.scale = (0.6, 1.8, 3.0)
        wall.rotation_euler.z = angle + math.pi / 2
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        add_material(wall, STONE, f"Chamber_Wall_{i}_Mat")
    # Central recessed portal
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.2))
    portal = bpy.context.active_object
    portal.scale = (1.8, 0.4, 2.2)
    bpy.ops.object.transform_apply(scale=True)
    add_material(portal, GLOW_VIOLET, "Chamber_Portal_Mat")
    # Lintel beam above portal
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 2.6))
    lintel = bpy.context.active_object
    lintel.scale = (2.6, 0.6, 0.4)
    bpy.ops.object.transform_apply(scale=True)
    add_material(lintel, STONE, "Chamber_Lintel_Mat")
    join_and_rename("SM_REM_STR_ResonanceChamber")


# ── Artifacts ─────────────────────────────────────────────────────────────────

def gen_data_shard():
    """Small angular crystal sliver — handheld."""
    clear_scene()
    bpy.ops.mesh.primitive_cone_add(radius1=0.025, radius2=0.0, depth=0.18, vertices=4, location=(0, 0, 0.09))
    add_material(bpy.context.active_object, CRYSTAL, "DataShard_Body_Mat")
    # Inner glow filament
    bpy.ops.mesh.primitive_cylinder_add(radius=0.004, depth=0.14, location=(0, 0, 0.09))
    add_material(bpy.context.active_object, GLOW_CYAN, "DataShard_Filament_Mat")
    join_and_rename("SM_REM_ART_DataShard")


def gen_power_cell():
    """Cylinder with banded glow rings — handheld energy store."""
    clear_scene()
    bpy.ops.mesh.primitive_cylinder_add(radius=0.04, depth=0.14, location=(0, 0, 0.07))
    add_material(bpy.context.active_object, DARKSTONE, "PowerCell_Body_Mat")
    # Glow rings at top, middle, bottom
    for z in [0.030, 0.070, 0.110]:
        bpy.ops.mesh.primitive_torus_add(major_radius=0.043, minor_radius=0.005, location=(0, 0, z))
        add_material(bpy.context.active_object, GLOW_GOLD, f"PowerCell_Ring_{int(z*1000)}_Mat")
    # End caps
    for z in [0.0, 0.14]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.034, depth=0.008, location=(0, 0, z))
        add_material(bpy.context.active_object, STONE, f"PowerCell_Cap_{int(z*1000)}_Mat")
    join_and_rename("SM_REM_ART_PowerCell")


def gen_signal_fragment():
    """Broken obelisk shard — angular jagged piece with one ring stub."""
    clear_scene()
    # Main jagged piece
    bpy.ops.mesh.primitive_cone_add(radius1=0.10, radius2=0.04, depth=0.30, vertices=6, location=(0, 0, 0.15))
    chunk = bpy.context.active_object
    chunk.rotation_euler = (0.3, 0.2, 0.0)
    add_material(chunk, STONE, "SigFrag_Main_Mat")
    # Embedded ring stub (broken half-torus)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.07, minor_radius=0.012, location=(0.04, 0, 0.18))
    ring = bpy.context.active_object
    ring.rotation_euler.y = math.pi / 2.5
    add_material(ring, GLOW_CYAN, "SigFrag_Ring_Mat")
    # Crack splinters
    for i in range(3):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0.05 - i * 0.04, 0.04 * (i - 1), 0.06))
        spl = bpy.context.active_object
        spl.scale = (0.04, 0.012, 0.012)
        spl.rotation_euler.z = i * 0.6
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        add_material(spl, DARKSTONE, f"SigFrag_Splinter_{i}_Mat")
    join_and_rename("SM_REM_ART_SignalFragment")


def gen_memory_core():
    """Geometric clustered cube — many small data crystals fused together."""
    clear_scene()
    # Outer cube shell
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.05))
    shell = bpy.context.active_object
    shell.scale = (0.10, 0.10, 0.10)
    bpy.ops.object.transform_apply(scale=True)
    add_material(shell, DARKSTONE, "MemCore_Shell_Mat")
    # Cluster of small embedded crystals on each face
    for fx, fy, fz in [(0.06, 0, 0.05), (-0.06, 0, 0.05),
                        (0, 0.06, 0.05), (0, -0.06, 0.05),
                        (0, 0, 0.11)]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(fx, fy, fz))
        c = bpy.context.active_object
        c.scale = (0.025, 0.025, 0.025)
        c.rotation_euler = (0.5, 0.5, 0.5)
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        add_material(c, GLOW_VIOLET, "MemCore_Crystal_Mat")
    join_and_rename("SM_REM_ART_MemoryCore")


# ── Dispatch ──────────────────────────────────────────────────────────────────

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
    print("\n=== Quiet Rift: Enigma — Remnant Asset Generator ===")
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
