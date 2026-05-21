"""
Quiet Rift: Enigma — turn the worldgen cell grid into UE Landscape
import data: a 16-bit heightmap PLUS one 8-bit weightmap per ground
material layer, so the imported terrain is shaped AND textured by the
same seed that drives biome placement.

How it works:
  1. Pull the cell grid from UQRWorldGenSubsystem (run
     AQRWorldGenSeedActor::Generate first).
  2. Heightmap — per-cell base elevation by depth band (Remnant sinks
     into the Rift, Hazard belt rings high) + multi-octave value noise
     for organic relief.
  3. Weightmaps — each macro biome maps to one ground layer (Basalt /
     Slate / Moss / Mud / Glass / Ice / Hazard); per-layer coverage is
     feathered at biome borders so the landscape material blends them.
  4. Files land in Saved/QRWorldGen/:
       Heightmap_seed_<n>.png   16-bit grayscale
       Heightmap_seed_<n>.r16   headerless raw 16-bit (preferred import)
       Weight_<Layer>_seed_<n>.png   8-bit grayscale, one per layer
  5. import_to_level=True prints the exact Landscape Import-tool
     settings (resolution + scale) and attempts a programmatic import.

Run from the UE Python console after Generate has run:
  exec(open(r'<Project>/Tools/EditorScripts/qr_generate_heightmap.py').read())

Or:
  import qr_generate_heightmap
  qr_generate_heightmap.run(import_to_level=True)

A "medium" bake (1009x1009) takes a few minutes in pure Python — pass
resolution_key="small" while iterating.
"""

import math
import os
import struct
import zlib
import unreal


# Standard UE landscape resolutions that align with single-component
# defaults. 1009 = 63 quads x 16 sections + 1. Larger = more terrain
# detail but longer bake + import time.
LANDSCAPE_RESOLUTIONS = {
    "small":  505,   # 31 x 16 + 1 — fast iteration
    "medium": 1009,  # 63 x 16 + 1 — default
    "large":  2017,  # 127 x 16 + 1 — full detail
}

# Per-band base elevation as a fraction of 16-bit range (0..65535).
# Remnant band sinks to make the Rift visible; Hazard belt rings high
# so traversal terminates naturally.
BAND_ELEVATION = {
    "Remnant":    0.10,  # crater / Rift entrance
    "Deep":       0.50,  # broken vertical terrain
    "Mid":        0.45,  # rolling
    "Surface":    0.42,  # plains
    "Hazardbelt": 0.90,  # raised barrier ring
}


# ─── Biome → ground material layer ──────────────────────────────────
# Each macro biome paints one canonical ground layer. The landscape
# material is a Layer Blend of these; the weightmaps decide where each
# layer shows. Keep in sync with the 14 biomes in
# UQRWorldGenSubsystem::PopulateDefaultBandPools.
GROUND_LAYERS = ["Basalt", "Slate", "Moss", "Mud", "Glass", "Ice", "Hazard"]

BIOME_GROUND_LAYER = {
    "BasaltShelf":    "Basalt",
    "CraterFloors":   "Basalt",
    "CanyonWebs":     "Basalt",
    "RidgeShadows":   "Basalt",
    "ThermalCracks":  "Slate",
    "MagneticRidges": "Slate",
    "HighRims":       "Slate",
    "MeltlineEdges":  "Moss",
    "ShallowFens":    "Moss",
    "MossFields":     "Moss",
    "WetBasins":      "Mud",
    "WindPlains":     "Glass",
    "GlassDunes":     "Glass",
    "ColdBasins":     "Ice",
    "HazardBelt":     "Hazard",
}
_DEFAULT_LAYER = "Basalt"


# ─── Value noise (smooth fractal) ───────────────────────────────────
# The previous sin-hash "perlin" had zero spatial correlation — it was
# white noise, which produces spiky unusable terrain. This is real
# value noise: hash the integer lattice, smoothstep-interpolate, sum
# octaves.

def _vn_hash(ix, iy, seed):
    """Integer lattice hash → float in [0, 1]."""
    h = (ix * 374761393 + iy * 668265263 + seed * 2654435761) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h = (h ^ (h >> 16)) & 0xFFFFFFFF
    return h / 4294967295.0


def _smoothstep(t):
    return t * t * (3.0 - 2.0 * t)


def _value_noise(x, y, seed):
    ix = int(math.floor(x))
    iy = int(math.floor(y))
    fx = x - ix
    fy = y - iy
    v00 = _vn_hash(ix,     iy,     seed)
    v10 = _vn_hash(ix + 1, iy,     seed)
    v01 = _vn_hash(ix,     iy + 1, seed)
    v11 = _vn_hash(ix + 1, iy + 1, seed)
    ux = _smoothstep(fx)
    uy = _smoothstep(fy)
    a = v00 + (v10 - v00) * ux
    b = v01 + (v11 - v01) * ux
    return a + (b - a) * uy


def _fbm(x, y, seed, octaves=4):
    """Fractal sum of value noise → [-1, 1]."""
    total = 0.0
    amp = 1.0
    freq = 1.0
    norm = 0.0
    for o in range(octaves):
        total += _value_noise(x * freq, y * freq, seed + o * 1013) * amp
        norm += amp
        amp *= 0.5
        freq *= 2.0
    return (total / norm) * 2.0 - 1.0


# ─── PNG / raw writers ──────────────────────────────────────────────

def _png(width, height, bit_depth, raw, path):
    sig = b'\x89PNG\r\n\x1a\n'

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff))

    ihdr = struct.pack(">IIBBBBB", width, height, bit_depth, 0, 0, 0, 0)
    idat = zlib.compress(bytes(raw), 9)
    with open(path, 'wb') as f:
        f.write(sig)
        f.write(chunk(b'IHDR', ihdr))
        f.write(chunk(b'IDAT', idat))
        f.write(chunk(b'IEND', b''))


def _write_png_16(width, height, pixels16, path):
    """16-bit grayscale PNG. pixels16 = width*height ints in [0..65535]."""
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter byte: none
        for px in pixels16[y * width:(y + 1) * width]:
            raw += struct.pack(">H", max(0, min(65535, int(px))))
    _png(width, height, 16, raw, path)


def _write_png_8(width, height, pixels8, path):
    """8-bit grayscale PNG. pixels8 = width*height ints in [0..255]."""
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for px in pixels8[y * width:(y + 1) * width]:
            raw.append(max(0, min(255, int(px))))
    _png(width, height, 8, raw, path)


def _write_raw_r16(width, height, pixels16, path):
    """Headerless little-endian 16-bit raw (.r16) — the format UE's
    Landscape Import tool reads most predictably."""
    buf = bytearray()
    for px in pixels16:
        buf += struct.pack("<H", max(0, min(65535, int(px))))
    with open(path, 'wb') as f:
        f.write(bytes(buf))


# ─── Cell → elevation ───────────────────────────────────────────────

def _band_elevation_for_cell(cell):
    """Map a UQRWorldGenSubsystem cell to a 0..1 base elevation."""
    if str(cell.macro_biome) == "HazardBelt":
        return BAND_ELEVATION["Hazardbelt"]
    # cell.depth_band stringifies as 'QRDepthBand.SURFACE' — normalize.
    band = str(cell.depth_band).split(".")[-1].title()
    return BAND_ELEVATION.get(band, BAND_ELEVATION["Surface"])


# ─── Subsystem fetch ────────────────────────────────────────────────

def _get_worldgen_subsystem(world):
    """UE 5.7 Python doesn't expose World.get_subsystem uniformly — try
    the known entry points so this survives minor-version drift."""
    for attr in ('get_subsystem', 'get_subsystem_base'):
        m = getattr(world, attr, None)
        if not m:
            continue
        try:
            sub = m(unreal.QRWorldGenSubsystem)
            if sub:
                return sub
        except Exception:
            pass
    helper = getattr(unreal, 'WorldSubsystemBlueprintLibrary', None)
    if helper:
        getter = getattr(helper, 'get_world_subsystem', None)
        if getter:
            try:
                return getter(world, unreal.QRWorldGenSubsystem)
            except Exception:
                pass
    return None


# ─── Heightmap ──────────────────────────────────────────────────────

def _build_heightmap(cells, grid_w, grid_h, res, seed, noise_scale, noise_amp):
    base = [_band_elevation_for_cell(c) for c in cells]
    out = [0] * (res * res)
    sx = grid_w / float(res)
    sy = grid_h / float(res)
    for py in range(res):
        cy = min(grid_h - 1, int(py * sy))
        row = py * res
        for px in range(res):
            cx = min(grid_w - 1, int(px * sx))
            v = base[cy * grid_w + cx]
            v += _fbm(px * noise_scale, py * noise_scale, seed) * noise_amp
            out[row + px] = int(max(0.0, min(1.0, v)) * 65535)
        if py % 200 == 0:
            print("[landscape]   heightmap {}/{}".format(py, res))
    return out


# ─── Weightmaps ─────────────────────────────────────────────────────

def _box_blur_grid(values, w, h, radius):
    """Separable running-sum box blur over a float grid. O(w*h)."""
    if radius < 1:
        return values
    win = 2 * radius + 1
    tmp = [0.0] * (w * h)
    for y in range(h):
        base = y * w
        acc = values[base] * (radius + 1)
        for x in range(1, radius + 1):
            acc += values[base + min(w - 1, x)]
        for x in range(w):
            tmp[base + x] = acc / win
            acc += (values[base + min(w - 1, x + radius + 1)]
                    - values[base + max(0, x - radius)])
    out = [0.0] * (w * h)
    for x in range(w):
        acc = tmp[x] * (radius + 1)
        for y in range(1, radius + 1):
            acc += tmp[min(h - 1, y) * w + x]
        for y in range(h):
            out[y * w + x] = acc / win
            acc += (tmp[min(h - 1, y + radius + 1) * w + x]
                    - tmp[max(0, y - radius) * w + x])
    return out


def _build_weightmaps(cells, grid_w, grid_h, res, feather_cells):
    """One 0..255 weightmap per ground layer, feathered at biome
    borders. Coverage is computed + blurred at cell resolution (cheap),
    then bilinear-upsampled to landscape resolution."""
    cover = {layer: [0.0] * (grid_w * grid_h) for layer in GROUND_LAYERS}
    for i, c in enumerate(cells):
        layer = BIOME_GROUND_LAYER.get(str(c.macro_biome), _DEFAULT_LAYER)
        cover[layer][i] = 1.0

    for layer in GROUND_LAYERS:
        cover[layer] = _box_blur_grid(cover[layer], grid_w, grid_h, feather_cells)

    maps = {}
    sx = grid_w / float(res)
    sy = grid_h / float(res)
    for layer in GROUND_LAYERS:
        src = cover[layer]
        dst = [0] * (res * res)
        for py in range(res):
            gy = py * sy
            y0 = int(gy)
            y1 = min(grid_h - 1, y0 + 1)
            ty = gy - y0
            for px in range(res):
                gx = px * sx
                x0 = int(gx)
                x1 = min(grid_w - 1, x0 + 1)
                tx = gx - x0
                v00 = src[y0 * grid_w + x0]
                v10 = src[y0 * grid_w + x1]
                v01 = src[y1 * grid_w + x0]
                v11 = src[y1 * grid_w + x1]
                a = v00 + (v10 - v00) * tx
                b = v01 + (v11 - v01) * tx
                dst[py * res + px] = int(max(0.0, min(1.0, a + (b - a) * ty)) * 255)
        maps[layer] = dst
        print("[landscape]   weightmap '{}' done".format(layer))
    return maps


# ─── Landscape import (best effort) ─────────────────────────────────

def _try_import_landscape(heightmap_path):
    """Best-effort programmatic import. UE has no version-stable Python
    API for landscape heightmap import, so this tries the known
    candidates and, failing that, leaves the precise manual settings
    printed by run(). Paste any error so the call can be corrected."""
    ess = None
    try:
        ess = unreal.get_editor_subsystem(unreal.LandscapeEditorSubsystem)
    except Exception:
        ess = None
    if ess is not None:
        for fn_name in ('import_landscape_heightmap', 'import_heightmap'):
            fn = getattr(ess, fn_name, None)
            if not fn:
                continue
            try:
                fn(heightmap_path)
                print("[landscape] imported via LandscapeEditorSubsystem.{}"
                      .format(fn_name))
                return True
            except Exception as e:
                print("[landscape] {} failed: {}".format(fn_name, e))
    print("[landscape] no programmatic import API available — use the")
    print("            Landscape Import settings above")
    print("            (Modes -> Landscape -> New -> Import from File).")
    return False


# ─── Main ───────────────────────────────────────────────────────────

def run(resolution_key="medium", noise_scale=0.012, noise_amp=0.18,
        import_to_level=False, landscape_label="QR_GeneratedLandscape",
        with_weightmaps=True, feather_cells=2):
    """Bake landscape import data from the worldgen cell grid.

    resolution_key  — small / medium / large (see LANDSCAPE_RESOLUTIONS).
    noise_scale     — frequency of the organic relief overlay.
    noise_amp       — strength of the relief overlay (fraction of Z).
    import_to_level — also attempt a programmatic Landscape import.
    with_weightmaps — also bake per-biome ground-layer weightmaps.
    feather_cells   — biome-border blur radius, in cells.
    """
    W = unreal.EditorLevelLibrary.get_editor_world()
    if not W:
        print("[landscape] no editor world")
        return

    Sub = _get_worldgen_subsystem(W)
    if not Sub or not Sub.b_generated:
        print("[landscape] worldgen subsystem hasn't run yet —")
        print("            run AQRWorldGenSeedActor::Generate first")
        return

    cells = Sub.cells
    grid_w = Sub.grid_w
    grid_h = Sub.grid_h
    seed = Sub.world_seed
    res = LANDSCAPE_RESOLUTIONS.get(resolution_key, LANDSCAPE_RESOLUTIONS["medium"])
    print("[landscape] grid {}x{} seed {} -> {}x{} landscape".format(
        grid_w, grid_h, seed, res, res))

    out_dir = os.path.join(unreal.Paths.project_saved_dir(), "QRWorldGen")
    os.makedirs(out_dir, exist_ok=True)

    # ── Heightmap ──
    hmap = _build_heightmap(cells, grid_w, grid_h, res, seed, noise_scale, noise_amp)
    png_path = os.path.join(out_dir, "Heightmap_seed_{}.png".format(seed))
    r16_path = os.path.join(out_dir, "Heightmap_seed_{}.r16".format(seed))
    _write_png_16(res, res, hmap, png_path)
    _write_raw_r16(res, res, hmap, r16_path)
    print("[landscape] heightmap -> {}".format(png_path))
    print("[landscape]           -> {} (raw 16-bit, preferred)".format(r16_path))

    # ── Weightmaps ──
    if with_weightmaps:
        wmaps = _build_weightmaps(cells, grid_w, grid_h, res, feather_cells)
        for layer, data in wmaps.items():
            wpath = os.path.join(out_dir, "Weight_{}_seed_{}.png".format(layer, seed))
            _write_png_8(res, res, data, wpath)
            print("[landscape] weightmap -> {}".format(wpath))

    # ── Import settings ──
    world_m = Sub.cell_size_meters * grid_w
    m_per_quad = world_m / float(res - 1)
    scale_xy = m_per_quad * 100.0   # UE landscape scale: 100 == 1 m / quad
    print("")
    print("[landscape] ===== Landscape Import settings =====")
    print("[landscape]   Heightmap file : {}".format(r16_path))
    print("[landscape]   Resolution     : {} x {}".format(res, res))
    print("[landscape]   Scale          : X={:.2f}  Y={:.2f}  Z=100"
          .format(scale_xy, scale_xy))
    print("[landscape]   World span     : {:.1f} km across".format(world_m / 1000.0))
    if with_weightmaps:
        print("[landscape]   Paint layers   : {}".format(", ".join(GROUND_LAYERS)))
        print("[landscape]   assign each Weight_<Layer>_seed_{}.png to its layer"
              .format(seed))
    print("[landscape] =====================================")

    if import_to_level:
        _try_import_landscape(r16_path)

    print("[landscape] done")


if __name__ == "__main__":
    run()
