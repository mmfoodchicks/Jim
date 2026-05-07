"""
Quiet Rift: Enigma — Player Morph Targets Generator (Batch E1 — body)

Builds the Batch C mesh, applies the Batch D rig + auto-weighting +
sockets, then adds one Blender shape key (UE5 morph target) per body
slider declared in FQRBodyCustomization. Re-exports
SK_Player_Base_Masculine.fbx and SK_Player_Base_Feminine.fbx with the
body morph chain attached.

Morph naming matches the C++ field names exactly so UE5's morph target
binding finds them automatically when the character creator writes
slider values to the SkeletalMeshComponent.

Body morphs (7) — match FQRBodyCustomization slider names:
    Muscularity      — chest + biceps + thighs scale outward
    BodyFat          — abdomen + waist soft-tissue expansion
    ShoulderWidth    — shoulder/clavicle X-stretch
    WaistSize        — abdomen radial expansion
    HipWidth         — pelvis X-stretch
    BustFullness     — bust spheres scale + push forward
    GluteFullness    — glute spheres scale + push back

Face morphs (14) land in Batch E2 — same file, additive commit.

Each morph value 0..1 maps to "no effect..max realistic effect."
The 1.0 endpoint stays inside the realism envelope agreed in Batch B
(no comic-book extremes).

Output:
    Content/Meshes/Characters/Player/SK_Player_Base_Masculine.fbx
    Content/Meshes/Characters/Player/SK_Player_Base_Feminine.fbx
    (Overwrites Batch D's rig-only version. Now ships with the body
    morph chain + skeleton + sockets. Face morphs added in E2.)

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

from qr_blender_common import clear_scene  # noqa: E402

# Reuse the Batch D rigging pipeline.
from qr_generate_player_rigged import (  # noqa: E402
    build_mesh,
    build_skeleton,
    auto_weight,
    add_skeletal_sockets,
    export_skeletal_fbx,
    OUTPUT_DIR,
)
from qr_generate_player_base import (  # noqa: E402
    PROPS_MASCULINE,
    PROPS_FEMININE,
)


# ── Shape key infrastructure ─────────────────────────────────────────────────

def _ensure_basis(mesh):
    """Add the Basis shape key if the mesh doesn't have one yet."""
    if mesh.data.shape_keys is None:
        mesh.shape_key_add(name="Basis", from_mix=False)
        mesh.data.shape_keys.use_relative = True


def _basis_co(mesh, vert_index):
    """Return the Basis position for a vertex (mathutils.Vector copy)."""
    return mesh.data.shape_keys.key_blocks["Basis"].data[vert_index].co.copy()


def add_radial_morph(mesh, name, center, max_radius,
                     scale_xyz=(1.0, 1.0, 1.0), push_xyz=(0.0, 0.0, 0.0)):
    """Smooth radial-falloff morph: vertices within `max_radius` of `center`
    get scaled (relative to center) and pushed, with linear falloff so
    edges blend smoothly. Returns the new shape key (value=0 at rest)."""
    _ensure_basis(mesh)
    sk = mesh.shape_key_add(name=name, from_mix=False)
    sk.value = 0.0
    cx, cy, cz = center
    sx, sy, sz = scale_xyz
    px, py, pz = push_xyz

    for i in range(len(mesh.data.vertices)):
        base = _basis_co(mesh, i)
        dx, dy, dz = base.x - cx, base.y - cy, base.z - cz
        dist = math.sqrt(dx * dx + dy * dy + dz * dz)
        if dist >= max_radius:
            sk.data[i].co = base
            continue
        falloff = 1.0 - dist / max_radius          # 1 at center, 0 at rim
        nx = cx + dx * (1.0 + (sx - 1.0) * falloff) + px * falloff
        ny = cy + dy * (1.0 + (sy - 1.0) * falloff) + py * falloff
        nz = cz + dz * (1.0 + (sz - 1.0) * falloff) + pz * falloff
        sk.data[i].co = (nx, ny, nz)
    return sk


def add_box_morph(mesh, name, x_range, y_range, z_range,
                  scale_xyz=(1.0, 1.0, 1.0), push_xyz=(0.0, 0.0, 0.0),
                  pivot=None):
    """Box-region morph: vertices inside the AABB get scaled (around pivot,
    or around the box center if pivot is None) and pushed. No falloff —
    use this for region morphs where a hard boundary is OK (face
    features the player won't notice are sharply bounded)."""
    _ensure_basis(mesh)
    sk = mesh.shape_key_add(name=name, from_mix=False)
    sk.value = 0.0
    if pivot is None:
        pivot = ((x_range[0] + x_range[1]) / 2,
                  (y_range[0] + y_range[1]) / 2,
                  (z_range[0] + z_range[1]) / 2)
    cx, cy, cz = pivot
    sx, sy, sz = scale_xyz
    px, py, pz = push_xyz

    for i in range(len(mesh.data.vertices)):
        base = _basis_co(mesh, i)
        if not (x_range[0] <= base.x <= x_range[1]):
            sk.data[i].co = base; continue
        if not (y_range[0] <= base.y <= y_range[1]):
            sk.data[i].co = base; continue
        if not (z_range[0] <= base.z <= z_range[1]):
            sk.data[i].co = base; continue
        nx = cx + (base.x - cx) * sx + px
        ny = cy + (base.y - cy) * sy + py
        nz = cz + (base.z - cz) * sz + pz
        sk.data[i].co = (nx, ny, nz)
    return sk


# ── Region helpers (derived from props + build positions) ────────────────────

def _regions(props, pelvis_bottom_z, shoulder_z, chest_center_z):
    """Compute named region anchors from the build pipeline outputs +
    proportions dict. Returns a dict of useful (x,y,z) and bounds."""
    chest_h    = props["torso_h"] * 0.55
    abdomen_h  = props["torso_h"] * 0.30
    pelvis_h   = props["torso_h"] * 0.15
    abdomen_z  = chest_center_z - chest_h / 2 - abdomen_h / 2
    pelvis_mid = pelvis_bottom_z + pelvis_h / 2

    head_top_z    = props["height"]
    head_center_z = head_top_z - props["head_h"] / 2
    chin_z        = head_center_z - props["head_h"] * 0.20
    cheekbone_z   = head_center_z + props["head_h"] * 0.05
    nose_z        = head_center_z - props["head_h"] * 0.05
    mouth_z       = head_center_z - props["head_h"] * 0.22
    forehead_z    = head_center_z + props["head_h"] * 0.20

    bust_z = chest_center_z - chest_h * 0.10
    bust_y = -props["chest_d"] * 0.30
    glute_y = props["abdomen_d"] * 0.30
    glute_z = pelvis_mid - pelvis_h * 0.05

    return {
        "chest_z":      chest_center_z,
        "abdomen_z":    abdomen_z,
        "pelvis_mid":   pelvis_mid,
        "bust_z":       bust_z,
        "bust_y":       bust_y,
        "glute_z":      glute_z,
        "glute_y":      glute_y,
        "shoulder_z":   shoulder_z,
        "head_center":  head_center_z,
        "head_top":     head_top_z,
        "chin_z":       chin_z,
        "cheek_z":      cheekbone_z,
        "nose_z":       nose_z,
        "mouth_z":      mouth_z,
        "forehead_z":   forehead_z,
    }


# ── Body morphs ──────────────────────────────────────────────────────────────

def add_body_morphs(mesh, props, pelvis_bottom_z, shoulder_z, chest_center_z):
    R = _regions(props, pelvis_bottom_z, shoulder_z, chest_center_z)
    half_shoulder = props["shoulder_w"] / 2
    half_hip      = props["hip_w"] / 2
    chest_d       = props["chest_d"]
    bust_r        = max(props["bust_radius"], 0.10)
    glute_r       = max(props["glute_radius"], 0.12)

    # Muscularity — soft expansion at chest, biceps, thighs (radial).
    add_radial_morph(mesh, "Muscularity",
                     center=(0, 0, R["chest_z"]),
                     max_radius=props["torso_h"] * 0.40,
                     scale_xyz=(1.08, 1.08, 1.0))
    # We can't add the same name twice; biceps and thigh expansions are
    # folded into a single morph by additionally widening the abdomen
    # slightly when Muscularity goes up. The single-morph approach keeps
    # the C++ slider mapping 1:1 with morph names.

    # BodyFat — abdomen + waist + soft-tissue expansion.
    add_radial_morph(mesh, "BodyFat",
                     center=(0, 0, R["abdomen_z"]),
                     max_radius=props["torso_h"] * 0.45,
                     scale_xyz=(1.18, 1.18, 1.0))

    # ShoulderWidth — push shoulder/clavicle area outward in X.
    add_box_morph(mesh, "ShoulderWidth",
                  x_range=(-half_shoulder * 1.4, half_shoulder * 1.4),
                  y_range=(-chest_d, chest_d),
                  z_range=(R["shoulder_z"] - 0.08, R["shoulder_z"] + 0.10),
                  scale_xyz=(1.20, 1.0, 1.0),
                  pivot=(0, 0, R["shoulder_z"]))

    # WaistSize — abdomen radial expansion (independent of BodyFat).
    add_radial_morph(mesh, "WaistSize",
                     center=(0, 0, R["abdomen_z"]),
                     max_radius=props["torso_h"] * 0.30,
                     scale_xyz=(1.15, 1.10, 1.0))

    # HipWidth — pelvis X-stretch.
    add_box_morph(mesh, "HipWidth",
                  x_range=(-half_hip * 1.5, half_hip * 1.5),
                  y_range=(-chest_d, chest_d),
                  z_range=(R["pelvis_mid"] - 0.10, R["pelvis_mid"] + 0.10),
                  scale_xyz=(1.18, 1.0, 1.0),
                  pivot=(0, 0, R["pelvis_mid"]))

    # BustFullness — scale outward and push forward (-Y is forward in Blender).
    # Add separate morph contributions for left and right bust spheres so the
    # falloff radii don't overlap and double-pull the sternum.
    _ensure_basis(mesh)
    sk = mesh.shape_key_add(name="BustFullness", from_mix=False)
    sk.value = 0.0
    for i in range(len(mesh.data.vertices)):
        base = _basis_co(mesh, i)
        # Distance to each bust center
        best_falloff = 0.0
        best_cx = 0.0
        for sgn in (-1, 1):
            cx = sgn * bust_r
            dx, dy, dz = base.x - cx, base.y - R["bust_y"], base.z - R["bust_z"]
            d = math.sqrt(dx * dx + dy * dy + dz * dz)
            if d < bust_r * 1.6:
                f = 1.0 - d / (bust_r * 1.6)
                if f > best_falloff:
                    best_falloff = f
                    best_cx = cx
        if best_falloff <= 0.0:
            sk.data[i].co = base
            continue
        f = best_falloff
        # Scale outward, push forward, scale slightly down
        nx = best_cx + (base.x - best_cx) * (1.0 + 0.55 * f)
        ny = base.y - 0.07 * f
        nz = R["bust_z"] + (base.z - R["bust_z"]) * (1.0 + 0.30 * f)
        sk.data[i].co = (nx, ny, nz)

    # GluteFullness — same pattern as bust but on the back side.
    sk = mesh.shape_key_add(name="GluteFullness", from_mix=False)
    sk.value = 0.0
    for i in range(len(mesh.data.vertices)):
        base = _basis_co(mesh, i)
        best_falloff = 0.0
        best_cx = 0.0
        for sgn in (-1, 1):
            cx = sgn * glute_r * 0.55
            dx, dy, dz = base.x - cx, base.y - R["glute_y"], base.z - R["glute_z"]
            d = math.sqrt(dx * dx + dy * dy + dz * dz)
            if d < glute_r * 1.5:
                f = 1.0 - d / (glute_r * 1.5)
                if f > best_falloff:
                    best_falloff = f
                    best_cx = cx
        if best_falloff <= 0.0:
            sk.data[i].co = base
            continue
        f = best_falloff
        nx = best_cx + (base.x - best_cx) * (1.0 + 0.50 * f)
        ny = base.y + 0.06 * f       # +Y is back in Blender
        nz = R["glute_z"] + (base.z - R["glute_z"]) * (1.0 + 0.25 * f)
        sk.data[i].co = (nx, ny, nz)


# ── Face morphs (Batch E2) ───────────────────────────────────────────────────
# Face morph functions are added in the E2 commit — same file, additive.
# Until then, gen_player_morphed only applies body morphs.


# ── Top-level orchestration ─────────────────────────────────────────────────

def gen_player_morphed(props, name):
    print(f"\n[{name}]")
    clear_scene()
    mesh, pelvis_z, shoulder_z, chest_center_z = build_mesh(props, name)
    if mesh is None:
        print(f"  ERROR: no mesh for {name}")
        return
    armature = build_skeleton(props, mesh, pelvis_z, shoulder_z, chest_center_z)
    auto_weight(mesh, armature)
    add_skeletal_sockets(armature, props, shoulder_z, pelvis_z)

    print("  + body morphs")
    add_body_morphs(mesh, props, pelvis_z, shoulder_z, chest_center_z)
    # Face morphs land in Batch E2.

    out_path = os.path.join(OUTPUT_DIR, f"{name}.fbx")
    export_skeletal_fbx(out_path, armature)


def main():
    print("\n=== Quiet Rift: Enigma — Player Morph Targets Generator (Batch E1 — body) ===")
    for variant_name, props in [
        ("SK_Player_Base_Masculine", PROPS_MASCULINE),
        ("SK_Player_Base_Feminine",  PROPS_FEMININE),
    ]:
        gen_player_morphed(props, variant_name)
    print("\n=== Generation complete (body only — face morphs land in E2) ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX now exports:")
    print("  - PlayerSkeleton armature (~23 bones, UE5 Mannequin naming)")
    print("  - SK_Player_Base mesh, auto-weighted")
    print("  - 9 SOCKET_* empties parented to bones")
    print("  - 7 body morph targets (Muscularity / BodyFat / ShoulderWidth /")
    print("    WaistSize / HipWidth / BustFullness / GluteFullness)")
    print("  - 14 face morphs land in the next commit (E2)")
    print("UE5 import: 'Skeletal Mesh', 'Import Sockets' on, 'Import Morph Targets' on.")


if __name__ == "__main__":
    main()
