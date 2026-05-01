"""
Quiet Rift: Enigma — Walls & Structures Asset Generator (Blender 4.x)

Run this in Blender's Scripting workspace or via:
    blender --background --python qr_generate_walls_structures.py

Generates the modular building-piece kit (foundations, floors, walls,
doors, doorways, windows, roofs, pillars, stairs, ramps) using the
helpers in qr_blender_common.py. Sized for UE5 import (1 UU = 1 cm)
on a 4m grid:

    GRID    = 400 cm   (wall width / floor side / roof side)
    HEIGHT  =  300 cm   (full wall height)
    HALF_H  =  150 cm   (half wall / railing)
    THICK   =   20 cm   (wall thickness / floor depth)

Origin convention:
    Walls:        origin at bottom-center, length runs along +Y
    Floors/roofs: origin at bottom corner (0,0,0)
    Doors:        origin at bottom-center, hinge edge at -Y

Two material variants per piece where applicable: wood (early game)
and stone/composite (mid game). Roofs use thatch (wood tier) or metal
(composite tier).

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

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "../../Content/Meshes/walls_structures")

# ── Grid constants (in Blender units; 1 BU = 1 m at SCALE=100 → 1 UU = 1 cm) ─
GRID    = 4.00
HEIGHT  = 3.00
HALF_H  = 1.50
THICK   = 0.20

# Material palettes
WOOD       = (0.50, 0.32, 0.18, 1.0)
DARK_WOOD  = (0.30, 0.18, 0.10, 1.0)
STONE      = (0.55, 0.55, 0.52, 1.0)
DARK_STONE = (0.35, 0.35, 0.32, 1.0)
THATCH     = (0.65, 0.55, 0.30, 1.0)
METAL      = (0.40, 0.42, 0.44, 1.0)
DARK_METAL = (0.22, 0.24, 0.26, 1.0)
GLASS      = (0.55, 0.70, 0.80, 0.4)


# ── Reusable shape primitives ────────────────────────────────────────────────

def _slab(x, y, z, w, d, h, color, name):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z))
    obj = bpy.context.active_object
    obj.scale = (w, d, h)
    bpy.ops.object.transform_apply(scale=True)
    add_material(obj, color, name)
    return obj


def _plank_rows(cx, cy, cz, w, d, h, color, name, rows=4, gap=0.015):
    """Visual plank striping — multiple thin slabs stacked along Z."""
    plank_h = (h - gap * (rows - 1)) / rows
    for i in range(rows):
        z = cz - h / 2 + plank_h / 2 + i * (plank_h + gap)
        _slab(cx, cy, z, w, d, plank_h, color, f"{name}_Plank_{i}")


# ── Foundations ───────────────────────────────────────────────────────────────

def gen_foundation_square_wood():
    """Square wood foundation: stilt frame with deck planks on top."""
    clear_scene()
    # Cross beams (under-deck frame)
    for sy in [-GRID / 2 + 0.2, 0, GRID / 2 - 0.2]:
        _slab(GRID / 2, sy + GRID / 2, 0.20, GRID - 0.1, 0.18, 0.20, DARK_WOOD, "Foundation_Beam")
    # Stilt posts at corners
    for sx, sy in [(0.2, 0.2), (GRID - 0.2, 0.2), (0.2, GRID - 0.2), (GRID - 0.2, GRID - 0.2)]:
        _slab(sx, sy, 0.20, 0.30, 0.30, 0.40, DARK_WOOD, "Foundation_Post")
    # Deck (planks on top)
    for i in range(8):
        x = GRID / 2
        y = GRID / 16 + i * (GRID / 8)
        _slab(x, y, 0.42, GRID - 0.06, GRID / 8 - 0.02, 0.04, WOOD, f"Foundation_Deck_{i}")
    join_and_rename("SM_BLD_FOUNDATION_SQUARE_WOOD")


def gen_foundation_square_stone():
    """Square stone foundation: solid masonry block with capstone bevel."""
    clear_scene()
    # Main block
    _slab(GRID / 2, GRID / 2, 0.22, GRID, GRID, 0.44, STONE, "Foundation_Block")
    # Capstone bevel ring
    _slab(GRID / 2, GRID / 2, 0.46, GRID - 0.08, GRID - 0.08, 0.04, DARK_STONE, "Foundation_Cap")
    # Visible mortar lines (cross slabs at low alpha-equivalent darker color)
    for i in range(1, 4):
        y = i * (GRID / 4)
        _slab(GRID / 2, y, 0.45, GRID, 0.04, 0.005, DARK_STONE, f"Foundation_Mortar_{i}")
    join_and_rename("SM_BLD_FOUNDATION_SQUARE_STONE")


def gen_foundation_triangle_wood():
    """Triangular foundation for 45-degree corner fills (wood)."""
    clear_scene()
    bpy.ops.mesh.primitive_cylinder_add(radius=GRID / math.sqrt(2), depth=0.40,
                                        vertices=3, location=(0, 0, 0.20))
    tri = bpy.context.active_object
    tri.rotation_euler.z = math.pi / 2
    add_material(tri, WOOD, "TriFoundation_Body_Mat")
    # Edge band
    bpy.ops.mesh.primitive_cylinder_add(radius=GRID / math.sqrt(2) - 0.04, depth=0.04,
                                        vertices=3, location=(0, 0, 0.42))
    band = bpy.context.active_object
    band.rotation_euler.z = math.pi / 2
    add_material(band, DARK_WOOD, "TriFoundation_Band_Mat")
    join_and_rename("SM_BLD_FOUNDATION_TRIANGLE_WOOD")


# ── Floors ────────────────────────────────────────────────────────────────────

def gen_floor_wood():
    """Thin wood floor tile — plank striping visible from above."""
    clear_scene()
    # Underbeams (cross supports below)
    for sx in [GRID / 4, GRID * 3 / 4]:
        _slab(sx, GRID / 2, -0.08, 0.10, GRID, 0.10, DARK_WOOD, "Floor_Underbeam")
    # 6 planks across
    for i in range(6):
        y = GRID / 12 + i * (GRID / 6)
        _slab(GRID / 2, y, 0.04, GRID, GRID / 6 - 0.015, 0.08, WOOD, f"Floor_Plank_{i}")
    join_and_rename("SM_BLD_FLOOR_WOOD")


def gen_floor_stone():
    """Stone tile floor — 4 quadrant flagstones with mortar gaps."""
    clear_scene()
    half = GRID / 2
    for ix in range(2):
        for iy in range(2):
            cx = (ix + 0.5) * half
            cy = (iy + 0.5) * half
            _slab(cx, cy, 0.05, half - 0.04, half - 0.04, 0.10, STONE, f"Floor_Stone_{ix}_{iy}")
    # Mortar cross
    _slab(GRID / 2, GRID / 2, 0.005, GRID, 0.04, 0.005, DARK_STONE, "Floor_Mortar_X")
    _slab(GRID / 2, GRID / 2, 0.005, 0.04, GRID, 0.005, DARK_STONE, "Floor_Mortar_Y")
    join_and_rename("SM_BLD_FLOOR_STONE")


# ── Walls ─────────────────────────────────────────────────────────────────────

def gen_wall_wood():
    """Solid wood wall — 4m wide, 3m tall, plank-striped."""
    clear_scene()
    _slab(0, 0, HEIGHT / 2, THICK, GRID, HEIGHT, WOOD, "Wall_Body")
    # Vertical plank seams (faintly darker strips)
    for i in range(1, 6):
        y = -GRID / 2 + i * (GRID / 6)
        _slab(0.011, y, HEIGHT / 2, 0.005, 0.02, HEIGHT, DARK_WOOD, f"Wall_Seam_{i}")
    # Horizontal cap rail
    _slab(0, 0, HEIGHT - 0.06, THICK + 0.04, GRID, 0.10, DARK_WOOD, "Wall_CapRail")
    join_and_rename("SM_BLD_WALL_WOOD")


def gen_wall_stone():
    """Solid stone wall — 4m wide, 3m tall, masonry coursing visible."""
    clear_scene()
    _slab(0, 0, HEIGHT / 2, THICK, GRID, HEIGHT, STONE, "Wall_Body")
    # Horizontal mortar courses
    for i in range(1, 6):
        z = i * (HEIGHT / 6)
        _slab(0.011, 0, z, 0.005, GRID, 0.02, DARK_STONE, f"Wall_Course_{i}")
    # Cap stone
    _slab(0, 0, HEIGHT - 0.06, THICK + 0.06, GRID + 0.06, 0.10, DARK_STONE, "Wall_Cap")
    join_and_rename("SM_BLD_WALL_STONE")


def gen_wall_doorway():
    """Wall with a centered doorway cutout (no door slab — see Door pieces)."""
    clear_scene()
    door_w = 1.20
    door_h = 2.20
    side_w = (GRID - door_w) / 2
    # Two side panels
    _slab(0, -GRID / 2 + side_w / 2, HEIGHT / 2, THICK, side_w, HEIGHT, WOOD, "Doorway_Left")
    _slab(0, GRID / 2 - side_w / 2, HEIGHT / 2, THICK, side_w, HEIGHT, WOOD, "Doorway_Right")
    # Header above the opening
    _slab(0, 0, door_h + (HEIGHT - door_h) / 2, THICK, door_w, HEIGHT - door_h, WOOD, "Doorway_Header")
    # Frame band around opening
    _slab(0.011, -door_w / 2, door_h / 2, 0.005, 0.06, door_h, DARK_WOOD, "Doorway_FrameL")
    _slab(0.011, door_w / 2, door_h / 2, 0.005, 0.06, door_h, DARK_WOOD, "Doorway_FrameR")
    _slab(0.011, 0, door_h, 0.005, door_w, 0.06, DARK_WOOD, "Doorway_FrameTop")
    join_and_rename("SM_BLD_WALL_DOORWAY")


def gen_wall_window():
    """Wall with a centered window cutout — sill, header, two side jambs."""
    clear_scene()
    win_w = 1.40
    win_h = 1.20
    win_z = 1.00   # sill height above floor
    # Bottom under the sill (full width below window)
    _slab(0, 0, win_z / 2, THICK, GRID, win_z, WOOD, "Window_Below")
    # Top above header
    top_h = HEIGHT - (win_z + win_h)
    _slab(0, 0, win_z + win_h + top_h / 2, THICK, GRID, top_h, WOOD, "Window_Above")
    # Left jamb
    _slab(0, -GRID / 2 + (GRID - win_w) / 4, win_z + win_h / 2, THICK, (GRID - win_w) / 2, win_h, WOOD, "Window_JambL")
    # Right jamb
    _slab(0, GRID / 2 - (GRID - win_w) / 4, win_z + win_h / 2, THICK, (GRID - win_w) / 2, win_h, WOOD, "Window_JambR")
    # Glass pane
    _slab(0, 0, win_z + win_h / 2, THICK * 0.2, win_w - 0.06, win_h - 0.06, GLASS, "Window_Glass")
    # Frame band
    _slab(0.011, -win_w / 2, win_z + win_h / 2, 0.005, 0.04, win_h, DARK_WOOD, "Window_FrameL")
    _slab(0.011, win_w / 2, win_z + win_h / 2, 0.005, 0.04, win_h, DARK_WOOD, "Window_FrameR")
    _slab(0.011, 0, win_z, 0.005, win_w, 0.04, DARK_WOOD, "Window_Sill")
    _slab(0.011, 0, win_z + win_h, 0.005, win_w, 0.04, DARK_WOOD, "Window_Header")
    # Cross muntin
    _slab(0.012, 0, win_z + win_h / 2, 0.004, win_w - 0.06, 0.018, DARK_WOOD, "Window_MuntinH")
    _slab(0.012, 0, win_z + win_h / 2, 0.004, 0.018, win_h - 0.06, DARK_WOOD, "Window_MuntinV")
    join_and_rename("SM_BLD_WALL_WINDOW")


def gen_wall_half():
    """Half-height wall (railing height) — 4m wide, 1.5m tall."""
    clear_scene()
    _slab(0, 0, HALF_H / 2, THICK, GRID, HALF_H, WOOD, "HalfWall_Body")
    # Cap rail
    _slab(0, 0, HALF_H - 0.04, THICK + 0.04, GRID + 0.04, 0.08, DARK_WOOD, "HalfWall_Cap")
    # Vertical seams
    for i in range(1, 5):
        y = -GRID / 2 + i * (GRID / 5)
        _slab(0.011, y, HALF_H / 2, 0.005, 0.02, HALF_H, DARK_WOOD, f"HalfWall_Seam_{i}")
    join_and_rename("SM_BLD_WALL_HALF")


def gen_wall_triangle():
    """Triangular gable infill that sits on top of a 4m wall to close a pitched roof."""
    clear_scene()
    # Triangular prism using a cone with 3 verts oriented sideways
    bpy.ops.mesh.primitive_cone_add(radius1=GRID / 2, radius2=0.0, depth=THICK,
                                    vertices=3, location=(0, 0, GRID / 4))
    tri = bpy.context.active_object
    # Cone points along +Z by default with 3 verts forming a triangle in XY.
    # Rotate so the triangular face faces +X (the wall plane).
    tri.rotation_euler = (0, math.pi / 2, math.pi / 2)
    add_material(tri, WOOD, "WallTri_Body_Mat")
    # Edge bands (top edges of triangle)
    for sy in [-1, 1]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0.011, sy * GRID / 4, GRID / 8))
        edge = bpy.context.active_object
        edge_len = math.sqrt((GRID / 2) ** 2 + (GRID / 4) ** 2)
        edge.scale = (0.005, edge_len * 0.95, 0.06)
        edge.rotation_euler.x = sy * math.atan2(GRID / 4, GRID / 2)
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        add_material(edge, DARK_WOOD, f"WallTri_Edge_{sy}_Mat")
    join_and_rename("SM_BLD_WALL_TRIANGLE")


# ── Doors ─────────────────────────────────────────────────────────────────────

def gen_door_wood():
    """Wood door slab sized to fit a doorway cutout (1.18m wide x 2.18m tall)."""
    clear_scene()
    door_w = 1.18
    door_h = 2.18
    # Slab body
    _slab(0, 0, door_h / 2, 0.06, door_w, door_h, WOOD, "Door_Body")
    # Plank seams
    for i in range(1, 4):
        y = -door_w / 2 + i * (door_w / 4)
        _slab(0.032, y, door_h / 2, 0.005, 0.02, door_h, DARK_WOOD, f"Door_Seam_{i}")
    # Cross brace
    _slab(0.032, 0, door_h * 0.30, 0.005, door_w - 0.04, 0.06, DARK_WOOD, "Door_Brace_Lo")
    _slab(0.032, 0, door_h * 0.70, 0.005, door_w - 0.04, 0.06, DARK_WOOD, "Door_Brace_Hi")
    # Iron hinges
    for z in [door_h * 0.20, door_h * 0.80]:
        _slab(0.034, -door_w / 2 + 0.04, z, 0.006, 0.10, 0.05, DARK_METAL, "Door_Hinge")
    # Handle
    bpy.ops.mesh.primitive_torus_add(major_radius=0.05, minor_radius=0.012,
                                     location=(0.04, door_w / 2 - 0.10, door_h / 2))
    handle = bpy.context.active_object
    handle.rotation_euler.y = math.pi / 2
    add_material(handle, DARK_METAL, "Door_Handle_Mat")
    join_and_rename("SM_BLD_DOOR_WOOD")


def gen_door_reinforced():
    """Reinforced steel-banded door — heavier, with strap plates."""
    clear_scene()
    door_w = 1.18
    door_h = 2.18
    _slab(0, 0, door_h / 2, 0.08, door_w, door_h, DARK_WOOD, "RDoor_Body")
    # Steel strap plates (top + bottom)
    for z in [door_h * 0.15, door_h * 0.85]:
        _slab(0.042, 0, z, 0.005, door_w - 0.04, 0.16, METAL, "RDoor_Strap")
        # Visible bolt heads on strap
        for sy in [-door_w / 3, 0, door_w / 3]:
            bpy.ops.mesh.primitive_cylinder_add(radius=0.014, depth=0.010, location=(0.046, sy, z))
            bolt = bpy.context.active_object
            bolt.rotation_euler.y = math.pi / 2
            add_material(bolt, DARK_METAL, "RDoor_Bolt_Mat")
    # Center diamond plate
    _slab(0.044, 0, door_h / 2, 0.005, 0.50, 0.50, METAL, "RDoor_DiamondPlate")
    # Heavy hinges
    for z in [door_h * 0.18, door_h * 0.82]:
        _slab(0.046, -door_w / 2 + 0.06, z, 0.008, 0.14, 0.08, DARK_METAL, "RDoor_Hinge")
    # Handle (lever style)
    _slab(0.05, door_w / 2 - 0.16, door_h / 2, 0.012, 0.18, 0.025, DARK_METAL, "RDoor_Lever")
    join_and_rename("SM_BLD_DOOR_REINFORCED")


# ── Roofs ─────────────────────────────────────────────────────────────────────

def gen_roof_flat():
    """Flat 4m x 4m roof tile — metal sheeting with seam lines."""
    clear_scene()
    _slab(GRID / 2, GRID / 2, 0.05, GRID, GRID, 0.10, METAL, "Roof_Flat_Body")
    # Seam strips
    for i in range(1, 4):
        x = i * (GRID / 4)
        _slab(x, GRID / 2, 0.105, 0.06, GRID, 0.012, DARK_METAL, f"Roof_Flat_Seam_{i}")
    # Edge gutter
    _slab(GRID / 2, 0.04, 0.10, GRID, 0.08, 0.06, DARK_METAL, "Roof_Flat_Gutter")
    join_and_rename("SM_BLD_ROOF_FLAT")


def gen_roof_pitched():
    """Pitched roof tile — triangular prism, 30deg pitch, 4m x 4m footprint."""
    clear_scene()
    pitch_h = 1.20  # rise at apex
    # Build using a prism approximated by two angled slabs meeting at the ridge
    angle = math.atan2(pitch_h, GRID / 2)
    panel_len = math.sqrt((GRID / 2) ** 2 + pitch_h ** 2)
    for sy in [-1, 1]:
        bpy.ops.mesh.primitive_cube_add(size=1,
                                        location=(GRID / 2, GRID / 2 + sy * GRID / 4,
                                                  pitch_h / 2 + 0.05))
        panel = bpy.context.active_object
        panel.scale = (GRID, panel_len, 0.08)
        panel.rotation_euler.x = sy * -angle
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        add_material(panel, THATCH, f"Roof_Pitch_Panel_{sy}_Mat")
    # Ridge cap
    _slab(GRID / 2, GRID / 2, pitch_h + 0.10, GRID, 0.20, 0.10, DARK_WOOD, "Roof_Pitch_Ridge")
    # Eave fascia
    for sy in [-1, 1]:
        _slab(GRID / 2, GRID / 2 + sy * GRID / 2, 0.06, GRID, 0.06, 0.16, DARK_WOOD, f"Roof_Pitch_Fascia_{sy}")
    join_and_rename("SM_BLD_ROOF_PITCHED")


def gen_roof_ridge():
    """Standalone ridge cap piece — 4m long capping strip for joining pitched roofs."""
    clear_scene()
    # Ridge body
    bpy.ops.mesh.primitive_cylinder_add(radius=0.16, depth=GRID, vertices=12, location=(GRID / 2, GRID / 2, 0.16))
    body = bpy.context.active_object
    body.rotation_euler.y = math.pi / 2
    add_material(body, DARK_METAL, "Ridge_Body_Mat")
    # End caps
    for x_end in [GRID / 2 - GRID / 2 + 0.005, GRID / 2 + GRID / 2 - 0.005]:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.18, depth=0.01, vertices=12, location=(x_end, GRID / 2, 0.16))
        cap = bpy.context.active_object
        cap.rotation_euler.y = math.pi / 2
        add_material(cap, METAL, "Ridge_EndCap_Mat")
    join_and_rename("SM_BLD_ROOF_RIDGE")


# ── Structural ────────────────────────────────────────────────────────────────

def gen_pillar():
    """Single 3m vertical column with carved bands top + bottom."""
    clear_scene()
    bpy.ops.mesh.primitive_cylinder_add(radius=0.18, depth=HEIGHT, vertices=12, location=(0, 0, HEIGHT / 2))
    add_material(bpy.context.active_object, STONE, "Pillar_Shaft_Mat")
    # Base block
    _slab(0, 0, 0.10, 0.50, 0.50, 0.20, DARK_STONE, "Pillar_Base")
    # Capital block
    _slab(0, 0, HEIGHT - 0.10, 0.50, 0.50, 0.20, DARK_STONE, "Pillar_Capital")
    # Carved bands
    for z in [0.30, HEIGHT - 0.30]:
        bpy.ops.mesh.primitive_torus_add(major_radius=0.20, minor_radius=0.02, location=(0, 0, z))
        add_material(bpy.context.active_object, DARK_STONE, "Pillar_Band_Mat")
    join_and_rename("SM_BLD_PILLAR")


def gen_stairs():
    """4m straight stair piece — 8 steps rising 3m."""
    clear_scene()
    steps = 8
    rise = HEIGHT / steps
    run = GRID / steps
    for i in range(steps):
        x = (i + 0.5) * run
        z = (i + 0.5) * rise
        _slab(x, GRID / 2, z, run, GRID, rise, WOOD, f"Stair_Tread_{i}")
    # Side stringers
    for sy in [0.05, GRID - 0.05]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(GRID / 2, sy, HEIGHT / 2))
        stringer = bpy.context.active_object
        stringer.scale = (GRID, 0.10, HEIGHT)
        bpy.ops.object.transform_apply(scale=True)
        add_material(stringer, DARK_WOOD, "Stair_Stringer_Mat")
    join_and_rename("SM_BLD_STAIRS")


def gen_ramp():
    """4m straight ramp piece rising 3m — wedge with side walls."""
    clear_scene()
    # Wedge using a 3-vert cone oriented as a triangular prism
    bpy.ops.mesh.primitive_cone_add(radius1=GRID / 2, radius2=0.0, depth=GRID,
                                    vertices=3, location=(GRID / 2, GRID / 2, HEIGHT / 2))
    wedge = bpy.context.active_object
    wedge.rotation_euler = (0, 0, 0)
    # Stretch into a prism shape (scale Y to bridge full grid width)
    wedge.scale = (1.0, 1.0, HEIGHT / GRID)
    bpy.ops.object.transform_apply(scale=True)
    add_material(wedge, WOOD, "Ramp_Wedge_Mat")
    # Side wall stringers (triangular flanks)
    for sy in [0.04, GRID - 0.04]:
        bpy.ops.mesh.primitive_cone_add(radius1=GRID / 2, radius2=0.0, depth=0.08,
                                        vertices=3, location=(GRID / 2, sy, HEIGHT / 2))
        flank = bpy.context.active_object
        flank.rotation_euler = (math.pi / 2, 0, 0)
        flank.scale = (1.0, HEIGHT / GRID, 1.0)
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        add_material(flank, DARK_WOOD, f"Ramp_Flank_{sy}_Mat")
    join_and_rename("SM_BLD_RAMP")


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
    print("\n=== Quiet Rift: Enigma — Walls & Structures Asset Generator ===")
    print(f"Grid: {GRID}m  Wall height: {HEIGHT}m  Half wall: {HALF_H}m  Thickness: {THICK}m")
    for asset_id, gen in GENERATORS.items():
        print(f"\n[{asset_id}]")
        gen()
        out_path = os.path.join(OUTPUT_DIR, f"SM_{asset_id}.fbx")
        export_fbx(asset_id, out_path)
    print("\n=== Generation complete ===")
    print(f"Output directory: {os.path.abspath(OUTPUT_DIR)}")
    print("Import these FBX files into UE5 via Content Browser > Import.")
    print("Set 'Import Uniform Scale' = 1 and 'Convert Scene Unit' off.")


if __name__ == "__main__":
    main()
