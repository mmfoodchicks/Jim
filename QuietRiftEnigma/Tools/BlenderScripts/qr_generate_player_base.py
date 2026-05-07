"""
Quiet Rift: Enigma — Player Base Humanoid Generator (Batch C)

Builds two T-pose placeholder humanoid base meshes — Masculine and
Feminine — that the character creator will start from. The bodies are
intentionally placeholder ("mannequin-grade"): correct proportions,
proper material slots, smooth shading, pivot at the ground between the
feet. The final character art (skin detail, hair, etc.) will replace
these later — this batch just delivers a riggable T-pose with the
right surface partitions.

Honest scope:
    - This is geometry only. NO skeleton yet (Batch D adds the rig).
    - NO morph targets yet (Batch E adds the customization shape keys).
    - The mesh resolution is tuned so morph targets in Batch E will
      look acceptable: each major body region is a separate ellipsoid
      with ~16 segments / 12 rings of subdivision.

Material slot layout:
    Slot 0  Skin           — overall body + face surface
    Slot 1  Lips           — distinct so lip color customization works
    Slot 2  Eyes_Sclera    — white of the eye
    Slot 3  Eyes_Iris      — colored part of the eye
    Slot 4  Teeth          — white teeth slabs
    Slot 5  InsideMouth    — dark oral cavity
    Slot 6  Nails          — fingernails / toenails

Output:
    Content/Meshes/Characters/Player/SK_Player_Base_Masculine.fbx
    Content/Meshes/Characters/Player/SK_Player_Base_Feminine.fbx

Convention:
    - Origin at the ground between the feet (so the character stands at
      world origin Z=0 once imported into UE5)
    - +Z up, +Y forward (Blender native; export converts to UE5 axes)
    - T-pose: arms straight out to the sides

Usage inside Blender:
    1. Open Blender > Scripting tab
    2. Open this file
    3. Set OUTPUT_DIR if needed
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
    finalize_asset,
)

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/Characters/Player")


# ── Material palette (per-character creator-driven defaults) ─────────────────

def _skin_mat():
    # Default warm tan — runtime will override via the character creator's
    # SkinTone parameter binding to a Material Instance Dynamic.
    return get_or_create_material("Player_Skin",
                                   (0.78, 0.62, 0.50, 1.0),
                                   roughness=0.55, metallic=0.0)


def _lips_mat():
    return get_or_create_material("Player_Lips",
                                   (0.55, 0.30, 0.28, 1.0),
                                   roughness=0.50, metallic=0.0)


def _eye_sclera_mat():
    return get_or_create_material("Player_EyeSclera",
                                   (0.94, 0.94, 0.92, 1.0),
                                   roughness=0.20, metallic=0.0)


def _eye_iris_mat():
    return get_or_create_material("Player_EyeIris",
                                   (0.30, 0.45, 0.55, 1.0),
                                   roughness=0.20, metallic=0.0)


def _teeth_mat():
    return get_or_create_material("Player_Teeth",
                                   (0.96, 0.94, 0.88, 1.0),
                                   roughness=0.30, metallic=0.0)


def _inside_mouth_mat():
    return get_or_create_material("Player_InsideMouth",
                                   (0.25, 0.10, 0.10, 1.0),
                                   roughness=0.85, metallic=0.0)


def _nails_mat():
    return get_or_create_material("Player_Nails",
                                   (0.85, 0.72, 0.65, 1.0),
                                   roughness=0.30, metallic=0.0)


# ── Geometry helpers ─────────────────────────────────────────────────────────

def _ellipsoid(loc, rx, ry, rz, mat, name, segments=24, rings=16):
    """Add a UV-sphere scaled to (rx, ry, rz). Material assigned, transform applied."""
    bpy.ops.mesh.primitive_uv_sphere_add(radius=1.0, segments=segments,
                                          ring_count=rings, location=loc)
    obj = bpy.context.active_object
    obj.scale = (rx, ry, rz)
    bpy.ops.object.transform_apply(scale=True)
    obj.name = name
    assign_material(obj, mat)
    return obj


def _capsule(start, end, radius_start, radius_end, mat, name, segments=20):
    """Tapered capsule oriented along the start→end vector."""
    sx, sy, sz = start
    ex, ey, ez = end
    dx, dy, dz = (ex - sx, ey - sy, ez - sz)
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    mid = ((sx + ex) / 2, (sy + ey) / 2, (sz + ez) / 2)

    bpy.ops.mesh.primitive_cylinder_add(radius=1.0, depth=length, vertices=segments, location=mid)
    obj = bpy.context.active_object
    # Taper by adjusting end caps after creation: we approximate with a uniform
    # cylinder + scaled end caps via two extra uv-spheres at the ends.
    avg_r = (radius_start + radius_end) / 2
    obj.scale = (avg_r, avg_r, 1.0)

    # Orient cylinder along the segment direction.
    if length > 1e-6:
        # Default cylinder is along Z. Rotate Z axis to align with (dx,dy,dz).
        target = (dx / length, dy / length, dz / length)
        # Compute Euler rotation from default Z-up to target.
        # Use rotation_difference via mathutils Vector.
        from mathutils import Vector
        z_axis = Vector((0.0, 0.0, 1.0))
        target_v = Vector(target)
        quat = z_axis.rotation_difference(target_v)
        obj.rotation_euler = quat.to_euler()

    bpy.ops.object.transform_apply(scale=True, rotation=True)
    obj.name = name
    assign_material(obj, mat)

    # End-cap spheres so taper reads visually (also fills any seam at joints).
    _ellipsoid(start, radius_start, radius_start, radius_start, mat,
                f"{name}_StartCap", segments=12, rings=8)
    _ellipsoid(end,   radius_end,   radius_end,   radius_end,   mat,
                f"{name}_EndCap",   segments=12, rings=8)
    return obj


def _slab(x, y, z, w, d, h, mat, name):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z))
    obj = bpy.context.active_object
    obj.scale = (w, d, h)
    bpy.ops.object.transform_apply(scale=True)
    assign_material(obj, mat)
    obj.name = name
    return obj


# ── Build proportions ────────────────────────────────────────────────────────

# All units are meters. UE5 import scales 1m → 100cm.
PROPS_MASCULINE = dict(
    height=1.75, head_h=0.24, neck_h=0.06, torso_h=0.55,
    thigh_h=0.45, calf_h=0.40, foot_h=0.06,
    shoulder_w=0.45, hip_w=0.38, chest_d=0.22, abdomen_d=0.20,
    arm_upper=0.32, arm_lower=0.30, hand_l=0.18,
    bust_radius=0.0, glute_radius=0.18,
)

PROPS_FEMININE = dict(
    height=1.62, head_h=0.22, neck_h=0.05, torso_h=0.50,
    thigh_h=0.42, calf_h=0.38, foot_h=0.05,
    shoulder_w=0.38, hip_w=0.36, chest_d=0.20, abdomen_d=0.18,
    arm_upper=0.30, arm_lower=0.28, hand_l=0.17,
    bust_radius=0.10, glute_radius=0.20,
)


# ── Body part builders ───────────────────────────────────────────────────────

def _build_head(p, skin):
    """Head + face features. Returns the top-of-head Z coordinate."""
    # Y forward in Blender; head's facial features face -Y so the mesh
    # exports facing +X in UE5 (default character forward).
    head_top_z = p["height"]
    head_center_z = head_top_z - p["head_h"] / 2

    # Skull (slightly elongated front-to-back so it reads as a head, not a ball)
    _ellipsoid((0, 0, head_center_z),
                p["head_h"] * 0.45, p["head_h"] * 0.50, p["head_h"] * 0.50,
                skin, "Head_Skull")

    # Jaw (slight chin extension below the skull center)
    _ellipsoid((0, -p["head_h"] * 0.05, head_center_z - p["head_h"] * 0.20),
                p["head_h"] * 0.32, p["head_h"] * 0.38, p["head_h"] * 0.18,
                skin, "Head_Jaw")

    # Eye sockets — small recesses (just a deeper-skin ellipsoid placeholder)
    eye_z = head_center_z + p["head_h"] * 0.05
    eye_back_y = -p["head_h"] * 0.42
    eye_offset_x = p["head_h"] * 0.18

    # Eye balls (sclera) and iris — separate small spheres
    sclera = _eye_sclera_mat()
    iris = _eye_iris_mat()
    for sgn in [-1, 1]:
        _ellipsoid((sgn * eye_offset_x, eye_back_y, eye_z),
                    p["head_h"] * 0.05, p["head_h"] * 0.05, p["head_h"] * 0.05,
                    sclera, f"Eye_Sclera_{'L' if sgn < 0 else 'R'}",
                    segments=20, rings=12)
        _ellipsoid((sgn * eye_offset_x, eye_back_y - p["head_h"] * 0.025, eye_z),
                    p["head_h"] * 0.025, p["head_h"] * 0.020, p["head_h"] * 0.025,
                    iris, f"Eye_Iris_{'L' if sgn < 0 else 'R'}",
                    segments=12, rings=8)

    # Mouth interior (dark cavity sphere set inside the lower head)
    mouth_z = head_center_z - p["head_h"] * 0.22
    mouth_y = -p["head_h"] * 0.22
    _ellipsoid((0, mouth_y, mouth_z),
                p["head_h"] * 0.10, p["head_h"] * 0.06, p["head_h"] * 0.05,
                _inside_mouth_mat(), "InsideMouth",
                segments=14, rings=10)

    # Teeth — two thin slabs (upper / lower)
    teeth = _teeth_mat()
    _slab(0, mouth_y - p["head_h"] * 0.005, mouth_z + p["head_h"] * 0.020,
           p["head_h"] * 0.16, p["head_h"] * 0.04, p["head_h"] * 0.020,
           teeth, "Teeth_Upper")
    _slab(0, mouth_y - p["head_h"] * 0.005, mouth_z - p["head_h"] * 0.020,
           p["head_h"] * 0.16, p["head_h"] * 0.04, p["head_h"] * 0.020,
           teeth, "Teeth_Lower")

    # Lips — flat ring around the mouth opening
    lips = _lips_mat()
    _ellipsoid((0, mouth_y - p["head_h"] * 0.04, mouth_z),
                p["head_h"] * 0.12, p["head_h"] * 0.04, p["head_h"] * 0.04,
                lips, "Lips",
                segments=18, rings=10)

    # Nose (small wedge in front of the face)
    nose_z = head_center_z - p["head_h"] * 0.05
    nose_y = -p["head_h"] * 0.50
    _ellipsoid((0, nose_y, nose_z),
                p["head_h"] * 0.06, p["head_h"] * 0.10, p["head_h"] * 0.08,
                skin, "Nose")

    # Ears (two small flattened ellipsoids on the sides)
    for sgn in [-1, 1]:
        _ellipsoid((sgn * p["head_h"] * 0.45, p["head_h"] * 0.05, head_center_z),
                    p["head_h"] * 0.04, p["head_h"] * 0.10, p["head_h"] * 0.10,
                    skin, f"Ear_{'L' if sgn < 0 else 'R'}")

    # Return Y-forward facing baseline so caller can chain neck/torso below.
    return head_center_z - p["head_h"] / 2  # bottom of head


def _build_neck(p, skin, head_bottom_z):
    """Neck cylinder linking head to torso."""
    neck_top = head_bottom_z
    neck_bot = head_bottom_z - p["neck_h"]
    _capsule((0, 0, neck_bot), (0, 0, neck_top),
              p["head_h"] * 0.18, p["head_h"] * 0.22,
              skin, "Neck", segments=16)
    return neck_bot


def _build_torso(p, skin, torso_top_z):
    """Chest + abdomen + pelvis."""
    # Chest (broader at top, drives shoulder line)
    chest_top = torso_top_z
    chest_h = p["torso_h"] * 0.55
    chest_z = chest_top - chest_h / 2
    _ellipsoid((0, 0, chest_z),
                p["shoulder_w"] / 2, p["chest_d"] / 2, chest_h / 2,
                skin, "Chest")

    # Abdomen
    abdomen_top = chest_top - chest_h
    abdomen_h = p["torso_h"] * 0.30
    abdomen_z = abdomen_top - abdomen_h / 2
    _ellipsoid((0, 0, abdomen_z),
                p["shoulder_w"] * 0.45, p["abdomen_d"] / 2 * 0.95, abdomen_h / 2,
                skin, "Abdomen")

    # Pelvis (transitions to thighs)
    pelvis_top = abdomen_top - abdomen_h
    pelvis_h = p["torso_h"] * 0.15
    pelvis_z = pelvis_top - pelvis_h / 2
    _ellipsoid((0, 0, pelvis_z),
                p["hip_w"] / 2, p["abdomen_d"] / 2, pelvis_h / 2,
                skin, "Pelvis")

    pelvis_bottom = pelvis_top - pelvis_h

    # Bust shape — only for Feminine bodies; Masculine has bust_radius=0
    if p["bust_radius"] > 0:
        bust_z = chest_z - chest_h * 0.10
        bust_y = -p["chest_d"] * 0.30
        for sgn in [-1, 1]:
            _ellipsoid((sgn * p["bust_radius"] * 1.0, bust_y, bust_z),
                        p["bust_radius"], p["bust_radius"] * 0.75, p["bust_radius"] * 0.85,
                        skin, f"Bust_{'L' if sgn < 0 else 'R'}")

    # Glute pair on the back of the pelvis
    if p["glute_radius"] > 0:
        glute_y = p["abdomen_d"] * 0.30
        glute_z = pelvis_z - pelvis_h * 0.05
        for sgn in [-1, 1]:
            _ellipsoid((sgn * p["glute_radius"] * 0.55, glute_y, glute_z),
                        p["glute_radius"] * 0.6, p["glute_radius"] * 0.55, p["glute_radius"] * 0.55,
                        skin, f"Glute_{'L' if sgn < 0 else 'R'}")

    return pelvis_bottom, chest_top, chest_z


def _build_arm(p, skin, side, shoulder_z):
    """Shoulder + upper arm + forearm + hand. side = +1 for right, -1 for left."""
    shoulder_x = side * p["shoulder_w"] / 2

    # Shoulder ball (joint)
    shoulder_r = p["arm_upper"] * 0.18
    _ellipsoid((shoulder_x, 0, shoulder_z),
                shoulder_r, shoulder_r, shoulder_r,
                skin, f"Shoulder_{'R' if side > 0 else 'L'}")

    # Upper arm (T-pose: arm extends along +/- X)
    upper_start = (shoulder_x, 0, shoulder_z)
    upper_end_x = shoulder_x + side * p["arm_upper"]
    upper_end = (upper_end_x, 0, shoulder_z)
    _capsule(upper_start, upper_end,
              shoulder_r * 0.95, shoulder_r * 0.85,
              skin, f"UpperArm_{'R' if side > 0 else 'L'}",
              segments=16)

    # Elbow joint
    elbow_r = shoulder_r * 0.85
    _ellipsoid(upper_end, elbow_r, elbow_r, elbow_r,
                skin, f"Elbow_{'R' if side > 0 else 'L'}")

    # Forearm
    forearm_end_x = upper_end_x + side * p["arm_lower"]
    forearm_end = (forearm_end_x, 0, shoulder_z)
    _capsule(upper_end, forearm_end,
              elbow_r * 0.95, elbow_r * 0.75,
              skin, f"Forearm_{'R' if side > 0 else 'L'}",
              segments=14)

    # Wrist
    wrist_r = elbow_r * 0.75
    _ellipsoid(forearm_end, wrist_r, wrist_r, wrist_r,
                skin, f"Wrist_{'R' if side > 0 else 'L'}")

    # Hand (block placeholder — Batch D will refine with finger bones)
    hand_x = forearm_end_x + side * p["hand_l"] / 2
    _slab(hand_x, 0, shoulder_z,
           p["hand_l"], elbow_r * 1.0, elbow_r * 1.4,
           skin, f"Hand_{'R' if side > 0 else 'L'}")

    # Fingernails (4 small slabs at the tip of the hand)
    nails = _nails_mat()
    nail_tip_x = forearm_end_x + side * p["hand_l"]
    for fi in range(4):
        offset_y = (fi - 1.5) * elbow_r * 0.3
        _slab(nail_tip_x - side * 0.005, offset_y, shoulder_z + elbow_r * 0.3,
               0.012, elbow_r * 0.12, elbow_r * 0.20,
               nails, f"Nail_Hand_{'R' if side > 0 else 'L'}_{fi}")


def _build_leg(p, skin, side, hip_z):
    """Hip + thigh + calf + foot."""
    hip_x = side * p["hip_w"] / 4

    # Hip joint sphere
    hip_r = p["thigh_h"] * 0.18
    _ellipsoid((hip_x, 0, hip_z), hip_r, hip_r, hip_r,
                skin, f"HipBall_{'R' if side > 0 else 'L'}")

    # Thigh
    knee_z = hip_z - p["thigh_h"]
    _capsule((hip_x, 0, hip_z), (hip_x, 0, knee_z),
              hip_r * 1.0, hip_r * 0.80,
              skin, f"Thigh_{'R' if side > 0 else 'L'}",
              segments=16)

    # Knee
    knee_r = hip_r * 0.85
    _ellipsoid((hip_x, 0, knee_z), knee_r, knee_r, knee_r,
                skin, f"Knee_{'R' if side > 0 else 'L'}")

    # Calf
    ankle_z = knee_z - p["calf_h"]
    _capsule((hip_x, 0, knee_z), (hip_x, 0, ankle_z),
              knee_r * 0.95, knee_r * 0.70,
              skin, f"Calf_{'R' if side > 0 else 'L'}",
              segments=14)

    # Ankle
    ankle_r = knee_r * 0.65
    _ellipsoid((hip_x, 0, ankle_z), ankle_r, ankle_r, ankle_r,
                skin, f"Ankle_{'R' if side > 0 else 'L'}")

    # Foot (wedge: longer in -Y direction = forward in Blender, rotated to UE5 +X on export)
    foot_x = hip_x
    foot_y = -p["foot_h"] * 1.5
    foot_z = ankle_z - p["foot_h"] / 2
    _slab(foot_x, foot_y, foot_z,
           ankle_r * 1.6, p["foot_h"] * 4.0, p["foot_h"],
           skin, f"Foot_{'R' if side > 0 else 'L'}")

    # Toenails (5 small slabs at the front of the foot)
    nails = _nails_mat()
    toe_y = foot_y - p["foot_h"] * 2.0
    for fi in range(5):
        offset_x = (fi - 2.0) * ankle_r * 0.25
        _slab(foot_x + offset_x, toe_y, foot_z + p["foot_h"] * 0.4,
               ankle_r * 0.18, 0.012, p["foot_h"] * 0.30,
               nails, f"Nail_Foot_{'R' if side > 0 else 'L'}_{fi}")


# ── Top-level builders ───────────────────────────────────────────────────────

def gen_player_base(props, name):
    clear_scene()
    skin = _skin_mat()

    # Build top-down so each piece can chain to the next
    head_bottom_z = _build_head(props, skin)
    neck_bottom_z = _build_neck(props, skin, head_bottom_z)
    pelvis_bottom_z, _chest_top_z, chest_center_z = _build_torso(props, skin, neck_bottom_z)

    # Arms attach at the shoulder (top of chest area)
    shoulder_z = chest_center_z + (props["torso_h"] * 0.55) * 0.30
    for side in [-1, 1]:
        _build_arm(props, skin, side, shoulder_z)

    # Legs hang from the pelvis bottom
    for side in [-1, 1]:
        _build_leg(props, skin, side, pelvis_bottom_z)

    # Skeletal sockets that the rig in Batch D will hook bones to. These
    # also serve as canonical attach points for hats / weapons / props
    # before the full skeleton lands.
    add_socket("HeadAttach",      location=(0, 0, props["height"]))
    add_socket("ChestAttach",     location=(0, 0, chest_center_z))
    add_socket("WaistAttach",     location=(0, 0, pelvis_bottom_z + 0.05))
    add_socket("HandAttach_L",    location=(-(props["shoulder_w"] / 2 + props["arm_upper"] + props["arm_lower"] + props["hand_l"] / 2), 0, shoulder_z))
    add_socket("HandAttach_R",    location=( (props["shoulder_w"] / 2 + props["arm_upper"] + props["arm_lower"] + props["hand_l"] / 2), 0, shoulder_z))
    add_socket("FootAttach_L",    location=(-props["hip_w"] / 4, -props["foot_h"] * 1.5, props["foot_h"] / 2))
    add_socket("FootAttach_R",    location=( props["hip_w"] / 4, -props["foot_h"] * 1.5, props["foot_h"] / 2))

    finalize_asset(name,
                    bevel_width=0.0,           # human surfaces have no hard creases
                    smooth_angle_deg=180,       # smooth-shade everything
                    uv_unwrap=True,
                    cleanup=True,
                    collision="none",          # UE5 character uses a phys-asset capsule
                    lods=None,
                    pivot="bottom_center")


def main():
    print("\n=== Quiet Rift: Enigma — Player Base Humanoid Generator (Batch C) ===")
    for variant_name, props in [
        ("SK_Player_Base_Masculine", PROPS_MASCULINE),
        ("SK_Player_Base_Feminine",  PROPS_FEMININE),
    ]:
        print(f"\n[{variant_name}]  height={props['height']}m")
        gen_player_base(props, variant_name)
        out_path = os.path.join(OUTPUT_DIR, f"{variant_name}.fbx")
        export_fbx(variant_name, out_path)
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX exports the SK_Player_Base mesh with seven material slots")
    print("(Skin / Lips / Eyes_Sclera / Eyes_Iris / Teeth / InsideMouth / Nails) +")
    print("attach-point empties. Skeleton wiring lands in Batch D; morph targets in Batch E.")


if __name__ == "__main__":
    main()
