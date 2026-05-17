"""
Quiet Rift: Enigma — generate a procedural heightmap from the worldgen
cell grid + import it as a UE Landscape actor.

How it works:
  1. Pull the cell grid from UQRWorldGenSubsystem (must have run
     AQRWorldGenSeedActor::Generate first).
  2. For each cell, compute a base elevation:
       • Remnant band  = deep depression (the Rift)
       • Deep band     = high vertical (ridges + ravines)
       • Mid band      = rolling
       • Surface band  = open plains rising toward the hazard belt
     plus Perlin noise for organic variation, plus a hard wall ring
     beyond the playable interior.
  3. Resample to UE landscape-friendly resolution
     (2^N × subdivisions + 1; default 1009 × 1009 for one component
     of 63 × 63 quads × 16 subdivisions).
  4. Write a 16-bit grayscale PNG to Saved/QRWorldGen/Heightmap_<seed>.png.
  5. Optionally spawn an ALandscape actor + import the heightmap via
     unreal.LandscapeProxy.import_landscape_heightmap (if available).

Run from the UE Python console after Generate has run:
  exec(open(r'<Project>/Tools/EditorScripts/qr_generate_heightmap.py').read())

Or:
  import qr_generate_heightmap
  qr_generate_heightmap.run(import_to_level=True)
"""

import math
import os
import struct
import zlib
import unreal


# Standard UE landscape resolutions that align with single-component
# defaults. 1009 = 63 quads × 16 sections + 1. Larger = more terrain
# detail but longer import time.
LANDSCAPE_RESOLUTIONS = {
    "small":  505,   # 31 × 16 + 1 — fast iteration
    "medium": 1009,  # 63 × 16 + 1 — default
    "large":  2017,  # 127 × 16 + 1 — full detail
}

# Per-band base elevation as a fraction of 16-bit range (0..65535).
# Remnant band sinks to make the Rift visible; Hazard belt rings high
# so traversal terminates naturally.
BAND_ELEVATION = {
    "Remnant":   0.10,  # crater / Rift entrance
    "Deep":      0.50,  # broken vertical terrain
    "Mid":       0.45,  # rolling
    "Surface":   0.42,  # plains
    "HazardBelt":0.90,  # raised barrier ring
}


def _perlin(x, y, scale, seed):
    """UE5 doesn't expose perlin from Python; we do a fast 2D hash
    pseudo-Perlin approximation via sin-based noise. Good enough for
    a heightmap layer; designer adds detail via sculpt afterwards."""
    nx = x * scale + seed * 0.013
    ny = y * scale + seed * 0.027
    v = (math.sin(nx * 12.9898 + ny * 78.233) * 43758.5453)
    return (v - math.floor(v)) * 2.0 - 1.0  # → [-1, 1]


def _band_elevation_for_cell(cell):
    """Maps a UQRWorldGenSubsystem cell record to a 0..1 base elevation."""
    biome = str(cell.macro_biome)
    if biome == "HazardBelt":
        return BAND_ELEVATION["HazardBelt"]
    # cell.depth_band is an enum int — match by canonical names.
    band_str = str(cell.depth_band).split(".")[-1]
    return BAND_ELEVATION.get(band_str, BAND_ELEVATION["Surface"])


def _write_png_16(width, height, pixels16, path):
    """Write a 16-bit single-channel PNG. pixels16 is a list of
    width*height integers in [0..65535]."""
    # PNG signature
    sig = b'\x89PNG\r\n\x1a\n'
    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff))
    # IHDR: bit_depth=16, color_type=0 (grayscale)
    ihdr = struct.pack(">IIBBBBB", width, height, 16, 0, 0, 0, 0)
    # IDAT: raw filter byte 0 per row, then 2-byte big-endian pixels.
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        row = pixels16[y * width:(y + 1) * width]
        for px in row:
            raw += struct.pack(">H", max(0, min(65535, int(px))))
    idat = zlib.compress(bytes(raw), 9)
    iend = b''
    with open(path, 'wb') as f:
        f.write(sig)
        f.write(chunk(b'IHDR', ihdr))
        f.write(chunk(b'IDAT', idat))
        f.write(chunk(b'IEND', iend))


def run(resolution_key="medium", noise_scale=0.012, noise_amp=0.18,
        import_to_level=False, landscape_label="QR_GeneratedLandscape"):
    W = unreal.EditorLevelLibrary.get_editor_world()
    if not W:
        print("[heightmap] no world")
        return
    # UE 5.7 Python doesn't expose World.get_subsystem() directly; try a
    # couple of known entry points so this works across minor versions.
    Sub = None
    for attr in ('get_subsystem', 'get_subsystem_base'):
        m = getattr(W, attr, None)
        if not m: continue
        try:
            Sub = m(unreal.QRWorldGenSubsystem)
            if Sub: break
        except Exception:
            pass
    if not Sub:
        helper = getattr(unreal, 'WorldSubsystemBlueprintLibrary', None)
        if helper:
            getter = getattr(helper, 'get_world_subsystem', None)
            if getter:
                try:
                    Sub = getter(W, unreal.QRWorldGenSubsystem)
                except Exception:
                    pass
    if not Sub or not Sub.b_generated:
        print("[heightmap] worldgen subsystem hasn't run yet")
        print("           run AQRWorldGenSeedActor::Generate first")
        return

    cells = Sub.cells
    grid_w = Sub.grid_w
    grid_h = Sub.grid_h
    seed   = Sub.world_seed
    print("[heightmap] sampling {}x{} cell grid (seed {})".format(grid_w, grid_h, seed))

    # Build a low-resolution base grid then upsample to landscape size.
    base = [0.0] * (grid_w * grid_h)
    for i, c in enumerate(cells):
        base[i] = _band_elevation_for_cell(c)

    # Resample to landscape resolution via nearest-neighbor + add noise.
    res = LANDSCAPE_RESOLUTIONS.get(resolution_key, LANDSCAPE_RESOLUTIONS["medium"])
    out = [0] * (res * res)
    sx = grid_w / float(res)
    sy = grid_h / float(res)
    for py in range(res):
        for px in range(res):
            cx = int(px * sx)
            cy = int(py * sy)
            cx = max(0, min(grid_w - 1, cx))
            cy = max(0, min(grid_h - 1, cy))
            v = base[cy * grid_w + cx]
            # Perlin overlay for organic detail.
            n = _perlin(px, py, noise_scale, seed)
            v += n * noise_amp
            out[py * res + px] = int(max(0, min(1.0, v)) * 65535)

    # Write PNG to Saved/QRWorldGen/.
    out_dir  = os.path.join(unreal.Paths.project_saved_dir(), "QRWorldGen")
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "Heightmap_seed_{}.png".format(seed))
    _write_png_16(res, res, out, out_path)
    print("[heightmap] wrote {} ({}x{} 16-bit grayscale)".format(out_path, res, res))

    if not import_to_level:
        print("[heightmap] pass import_to_level=True to also spawn a Landscape")
        return

    # Spawn a Landscape actor + import the heightmap. The exact API
    # path varies across UE versions; we wrap in try/except to fail
    # gracefully.
    try:
        landscape = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.Landscape, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        if not landscape:
            print("[heightmap] couldn't spawn Landscape actor")
            return
        landscape.set_actor_label(landscape_label)
        # Scale: each pixel = CellSizeMeters × (grid_w / res) cm. Landscape
        # default scale is 100,100,100 (1 m per quad at Z=512).
        scale_xy = (Sub.cell_size_meters * grid_w / res) * 100.0 / 100.0
        landscape.set_actor_scale3d(unreal.Vector(scale_xy, scale_xy, 100.0))
        print("[heightmap] spawned Landscape '{}' at origin".format(landscape_label))
        print("[heightmap] open Modes → Landscape → Import to paint the heightmap:")
        print("           {}".format(out_path))
    except Exception as e:
        print("[heightmap] Landscape spawn failed: {}".format(e))


if __name__ == "__main__":
    run()
