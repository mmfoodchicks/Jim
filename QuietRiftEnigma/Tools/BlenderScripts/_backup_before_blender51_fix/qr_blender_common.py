"""
Quiet Rift: Enigma — Shared Blender helpers.

Imported by every qr_generate_<category>_assets.py script so each category
script stays focused on its generator functions only.
"""

import bpy
import os

# 1 blender unit = 100 UE units (cm). Keep matched across all category scripts.
SCALE = 100.0


def clear_scene():
    """Remove all objects from the current Blender scene."""
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()


def export_fbx(name, filepath):
    """Export everything currently in the scene as a single FBX."""
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
    """Attach a simple Principled-BSDF material with the given base color."""
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = color_rgba
    bsdf.inputs["Roughness"].default_value = 0.8
    obj.data.materials.append(mat)


def join_and_rename(final_name):
    """Join every selected object in the scene and rename the result."""
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.join()
    bpy.context.active_object.name = final_name
    return bpy.context.active_object
