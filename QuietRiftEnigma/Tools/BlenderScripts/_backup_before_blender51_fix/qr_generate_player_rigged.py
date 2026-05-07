"""
Quiet Rift: Enigma — Player Base Rigging Generator (Batch D)

Builds the Batch C T-pose mesh, adds a UE5 Mannequin-compatible
skeleton on top, auto-weights the mesh to the bones, and exports
both Masculine and Feminine variants as proper skeletal-mesh FBXs.

Honest scope:
    - Core skeleton: ~25 bones (root, pelvis, spine x3, neck, head,
      clavicles + arms + hands x2, legs + feet + toe-balls x2).
    - NO individual finger bones in this pass — hand_l / hand_r are
      single bones. Fingers can land in a later refinement once the
      rest of the rig is verified in UE5.
    - Auto-weight uses Blender's built-in "Armature Auto" weighting.
      On this "bag of primitives" mesh the result will be acceptable
      for placeholder gameplay but will need cleanup before final art
      replaces the placeholder mesh — that's expected and noted.
    - NO IK helper bones, twist bones, or ik_foot_root yet. Adding
      those is a separate cleanup pass once you've test-imported the
      base rig in UE5.

Bone names follow UE5 Mannequin convention so retargeting from the
Mannequin's animation library will work out of the box:

    root
      pelvis
        spine_01 -> spine_02 -> spine_03 -> neck_01 -> head
        clavicle_l -> upperarm_l -> lowerarm_l -> hand_l
        clavicle_r -> upperarm_r -> lowerarm_r -> hand_r
        thigh_l -> calf_l -> foot_l -> ball_l
        thigh_r -> calf_r -> foot_r -> ball_r

Output:
    Content/Meshes/Characters/Player/SK_Player_Base_Masculine.fbx
    Content/Meshes/Characters/Player/SK_Player_Base_Feminine.fbx
    (Overwrites the static-mesh exports from Batch C.)

Usage inside Blender:
    1. Open Blender > Scripting tab
    2. Open this file
    3. Press Run Script
"""

import bpy
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from qr_blender_common import (  # noqa: E402
    SCALE,
    clear_scene,
)
from qr_blender_detail import (  # noqa: E402
    palette_material,  # noqa: F401  (kept for any future direct mat lookup)
    add_socket,
)

# Reuse the Batch C body builders + proportions so the rigged variant
# matches the static-mesh variant proportionally.
from qr_generate_player_base import (  # noqa: E402
    PROPS_MASCULINE,
    PROPS_FEMININE,
    _skin_mat,
    _build_head,
    _build_neck,
    _build_torso,
    _build_arm,
    _build_leg,
)

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/Characters/Player")


# ── Skeletal export helper (separate from qr_blender_common.export_fbx) ──────

def export_skeletal_fbx(filepath, root_obj):
    """Export the armature + parented mesh + sockets as a skeletal-mesh FBX
    with UE5-friendly options. Selects everything in the hierarchy under
    root_obj so we don't accidentally export stray scene objects."""
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')

    def select_recursive(obj):
        obj.select_set(True)
        for c in obj.children:
            select_recursive(c)

    select_recursive(root_obj)
    bpy.context.view_layer.objects.active = root_obj

    bpy.ops.export_scene.fbx(
        filepath=filepath,
        use_selection=True,
        global_scale=SCALE,
        apply_scale_options='FBX_SCALE_ALL',
        axis_forward='-Z',
        axis_up='Y',
        # Skeletal mesh specifics:
        object_types={'ARMATURE', 'MESH', 'EMPTY'},
        use_armature_deform_only=True,
        add_leaf_bones=False,
        primary_bone_axis='Y',
        secondary_bone_axis='X',
        bake_anim=False,
        mesh_smooth_type='FACE',
        bake_space_transform=False,   # leave armature transforms intact
    )
    print(f"  Exported skeletal: {filepath}")


# ── Mesh assembly (no finalize_asset — we need to keep the join + UV pass
#    in our control so we can rig before pivot is messed with) ───────────────

def build_mesh(props, name):
    """Run the Batch C body builders, then join + smooth + UV unwrap.
    Leaves the joined mesh selected and active. Pivot is NOT moved here —
    the armature parenting handles transforms instead."""
    skin = _skin_mat()
    head_bottom_z = _build_head(props, skin)
    neck_bottom_z = _build_neck(props, skin, head_bottom_z)
    pelvis_bottom_z, _chest_top_z, chest_center_z = _build_torso(props, skin, neck_bottom_z)
    shoulder_z = chest_center_z + (props["torso_h"] * 0.55) * 0.30
    for side in [-1, 1]:
        _build_arm(props, skin, side, shoulder_z)
    for side in [-1, 1]:
        _build_leg(props, skin, side, pelvis_bottom_z)

    # Collect mesh objects (ignore empties / sockets / armatures).
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    if not meshes:
        return None, pelvis_bottom_z, shoulder_z, chest_center_z

    bpy.ops.object.select_all(action='DESELECT')
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    mesh = bpy.context.active_object
    mesh.name = name
    mesh.data.name = f"{name}_Mesh"

    # Hygiene + smooth shading (skip bevel — humans have no hard creases).
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.remove_doubles(threshold=1e-5)
    bpy.ops.mesh.normals_make_consistent(inside=False)
    # Smart UV project.
    try:
        bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
    except Exception:
        bpy.ops.uv.smart_project()
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.shade_smooth()

    return mesh, pelvis_bottom_z, shoulder_z, chest_center_z


# ── Skeleton ─────────────────────────────────────────────────────────────────

def build_skeleton(props, mesh, pelvis_bottom_z, shoulder_z, chest_center_z):
    """Build the UE5 Mannequin-compatible armature, oriented in T-pose to
    match the mesh exactly. Returns the armature object."""

    # Add armature at origin, enter edit mode immediately.
    bpy.ops.object.armature_add(enter_editmode=True, location=(0, 0, 0))
    arm_obj = bpy.context.active_object
    arm_obj.name = "Player_Armature"
    arm_data = arm_obj.data
    arm_data.name = "PlayerSkeleton"

    # Drop the default bone Blender adds.
    for b in list(arm_data.edit_bones):
        arm_data.edit_bones.remove(b)

    # Key vertical positions, derived from props so Masculine/Feminine align.
    pelvis_z       = pelvis_bottom_z          # bottom of pelvis (top of legs)
    spine_01_z     = pelvis_z + 0.05
    spine_02_z     = pelvis_z + props["torso_h"] * 0.30
    spine_03_z     = pelvis_z + props["torso_h"] * 0.55
    neck_z         = pelvis_z + props["torso_h"]
    head_z         = neck_z + props["neck_h"]
    head_top_z     = props["height"]

    # Helper to make a bone in edit mode.
    def MB(name, head, tail, parent_name=None):
        eb = arm_data.edit_bones.new(name)
        eb.head = head
        eb.tail = tail
        if parent_name and parent_name in arm_data.edit_bones:
            eb.parent = arm_data.edit_bones[parent_name]
            eb.use_connect = False
        return eb

    # Root — short bone at world origin. UE5 root motion bone.
    MB("root", (0, 0, 0), (0, 0, 0.10), parent_name=None)

    # Pelvis — slightly above root.
    MB("pelvis", (0, 0, pelvis_z), (0, 0, pelvis_z + 0.12), parent_name="root")

    # Spine chain pointing up.
    MB("spine_01", (0, 0, pelvis_z + 0.05), (0, 0, spine_02_z), parent_name="pelvis")
    MB("spine_02", (0, 0, spine_02_z),       (0, 0, spine_03_z), parent_name="spine_01")
    MB("spine_03", (0, 0, spine_03_z),       (0, 0, neck_z),     parent_name="spine_02")
    MB("neck_01",  (0, 0, neck_z),           (0, 0, head_z),     parent_name="spine_03")
    MB("head",     (0, 0, head_z),           (0, 0, head_top_z), parent_name="neck_01")

    # Arms — T-pose extends along ±X.
    half_shoulder = props["shoulder_w"] / 2
    arm_upper_end = half_shoulder + props["arm_upper"]
    arm_lower_end = arm_upper_end + props["arm_lower"]
    hand_end      = arm_lower_end + props["hand_l"]

    for sign, suffix in [(-1, "_l"), (1, "_r")]:
        clav_head = (0,                    0, shoulder_z)
        clav_tail = (sign * half_shoulder, 0, shoulder_z)
        MB(f"clavicle{suffix}", clav_head, clav_tail, parent_name="spine_03")

        ua_head = (sign * half_shoulder, 0, shoulder_z)
        ua_tail = (sign * arm_upper_end,  0, shoulder_z)
        MB(f"upperarm{suffix}", ua_head, ua_tail, parent_name=f"clavicle{suffix}")

        la_head = (sign * arm_upper_end, 0, shoulder_z)
        la_tail = (sign * arm_lower_end,  0, shoulder_z)
        MB(f"lowerarm{suffix}", la_head, la_tail, parent_name=f"upperarm{suffix}")

        hand_head = (sign * arm_lower_end, 0, shoulder_z)
        hand_tail = (sign * hand_end,      0, shoulder_z)
        MB(f"hand{suffix}", hand_head, hand_tail, parent_name=f"lowerarm{suffix}")

    # Legs — straight down.
    knee_z  = pelvis_z - props["thigh_h"]
    ankle_z = knee_z - props["calf_h"]
    foot_z  = ankle_z - props["foot_h"]
    foot_y_tip = -props["foot_h"] * 3.5

    for sign, suffix in [(-1, "_l"), (1, "_r")]:
        hip_x = sign * props["hip_w"] / 4

        MB(f"thigh{suffix}", (hip_x, 0, pelvis_z), (hip_x, 0, knee_z), parent_name="pelvis")
        MB(f"calf{suffix}",  (hip_x, 0, knee_z),    (hip_x, 0, ankle_z), parent_name=f"thigh{suffix}")
        MB(f"foot{suffix}",  (hip_x, 0, ankle_z),   (hip_x, -props["foot_h"] * 1.5, foot_z + props["foot_h"] * 0.5),
            parent_name=f"calf{suffix}")
        MB(f"ball{suffix}",  (hip_x, -props["foot_h"] * 1.5, foot_z + props["foot_h"] * 0.5),
            (hip_x, foot_y_tip, foot_z + props["foot_h"] * 0.3),
            parent_name=f"foot{suffix}")

    # Drop out of edit mode so the bones commit.
    bpy.ops.object.mode_set(mode='OBJECT')

    return arm_obj


# ── Auto-weighting ───────────────────────────────────────────────────────────

def auto_weight(mesh, armature):
    """Parent mesh to armature using Armature Deform with Auto Weights.
    Acceptable for a placeholder mesh; will need cleanup later when the
    final art replaces the placeholder."""
    bpy.ops.object.select_all(action='DESELECT')
    mesh.select_set(True)
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')


# ── Socket reparenting ──────────────────────────────────────────────────────

def add_skeletal_sockets(armature, props, shoulder_z, pelvis_bottom_z):
    """Add UE5-style sockets that target specific bones. Each socket is an
    empty parented to a bone in the armature so UE5's importer recognizes
    it as a bone-attached socket. These are the canonical attach points
    weapons / hats / props will hook to in gameplay code."""

    # Socket layout: (socket_name, bone_name, local_offset)
    # Local offsets are in the bone's local space — UE5 converts on import.
    sockets = [
        ("HeadAttach",     "head",      (0, 0, props["head_h"] / 2)),
        ("ChestAttach",    "spine_03",  (0, 0, 0.0)),
        ("WaistAttach",    "pelvis",    (0, 0, 0.0)),
        ("HandAttach_L",   "hand_l",    (0, 0, 0.0)),
        ("HandAttach_R",   "hand_r",    (0, 0, 0.0)),
        ("FootAttach_L",   "foot_l",    (0, 0, 0.0)),
        ("FootAttach_R",   "foot_r",    (0, 0, 0.0)),
        # Common props
        ("BackpackAttach", "spine_02",  (0, props["chest_d"] * 0.4, 0.0)),
        ("ChestRigAttach", "spine_03",  (0, -props["chest_d"] * 0.3, 0.0)),
    ]

    bpy.ops.object.select_all(action='DESELECT')
    bpy.context.view_layer.objects.active = armature

    for socket_name, bone_name, offset in sockets:
        # Empties attached to bones via "child of" constraint or via direct
        # bone parenting. Direct bone parenting is the FBX-friendly path.
        bpy.ops.object.empty_add(type='ARROWS', radius=0.05, location=(0, 0, 0))
        empty = bpy.context.active_object
        empty.name = f"SOCKET_{socket_name}"
        # Parent to armature bone.
        empty.parent = armature
        empty.parent_type = 'BONE'
        empty.parent_bone = bone_name
        # Local offset (relative to bone tail in Blender's parent_bone scheme).
        empty.location = offset


# ── Top-level orchestration ─────────────────────────────────────────────────

def gen_player_rigged(props, name):
    print(f"\n[{name}]")
    clear_scene()

    mesh, pelvis_bottom_z, shoulder_z, chest_center_z = build_mesh(props, name)
    if mesh is None:
        print(f"  ERROR: no mesh produced for {name}")
        return

    armature = build_skeleton(props, mesh, pelvis_bottom_z, shoulder_z, chest_center_z)
    auto_weight(mesh, armature)
    add_skeletal_sockets(armature, props, shoulder_z, pelvis_bottom_z)

    out_path = os.path.join(OUTPUT_DIR, f"{name}.fbx")
    export_skeletal_fbx(out_path, armature)


def main():
    print("\n=== Quiet Rift: Enigma — Player Base Rigging Generator (Batch D) ===")
    for variant_name, props in [
        ("SK_Player_Base_Masculine", PROPS_MASCULINE),
        ("SK_Player_Base_Feminine",  PROPS_FEMININE),
    ]:
        gen_player_rigged(props, variant_name)
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX exports:")
    print("  - PlayerSkeleton armature (~25 bones, UE5 Mannequin naming)")
    print("  - SK_Player_Base mesh, auto-weighted to the skeleton")
    print("  - 9 SOCKET_* empties parented to specific bones")
    print("UE5 import: 'Skeletal Mesh' option, 'Import Sockets' on, leave 'Import")
    print("Animations' off (no anims yet — animations come from Mannequin retarget).")


if __name__ == "__main__":
    main()
