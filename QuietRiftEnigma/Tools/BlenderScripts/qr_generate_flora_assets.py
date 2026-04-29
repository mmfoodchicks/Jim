"""
Quiet Rift: Enigma — Flora Procedural Asset Generator (Blender 4.x)

Run this in Blender's Scripting workspace or via:
    blender --background --python qr_generate_flora_assets.py

Generates FBX mesh stubs for all v1.5 flora species.
Each mesh is a recognizable silhouette placeholder sized for UE5 import (1 UU = 1 cm).
Export path: set OUTPUT_DIR below or pass as --output <path> via sys.argv.

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

# ── Configuration ──────────────────────────────────────────────────────────────
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/Flora")
SCALE = 100.0  # 1 blender unit = 100 UE units (cm); adjust if needed

# ── Helpers ────────────────────────────────────────────────────────────────────

def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()


def export_fbx(name, filepath):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.export_scene.fbx(
        filepath=filepath,
        use_selection=True,
        global_scale=SCALE,
        apply_scale_options='FBX_SCALE_ALL',
        axis_forward='-Z',
        axis_up='Y',
        bake_space_transform=True,
    )
    print(f"  Exported: {filepath}")


def add_material(obj, color_rgba, name="PlaceholderMat"):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = color_rgba
    bsdf.inputs["Roughness"].default_value = 0.8
    obj.data.materials.append(mat)


# ── Generator Functions ────────────────────────────────────────────────────────

def gen_lattice_bulb():
    """Hollow polygon bulb cluster — waist-high tuber geometry."""
    clear_scene()
    for i in range(6):
        angle = (i / 6.0) * math.tau
        x = math.cos(angle) * 0.25
        y = math.sin(angle) * 0.25
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.12, location=(x, y, 0.15))
        obj = bpy.context.active_object
        obj.name = f"LatticeBulb_{i}"
        # Flatten slightly for pod look
        obj.scale.z = 0.7
        bpy.ops.object.transform_apply(scale=True)
        add_material(obj, (0.85, 0.88, 0.90, 1.0), "LatticeBulb_Mat")
    # Join all bulbs
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.join()
    bpy.context.active_object.name = "SM_PLT_LATTICE_BULB"
    return bpy.context.active_object


def gen_spiral_reed(count=8):
    """Corkscrew reed patch — tall helix stalks with luminous tips."""
    clear_scene()
    for i in range(count):
        angle = (i / count) * math.tau
        x = math.cos(angle) * 0.4
        y = math.sin(angle) * 0.4
        bpy.ops.mesh.primitive_cylinder_add(
            radius=0.025, depth=1.2, location=(x, y, 0.6)
        )
        obj = bpy.context.active_object
        obj.name = f"Reed_{i}"
        # Twist the reed stalk
        obj.rotation_euler.z = angle * 2
        add_material(obj, (0.72, 0.68, 0.42, 1.0), "SpiralReed_Mat")
        # Tip sphere (bioluminescent)
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.04, location=(x, y, 1.25))
        tip = bpy.context.active_object
        tip.name = f"ReedTip_{i}"
        add_material(tip, (0.3, 0.9, 0.75, 1.0), "SpiralReedTip_Mat")
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.join()
    bpy.context.active_object.name = "SM_PLT_SPIRAL_REED"


def gen_mawcap_bloom():
    """Heavy stalk fungus with lip-like cap aperture."""
    clear_scene()
    # Stalk
    bpy.ops.mesh.primitive_cylinder_add(radius=0.08, depth=0.6, location=(0, 0, 0.3))
    stalk = bpy.context.active_object
    stalk.name = "MawcapStalk"
    add_material(stalk, (0.22, 0.32, 0.18, 1.0), "Mawcap_Stalk_Mat")
    # Cap base
    bpy.ops.mesh.primitive_cone_add(
        radius1=0.5, radius2=0.05, depth=0.2, location=(0, 0, 0.65)
    )
    cap = bpy.context.active_object
    cap.name = "MawcapCap"
    add_material(cap, (0.45, 0.60, 0.30, 1.0), "Mawcap_Cap_Mat")
    # Lip aperture (torus)
    bpy.ops.mesh.primitive_torus_add(
        major_radius=0.3, minor_radius=0.05, location=(0, 0, 0.72)
    )
    lip = bpy.context.active_object
    lip.name = "MawcapLip"
    add_material(lip, (0.60, 0.42, 0.55, 1.0), "Mawcap_Lip_Mat")
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.join()
    bpy.context.active_object.name = "SM_PLT_MAWCAP_BLOOM"


def gen_cinder_thorn():
    """Low black shrub with ember-red thorn clusters and ash pods."""
    clear_scene()
    # Main shrub body
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.35, location=(0, 0, 0.35))
    shrub = bpy.context.active_object
    shrub.name = "CinderThorn_Body"
    shrub.scale.z = 0.6
    bpy.ops.object.transform_apply(scale=True)
    add_material(shrub, (0.10, 0.08, 0.08, 1.0), "CinderThorn_Body_Mat")
    # Thorns (cone spikes radiating outward)
    for i in range(12):
        angle = (i / 12.0) * math.tau
        x = math.cos(angle) * 0.32
        y = math.sin(angle) * 0.32
        bpy.ops.mesh.primitive_cone_add(
            radius1=0.03, radius2=0.005, depth=0.18,
            location=(x, y, 0.22)
        )
        thorn = bpy.context.active_object
        thorn.name = f"Thorn_{i}"
        thorn.rotation_euler.x = math.pi / 2
        thorn.rotation_euler.z = angle
        add_material(thorn, (0.90, 0.25, 0.05, 1.0), "CinderThorn_Thorn_Mat")
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.join()
    bpy.context.active_object.name = "SM_PLT_CINDER_THORN"


def gen_ironbrine_cups():
    """Cup-shaped saline growths collecting rust-colored brine."""
    clear_scene()
    for i in range(4):
        angle = (i / 4.0) * math.tau
        x = math.cos(angle) * 0.3
        y = math.sin(angle) * 0.3
        # Cup = cone (open at top)
        bpy.ops.mesh.primitive_cone_add(
            radius1=0.18, radius2=0.08, depth=0.15, location=(x, y, 0.15)
        )
        cup = bpy.context.active_object
        cup.name = f"Cup_{i}"
        add_material(cup, (0.55, 0.20, 0.10, 1.0), "IronbrineCup_Mat")
        # Brine fill (disc)
        bpy.ops.mesh.primitive_cylinder_add(
            radius=0.07, depth=0.02, location=(x, y, 0.23)
        )
        brine = bpy.context.active_object
        brine.name = f"Brine_{i}"
        add_material(brine, (0.35, 0.55, 0.28, 1.0), "IronbrineBrine_Mat")
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.join()
    bpy.context.active_object.name = "SM_PLT_IRONBRINE_CUPS"


def gen_glassbark_tree():
    """Pale translucent tree with visible internal rib structure."""
    clear_scene()
    # Trunk
    bpy.ops.mesh.primitive_cylinder_add(radius=0.15, depth=3.0, location=(0, 0, 1.5))
    trunk = bpy.context.active_object
    trunk.name = "GlasbarkTrunk"
    add_material(trunk, (0.88, 0.92, 0.95, 0.8), "Glassbark_Trunk_Mat")
    # Ribs (vertical strips on trunk)
    for i in range(6):
        angle = (i / 6.0) * math.tau
        x = math.cos(angle) * 0.16
        y = math.sin(angle) * 0.16
        bpy.ops.mesh.primitive_cube_add(
            size=1, location=(x, y, 1.5)
        )
        rib = bpy.context.active_object
        rib.scale = (0.02, 0.02, 1.5)
        bpy.ops.object.transform_apply(scale=True)
        rib.name = f"Rib_{i}"
        add_material(rib, (0.6, 0.75, 0.85, 0.6), "Glassbark_Rib_Mat")
    # Canopy (icosphere)
    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.9, location=(0, 0, 3.4))
    canopy = bpy.context.active_object
    canopy.name = "GlasbarkCanopy"
    canopy.scale.z = 0.7
    bpy.ops.object.transform_apply(scale=True)
    add_material(canopy, (0.80, 0.88, 0.92, 0.7), "Glassbark_Canopy_Mat")
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.join()
    bpy.context.active_object.name = "SM_TRE_GLASSBARK"


def gen_slagroot_tree():
    """Squat heat-scarred tree with fused root pedestal."""
    clear_scene()
    # Root pedestal
    bpy.ops.mesh.primitive_cylinder_add(
        radius=0.6, depth=0.4, location=(0, 0, 0.2)
    )
    pedestal = bpy.context.active_object
    pedestal.name = "SlagrootPedestal"
    add_material(pedestal, (0.18, 0.10, 0.06, 1.0), "Slagroot_Pedestal_Mat")
    # Trunk (squat)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.22, depth=2.0, location=(0, 0, 1.4))
    trunk = bpy.context.active_object
    trunk.name = "SlagrootTrunk"
    add_material(trunk, (0.15, 0.08, 0.05, 1.0), "Slagroot_Trunk_Mat")
    # Thermal cracks (dark seams — cube slices)
    for i in range(4):
        angle = (i / 4.0) * math.tau
        x = math.cos(angle) * 0.23
        y = math.sin(angle) * 0.23
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 1.4))
        crack = bpy.context.active_object
        crack.scale = (0.01, 0.18, 1.0)
        crack.rotation_euler.z = angle
        bpy.ops.object.transform_apply(scale=True)
        add_material(crack, (0.55, 0.18, 0.03, 1.0), "Slagroot_Crack_Mat")
    # Squat canopy
    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.7, location=(0, 0, 2.6))
    canopy = bpy.context.active_object
    canopy.scale.z = 0.5
    bpy.ops.object.transform_apply(scale=True)
    add_material(canopy, (0.12, 0.07, 0.04, 1.0), "Slagroot_Canopy_Mat")
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.join()
    bpy.context.active_object.name = "SM_TRE_SLAGROOT"


def gen_asterbark_tree():
    """Dark hardwood with star-like mineral fleck sparkles."""
    clear_scene()
    # Trunk
    bpy.ops.mesh.primitive_cylinder_add(radius=0.2, depth=4.0, location=(0, 0, 2.0))
    trunk = bpy.context.active_object
    trunk.name = "AsterbarkTrunk"
    add_material(trunk, (0.10, 0.07, 0.06, 1.0), "Asterbark_Trunk_Mat")
    # Star fleck points (icospheres on surface)
    for i in range(20):
        angle = (i / 20.0) * math.tau
        height = (i / 20.0) * 3.5 + 0.3
        x = math.cos(angle) * 0.22
        y = math.sin(angle) * 0.22
        bpy.ops.mesh.primitive_ico_sphere_add(
            radius=0.04, subdivisions=1, location=(x, y, height)
        )
        fleck = bpy.context.active_object
        fleck.name = f"Fleck_{i}"
        add_material(fleck, (0.85, 0.88, 0.90, 1.0), "Asterbark_Fleck_Mat")
    # Canopy
    bpy.ops.mesh.primitive_ico_sphere_add(radius=1.1, location=(0, 0, 4.5))
    canopy = bpy.context.active_object
    canopy.scale.z = 0.75
    bpy.ops.object.transform_apply(scale=True)
    add_material(canopy, (0.08, 0.06, 0.05, 1.0), "Asterbark_Canopy_Mat")
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.join()
    bpy.context.active_object.name = "SM_TRE_ASTERBARK"


# ── Fauna Generator Functions ──────────────────────────────────────────────────

def gen_shardback_grazer():
    """Low hexapod with ceramic back plates and tri-lobed mouth."""
    clear_scene()
    # Body
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.6, location=(0, 0, 0.5))
    body = bpy.context.active_object
    body.scale = (1.4, 0.8, 0.6)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, (0.78, 0.72, 0.60, 1.0), "Shardback_Body_Mat")
    # Ceramic plates (flat cuboid segments on dorsal surface)
    for i in range(5):
        bpy.ops.mesh.primitive_cube_add(
            size=1, location=((i - 2) * 0.22, 0, 0.72)
        )
        plate = bpy.context.active_object
        plate.scale = (0.20, 0.50, 0.06)
        bpy.ops.object.transform_apply(scale=True)
        add_material(plate, (0.88, 0.85, 0.78, 1.0), "Shardback_Plate_Mat")
    # Six legs
    for i in range(6):
        side = 1 if i < 3 else -1
        row = (i % 3) - 1
        bpy.ops.mesh.primitive_cylinder_add(
            radius=0.05, depth=0.45,
            location=(row * 0.35, side * 0.65, 0.25)
        )
        leg = bpy.context.active_object
        leg.rotation_euler.x = math.pi / 6
        add_material(leg, (0.60, 0.55, 0.45, 1.0), "Shardback_Leg_Mat")
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.join()
    bpy.context.active_object.name = "SM_ANI_SHARDBACK_GRAZER"


def gen_ironstag_stalker():
    """Tall territorial predator with antler-like ferric blade growths."""
    clear_scene()
    # Body
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.7, location=(0, 0, 1.2))
    body = bpy.context.active_object
    body.scale = (1.6, 0.9, 0.9)
    bpy.ops.object.transform_apply(scale=True)
    add_material(body, (0.15, 0.10, 0.10, 1.0), "Ironstag_Body_Mat")
    # Neck
    bpy.ops.mesh.primitive_cylinder_add(radius=0.2, depth=0.6, location=(0.7, 0, 1.6))
    neck = bpy.context.active_object
    neck.rotation_euler.y = math.pi / 4
    add_material(neck, (0.12, 0.08, 0.08, 1.0), "Ironstag_Neck_Mat")
    # Head
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.3, location=(1.1, 0, 1.9))
    head = bpy.context.active_object
    add_material(head, (0.12, 0.08, 0.08, 1.0), "Ironstag_Head_Mat")
    # Antler blades (flat elongated cuboids)
    for side in [-1, 1]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(1.0, side * 0.25, 2.3))
        antler = bpy.context.active_object
        antler.scale = (0.05, 0.08, 0.55)
        antler.rotation_euler.z = math.pi / 8 * side
        bpy.ops.object.transform_apply(scale=True)
        add_material(antler, (0.65, 0.22, 0.08, 1.0), "Ironstag_Antler_Mat")
    # Chest plate
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1.0))
    plate = bpy.context.active_object
    plate.scale = (0.35, 0.55, 0.28)
    bpy.ops.object.transform_apply(scale=True)
    add_material(plate, (0.30, 0.28, 0.35, 1.0), "Ironstag_ChestPlate_Mat")
    # Four legs
    for i in range(4):
        row = 1 if i < 2 else -1
        side = 1 if i % 2 == 0 else -1
        bpy.ops.mesh.primitive_cylinder_add(
            radius=0.08, depth=1.0,
            location=(row * 0.5, side * 0.4, 0.5)
        )
        leg = bpy.context.active_object
        add_material(leg, (0.13, 0.09, 0.09, 1.0), "Ironstag_Leg_Mat")
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.join()
    bpy.context.active_object.name = "SM_PRD_IRONSTAG_STALKER"


def gen_shellmaw_ambusher():
    """Broad shell-backed beast; hides as mineral hump until opening trapdoor maw."""
    clear_scene()
    # Shell (hemisphere)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.9, location=(0, 0, 0.4))
    shell = bpy.context.active_object
    shell.scale.z = 0.45
    bpy.ops.object.transform_apply(scale=True)
    add_material(shell, (0.38, 0.28, 0.20, 1.0), "Shellmaw_Shell_Mat")
    # Mud/mineral encrustation layer
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.95, location=(0, 0, 0.38))
    crust = bpy.context.active_object
    crust.scale.z = 0.43
    bpy.ops.object.transform_apply(scale=True)
    add_material(crust, (0.42, 0.35, 0.25, 0.6), "Shellmaw_Crust_Mat")
    # Maw opening (torus at front)
    bpy.ops.mesh.primitive_torus_add(
        major_radius=0.45, minor_radius=0.12, location=(0.65, 0, 0.25)
    )
    maw = bpy.context.active_object
    maw.rotation_euler.y = math.pi / 2
    add_material(maw, (0.65, 0.48, 0.40, 1.0), "Shellmaw_Maw_Mat")
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.join()
    bpy.context.active_object.name = "SM_PRD_SHELLMAW_AMBUSHER"


def gen_fogleech_swarm():
    """Swarm cloud: many small leech bodies grouped together."""
    clear_scene()
    import random
    random.seed(42)
    for i in range(40):
        x = random.uniform(-0.8, 0.8)
        y = random.uniform(-0.8, 0.8)
        z = random.uniform(0.2, 1.2)
        bpy.ops.mesh.primitive_uv_sphere_add(
            radius=0.07, subdivisions=2, location=(x, y, z)
        )
        leech = bpy.context.active_object
        leech.scale.z = 2.0
        bpy.ops.object.transform_apply(scale=True)
        green_tint = random.uniform(0.0, 0.15)
        add_material(leech, (0.12, 0.18 + green_tint, 0.12, 1.0), "Fogleech_Mat")
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.join()
    bpy.context.active_object.name = "SM_PRD_FOGLEECH_SWARM"


# ── Main export pipeline ───────────────────────────────────────────────────────

GENERATORS = {
    # Flora
    "PLT_LATTICE_BULB":    (gen_lattice_bulb,    "Flora"),
    "PLT_SPIRAL_REED":     (gen_spiral_reed,     "Flora"),
    "PLT_MAWCAP_BLOOM":    (gen_mawcap_bloom,    "Flora"),
    "PLT_CINDER_THORN":    (gen_cinder_thorn,    "Flora"),
    "PLT_IRONBRINE_CUPS":  (gen_ironbrine_cups,  "Flora"),
    "TRE_GLASSBARK":       (gen_glassbark_tree,  "Trees"),
    "TRE_SLAGROOT":        (gen_slagroot_tree,   "Trees"),
    "TRE_ASTERBARK":       (gen_asterbark_tree,  "Trees"),
    # Fauna
    "ANI_SHARDBACK_GRAZER":  (gen_shardback_grazer,  "Fauna/Animals"),
    "PRD_IRONSTAG_STALKER":  (gen_ironstag_stalker,  "Fauna/Predators"),
    "PRD_SHELLMAW_AMBUSHER": (gen_shellmaw_ambusher, "Fauna/Predators"),
    "PRD_FOGLEECH_SWARM":    (gen_fogleech_swarm,    "Fauna/Predators"),
}


def main():
    print("\n=== Quiet Rift: Enigma — Asset Generator ===")
    for entity_id, (gen_fn, subfolder) in GENERATORS.items():
        print(f"\n[{entity_id}]")
        gen_fn()
        out_path = os.path.join(OUTPUT_DIR, subfolder, f"SM_{entity_id}.fbx")
        export_fbx(entity_id, out_path)

    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Import these FBX files into UE5 via Content Browser > Import.")
    print("Assign materials from M_QR_Flora and M_QR_Fauna master material instances.")


if __name__ == "__main__":
    main()
