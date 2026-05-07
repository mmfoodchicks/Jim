"""
Quiet Rift: Enigma — Shared Blender Detail / Finalization Module

A higher-level companion to qr_blender_common.py. While qr_blender_common.py
provides primitive helpers (clear_scene, export_fbx, add_material,
join_and_rename), this module provides the production-grade *finalize*
pipeline used by upgraded category scripts:

  - Material palette dedupe (get_or_create_material, palette_material)
  - Per-face material zone assignment
  - Mesh hygiene (cleanup_mesh: doubles, normals, loose verts)
  - Smooth shading + auto-smooth angle (Blender 3.x and 4.1+ compatible)
  - Bevel modifier with angle-limited application
  - Smart UV unwrap so meshes import texture-ready
  - Standardized pivot/origin (bottom_center, bottom_corner, geometry_center)
  - Repeated-detail helpers (rivet rings, rivet grids, panel-seam strips)
  - Empty-based sockets (SOCKET_*) for UE5 socket import
  - Convex-hull collision (UCX_*) for UE5 collision import
  - Decimated LOD chain (<mesh>_LOD1, _LOD2, ...) for UE5 LOD import
  - finalize_asset(): single-call wrap-up that joins mesh objects, applies
    every step above, parents sockets, and emits collision + LODs alongside.

Usage pattern in an upgraded generator:

    def gen_widget():
        clear_scene()
        # build geometry, calling palette_material() for shared material slots
        ...
        add_socket("Muzzle", location=(1.5, 0, 0))
        add_panel_seam_strip(start=(...), end=(...))
        finalize_asset(
            "SM_WIDGET",
            bevel_width=0.004,
            collision="convex",
            lods=[0.6, 0.3],
            pivot="bottom_center",
        )

All helpers are idempotent where reasonable and degrade gracefully if a
given Blender op isn't available — older Blender 3.x lacks
shade_smooth_by_angle, newer 4.1+ removed Mesh.use_auto_smooth, both
paths are handled.
"""

import bpy
import bmesh
import math


# ── Material Palette ──────────────────────────────────────────────────────────

# Canonical palette so every category script reads from the same color values.
# Keyed by slot name; values are (color_rgba, roughness, metallic, emissive_or_None).
_PALETTE = {
    # Hard surface
    "Steel":        ((0.30, 0.32, 0.36, 1.0), 0.55, 0.95, None),
    "DarkSteel":    ((0.18, 0.20, 0.24, 1.0), 0.50, 0.95, None),
    "Gunmetal":     ((0.16, 0.17, 0.19, 1.0), 0.40, 0.95, None),
    "Brass":        ((0.75, 0.55, 0.20, 1.0), 0.45, 0.85, None),
    "Copper":       ((0.65, 0.42, 0.22, 1.0), 0.45, 0.85, None),
    "Lead":         ((0.42, 0.42, 0.45, 1.0), 0.55, 0.80, None),
    "Aluminum":     ((0.78, 0.79, 0.82, 1.0), 0.40, 0.95, None),
    # Polymer / rubber / glass
    "Polymer":      ((0.10, 0.10, 0.10, 1.0), 0.65, 0.0,  None),
    "Rubber":       ((0.07, 0.07, 0.07, 1.0), 0.85, 0.0,  None),
    "Glass":        ((0.55, 0.70, 0.80, 0.4), 0.05, 0.0,  None),
    "GlassDark":    ((0.10, 0.15, 0.20, 0.5), 0.05, 0.0,  None),
    # Wood / cloth / leather
    "Wood":         ((0.45, 0.30, 0.18, 1.0), 0.85, 0.0,  None),
    "DarkWood":     ((0.28, 0.18, 0.10, 1.0), 0.85, 0.0,  None),
    "Canvas":       ((0.62, 0.55, 0.42, 1.0), 0.95, 0.0,  None),
    "DarkCanvas":   ((0.40, 0.36, 0.28, 1.0), 0.95, 0.0,  None),
    "Leather":      ((0.42, 0.28, 0.18, 1.0), 0.80, 0.0,  None),
    "DarkLeather":  ((0.28, 0.18, 0.10, 1.0), 0.80, 0.0,  None),
    "Olive":        ((0.35, 0.40, 0.25, 1.0), 0.90, 0.0,  None),
    "DarkOlive":    ((0.22, 0.26, 0.16, 1.0), 0.90, 0.0,  None),
    # Stone / earth
    "Stone":        ((0.55, 0.55, 0.52, 1.0), 0.95, 0.0,  None),
    "DarkStone":    ((0.30, 0.30, 0.32, 1.0), 0.95, 0.0,  None),
    "Rock":         ((0.45, 0.42, 0.38, 1.0), 0.95, 0.0,  None),
    "DarkRock":     ((0.25, 0.22, 0.20, 1.0), 0.95, 0.0,  None),
    # Lore-specific accents
    "VanguardRed":  ((0.55, 0.10, 0.12, 1.0), 0.65, 0.0,  None),
    "ProgenitorStone": ((0.62, 0.66, 0.72, 1.0), 0.55, 0.0, None),
    # Glows (emissive)
    "GlowCyan":     ((0.30, 0.85, 0.95, 1.0), 0.20, 0.0, (0.30, 0.85, 0.95, 1.0)),
    "GlowGold":     ((0.85, 0.75, 0.30, 1.0), 0.20, 0.0, (0.85, 0.75, 0.30, 1.0)),
    "GlowRed":      ((0.85, 0.20, 0.15, 1.0), 0.20, 0.0, (0.85, 0.20, 0.15, 1.0)),
    "GlowViolet":   ((0.55, 0.35, 0.85, 1.0), 0.20, 0.0, (0.55, 0.35, 0.85, 1.0)),
    "Ember":        ((0.95, 0.42, 0.05, 1.0), 0.30, 0.0, (0.95, 0.42, 0.05, 1.0)),
}


def get_or_create_material(slot_name, color_rgba=None, roughness=0.7,
                           metallic=0.0, emissive=None):
    """Get a material by name; create with given properties if it doesn't exist.
    Materials are scoped to the .blend file, so the same `slot_name` in two
    generators reuses the same material instance."""
    mat = bpy.data.materials.get(slot_name)
    if mat is not None:
        return mat
    mat = bpy.data.materials.new(name=slot_name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf is not None and color_rgba is not None:
        bsdf.inputs["Base Color"].default_value = color_rgba
        if "Roughness" in bsdf.inputs:
            bsdf.inputs["Roughness"].default_value = roughness
        if "Metallic" in bsdf.inputs:
            bsdf.inputs["Metallic"].default_value = metallic
        if emissive is not None:
            # Blender 4.x renamed Emission → Emission Color; support both.
            emit_key = "Emission Color" if "Emission Color" in bsdf.inputs else "Emission"
            if emit_key in bsdf.inputs:
                bsdf.inputs[emit_key].default_value = emissive
            if "Emission Strength" in bsdf.inputs:
                bsdf.inputs["Emission Strength"].default_value = 1.0
    return mat


def palette_material(name):
    """Look up a canonical palette entry and return a deduped material."""
    if name not in _PALETTE:
        # Unknown name → neutral gray, named so caller spots it later.
        return get_or_create_material(f"Unknown_{name}", (0.5, 0.5, 0.5, 1.0))
    color, rough, metal, emit = _PALETTE[name]
    return get_or_create_material(name, color, rough, metal, emit)


def assign_material(obj, mat):
    """Append `mat` to `obj`'s material slots if not already present.
    Returns the slot index."""
    if obj is None or obj.type != 'MESH' or mat is None:
        return -1
    for i, slot in enumerate(obj.material_slots):
        if slot.material == mat:
            return i
    obj.data.materials.append(mat)
    return len(obj.data.materials) - 1


def assign_material_to_faces(obj, face_indices, mat):
    """Set `mat` as the material for the listed face indices on `obj`."""
    slot = assign_material(obj, mat)
    if slot < 0:
        return
    for fi in face_indices:
        if 0 <= fi < len(obj.data.polygons):
            obj.data.polygons[fi].material_index = slot


# ── Mesh Hygiene & Shading ────────────────────────────────────────────────────

def cleanup_mesh(obj, merge_distance=1e-5):
    """Remove duplicate verts, recalculate normals, drop loose geometry."""
    if obj is None or obj.type != 'MESH':
        return
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.remove_doubles(threshold=merge_distance)
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.mesh.delete_loose()
    bpy.ops.object.mode_set(mode='OBJECT')


def shade_smooth(obj, autosmooth_deg=60.0):
    """Smooth shading + per-edge auto-smooth so panel seams stay crisp."""
    if obj is None or obj.type != 'MESH':
        return
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.shade_smooth()
    # Blender 4.1+ removed Mesh.use_auto_smooth in favor of an operator.
    if hasattr(obj.data, "use_auto_smooth"):
        obj.data.use_auto_smooth = True
        obj.data.auto_smooth_angle = math.radians(autosmooth_deg)
    else:
        try:
            bpy.ops.object.shade_smooth_by_angle(angle=math.radians(autosmooth_deg))
        except Exception:
            # Operator absent on some platform builds; smooth shading still applied.
            pass


def add_bevel(obj, width=0.004, segments=2, angle_deg=30.0, apply_modifier=False):
    """Add a Bevel modifier limited by edge angle. Apply immediately if requested."""
    if obj is None or obj.type != 'MESH':
        return None
    mod = obj.modifiers.new(name="QRBevel", type='BEVEL')
    mod.width = width
    mod.segments = max(1, int(segments))
    mod.limit_method = 'ANGLE'
    mod.angle_limit = math.radians(angle_deg)
    mod.miter_outer = 'MITER_ARC'
    if apply_modifier:
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
        return None
    return mod


def smart_uv_unwrap(obj, angle_limit_deg=66.0, island_margin=0.02):
    """Run smart_project so the asset imports with usable UVs. Skips silently
    if the mesh has no faces."""
    if obj is None or obj.type != 'MESH' or len(obj.data.polygons) == 0:
        return
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    try:
        bpy.ops.uv.smart_project(angle_limit=math.radians(angle_limit_deg),
                                  island_margin=island_margin)
    except Exception:
        # smart_project signature varies across versions; fall back to default.
        bpy.ops.uv.smart_project()
    bpy.ops.object.mode_set(mode='OBJECT')


# ── Pivot / Origin ────────────────────────────────────────────────────────────

def set_pivot(obj, mode="bottom_center"):
    """Move an object's origin to a canonical position. Modes:
        bottom_center    — XY centroid, Z = min
        bottom_corner    — min X, min Y, min Z (UE5-friendly for floors/walls)
        geometry_center  — centroid of all verts
        world_origin     — leave at (0,0,0)
    """
    if obj is None or obj.type != 'MESH' or not obj.data.vertices:
        return
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    if mode == "geometry_center":
        bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')
        return
    if mode == "world_origin":
        bpy.context.scene.cursor.location = (0.0, 0.0, 0.0)
        bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
        return
    verts = [obj.matrix_world @ v.co for v in obj.data.vertices]
    if mode == "bottom_center":
        cx = sum(v.x for v in verts) / len(verts)
        cy = sum(v.y for v in verts) / len(verts)
        mn = min(v.z for v in verts)
        bpy.context.scene.cursor.location = (cx, cy, mn)
    elif mode == "bottom_corner":
        bpy.context.scene.cursor.location = (
            min(v.x for v in verts),
            min(v.y for v in verts),
            min(v.z for v in verts),
        )
    else:
        return
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')


# ── Repeated Detail ───────────────────────────────────────────────────────────

def add_rivet_ring(center, radius, count, rivet_radius=0.004, depth=0.002,
                   color=(0.20, 0.20, 0.22, 1.0), normal_axis='Z',
                   material_name="Gunmetal"):
    """Place `count` small cylindrical rivets in a ring around `center`. The
    `normal_axis` is the axis the cylinder caps face along.
    Returns the list of rivet objects."""
    rivets = []
    cx, cy, cz = center
    mat = palette_material(material_name) if material_name in _PALETTE \
        else get_or_create_material(material_name, color)
    for i in range(count):
        angle = (i / count) * math.tau
        if normal_axis == 'Z':
            x, y, z = cx + math.cos(angle) * radius, cy + math.sin(angle) * radius, cz
            bpy.ops.mesh.primitive_cylinder_add(radius=rivet_radius, depth=depth, location=(x, y, z))
        elif normal_axis == 'Y':
            x, y, z = cx + math.cos(angle) * radius, cy, cz + math.sin(angle) * radius
            bpy.ops.mesh.primitive_cylinder_add(radius=rivet_radius, depth=depth, location=(x, y, z))
            bpy.context.active_object.rotation_euler.x = math.pi / 2
        else:  # X
            x, y, z = cx, cy + math.cos(angle) * radius, cz + math.sin(angle) * radius
            bpy.ops.mesh.primitive_cylinder_add(radius=rivet_radius, depth=depth, location=(x, y, z))
            bpy.context.active_object.rotation_euler.y = math.pi / 2
        r = bpy.context.active_object
        r.name = f"Rivet_{i}"
        assign_material(r, mat)
        rivets.append(r)
    return rivets


def add_rivet_grid(origin, spacing, rows, cols, rivet_radius=0.004, depth=0.002,
                   normal_axis='Z', material_name="Gunmetal"):
    """Rectangular grid of rivets. `origin` = (x, y, z) corner; `spacing` =
    (dx, dy) step between rivets in the plane perpendicular to `normal_axis`."""
    rivets = []
    ox, oy, oz = origin
    sx, sy = spacing
    mat = palette_material(material_name)
    for r in range(rows):
        for c in range(cols):
            if normal_axis == 'Z':
                loc = (ox + c * sx, oy + r * sy, oz)
                bpy.ops.mesh.primitive_cylinder_add(radius=rivet_radius, depth=depth, location=loc)
            elif normal_axis == 'Y':
                loc = (ox + c * sx, oy, oz + r * sy)
                bpy.ops.mesh.primitive_cylinder_add(radius=rivet_radius, depth=depth, location=loc)
                bpy.context.active_object.rotation_euler.x = math.pi / 2
            else:
                loc = (ox, oy + c * sx, oz + r * sy)
                bpy.ops.mesh.primitive_cylinder_add(radius=rivet_radius, depth=depth, location=loc)
                bpy.context.active_object.rotation_euler.y = math.pi / 2
            obj = bpy.context.active_object
            obj.name = f"Rivet_{r}_{c}"
            assign_material(obj, mat)
            rivets.append(obj)
    return rivets


def add_panel_seam_strip(start, end, width=0.003, depth=0.002,
                         material_name="DarkSteel", name="PanelSeam"):
    """Recessed panel-line strip running from `start` to `end` (XYZ tuples).
    Built as a thin slab oriented along the segment."""
    sx, sy, sz = start
    ex, ey, ez = end
    mid = ((sx + ex) / 2, (sy + ey) / 2, (sz + ez) / 2)
    length = math.sqrt((ex - sx) ** 2 + (ey - sy) ** 2 + (ez - sz) ** 2)
    if length <= 1e-6:
        return None
    bpy.ops.mesh.primitive_cube_add(size=1, location=mid)
    seam = bpy.context.active_object
    seam.scale = (length, width, depth)
    seam.rotation_euler.z = math.atan2(ey - sy, ex - sx)
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    seam.name = name
    assign_material(seam, palette_material(material_name))
    return seam


# ── Sockets ───────────────────────────────────────────────────────────────────

def add_socket(name, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0),
               radius=0.05):
    """Create a UE5-style socket as an ARROWS empty named SOCKET_<name>.
    finalize_asset will parent it to the joined mesh once geometry is settled.
    UE5 imports SOCKET_* empties as native sockets when 'Import Sockets' is on."""
    bpy.ops.object.empty_add(type='ARROWS', location=location, rotation=rotation,
                             radius=radius)
    e = bpy.context.active_object
    e.name = f"SOCKET_{name}"
    return e


# ── Collision ─────────────────────────────────────────────────────────────────

def add_collision_convex(source_obj, mesh_name, decimate=0.5):
    """Duplicate `source_obj`, convert to a convex hull, decimate, and rename
    UCX_<mesh_name>_00 — UE5 imports these as convex collision."""
    if source_obj is None or source_obj.type != 'MESH':
        return None
    bpy.ops.object.select_all(action='DESELECT')
    source_obj.select_set(True)
    bpy.context.view_layer.objects.active = source_obj
    bpy.ops.object.duplicate()
    hull = bpy.context.active_object
    hull.name = f"UCX_{mesh_name}_00"
    # Strip extra material slots — collision doesn't need them.
    hull.data.materials.clear()
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    try:
        bpy.ops.mesh.convex_hull(use_existing_faces=False, delete_unused=True)
    except TypeError:
        # Older Blender API
        bpy.ops.mesh.convex_hull()
    bpy.ops.object.mode_set(mode='OBJECT')
    if 0.0 < decimate < 1.0:
        mod = hull.modifiers.new(name="HullDecimate", type='DECIMATE')
        mod.ratio = decimate
        bpy.context.view_layer.objects.active = hull
        bpy.ops.object.modifier_apply(modifier=mod.name)
    hull.display_type = 'WIRE'
    hull.show_in_front = True
    return hull


def add_collision_box(source_obj, mesh_name):
    """Create a UCX_<mesh_name>_00 axis-aligned bounding box around source_obj."""
    if source_obj is None or source_obj.type != 'MESH' or not source_obj.data.vertices:
        return None
    verts = [source_obj.matrix_world @ v.co for v in source_obj.data.vertices]
    mn_x = min(v.x for v in verts); mx_x = max(v.x for v in verts)
    mn_y = min(v.y for v in verts); mx_y = max(v.y for v in verts)
    mn_z = min(v.z for v in verts); mx_z = max(v.z for v in verts)
    cx, cy, cz = (mn_x + mx_x) / 2, (mn_y + mx_y) / 2, (mn_z + mx_z) / 2
    sx, sy, sz = max(mx_x - mn_x, 0.001), max(mx_y - mn_y, 0.001), max(mx_z - mn_z, 0.001)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(cx, cy, cz))
    box = bpy.context.active_object
    box.scale = (sx, sy, sz)
    bpy.ops.object.transform_apply(scale=True)
    box.name = f"UCX_{mesh_name}_00"
    box.data.materials.clear()
    box.display_type = 'WIRE'
    box.show_in_front = True
    return box


# ── LODs ──────────────────────────────────────────────────────────────────────

def add_lod(source_obj, ratio, mesh_name, lod_index):
    """Duplicate `source_obj`, decimate to `ratio`, name `<mesh_name>_LOD<n>`.
    UE5 imports siblings named with the _LOD<n> suffix as the LOD chain."""
    if source_obj is None or source_obj.type != 'MESH':
        return None
    if ratio <= 0.0 or ratio >= 1.0:
        return None
    bpy.ops.object.select_all(action='DESELECT')
    source_obj.select_set(True)
    bpy.context.view_layer.objects.active = source_obj
    bpy.ops.object.duplicate()
    lod = bpy.context.active_object
    lod.name = f"{mesh_name}_LOD{lod_index}"
    mod = lod.modifiers.new(name=f"LOD{lod_index}Decimate", type='DECIMATE')
    mod.ratio = ratio
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return lod


# ── The Big Finalize ──────────────────────────────────────────────────────────

def finalize_asset(name, *,
                   bevel_width=0.004, bevel_segments=2, bevel_angle_deg=30.0,
                   smooth_angle_deg=60.0,
                   uv_unwrap=True,
                   cleanup=True,
                   collision="convex",        # "convex" | "box" | "none"
                   collision_decimate=0.5,
                   lods=None,                  # iterable of decimate ratios, e.g. [0.6, 0.3]
                   pivot="bottom_center",      # see set_pivot()
                   apply_bevel=False):
    """One-call wrap-up for an upgraded generator. Joins every mesh in the
    scene (excluding any UCX_*, SOCKET_*, *_LOD* objects), renames the
    result to `name`, runs hygiene + smooth shading + bevel + UVs + pivot,
    parents any preexisting SOCKET_* empties to the result, and emits
    collision + LOD copies alongside.

    Returns the primary mesh object."""
    # 1. Identify primary mesh objects (not collision, not socket, not pre-existing LOD).
    primaries = [o for o in bpy.context.scene.objects
                 if o.type == 'MESH'
                 and not o.name.startswith("UCX_")
                 and "_LOD" not in o.name]
    if not primaries:
        return None

    # 2. Join all primary meshes.
    bpy.ops.object.select_all(action='DESELECT')
    for o in primaries:
        o.select_set(True)
    bpy.context.view_layer.objects.active = primaries[0]
    if len(primaries) > 1:
        bpy.ops.object.join()
    obj = bpy.context.active_object
    obj.name = name
    obj.data.name = f"{name}_Mesh"

    # 3. Hygiene + shading + bevel + UV.
    if cleanup:
        cleanup_mesh(obj)
    shade_smooth(obj, smooth_angle_deg)
    if bevel_width > 0:
        add_bevel(obj, width=bevel_width, segments=bevel_segments,
                  angle_deg=bevel_angle_deg, apply_modifier=apply_bevel)
    if uv_unwrap:
        smart_uv_unwrap(obj)

    # 4. Standardize pivot. Sockets get parented next so they pick up the new origin.
    set_pivot(obj, pivot)

    # 5. Parent any sockets that the generator added to the scene.
    sockets = [o for o in bpy.context.scene.objects if o.name.startswith("SOCKET_")]
    for s in sockets:
        s.parent = obj
        s.matrix_parent_inverse = obj.matrix_world.inverted()

    # 6. Collision.
    if collision == "convex":
        add_collision_convex(obj, name, decimate=collision_decimate)
    elif collision == "box":
        add_collision_box(obj, name)

    # 7. LODs. The first LOD is the original; we generate _LOD1 .. _LODn.
    if lods:
        for i, ratio in enumerate(lods, start=1):
            add_lod(obj, ratio, name, i)

    return obj
