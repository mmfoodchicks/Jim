"""
Quiet Rift: Enigma -- RIGGED Wildlife Generator (Blender 4.x/5.x headless).

Where qr_generate_wildlife_assets.py exports static placeholder meshes,
this script exports SKELETAL meshes with four baked animation takes per
species (A_Idle / A_Walk / A_Run / A_Death), sized and colored from the
species descriptions in DT_Species_Wildlife.csv + DT_Species_v15.csv.

They land in AQRWildlifeBase's single-node animation slots
(DefaultBodyMesh / IdleAnim / WalkAnim / RunAnim / DeathAnim) via
qr_import_wildlife_rigged.py -- the same no-AnimBP pattern the NPC
brain and the German Shepherd colony dog already use.

Design notes
------------
- RIGID PER-PART BINDING, not auto-weights: every primitive is
  vertex-grouped 100% to exactly one bone at build time. Deterministic
  in --background mode (heat-map weights are not), and reads cleanly
  on stylized low-poly creatures -- each plate/leg/jaw moves as a unit.
- One generic body-plan builder covers all 37 species through a params
  table. Archetypes:
      walker  -- body + head + N legs (3/4/6/8) + tail. Legs swing in
                 diagonal-pair phase; body bobs; tail sways.
      floater -- no legs; hover bob + pitch drift (gas bladders,
                 wisps, swarm clouds).
      buried  -- anchored cone/hump; idle pulse; death = sink.
      roller  -- Crackrunner's spine-wheel; locomotion = spin.
- Material slots reuse the exact "Wildlife_<Species>_<Part>" names the
  static generator established, so qr_build_qr_materials.py colors
  BOTH mesh sets from one table.
- Export flags mirror qr_generate_player_rigged.py's skeletal export,
  plus bake_anim so every action becomes an FBX take (-> one UE
  AnimSequence each on import).

Usage (headless, from regenerate_all_fbx.bat):
    blender --background --python qr_generate_wildlife_rigged.py
"""

import bpy
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from qr_blender_common import SCALE, clear_scene  # noqa: E402
from qr_blender_detail import (  # noqa: E402
    get_or_create_material,
    assign_material,
    smart_uv_unwrap,
)
from qr_blender_photoreal import pbr_material, bake_pbr  # noqa: E402

OUTPUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "../../Content/Meshes/wildlife_rigged")

FPS = 24


# ─── Species table ───────────────────────────────────────────────────
# key      -> export token: file is SK_<key>.fbx
# arch     -> walker | floater | buried | roller
# L/W/H    -> body bounds in meters (nose-to-tail / width / total height)
# legs     -> walker leg count (3/4/6/8); leg_h = ground-to-belly
# body/acc -> (rgb, roughness, emissive_or_None) material specs
# feats    -> list of feature tags the builder understands
# gait     -> anim speed scale (1 = default)
SPECIES = {
    # ── legacy ANM_ roster ────────────────────────────────────────
    "ANM_RidgebackGrazer": dict(
        arch="walker", L=2.2, W=0.9, H=1.5, legs=4, leg_h=0.75,
        body=((0.65, 0.55, 0.40), 0.85, None),
        acc=((0.55, 0.45, 0.32), 0.80, None),
        feats=["dorsal_plates:6", "tail"], gait=0.9),
    "ANM_GlasshornRunner": dict(
        arch="walker", L=1.3, W=0.5, H=1.0, legs=4, leg_h=0.55,
        body=((0.65, 0.55, 0.40), 0.85, None),
        acc=((0.85, 0.92, 0.95), 0.20, (0.50, 0.85, 0.95)),
        feats=["horn", "tail"], gait=1.5),
    "ANM_AshbackBoar": dict(
        arch="walker", L=1.4, W=0.7, H=0.9, legs=4, leg_h=0.40,
        body=((0.45, 0.35, 0.25), 0.85, None),
        acc=((0.20, 0.18, 0.18), 0.85, None),
        feats=["dorsal_stripe", "tusks", "snout"], gait=1.0),
    "ANM_HookjawStalker": dict(
        arch="walker", L=1.8, W=0.55, H=1.1, legs=4, leg_h=0.60,
        body=((0.20, 0.10, 0.10), 0.80, None),
        acc=((0.10, 0.05, 0.05), 0.70, None),
        feats=["hook_jaw", "tail"], gait=1.3),
    "ANM_ThornhideDray": dict(
        arch="walker", L=1.0, W=0.5, H=0.7, legs=4, leg_h=0.35,
        body=((0.45, 0.35, 0.25), 0.85, None),
        acc=((0.20, 0.15, 0.10), 0.75, None),
        feats=["thorn_ring:10", "tail"], gait=1.1),
    "ANM_MireOx": dict(
        arch="walker", L=3.4, W=1.6, H=1.9, legs=4, leg_h=0.95,
        body=((0.65, 0.55, 0.40), 0.90, None),
        acc=((0.18, 0.14, 0.10), 0.80, None),
        feats=["side_horns", "snout", "tail"], gait=0.6),
    "ANM_LanternMiteSwarm": dict(
        arch="floater", L=1.4, W=1.4, H=1.4, hover=1.0,
        body=((0.30, 0.55, 0.30), 0.60, (0.25, 0.65, 0.25)),
        acc=((0.30, 0.55, 0.30), 0.60, None),
        feats=["swarm:24"], gait=1.2),
    "ANM_BurrowEel": dict(
        arch="buried", L=0.5, W=0.5, H=1.3,
        body=((0.45, 0.50, 0.35), 0.80, None),
        acc=((0.45, 0.15, 0.15), 0.60, None),
        feats=["segments:4", "maw"], gait=1.0),
    "ANM_IronmantleBeetle": dict(
        arch="walker", L=1.0, W=0.6, H=0.45, legs=4, leg_h=0.18,
        body=((0.30, 0.30, 0.35), 0.55, None),
        acc=((0.18, 0.18, 0.20), 0.65, None),
        feats=["mantle_plates:4"], gait=0.8),
    "ANM_PaleRafter": dict(
        arch="walker", L=1.2, W=0.6, H=1.7, legs=4, leg_h=0.90,
        body=((0.85, 0.85, 0.80), 0.75, None),
        acc=((0.30, 0.20, 0.20), 0.70, None),
        feats=["wing_flaps", "hook_jaw"], gait=1.2),
    "ANM_StonebellyTortoise": dict(
        arch="walker", L=1.5, W=1.2, H=0.8, legs=4, leg_h=0.25,
        body=((0.55, 0.55, 0.50), 0.90, None),
        acc=((0.40, 0.35, 0.30), 0.95, None),
        feats=["shell_dome", "snout"], gait=0.4),
    "ANM_EmbermaneAlpha": dict(
        arch="walker", L=3.4, W=1.3, H=2.3, legs=4, leg_h=1.2,
        body=((0.20, 0.10, 0.10), 0.75, None),
        acc=((0.85, 0.30, 0.05), 0.40, (0.85, 0.30, 0.05)),
        feats=["mane_spikes:14", "claws", "tail"], gait=1.1),
    # ── canonical v15 ANI_ roster ─────────────────────────────────
    "ANI_PEBBLE_SKITTER": dict(
        arch="walker", L=0.16, W=0.16, H=0.10, legs=8, leg_h=0.05,
        body=((0.40, 0.40, 0.40), 0.90, None),
        acc=((0.85, 0.78, 0.70), 0.80, None),
        feats=["disc_body"], gait=2.0),
    "ANI_GLEAM_LARVER": dict(
        arch="floater", L=0.8, W=0.25, H=0.25, hover=0.12,
        body=((0.85, 0.82, 0.75), 0.35, None),
        acc=((0.72, 0.74, 0.78), 0.20, None),
        feats=["segments:5"], gait=0.7),
    "ANI_SHARDBACK_GRAZER": dict(
        arch="walker", L=2.3, W=1.1, H=1.5, legs=6, leg_h=0.70,
        body=((0.78, 0.72, 0.60), 0.85, None),
        acc=((0.88, 0.85, 0.78), 0.60, None),
        feats=["dorsal_plates:7", "tail"], gait=0.8),
    "ANI_SILT_STRIDER": dict(
        arch="walker", L=1.6, W=0.8, H=2.0, legs=3, leg_h=1.4,
        body=((0.40, 0.30, 0.20), 0.85, None),
        acc=((0.20, 0.55, 0.55), 0.50, None),
        feats=["belly_sac", "fins"], gait=0.9),
    "ANI_PILLARBACK_HAULER": dict(
        arch="walker", L=10.0, W=4.0, H=5.5, legs=4, leg_h=2.6,
        body=((0.12, 0.10, 0.08), 0.95, None),
        acc=((0.55, 0.48, 0.30), 0.60, (0.55, 0.48, 0.30)),
        feats=["dorsal_plates:5", "glow_nodes:6", "snout", "tail"], gait=0.3),
    "ANI_BASIN_TREADER": dict(
        arch="walker", L=2.4, W=2.4, H=1.0, legs=8, leg_h=0.55,
        body=((0.45, 0.45, 0.48), 0.85, None),
        acc=((0.80, 0.78, 0.72), 0.75, None),
        feats=["disc_body"], gait=0.7),
    "ANI_NESTWEAVER_DRIFTER": dict(
        arch="floater", L=1.0, W=0.9, H=0.8, hover=0.9,
        body=((0.60, 0.60, 0.58), 0.75, None),
        acc=((0.92, 0.88, 0.80), 0.25, None),
        feats=["fins", "belly_sac"], gait=0.8),
    "ANI_MILKBLADDER_HERDLING": dict(
        arch="walker", L=1.3, W=0.7, H=0.8, legs=6, leg_h=0.35,
        body=((0.65, 0.55, 0.42), 0.85, None),
        acc=((0.95, 0.92, 0.85), 0.60, None),
        feats=["belly_sac", "tail"], gait=0.9),
    "ANI_BONE_LANTERN": dict(
        arch="floater", L=0.7, W=0.7, H=1.0, hover=1.4,
        body=((0.92, 0.90, 0.85), 0.55, None),
        acc=((0.20, 0.45, 0.65), 0.30, (0.20, 0.45, 0.65)),
        feats=["tendrils:4"], gait=0.5),
    "ANI_LATCHFIN_MITE": dict(
        arch="walker", L=0.04, W=0.03, H=0.02, legs=4, leg_h=0.008,
        body=((0.20, 0.15, 0.10), 0.70, None),
        acc=((0.70, 0.65, 0.55), 0.40, None),
        feats=["disc_body"], gait=2.2),
    "ANI_CRACKRUNNER": dict(
        arch="roller", L=0.6, W=0.18, H=0.6,
        body=((0.10, 0.08, 0.08), 0.70, None),
        acc=((0.55, 0.25, 0.10), 0.60, None),
        feats=["edge_spines:8"], gait=1.6),
    "ANI_RIDGE_COURSER": dict(
        arch="walker", L=2.6, W=0.9, H=1.8, legs=6, leg_h=1.0,
        body=((0.55, 0.45, 0.32), 0.85, None),
        acc=((0.15, 0.13, 0.10), 0.70, None),
        feats=["dorsal_stripe", "faceplate", "tail"], gait=1.4),
    "ANI_VAULTBACK_DRAY": dict(
        arch="walker", L=5.0, W=2.2, H=3.0, legs=4, leg_h=1.5,
        body=((0.35, 0.30, 0.25), 0.90, None),
        acc=((0.10, 0.08, 0.08), 0.85, None),
        feats=["shell_dome", "snout", "tail"], gait=0.45),
    "ANI_TETHERBACK_PACKGRAZER": dict(
        arch="walker", L=3.3, W=1.5, H=1.9, legs=4, leg_h=0.95,
        body=((0.50, 0.45, 0.38), 0.90, None),
        acc=((0.72, 0.62, 0.48), 0.80, None),
        feats=["dorsal_pad", "snout", "tail"], gait=0.55),
    # ── canonical v15 PRD_ roster ─────────────────────────────────
    "PRD_SUTURE_WISP": dict(
        arch="floater", L=0.7, W=0.3, H=2.6, hover=0.7,
        body=((0.05, 0.04, 0.06), 0.30, (0.20, 0.05, 0.40)),
        acc=((0.55, 0.20, 0.75), 0.25, (0.55, 0.20, 0.75)),
        feats=["ribbon", "tendrils:3"], gait=0.9),
    "PRD_NEEDLE_MAW": dict(
        arch="buried", L=1.9, W=1.9, H=1.2,
        body=((0.78, 0.72, 0.62), 0.85, None),
        acc=((0.55, 0.10, 0.10), 0.50, None),
        feats=["needle_ring:9", "maw"], gait=1.0),
    "PRD_DRIFT_STALKER": dict(
        arch="floater", L=2.4, W=1.1, H=0.9, hover=0.35,
        body=((0.40, 0.38, 0.35), 0.75, None),
        acc=((0.80, 0.40, 0.10), 0.40, (0.80, 0.40, 0.10)),
        feats=["canopy", "fins"], gait=1.3),
    "PRD_VANE_RIPPERS": dict(
        arch="walker", L=1.3, W=0.35, H=1.0, legs=4, leg_h=0.55,
        body=((0.18, 0.16, 0.18), 0.75, None),
        acc=((0.45, 0.42, 0.38), 0.65, None),
        feats=["vanes:4", "faceplate", "tail"], gait=1.5),
    "PRD_GLASSJAW_CLUSTER": dict(
        arch="floater", L=0.8, W=0.8, H=0.4, hover=0.1,
        body=((0.75, 0.70, 0.55), 0.30, (0.55, 0.50, 0.35)),
        acc=((0.85, 0.55, 0.60), 0.60, None),
        feats=["swarm:9"], gait=1.0),
    "PRD_SILT_HOUNDS": dict(
        arch="walker", L=1.3, W=0.5, H=0.55, legs=4, leg_h=0.25,
        body=((0.10, 0.08, 0.06), 0.60, None),
        acc=((0.20, 0.50, 0.50), 0.30, None),
        feats=["dorsal_stripe", "hook_jaw", "tail"], gait=1.3),
    "PRD_TRENCH_DIGGERS": dict(
        arch="walker", L=2.6, W=0.9, H=1.0, legs=6, leg_h=0.45,
        body=((0.10, 0.08, 0.08), 0.85, None),
        acc=((0.82, 0.78, 0.65), 0.60, None),
        feats=["segments:4", "drill", "collar_rings:3"], gait=0.8),
    "PRD_CARRION_CHOIR": dict(
        arch="floater", L=1.0, W=1.0, H=0.9, hover=1.6,
        body=((0.78, 0.75, 0.65), 0.65, None),
        acc=((0.30, 0.55, 0.70), 0.40, (0.30, 0.55, 0.70)),
        feats=["swarm:8"], gait=0.6),
    "PRD_FOGLEECH_SWARM": dict(
        arch="floater", L=0.4, W=0.4, H=0.3, hover=0.8,
        body=((0.12, 0.22, 0.12), 0.70, (0.08, 0.18, 0.08)),
        acc=((0.12, 0.22, 0.12), 0.70, None),
        feats=["swarm:16"], gait=1.1),
    "PRD_IRONSTAG_STALKER": dict(
        arch="walker", L=3.2, W=1.1, H=2.3, legs=4, leg_h=1.3,
        body=((0.15, 0.10, 0.10), 0.75, None),
        acc=((0.65, 0.22, 0.08), 0.35, (0.65, 0.22, 0.08)),
        feats=["antlers", "chest_plate", "tail"], gait=1.0),
    "PRD_SHELLMAW_AMBUSHER": dict(
        arch="buried", L=3.6, W=3.0, H=1.7,
        body=((0.38, 0.28, 0.20), 0.95, None),
        acc=((0.65, 0.48, 0.40), 0.60, None),
        feats=["shell_dome", "maw"], gait=0.8),
}


# ─── Small helpers ───────────────────────────────────────────────────

def _mat(species_token, part, spec):
    rgb, rough, emissive = spec
    name = "Wildlife_{}_{}".format(species_token, part)
    em = (emissive[0], emissive[1], emissive[2], 1.0) if emissive else None
    return get_or_create_material(name, (rgb[0], rgb[1], rgb[2], 1.0),
                                  roughness=rough, emissive=em)


def _tag(obj, bone):
    """Bind every vertex of obj 100% to one bone (rigid part binding)."""
    vg = obj.vertex_groups.new(name=bone)
    vg.add(list(range(len(obj.data.vertices))), 1.0, 'REPLACE')


def _sphere(loc, r, scale, bone, mat):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r, segments=16, ring_count=8,
                                         location=loc)
    o = bpy.context.active_object
    o.scale = scale
    bpy.ops.object.transform_apply(scale=True)
    assign_material(o, mat)
    _tag(o, bone)
    return o


def _cyl(loc, r, depth, bone, mat, rot=None):
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=depth, vertices=10,
                                        location=loc)
    o = bpy.context.active_object
    if rot:
        o.rotation_euler = rot
        bpy.ops.object.transform_apply(rotation=True)
    assign_material(o, mat)
    _tag(o, bone)
    return o


def _cone(loc, r, depth, bone, mat, rot=None):
    bpy.ops.mesh.primitive_cone_add(radius1=r, radius2=0.0, depth=depth,
                                    vertices=10, location=loc)
    o = bpy.context.active_object
    if rot:
        o.rotation_euler = rot
        bpy.ops.object.transform_apply(rotation=True)
    assign_material(o, mat)
    _tag(o, bone)
    return o


def _box(loc, dims, bone, mat):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
    o = bpy.context.active_object
    o.scale = (dims[0] * 0.5, dims[1] * 0.5, dims[2] * 0.5)
    bpy.ops.object.transform_apply(scale=True)
    assign_material(o, mat)
    _tag(o, bone)
    return o


# ─── Rig ─────────────────────────────────────────────────────────────

def _build_armature(token, p):
    """Bone layout shared by all archetypes. Positions derive from the
    species dims so anims scale with the body. Returns (armature, legs)
    where legs is the list of leg bone names created."""
    arch = p["arch"]
    L, H = p["L"], p["H"]
    leg_h = p.get("leg_h", 0.0)
    body_z = leg_h + (H - leg_h) * 0.5 if arch == "walker" else \
        p.get("hover", 0.2) + H * 0.4

    arm_data = bpy.data.armatures.new("SK_{}_Skel".format(token))
    arm = bpy.data.objects.new("SK_{}_Armature".format(token), arm_data)
    bpy.context.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode='EDIT')
    eb = arm_data.edit_bones

    def B(name, head, tail, parent=None):
        b = eb.new(name)
        b.head = head
        b.tail = tail
        if parent:
            b.parent = eb[parent]
        return b

    B("root",   (0, 0, 0),                (0, 0.15 * max(L, 0.3), 0))
    B("pelvis", (0, 0, body_z),           (0, 0, body_z + max(H * 0.2, 0.05)), "root")
    B("spine",  (L * 0.05, 0, body_z),    (L * 0.30, 0, body_z), "pelvis")
    B("head",   (L * 0.38, 0, body_z + H * 0.15),
                (L * 0.55, 0, body_z + H * 0.15), "spine")
    B("tail_01", (-L * 0.35, 0, body_z),  (-L * 0.55, 0, body_z), "pelvis")

    legs = []
    if arch == "walker":
        n = p.get("legs", 4)
        # Leg stations along the body; 3-legged striders get a tripod.
        if n == 3:
            stations = [(L * 0.28, p["W"] * 0.35), (L * 0.28, -p["W"] * 0.35),
                        (-L * 0.32, 0.0)]
        else:
            per_side = n // 2
            xs = [L * (0.30 - 0.60 * (i / max(1, per_side - 1)))
                  for i in range(per_side)]
            stations = [(x, s * p["W"] * 0.38)
                        for x in xs for s in (1, -1)]
        for i, (sx, sy) in enumerate(stations):
            name = "leg_{:02d}".format(i)
            B(name, (sx, sy, leg_h), (sx, sy, 0.001), "pelvis")
            legs.append(name)

    bpy.ops.object.mode_set(mode='OBJECT')
    return arm, legs


# ─── Body construction ───────────────────────────────────────────────

def _build_body(token, p, legs, arm):
    """Primitives per archetype, every part rigid-bound to its bone."""
    arch = p["arch"]
    L, W, H = p["L"], p["W"], p["H"]
    leg_h = p.get("leg_h", 0.0)
    body_z = leg_h + (H - leg_h) * 0.5 if arch == "walker" else \
        p.get("hover", 0.2) + H * 0.4
    body_mat = _mat(token, "Body", p["body"])
    acc_mat = _mat(token, "Accent", p["acc"])
    glint = get_or_create_material("Wildlife_EyeGlint",
                                   (0.95, 0.85, 0.30, 1.0), roughness=0.10,
                                   emissive=(0.95, 0.85, 0.30, 1.0))
    feats = {}
    for f in p.get("feats", []):
        k, _, v = f.partition(":")
        feats[k] = int(v) if v else True

    core_h = (H - leg_h) if arch == "walker" else H * 0.8

    # Core body
    if "disc_body" in feats:
        _cyl((0, 0, body_z), max(L, W) * 0.5, core_h * 0.6, "pelvis", body_mat)
    elif "ribbon" in feats:
        _box((0, 0, body_z), (L * 0.6, W * 0.5, H * 0.9), "pelvis", body_mat)
    elif "swarm" in feats:
        n = feats["swarm"]
        rr = max(L, W) * 0.5
        for i in range(n):
            a = i * 2.399963  # golden angle scatter
            rad = rr * math.sqrt((i + 0.5) / n)
            _sphere((math.cos(a) * rad, math.sin(a) * rad,
                     body_z + math.sin(i * 1.7) * H * 0.3),
                    max(L, W) * 0.06, (1, 1, 1), "pelvis",
                    body_mat if i % 3 else acc_mat)
    elif arch == "buried":
        _cone((0, 0, H * 0.45), max(L, W) * 0.45, H * 0.9, "pelvis", body_mat)
    elif arch == "roller":
        _cyl((0, 0, H * 0.5), H * 0.5, W, "pelvis", body_mat,
             rot=(math.pi / 2, 0, 0))
    else:
        _sphere((0, 0, body_z), 1.0, (L * 0.38, W * 0.5, core_h * 0.5),
                "pelvis", body_mat)

    # Head (walkers + buried maws get one; swarms/ribbons/rollers don't)
    has_head = arch == "walker" or "maw" in feats
    if has_head and "disc_body" not in feats:
        hz = body_z + H * 0.15 if arch == "walker" else H * 0.85
        hr = max(min(L, H) * 0.16, 0.02)
        _sphere((L * 0.45, 0, hz) if arch == "walker" else (0, 0, hz),
                hr, (1.1, 0.9, 0.9), "head", body_mat)
        if arch == "walker":
            _sphere((L * 0.50, W * 0.12, hz + hr * 0.4), hr * 0.18,
                    (1, 1, 1), "head", glint)
            _sphere((L * 0.50, -W * 0.12, hz + hr * 0.4), hr * 0.18,
                    (1, 1, 1), "head", glint)

    # Legs -- read each leg bone's station straight off the armature.
    # (NOT via objects.active: every primitive_add call above made the
    # new MESH active, so that shortcut would explode on .data.bones.)
    for i, name in enumerate(legs):
        bone = arm.data.bones[name]
        sx, sy, _ = bone.head_local
        # Legs run up INTO the body ellipsoid: the belly curves away
        # from leg_h at the hip stations, so a leg that stops exactly
        # at leg_h floats visibly detached under the haunch.
        leg_top = leg_h + core_h * 0.40
        _cyl((sx, sy, leg_top * 0.5), max(W * 0.06, 0.008), leg_top,
             name, body_mat)

    # Tail
    if "tail" in feats and arch == "walker":
        _cone((-L * 0.45, 0, body_z), W * 0.12, L * 0.35, "tail_01",
              body_mat, rot=(0, -math.pi / 2, 0))

    # ── Feature shapes (all accent-material unless noted) ─────────
    if "dorsal_plates" in feats:
        n = feats["dorsal_plates"]
        for i in range(n):
            x = L * (0.28 - 0.56 * i / max(1, n - 1))
            _cone((x, 0, body_z + core_h * 0.45), W * 0.14, H * 0.22,
                  "spine", acc_mat)
    if "horn" in feats:
        _cone((L * 0.52, 0, body_z + H * 0.32), L * 0.03, L * 0.28,
              "head", acc_mat, rot=(0, math.pi / 3, 0))
    if "tusks" in feats:
        for s in (1, -1):
            _cone((L * 0.50, s * W * 0.18, body_z), L * 0.02, L * 0.14,
                  "head", acc_mat, rot=(0, math.pi / 2.4, 0))
    if "snout" in feats:
        _cyl((L * 0.52, 0, body_z + H * 0.10), min(W, H) * 0.12, L * 0.15,
             "head", body_mat, rot=(0, math.pi / 2, 0))
    if "hook_jaw" in feats:
        _cone((L * 0.55, 0, body_z + H * 0.05), min(W, H) * 0.10, L * 0.22,
              "head", acc_mat, rot=(0, math.pi / 2, 0))
    if "dorsal_stripe" in feats:
        _box((0, 0, body_z + core_h * 0.42), (L * 0.6, W * 0.18, H * 0.06),
             "spine", acc_mat)
    if "thorn_ring" in feats:
        n = feats["thorn_ring"]
        for i in range(n):
            a = 2 * math.pi * i / n
            _cone((math.cos(a) * L * 0.25, math.sin(a) * W * 0.42,
                   body_z + core_h * 0.30), W * 0.05, H * 0.20,
                  "pelvis", acc_mat,
                  rot=(-math.sin(a) * 0.6, math.cos(a) * 0.6, 0))
    if "side_horns" in feats:
        for s in (1, -1):
            _cone((L * 0.40, s * W * 0.35, body_z + H * 0.22), W * 0.06,
                  W * 0.55, "head", acc_mat, rot=(math.pi / 2 * s * -1, 0, 0))
    if "mantle_plates" in feats:
        n = feats["mantle_plates"]
        for i in range(n):
            x = L * (0.30 - 0.60 * i / max(1, n - 1))
            _box((x, 0, body_z + core_h * 0.35),
                 (L * 0.85 / n, W * 0.95, H * 0.10), "spine", acc_mat)
    if "wing_flaps" in feats:
        for s in (1, -1):
            _box((0, s * W * 0.55, body_z + H * 0.05),
                 (L * 0.55, W * 0.35, H * 0.04), "spine", acc_mat)
    if "shell_dome" in feats:
        _sphere((0, 0, body_z + core_h * 0.25), 1.0,
                (L * 0.42, W * 0.55, core_h * 0.55), "spine", acc_mat)
    if "mane_spikes" in feats:
        n = feats["mane_spikes"]
        for i in range(n):
            a = 2 * math.pi * i / n
            _cone((L * 0.32 + math.cos(a) * W * 0.10,
                   math.sin(a) * W * 0.32, body_z + H * 0.22 +
                   abs(math.cos(a)) * H * 0.10), W * 0.05, H * 0.28,
                  "head", acc_mat, rot=(0, -0.5, a))
    if "claws" in feats and legs:
        for name in legs[:2]:
            bone = arm.data.bones[name]
            sx, sy, _ = bone.head_local
            _cone((sx + L * 0.05, sy, 0.05), W * 0.05, L * 0.10,
                  name, acc_mat, rot=(0, math.pi / 2, 0))
    if "belly_sac" in feats:
        _sphere((0, 0, max(body_z - core_h * 0.35, 0.05)), 1.0,
                (L * 0.28, W * 0.40, core_h * 0.30), "pelvis", acc_mat)
    if "fins" in feats:
        for s in (1, -1):
            _box((-L * 0.1, s * W * 0.5, body_z + H * 0.1),
                 (L * 0.35, W * 0.25, H * 0.03), "spine", acc_mat)
    if "faceplate" in feats:
        _box((L * 0.50, 0, body_z + H * 0.18), (L * 0.04, W * 0.35, H * 0.18),
             "head", acc_mat)
    if "vanes" in feats:
        n = feats["vanes"]
        for i in range(n):
            x = L * (0.25 - 0.50 * i / max(1, n - 1))
            _box((x, 0, body_z + core_h * 0.45), (L * 0.05, W * 0.10, H * 0.30),
                 "spine", acc_mat)
    if "glow_nodes" in feats:
        n = feats["glow_nodes"]
        glow = _mat(token, "Glow", p["acc"])
        for i in range(n):
            x = L * (0.30 - 0.60 * i / max(1, n - 1))
            _sphere((x, 0, body_z + core_h * 0.50), min(W, H) * 0.06,
                    (1, 1, 1), "spine", glow)
    if "segments" in feats:
        n = feats["segments"]
        if p["arch"] == "buried":
            for i in range(n):
                z = H * 0.9 * (i + 0.5) / n
                _sphere((0, 0, z), max(L, W) * (0.45 - 0.06 * i), (1, 1, 0.6),
                        "pelvis" if i < n - 1 else "head", body_mat)
        else:
            for i in range(n):
                x = L * (0.40 - 0.80 * i / max(1, n - 1))
                _sphere((x, 0, body_z), 1.0, (L * 0.10, W * 0.5, H * 0.4),
                        "pelvis", body_mat if i % 2 else acc_mat)
    if "maw" in feats:
        _cyl((0, 0, H * 0.92), max(L, W) * 0.22, H * 0.08, "head", acc_mat)
    if "needle_ring" in feats:
        n = feats["needle_ring"]
        for i in range(n):
            a = 2 * math.pi * i / n
            _cone((math.cos(a) * L * 0.32, math.sin(a) * W * 0.32, H * 0.75),
                  L * 0.04, H * 0.40, "head", acc_mat,
                  rot=(math.sin(a) * 0.5, -math.cos(a) * 0.5, 0))
    if "antlers" in feats:
        for s in (1, -1):
            _cone((L * 0.42, s * W * 0.16, body_z + H * 0.35), W * 0.04,
                  H * 0.45, "head", acc_mat, rot=(s * 0.4, -0.5, 0))
            _cone((L * 0.38, s * W * 0.24, body_z + H * 0.40), W * 0.03,
                  H * 0.30, "head", acc_mat, rot=(s * 0.8, -0.3, 0))
    if "chest_plate" in feats:
        _box((L * 0.30, 0, body_z - H * 0.05), (L * 0.10, W * 0.6, H * 0.30),
             "spine", _mat(token, "Plate", ((0.30, 0.28, 0.35), 0.45, None)))
    if "drill" in feats:
        _cone((L * 0.55, 0, body_z), min(W, H) * 0.30, L * 0.30, "head",
              acc_mat, rot=(0, math.pi / 2, 0))
    if "collar_rings" in feats:
        n = feats["collar_rings"]
        ring = _mat(token, "Collar", ((0.45, 0.35, 0.25), 0.70, None))
        for i in range(n):
            _cyl((L * (0.35 - 0.08 * i), 0, body_z), min(W, H) * (0.40 + 0.03 * i),
                 L * 0.03, "head", ring, rot=(0, math.pi / 2, 0))
    if "edge_spines" in feats:
        n = feats["edge_spines"]
        for i in range(n):
            a = 2 * math.pi * i / n
            _cone((math.cos(a) * H * 0.5, 0, H * 0.5 + math.sin(a) * H * 0.5),
                  H * 0.05, H * 0.18, "pelvis", acc_mat,
                  rot=(0, a + math.pi / 2, 0))
    if "canopy" in feats:
        _sphere((L * 0.1, 0, body_z + H * 0.35), 1.0,
                (L * 0.25, W * 0.35, H * 0.20), "spine",
                _mat(token, "Canopy", ((0.20, 0.18, 0.20), 0.20, None)))
    if "tendrils" in feats:
        n = feats["tendrils"]
        for i in range(n):
            a = 2 * math.pi * i / n
            _cyl((math.cos(a) * W * 0.2, math.sin(a) * W * 0.2,
                  p.get("hover", 0.2) * 0.5),
                 W * 0.02, p.get("hover", 0.4) * 0.9, "tail_01", acc_mat)


# ─── Animation ───────────────────────────────────────────────────────

def _key(arm, bone, frame, rot=None, loc=None, scl=None):
    pb = arm.pose.bones[bone]
    pb.rotation_mode = 'XYZ'
    if rot is not None:
        pb.rotation_euler = rot
        pb.keyframe_insert('rotation_euler', frame=frame)
    if loc is not None:
        pb.location = loc
        pb.keyframe_insert('location', frame=frame)
    if scl is not None:
        pb.scale = scl
        pb.keyframe_insert('scale', frame=frame)


def _reset_pose(arm):
    for pb in arm.pose.bones:
        pb.rotation_mode = 'XYZ'
        pb.rotation_euler = (0, 0, 0)
        pb.location = (0, 0, 0)
        pb.scale = (1, 1, 1)


def _new_action(arm, name, end_frame):
    act = bpy.data.actions.new(name)
    act.use_fake_user = True
    if arm.animation_data is None:
        arm.animation_data_create()
    arm.animation_data.action = act
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = end_frame
    _reset_pose(arm)
    return act


def _anim_locomotion(arm, legs, p, name, cycle, swing, bob):
    """Looping gait: legs swing in diagonal phase, body bobs, tail sways.
    Floaters/rollers/buried get archetype motion instead of leg swings."""
    _new_action(arm, name, cycle)
    arch = p["arch"]
    H = p["H"]
    for f in range(1, cycle + 1, max(1, cycle // 8)):
        t = 2 * math.pi * (f - 1) / cycle
        if arch == "walker":
            for i, leg in enumerate(legs):
                phase = math.pi if i % 2 else 0.0
                _key(arm, leg, f, rot=(math.sin(t + phase) * swing, 0, 0))
            _key(arm, "pelvis", f,
                 loc=(0, 0, abs(math.sin(t * 2)) * bob * H * 0.03))
            _key(arm, "head", f, rot=(math.sin(t * 2) * 0.05, 0, 0))
            _key(arm, "tail_01", f, rot=(0, 0, math.sin(t) * 0.25))
        elif arch == "floater":
            _key(arm, "pelvis", f,
                 loc=(0, 0, math.sin(t) * bob * H * 0.08),
                 rot=(math.sin(t) * 0.06 * (1 + swing), 0, 0))
            _key(arm, "tail_01", f, rot=(math.sin(t + 1.0) * 0.3, 0, 0))
        elif arch == "roller":
            # Full revolution per cycle -- 2pi wraps seamlessly.
            _key(arm, "pelvis", f, rot=(0, t * (1 + swing), 0))
        else:  # buried: pulse
            s = 1.0 + math.sin(t) * 0.05 * (1 + swing)
            _key(arm, "head", f, scl=(s, s, s))
            _key(arm, "pelvis", f, scl=(1, 1, 1 + math.sin(t) * 0.02))
    # Close the loop exactly.
    if arch == "walker":
        for i, leg in enumerate(legs):
            phase = math.pi if i % 2 else 0.0
            _key(arm, leg, cycle + 1, rot=(math.sin(phase) * swing, 0, 0))


def _anim_idle(arm, legs, p):
    _new_action(arm, "A_Idle", 48)
    H = p["H"]
    for f in (1, 13, 25, 37, 49):
        t = 2 * math.pi * (f - 1) / 48
        _key(arm, "pelvis", f, loc=(0, 0, math.sin(t) * H * 0.010))
        _key(arm, "head", f, rot=(math.sin(t) * 0.04, 0, math.sin(t * 0.5) * 0.06))
        _key(arm, "tail_01", f, rot=(0, 0, math.sin(t + 0.8) * 0.15))


def _anim_death(arm, legs, p):
    _new_action(arm, "A_Death", 22)
    arch = p["arch"]
    _key(arm, "pelvis", 1, rot=(0, 0, 0), loc=(0, 0, 0))
    if arch == "walker":
        _key(arm, "pelvis", 18, rot=(0, 1.35, 0),
             loc=(0, 0, -p.get("leg_h", 0) * 0.6))
        for leg in legs:
            _key(arm, leg, 1, rot=(0, 0, 0))
            _key(arm, leg, 18, rot=(0.5, 0, 0))
    elif arch == "floater":
        hover = p.get("hover", 0.3)
        _key(arm, "pelvis", 20, rot=(0, 0.8, 0), loc=(0, 0, -hover))
    elif arch == "roller":
        _key(arm, "pelvis", 16, rot=(0, 0.4, 1.30))
    else:  # buried sinks
        _key(arm, "pelvis", 20, loc=(0, 0, -p["H"] * 0.7))
    # Hold the corpse pose.
    _key(arm, "pelvis", 22, rot=arm.pose.bones["pelvis"].rotation_euler[:],
         loc=arm.pose.bones["pelvis"].location[:])


# ─── Per-species build + export ──────────────────────────────────────

def _export_skeletal(filepath):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.export_scene.fbx(
        filepath=filepath,
        use_selection=True,
        global_scale=SCALE,
        apply_scale_options='FBX_SCALE_ALL',
        axis_forward='-Z',
        axis_up='Y',
        object_types={'ARMATURE', 'MESH'},
        use_armature_deform_only=True,
        add_leaf_bones=False,
        primary_bone_axis='Y',
        secondary_bone_axis='X',
        bake_anim=True,
        bake_anim_use_all_bones=True,
        bake_anim_use_nla_strips=False,
        bake_anim_use_all_actions=True,
        bake_anim_force_startend_keying=True,
        bake_anim_simplify_factor=0.0,
        mesh_smooth_type='FACE',
        bake_space_transform=False,
        path_mode='COPY',
        embed_textures=True,    # baked hide atlas travels inside the FBX
    )
    print("  Exported: {}".format(filepath))


def gen_species(species_id, p):
    clear_scene()
    # Purge actions from the previous species so bake_anim_use_all_actions
    # doesn't drag every prior species' takes into this FBX.
    for act in list(bpy.data.actions):
        bpy.data.actions.remove(act)

    token = species_id.split("_", 1)[1].title().replace("_", "")
    arm, legs = _build_armature(token, p)
    _build_body(token, p, legs, arm)

    # Join all meshes into one, bind to the armature via the vertex
    # groups each part carries.
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    body = bpy.context.active_object
    body.name = "SK_{}".format(species_id)
    smart_uv_unwrap(body)

    # ── Photoreal skin: rebuild each flat part material as procedural
    # PBR (hide for flesh, rock-chitin for plates/shells, emissive
    # crystal for glow zones) and bake the whole body to one atlas.
    # Runs BEFORE armature binding; bake uses the rest pose.
    _PART_KINDS = [
        (("plate", "armor", "mantle", "shell", "collar", "drill",
          "faceplate"), "rock"),
        (("glow", "mane", "horn", "canopy"), "leaf_alien"),
        (("eyeglint",), "leaf_alien"),
    ]
    for idx, mat in enumerate(list(body.data.materials)):
        if mat is None:
            continue
        tint = (0.5, 0.5, 0.5)
        emissive = None
        if mat.use_nodes:
            bsdf = mat.node_tree.nodes.get("Principled BSDF")
            if bsdf:
                c = bsdf.inputs["Base Color"].default_value
                tint = (c[0], c[1], c[2])
                for em_name in ("Emission Color", "Emission"):
                    if em_name in bsdf.inputs:
                        ec = bsdf.inputs[em_name].default_value
                        try:
                            if (ec[0] + ec[1] + ec[2]) > 0.05:
                                emissive = (ec[0], ec[1], ec[2])
                        except TypeError:
                            pass
                        break
        low = mat.name.lower()
        kind = "hide"
        for tokens, k in _PART_KINDS:
            if any(t in low for t in tokens):
                kind = k
                break
        body.data.materials[idx] = pbr_material(
            "QRSK_{}_{}".format(species_id, idx), kind, tint=tint,
            emissive=emissive, emissive_strength=3.0 if emissive else 0.0)
    bake_pbr(body, "SK_{}".format(species_id),
             os.path.join(OUTPUT_DIR, "Textures"), size=1024, samples=8)

    body.parent = arm
    mod = body.modifiers.new("Armature", 'ARMATURE')
    mod.object = arm

    # Animations.
    bpy.context.view_layer.objects.active = arm
    g = p.get("gait", 1.0)
    _anim_idle(arm, legs, p)
    _anim_locomotion(arm, legs, p, "A_Walk",
                     cycle=max(8, int(24 / g)), swing=0.35, bob=1.0)
    _anim_locomotion(arm, legs, p, "A_Run",
                     cycle=max(6, int(12 / g)), swing=0.55, bob=1.6)
    _anim_death(arm, legs, p)
    _reset_pose(arm)

    _export_skeletal(os.path.join(OUTPUT_DIR, "SK_{}.fbx".format(species_id)))


def main():
    print("=== qr_generate_wildlife_rigged ===")
    done = 0
    for sid, params in SPECIES.items():
        print("[{}]".format(sid))
        gen_species(sid, params)
        done += 1
    print("=== {} rigged species exported to {} ===".format(done, OUTPUT_DIR))
    print("Next: UE Python -> qr_import_wildlife_rigged.py run()")


if __name__ == "__main__":
    main()
