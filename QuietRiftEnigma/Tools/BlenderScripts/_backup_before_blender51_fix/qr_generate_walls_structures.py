"""
Quiet Rift: Enigma — Walls & Structures Asset Generator (Blender 4.x)

Upgraded in Batch 7 of the Blender detail pass — every modular building
piece flows through qr_blender_detail.py for production finalization
(palette dedupe, smooth shading, micro-bevels that DO NOT break grid
alignment, smart UV, sockets, UCX collision, LOD chain).

CRITICAL: This is the only category where grid alignment is mission-
critical. UE5 will tile these together by snapping the SOCKET_Snap*
empties to neighboring pieces. The bevel width is intentionally tiny
(0.001 m) and the auto-smooth angle is set high (75 deg) so corners
stay crisp at the tile boundaries. Pivot is preserved as authored so
walls snap on their bottom-center edge and floors snap on their
bottom-corner. Don't switch to bottom_center on floors — it breaks
the grid math.

Per-piece sockets (placement helpers, picked up by UE5 modular building):
    Walls:   SOCKET_SnapLeft / SOCKET_SnapRight / SOCKET_SnapTop /
             SOCKET_SnapBottom (cardinal edges of the wall plane)
    Floors:  SOCKET_SnapN / SOCKET_SnapS / SOCKET_SnapE / SOCKET_SnapW
             (cardinal edges of the floor footprint)
    Doors:   SOCKET_HingePivot / SOCKET_LatchPoint
    Roofs:   SOCKET_SnapN / SOCKET_SnapS / SOCKET_SnapE / SOCKET_SnapW
    Stairs / Ramps:  SOCKET_SnapBottom / SOCKET_SnapTop
    Pillar:          SOCKET_SnapBottom / SOCKET_SnapTop

Grid constants (Blender meters; SCALE=100 → 1 UU = 1 cm):
    GRID    = 4.00 m   (wall width / floor side / roof side)
    HEIGHT  = 3.00 m   (full wall height)
    HALF_H  = 1.50 m   (half wall / railing)
    THICK   = 0.20 m   (wall thickness / floor depth)

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
    palette_material,
    get_or_create_material,
    assign_material,
    add_socket,
    add_panel_seam_strip,
    add_rivet_grid,
    finalize_asset,
)

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/walls_structures")

# ── Grid constants ──────────────────────────────────────────────────────────
GRID    = 4.00
HEIGHT  = 3.00
HALF_H  = 1.50
THICK   = 0.20


def _add(obj, mat):
    assign_material(obj, mat)
    return obj


def _slab(x, y, z, w, d, h, mat, name):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z))
    obj = bpy.context.active_object
    obj.scale = (w, d, h); bpy.ops.object.transform_apply(scale=True)
    _add(obj, mat); obj.name = name
    return obj


def _finalize_wall(name, half=False):
    """Walls: bottom-center pivot. Snap sockets at the four cardinal edges
    of the wall plane (THICK along X, GRID along Y, height along Z)."""
    h = HALF_H if half else HEIGHT
    add_socket("SnapLeft",   location=(0, -GRID / 2, h / 2))
    add_socket("SnapRight",  location=(0,  GRID / 2, h / 2))
    add_socket("SnapBottom", location=(0, 0, 0))
    add_socket("SnapTop",    location=(0, 0, h))
    finalize_asset(name,
                    bevel_width=0.001, bevel_angle_deg=30,
                    smooth_angle_deg=75, collision="box",
                    lods=[0.50], pivot="bottom_center")


def _finalize_floor(name):
    """Floors: bottom-CORNER pivot at (0,0,0) so tiles snap edge-to-edge."""
    add_socket("SnapN", location=(GRID / 2, GRID,     0))
    add_socket("SnapS", location=(GRID / 2, 0,        0))
    add_socket("SnapE", location=(GRID,     GRID / 2, 0))
    add_socket("SnapW", location=(0,        GRID / 2, 0))
    finalize_asset(name,
                    bevel_width=0.001, bevel_angle_deg=30,
                    smooth_angle_deg=75, collision="box",
                    lods=[0.50], pivot="bottom_corner")


def _finalize_roof(name):
    """Roofs: bottom-corner pivot. Same snap convention as floors."""
    add_socket("SnapN", location=(GRID / 2, GRID,     0))
    add_socket("SnapS", location=(GRID / 2, 0,        0))
    add_socket("SnapE", location=(GRID,     GRID / 2, 0))
    add_socket("SnapW", location=(0,        GRID / 2, 0))
    finalize_asset(name,
                    bevel_width=0.001, bevel_angle_deg=30,
                    smooth_angle_deg=75, collision="convex",
                    lods=[0.50], pivot="bottom_corner")


# ── Foundations ───────────────────────────────────────────────────────────────

def gen_foundation_square_wood():
    """Square wood foundation: stilt frame with deck planks on top."""
    clear_scene()
    wood_mat = palette_material("Wood")
    dark_wood = palette_material("DarkWood")
    for sy in [-GRID / 2 + 0.2, 0, GRID / 2 - 0.2]:
        _slab(GRID / 2, sy + GRID / 2, 0.20, GRID - 0.1, 0.18, 0.20, dark_wood, "Foundation_Beam")
    for sx, sy in [(0.2, 0.2), (GRID - 0.2, 0.2), (0.2, GRID - 0.2), (GRID - 0.2, GRID - 0.2)]:
        _slab(sx, sy, 0.20, 0.30, 0.30, 0.40, dark_wood, "Foundation_Post")
    for i in range(8):
        x = GRID / 2
        y = GRID / 16 + i * (GRID / 8)
        _slab(x, y, 0.42, GRID - 0.06, GRID / 8 - 0.02, 0.04, wood_mat, f"Foundation_Deck_{i}")
    _finalize_floor("SM_BLD_FOUNDATION_SQUARE_WOOD")


def gen_foundation_square_stone():
    """Square stone foundation: solid masonry block with capstone bevel."""
    clear_scene()
    stone_mat = palette_material("Stone")
    dark_stone = palette_material("DarkStone")
    _slab(GRID / 2, GRID / 2, 0.22, GRID, GRID, 0.44, stone_mat, "Foundation_Block")
    _slab(GRID / 2, GRID / 2, 0.46, GRID - 0.08, GRID - 0.08, 0.04, dark_stone, "Foundation_Cap")
    for i in range(1, 4):
        y = i * (GRID / 4)
        _slab(GRID / 2, y, 0.45, GRID, 0.04, 0.005, dark_stone, f"Foundation_Mortar_{i}")
    _finalize_floor("SM_BLD_FOUNDATION_SQUARE_STONE")


def gen_foundation_triangle_wood():
    """Triangular foundation for 45-degree corner fills (wood)."""
    clear_scene()
    wood_mat = palette_material("Wood")
    dark_wood = palette_material("DarkWood")
    bpy.ops.mesh.primitive_cylinder_add(radius=GRID / math.sqrt(2), depth=0.40,
                                         vertices=3, location=(0, 0, 0.20))
    tri = bpy.context.active_object
    tri.rotation_euler.z = math.pi / 2
    _add(tri, wood_mat)
    bpy.ops.mesh.primitive_cylinder_add(radius=GRID / math.sqrt(2) - 0.04, depth=0.04,
                                         vertices=3, location=(0, 0, 0.42))
    band = bpy.context.active_object
    band.rotation_euler.z = math.pi / 2
    _add(band, dark_wood)
    add_socket("SnapBottom", location=(0, 0, 0))
    finalize_asset("SM_BLD_FOUNDATION_TRIANGLE_WOOD",
                    bevel_width=0.001, bevel_angle_deg=30,
                    smooth_angle_deg=75, collision="convex",
                    lods=[0.50], pivot="bottom_center")


# ── Floors ────────────────────────────────────────────────────────────────────

def gen_floor_wood():
    """Thin wood floor tile — plank striping visible from above."""
    clear_scene()
    wood_mat = palette_material("Wood")
    dark_wood = palette_material("DarkWood")
    for sx in [GRID / 4, GRID * 3 / 4]:
        _slab(sx, GRID / 2, -0.08, 0.10, GRID, 0.10, dark_wood, "Floor_Underbeam")
    for i in range(6):
        y = GRID / 12 + i * (GRID / 6)
        _slab(GRID / 2, y, 0.04, GRID, GRID / 6 - 0.015, 0.08, wood_mat, f"Floor_Plank_{i}")
    _finalize_floor("SM_BLD_FLOOR_WOOD")


def gen_floor_stone():
    """Stone tile floor — 4 quadrant flagstones with mortar gaps."""
    clear_scene()
    stone_mat = palette_material("Stone")
    dark_stone = palette_material("DarkStone")
    half = GRID / 2
    for ix in range(2):
        for iy in range(2):
            cx = (ix + 0.5) * half
            cy = (iy + 0.5) * half
            _slab(cx, cy, 0.05, half - 0.04, half - 0.04, 0.10, stone_mat, f"Floor_Stone_{ix}_{iy}")
    _slab(GRID / 2, GRID / 2, 0.005, GRID, 0.04, 0.005, dark_stone, "Floor_Mortar_X")
    _slab(GRID / 2, GRID / 2, 0.005, 0.04, GRID, 0.005, dark_stone, "Floor_Mortar_Y")
    _finalize_floor("SM_BLD_FLOOR_STONE")


# ── Walls ─────────────────────────────────────────────────────────────────────

def gen_wall_wood():
    """Solid wood wall — 4m wide, 3m tall, plank-striped."""
    clear_scene()
    wood_mat = palette_material("Wood")
    dark_wood = palette_material("DarkWood")
    _slab(0, 0, HEIGHT / 2, THICK, GRID, HEIGHT, wood_mat, "Wall_Body")
    for i in range(1, 6):
        y = -GRID / 2 + i * (GRID / 6)
        _slab(0.011, y, HEIGHT / 2, 0.005, 0.02, HEIGHT, dark_wood, f"Wall_Seam_{i}")
    _slab(0, 0, HEIGHT - 0.06, THICK + 0.04, GRID, 0.10, dark_wood, "Wall_CapRail")
    _finalize_wall("SM_BLD_WALL_WOOD")


def gen_wall_stone():
    """Solid stone wall — 4m wide, 3m tall, masonry coursing visible."""
    clear_scene()
    stone_mat = palette_material("Stone")
    dark_stone = palette_material("DarkStone")
    _slab(0, 0, HEIGHT / 2, THICK, GRID, HEIGHT, stone_mat, "Wall_Body")
    for i in range(1, 6):
        z = i * (HEIGHT / 6)
        _slab(0.011, 0, z, 0.005, GRID, 0.02, dark_stone, f"Wall_Course_{i}")
    _slab(0, 0, HEIGHT - 0.06, THICK + 0.06, GRID + 0.06, 0.10, dark_stone, "Wall_Cap")
    _finalize_wall("SM_BLD_WALL_STONE")


def gen_wall_doorway():
    """Wall with a centered doorway cutout (no door slab — see Door pieces)."""
    clear_scene()
    wood_mat = palette_material("Wood")
    dark_wood = palette_material("DarkWood")
    door_w = 1.20
    door_h = 2.20
    side_w = (GRID - door_w) / 2
    _slab(0, -GRID / 2 + side_w / 2, HEIGHT / 2, THICK, side_w, HEIGHT, wood_mat, "Doorway_Left")
    _slab(0, GRID / 2 - side_w / 2, HEIGHT / 2, THICK, side_w, HEIGHT, wood_mat, "Doorway_Right")
    _slab(0, 0, door_h + (HEIGHT - door_h) / 2, THICK, door_w, HEIGHT - door_h, wood_mat, "Doorway_Header")
    _slab(0.011, -door_w / 2, door_h / 2, 0.005, 0.06, door_h, dark_wood, "Doorway_FrameL")
    _slab(0.011, door_w / 2, door_h / 2, 0.005, 0.06, door_h, dark_wood, "Doorway_FrameR")
    _slab(0.011, 0, door_h, 0.005, door_w, 0.06, dark_wood, "Doorway_FrameTop")
    _finalize_wall("SM_BLD_WALL_DOORWAY")


def gen_wall_window():
    """Wall with a centered window cutout — sill, header, two side jambs, glass."""
    clear_scene()
    wood_mat = palette_material("Wood")
    dark_wood = palette_material("DarkWood")
    glass_mat = palette_material("Glass")
    win_w = 1.40
    win_h = 1.20
    win_z = 1.00
    _slab(0, 0, win_z / 2, THICK, GRID, win_z, wood_mat, "Window_Below")
    top_h = HEIGHT - (win_z + win_h)
    _slab(0, 0, win_z + win_h + top_h / 2, THICK, GRID, top_h, wood_mat, "Window_Above")
    _slab(0, -GRID / 2 + (GRID - win_w) / 4, win_z + win_h / 2, THICK, (GRID - win_w) / 2, win_h, wood_mat, "Window_JambL")
    _slab(0, GRID / 2 - (GRID - win_w) / 4, win_z + win_h / 2, THICK, (GRID - win_w) / 2, win_h, wood_mat, "Window_JambR")
    _slab(0, 0, win_z + win_h / 2, THICK * 0.2, win_w - 0.06, win_h - 0.06, glass_mat, "Window_Glass")
    _slab(0.011, -win_w / 2, win_z + win_h / 2, 0.005, 0.04, win_h, dark_wood, "Window_FrameL")
    _slab(0.011, win_w / 2, win_z + win_h / 2, 0.005, 0.04, win_h, dark_wood, "Window_FrameR")
    _slab(0.011, 0, win_z, 0.005, win_w, 0.04, dark_wood, "Window_Sill")
    _slab(0.011, 0, win_z + win_h, 0.005, win_w, 0.04, dark_wood, "Window_Header")
    _slab(0.012, 0, win_z + win_h / 2, 0.004, win_w - 0.06, 0.018, dark_wood, "Window_MuntinH")
    _slab(0.012, 0, win_z + win_h / 2, 0.004, 0.018, win_h - 0.06, dark_wood, "Window_MuntinV")
    _finalize_wall("SM_BLD_WALL_WINDOW")


def gen_wall_half():
    """Half-height wall (railing height) — 4m wide, 1.5m tall."""
    clear_scene()
    wood_mat = palette_material("Wood")
    dark_wood = palette_material("DarkWood")
    _slab(0, 0, HALF_H / 2, THICK, GRID, HALF_H, wood_mat, "HalfWall_Body")
    _slab(0, 0, HALF_H - 0.04, THICK + 0.04, GRID + 0.04, 0.08, dark_wood, "HalfWall_Cap")
    for i in range(1, 5):
        y = -GRID / 2 + i * (GRID / 5)
        _slab(0.011, y, HALF_H / 2, 0.005, 0.02, HALF_H, dark_wood, f"HalfWall_Seam_{i}")
    _finalize_wall("SM_BLD_WALL_HALF", half=True)


def gen_wall_triangle():
    """Triangular gable infill that sits on top of a 4m wall to close a pitched roof."""
    clear_scene()
    wood_mat = palette_material("Wood")
    dark_wood = palette_material("DarkWood")
    bpy.ops.mesh.primitive_cone_add(radius1=GRID / 2, radius2=0.0, depth=THICK,
                                    vertices=3, location=(0, 0, GRID / 4))
    tri = bpy.context.active_object
    tri.rotation_euler = (0, math.pi / 2, math.pi / 2)
    _add(tri, wood_mat)
    for sy in [-1, 1]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0.011, sy * GRID / 4, GRID / 8))
        edge = bpy.context.active_object
        edge_len = math.sqrt((GRID / 2) ** 2 + (GRID / 4) ** 2)
        edge.scale = (0.005, edge_len * 0.95, 0.06)
        edge.rotation_euler.x = sy * math.atan2(GRID / 4, GRID / 2)
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(edge, dark_wood)
    add_socket("SnapBottom", location=(0, 0, 0))
    add_socket("SnapApex",   location=(0, 0, GRID / 2))
    finalize_asset("SM_BLD_WALL_TRIANGLE",
                    bevel_width=0.001, bevel_angle_deg=30,
                    smooth_angle_deg=75, collision="convex",
                    lods=[0.50], pivot="bottom_center")


# ── Doors ─────────────────────────────────────────────────────────────────────

def gen_door_wood():
    """Wood door slab sized to fit a doorway cutout (1.18m wide x 2.18m tall)."""
    clear_scene()
    wood_mat = palette_material("Wood")
    dark_wood = palette_material("DarkWood")
    dark_metal = palette_material("DarkSteel")
    door_w = 1.18
    door_h = 2.18
    _slab(0, 0, door_h / 2, 0.06, door_w, door_h, wood_mat, "Door_Body")
    for i in range(1, 4):
        y = -door_w / 2 + i * (door_w / 4)
        _slab(0.032, y, door_h / 2, 0.005, 0.02, door_h, dark_wood, f"Door_Seam_{i}")
    _slab(0.032, 0, door_h * 0.30, 0.005, door_w - 0.04, 0.06, dark_wood, "Door_Brace_Lo")
    _slab(0.032, 0, door_h * 0.70, 0.005, door_w - 0.04, 0.06, dark_wood, "Door_Brace_Hi")
    for z in [door_h * 0.20, door_h * 0.80]:
        _slab(0.034, -door_w / 2 + 0.04, z, 0.006, 0.10, 0.05, dark_metal, "Door_Hinge")
    bpy.ops.mesh.primitive_torus_add(major_radius=0.05, minor_radius=0.012, location=(0.04, door_w / 2 - 0.10, door_h / 2))
    handle = bpy.context.active_object
    handle.rotation_euler.y = math.pi / 2
    _add(handle, dark_metal)
    add_socket("HingePivot",  location=(0, -door_w / 2 + 0.04, door_h / 2))
    add_socket("LatchPoint",  location=(0.04, door_w / 2 - 0.10, door_h / 2))
    finalize_asset("SM_BLD_DOOR_WOOD",
                    bevel_width=0.001, bevel_angle_deg=30,
                    smooth_angle_deg=75, collision="box",
                    lods=[0.50], pivot="bottom_center")


def gen_door_reinforced():
    """Reinforced steel-banded door — heavier, with strap plates."""
    clear_scene()
    dark_wood = palette_material("DarkWood")
    metal_mat = palette_material("Steel")
    dark_metal = palette_material("DarkSteel")
    door_w = 1.18
    door_h = 2.18
    _slab(0, 0, door_h / 2, 0.08, door_w, door_h, dark_wood, "RDoor_Body")
    for z in [door_h * 0.15, door_h * 0.85]:
        _slab(0.042, 0, z, 0.005, door_w - 0.04, 0.16, metal_mat, "RDoor_Strap")
        for sy in [-door_w / 3, 0, door_w / 3]:
            bpy.ops.mesh.primitive_cylinder_add(radius=0.014, depth=0.010, location=(0.046, sy, z))
            bolt = bpy.context.active_object
            bolt.rotation_euler.y = math.pi / 2
            _add(bolt, dark_metal)
    _slab(0.044, 0, door_h / 2, 0.005, 0.50, 0.50, metal_mat, "RDoor_DiamondPlate")
    for z in [door_h * 0.18, door_h * 0.82]:
        _slab(0.046, -door_w / 2 + 0.06, z, 0.008, 0.14, 0.08, dark_metal, "RDoor_Hinge")
    _slab(0.05, door_w / 2 - 0.16, door_h / 2, 0.012, 0.18, 0.025, dark_metal, "RDoor_Lever")
    add_socket("HingePivot",  location=(0, -door_w / 2 + 0.06, door_h / 2))
    add_socket("LatchPoint",  location=(0.05, door_w / 2 - 0.16, door_h / 2))
    finalize_asset("SM_BLD_DOOR_REINFORCED",
                    bevel_width=0.001, bevel_angle_deg=30,
                    smooth_angle_deg=75, collision="box",
                    lods=[0.50], pivot="bottom_center")


# ── Roofs ─────────────────────────────────────────────────────────────────────

def gen_roof_flat():
    """Flat 4m x 4m roof tile — metal sheeting with seam lines."""
    clear_scene()
    metal_mat = palette_material("Steel")
    dark_metal = palette_material("DarkSteel")
    _slab(GRID / 2, GRID / 2, 0.05, GRID, GRID, 0.10, metal_mat, "Roof_Flat_Body")
    for i in range(1, 4):
        x = i * (GRID / 4)
        _slab(x, GRID / 2, 0.105, 0.06, GRID, 0.012, dark_metal, f"Roof_Flat_Seam_{i}")
    _slab(GRID / 2, 0.04, 0.10, GRID, 0.08, 0.06, dark_metal, "Roof_Flat_Gutter")
    _finalize_roof("SM_BLD_ROOF_FLAT")


def gen_roof_pitched():
    """Pitched roof tile — triangular prism, 30deg pitch, 4m x 4m footprint."""
    clear_scene()
    thatch_mat = get_or_create_material("Building_Thatch", (0.65, 0.55, 0.30, 1.0), roughness=0.95)
    dark_wood = palette_material("DarkWood")
    pitch_h = 1.20
    angle = math.atan2(pitch_h, GRID / 2)
    panel_len = math.sqrt((GRID / 2) ** 2 + pitch_h ** 2)
    for sy in [-1, 1]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(GRID / 2,
                                                            GRID / 2 + sy * GRID / 4,
                                                            pitch_h / 2 + 0.05))
        panel = bpy.context.active_object
        panel.scale = (GRID, panel_len, 0.08)
        panel.rotation_euler.x = sy * -angle
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(panel, thatch_mat)
    _slab(GRID / 2, GRID / 2, pitch_h + 0.10, GRID, 0.20, 0.10, dark_wood, "Roof_Pitch_Ridge")
    for sy in [-1, 1]:
        _slab(GRID / 2, GRID / 2 + sy * GRID / 2, 0.06, GRID, 0.06, 0.16, dark_wood, f"Roof_Pitch_Fascia_{sy}")
    _finalize_roof("SM_BLD_ROOF_PITCHED")


def gen_roof_ridge():
    """Standalone ridge cap piece — 4m long capping strip for joining pitched roofs."""
    clear_scene()
    dark_metal = palette_material("DarkSteel")
    metal_mat = palette_material("Steel")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.16, depth=GRID, vertices=12, location=(GRID / 2, GRID / 2, 0.16))
    body = bpy.context.active_object
    body.rotation_euler.y = math.pi / 2
    _add(body, dark_metal)
    for x_end in [0.005, GRID - 0.005]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.18, depth=0.01, vertices=12, location=(x_end, GRID / 2, 0.16))
        cap = bpy.context.active_object
        cap.rotation_euler.y = math.pi / 2
        _add(cap, metal_mat)
    add_socket("SnapW", location=(0,    GRID / 2, 0.16))
    add_socket("SnapE", location=(GRID, GRID / 2, 0.16))
    finalize_asset("SM_BLD_ROOF_RIDGE",
                    bevel_width=0.001, bevel_angle_deg=30,
                    smooth_angle_deg=75, collision="convex",
                    lods=[0.50], pivot="bottom_corner")


# ── Structural ────────────────────────────────────────────────────────────────

def gen_pillar():
    """Single 3m vertical column with carved bands top + bottom."""
    clear_scene()
    stone_mat = palette_material("Stone")
    dark_stone = palette_material("DarkStone")
    bpy.ops.mesh.primitive_cylinder_add(radius=0.18, depth=HEIGHT, vertices=12, location=(0, 0, HEIGHT / 2))
    _add(bpy.context.active_object, stone_mat)
    _slab(0, 0, 0.10, 0.50, 0.50, 0.20, dark_stone, "Pillar_Base")
    _slab(0, 0, HEIGHT - 0.10, 0.50, 0.50, 0.20, dark_stone, "Pillar_Capital")
    for z in [0.30, HEIGHT - 0.30]:
        bpy.ops.mesh.primitive_torus_add(major_radius=0.20, minor_radius=0.02, location=(0, 0, z))
        _add(bpy.context.active_object, dark_stone)
    add_socket("SnapBottom", location=(0, 0, 0))
    add_socket("SnapTop",    location=(0, 0, HEIGHT))
    finalize_asset("SM_BLD_PILLAR",
                    bevel_width=0.001, bevel_angle_deg=30,
                    smooth_angle_deg=75, collision="convex",
                    lods=[0.50], pivot="bottom_center")


def gen_stairs():
    """4m straight stair piece — 8 steps rising 3m."""
    clear_scene()
    wood_mat = palette_material("Wood")
    dark_wood = palette_material("DarkWood")
    steps = 8
    rise = HEIGHT / steps
    run = GRID / steps
    for i in range(steps):
        x = (i + 0.5) * run
        z = (i + 0.5) * rise
        _slab(x, GRID / 2, z, run, GRID, rise, wood_mat, f"Stair_Tread_{i}")
    for sy in [0.05, GRID - 0.05]:
        _slab(GRID / 2, sy, HEIGHT / 2, GRID, 0.10, HEIGHT, dark_wood, "Stair_Stringer")
    add_socket("SnapBottom", location=(0,    GRID / 2, 0))
    add_socket("SnapTop",    location=(GRID, GRID / 2, HEIGHT))
    finalize_asset("SM_BLD_STAIRS",
                    bevel_width=0.001, bevel_angle_deg=30,
                    smooth_angle_deg=75, collision="convex",
                    lods=[0.50], pivot="bottom_corner")


def gen_ramp():
    """4m straight ramp piece rising 3m — wedge with side walls."""
    clear_scene()
    wood_mat = palette_material("Wood")
    dark_wood = palette_material("DarkWood")
    bpy.ops.mesh.primitive_cone_add(radius1=GRID / 2, radius2=0.0, depth=GRID,
                                    vertices=3, location=(GRID / 2, GRID / 2, HEIGHT / 2))
    wedge = bpy.context.active_object
    wedge.scale = (1.0, 1.0, HEIGHT / GRID)
    bpy.ops.object.transform_apply(scale=True)
    _add(wedge, wood_mat)
    for sy in [0.04, GRID - 0.04]:
        bpy.ops.mesh.primitive_cone_add(radius1=GRID / 2, radius2=0.0, depth=0.08,
                                        vertices=3, location=(GRID / 2, sy, HEIGHT / 2))
        flank = bpy.context.active_object
        flank.rotation_euler = (math.pi / 2, 0, 0)
        flank.scale = (1.0, HEIGHT / GRID, 1.0)
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        _add(flank, dark_wood)
    add_socket("SnapBottom", location=(0,    GRID / 2, 0))
    add_socket("SnapTop",    location=(GRID, GRID / 2, HEIGHT))
    finalize_asset("SM_BLD_RAMP",
                    bevel_width=0.001, bevel_angle_deg=30,
                    smooth_angle_deg=75, collision="convex",
                    lods=[0.50], pivot="bottom_corner")


# ── Dispatch ──────────────────────────────────────────────────────────────────

GENERATORS = {
    "BLD_FOUNDATION_SQUARE_WOOD":   gen_foundation_square_wood,
    "BLD_FOUNDATION_SQUARE_STONE":  gen_foundation_square_stone,
    "BLD_FOUNDATION_TRIANGLE_WOOD": gen_foundation_triangle_wood,
    "BLD_FLOOR_WOOD":               gen_floor_wood,
    "BLD_FLOOR_STONE":              gen_floor_stone,
    "BLD_WALL_WOOD":                gen_wall_wood,
    "BLD_WALL_STONE":               gen_wall_stone,
    "BLD_WALL_DOORWAY":             gen_wall_doorway,
    "BLD_WALL_WINDOW":              gen_wall_window,
    "BLD_WALL_HALF":                gen_wall_half,
    "BLD_WALL_TRIANGLE":            gen_wall_triangle,
    "BLD_DOOR_WOOD":                gen_door_wood,
    "BLD_DOOR_REINFORCED":          gen_door_reinforced,
    "BLD_ROOF_FLAT":                gen_roof_flat,
    "BLD_ROOF_PITCHED":             gen_roof_pitched,
    "BLD_ROOF_RIDGE":               gen_roof_ridge,
    "BLD_PILLAR":                   gen_pillar,
    "BLD_STAIRS":                   gen_stairs,
    "BLD_RAMP":                     gen_ramp,
}


def main():
    print("\n=== Quiet Rift: Enigma — Walls & Structures Asset Generator (Batch 7 detail upgrade) ===")
    print(f"Grid: {GRID}m  Wall height: {HEIGHT}m  Half wall: {HALF_H}m  Thickness: {THICK}m")
    for asset_id, gen in GENERATORS.items():
        print(f"\n[{asset_id}]")
        gen()
        out_path = os.path.join(OUTPUT_DIR, f"SM_{asset_id}.fbx")
        export_fbx(asset_id, out_path)
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Each FBX exports SM_<id> + UCX_ collision + LOD0.50 + cardinal SOCKET_Snap*")
    print("empties; doors expose SOCKET_HingePivot and SOCKET_LatchPoint.")
    print("Tiny 1mm bevels and 75 deg auto-smooth keep grid-tile alignment exact.")


if __name__ == "__main__":
    main()
