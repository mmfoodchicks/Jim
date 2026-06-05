"""
qr_terrain_devtest.py -- add rolling terrain to the currently-open
dev map so wildlife has hills/slopes to climb and the world stops
looking like an airport runway.

Why this exists:
  L_DevTest spawns on a flat default floor. Wildlife now has gravity +
  slope-conforming movement (commit b367afb5), but with nothing to
  conform to, you can't see any of it. This script drops real terrain
  geometry into the level so the gameplay shows itself.

Two paths, the second always works:

  PATH A (preferred): programmatic UE Landscape
    Generates a rolling-hills heightmap and tries
    LandscapeEditorSubsystem.import_landscape_heightmap. UE Python's
    Landscape API is version-brittle -- some builds expose it, some
    don't -- so the call is guarded and falls through on failure.

  PATH B (fallback / always works): static-mesh hill formation
    Scatters large jungle cliff / island rocks as displaced "hills"
    forming a ~120m playable bowl, plus a base ground plane with the
    jungle landscape material applied to the existing floor. Wildlife
    walks over and around these naturally because they're solid
    static meshes with collision.

Both paths label every actor they spawn 'QR_Terrain_*' so re-running
the script removes the previous attempt before laying down a fresh
one (idempotent).

Run from the UE Python console with L_DevTest open:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_terrain_devtest.py').read())

  # default: scatter hill formation centred on origin
  run()

  # try landscape path first, fall back to hills if it fails
  run(try_landscape=True)
"""

import math
import os
import struct
import zlib
import unreal


# ─── Asset paths (verified by find against the repo) ───────────────────

# Ready-made jungle landscape material instance (auto-blended grass /
# moss / rock / swamp / sand layers).
JUNGLE_LANDSCAPE_MTL = ("/Game/Fabs/ScifiJungle/Materials/Nature/Landscape/"
                        "MI_LandscapeJungle.MI_LandscapeJungle")
# Backup landscape material from the MW auto-material pack -- works if
# the jungle one isn't in the project for some reason.
FALLBACK_LANDSCAPE_MTL = ("/Game/Fabs/MWLandscapeAutoMaterial/Materials/Landscape/"
                          "MTL_MWAM_Landscape_IslandExample.MTL_MWAM_Landscape_IslandExample")
# Soil instance for the base floor in the static-mesh fallback path.
GROUND_SOIL_MTL = ("/Game/Fabs/ScifiJungle/Materials/Nature/Ground/"
                   "MI_Soil_Jungle.MI_Soil_Jungle")

# Hill meshes -- jungle cliff rocks at various shapes. Each is scaled
# 4-7x in placement so they read as terrain features, not props.
HILL_MESHES = [
    "/Game/Fabs/ScifiJungle/Models/Nature/Rocks/SM_Island_Rock_1.SM_Island_Rock_1",
    "/Game/Fabs/ScifiJungle/Models/Nature/Rocks/SM_Island_Rock_2.SM_Island_Rock_2",
    "/Game/Fabs/ScifiJungle/Models/Nature/Rocks/SM_Island_Rock_3.SM_Island_Rock_3",
    "/Game/Fabs/ScifiJungle/Models/Nature/Rocks/SM_Rock_Cliff_Terrace1.SM_Rock_Cliff_Terrace1",
    "/Game/Fabs/ScifiJungle/Models/Nature/Rocks/SM_Rock_Cliff_Terrace2.SM_Rock_Cliff_Terrace2",
    "/Game/Fabs/ScifiJungle/Models/Nature/Rocks/SM_Rock_Cliff_Terrace3.SM_Rock_Cliff_Terrace3",
    "/Game/Fabs/ScifiJungle/Models/Nature/Rocks/SM_Rock_Cliff_Slope1.SM_Rock_Cliff_Slope1",
    "/Game/Fabs/ScifiJungle/Models/Nature/Rocks/SM_Rock_Cliff_Slope2.SM_Rock_Cliff_Slope2",
]

# UE-engine basic shapes for the fallback ground plane.
ENGINE_PLANE = "/Engine/BasicShapes/Plane.Plane"

# Heightmap resolution for the Landscape path. 505 = small, fast.
LANDSCAPE_RES = 505


# ─── Utility helpers ───────────────────────────────────────────────────

def _try(fn, label):
    try:
        return fn()
    except Exception as e:
        print("[terrain] {} skipped: {}".format(label, e))
        return None


def _editor_world():
    """Get the current editor world (the level the user is editing)."""
    sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if sub:
        w = sub.get_editor_world()
        if w:
            return w
    # Older API fallback.
    try:
        return unreal.EditorLevelLibrary.get_editor_world()
    except Exception:
        return None


def _spawn(actor_class, location, rotation=None, label=None):
    """Spawn an actor in the current editor world and (best-effort) label it."""
    if rotation is None:
        rotation = unreal.Rotator(0.0, 0.0, 0.0)
    actor = _try(
        lambda: unreal.EditorLevelLibrary.spawn_actor_from_class(
            actor_class, location, rotation),
        "spawn {}".format(actor_class.get_name()))
    if actor and label:
        _try(lambda: actor.set_actor_label(label), "label")
    return actor


def _load_static_mesh(pkg):
    asset = unreal.EditorAssetLibrary.load_asset(pkg)
    if isinstance(asset, unreal.StaticMesh):
        return asset
    return None


def _load_material(pkg):
    asset = unreal.EditorAssetLibrary.load_asset(pkg)
    if isinstance(asset, (unreal.MaterialInterface,)):
        return asset
    return None


def _clear_previous_terrain():
    """Remove every actor labelled QR_Terrain_* so re-running this
    script leaves a single fresh terrain set, not a stack of them."""
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not actor_sub:
        return 0
    killed = 0
    for a in actor_sub.get_all_level_actors():
        try:
            if a.get_actor_label().startswith("QR_Terrain_"):
                _try(lambda act=a: unreal.EditorLevelLibrary.destroy_actor(act),
                     "destroy stale terrain actor")
                killed += 1
        except Exception:
            pass
    if killed:
        print("[terrain] removed {} stale QR_Terrain_* actors".format(killed))
    return killed


# ─── Heightmap generation (small, standalone, no worldgen dep) ─────────

def _hash01(ix, iy, seed):
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
    v00 = _hash01(ix,     iy,     seed)
    v10 = _hash01(ix + 1, iy,     seed)
    v01 = _hash01(ix,     iy + 1, seed)
    v11 = _hash01(ix + 1, iy + 1, seed)
    ux = _smoothstep(fx)
    uy = _smoothstep(fy)
    a = v00 + (v10 - v00) * ux
    b = v01 + (v11 - v01) * ux
    return a + (b - a) * uy


def _fbm(x, y, seed, octaves=4):
    total = 0.0
    amp = 1.0
    freq = 1.0
    norm = 0.0
    for o in range(octaves):
        total += _value_noise(x * freq, y * freq, seed + o * 1013) * amp
        norm += amp
        amp *= 0.5
        freq *= 2.0
    return total / norm   # in [0, 1]


def _bake_rolling_hills(res, seed):
    """Generate a res x res 16-bit heightmap of rolling hills (no
    extreme cliffs). Centered at mid-grey so the imported Landscape
    has plenty of vertical headroom in both directions."""
    pixels = [0] * (res * res)
    freq = 0.012   # frequency of the main hill features
    for y in range(res):
        for x in range(res):
            n = _fbm(x * freq, y * freq, seed, octaves=4)
            # Map [0,1] -> [0.30, 0.70] so the heightmap stays away from
            # the rails (full black/white snap UE's Landscape import to
            # extreme heights).
            v = 0.30 + n * 0.40
            pixels[y * res + x] = int(v * 65535)
        if y % 100 == 0:
            print("[terrain]   heightmap row {}/{}".format(y, res))
    return pixels


def _write_r16(width, height, pixels16, path):
    buf = bytearray()
    for px in pixels16:
        buf += struct.pack("<H", max(0, min(65535, int(px))))
    with open(path, 'wb') as f:
        f.write(bytes(buf))


# ─── PATH A: real Landscape (best-effort) ──────────────────────────────

def try_landscape_path(seed=1):
    """Bake a heightmap and ask UE to import it as a Landscape actor.
    Returns the landscape actor on success, None on any failure
    (caller should then fall back to the static-mesh path)."""
    print("[terrain] trying Landscape path...")

    out_dir = os.path.join(unreal.Paths.project_saved_dir(), "QRTerrainDevTest")
    os.makedirs(out_dir, exist_ok=True)
    hmap_path = os.path.join(out_dir, "DevTestHeightmap_seed_{}.r16".format(seed))

    if not os.path.exists(hmap_path):
        hmap = _bake_rolling_hills(LANDSCAPE_RES, seed)
        _write_r16(LANDSCAPE_RES, LANDSCAPE_RES, hmap, hmap_path)
        print("[terrain] heightmap -> {}".format(hmap_path))
    else:
        print("[terrain] heightmap already baked -> {}".format(hmap_path))

    ess = _try(
        lambda: unreal.get_editor_subsystem(unreal.LandscapeEditorSubsystem),
        "fetch LandscapeEditorSubsystem")
    if not ess:
        print("[terrain] LandscapeEditorSubsystem not available on this UE build")
        return None

    for fn_name in ("import_landscape_heightmap", "import_heightmap"):
        fn = getattr(ess, fn_name, None)
        if not fn:
            continue
        ok = _try(lambda f=fn, p=hmap_path: f(p),
                  "LandscapeEditorSubsystem.{}".format(fn_name))
        if ok is not None:
            print("[terrain] Landscape imported via {}".format(fn_name))
            # Find the landscape actor we just created and apply the
            # jungle material to it. Best-effort -- if find returns
            # nothing (the API didn't expose a handle), the user can
            # apply the material manually.
            actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            if actor_sub:
                for a in actor_sub.get_all_level_actors():
                    if isinstance(a, unreal.Landscape):
                        _apply_landscape_material(a)
                        _try(lambda act=a: act.set_actor_label("QR_Terrain_Landscape"),
                             "label landscape")
                        return a
            return True

    print("[terrain] LandscapeEditorSubsystem has no usable import_*")
    return None


def _apply_landscape_material(landscape):
    mtl = _load_material(JUNGLE_LANDSCAPE_MTL) or _load_material(FALLBACK_LANDSCAPE_MTL)
    if mtl:
        _try(lambda: landscape.set_editor_property("landscape_material", mtl),
             "set landscape material")
        print("[terrain]   landscape material applied")
    else:
        print("[terrain]   no landscape material found -- assign one in the editor")


# ─── PATH B: static-mesh hill formation (always works) ─────────────────

def _spawn_ground_plane(material):
    """Big jungle-textured plane under the hills so wildlife and the
    player still have a solid floor between hill formations."""
    mesh = _load_static_mesh(ENGINE_PLANE)
    if not mesh:
        print("[terrain] Engine basic plane mesh missing -- skipping ground plane")
        return None

    actor = _spawn(unreal.StaticMeshActor,
                   unreal.Vector(0.0, 0.0, 0.0),
                   unreal.Rotator(0.0, 0.0, 0.0),
                   "QR_Terrain_GroundPlane")
    if not actor:
        return None

    smc = actor.static_mesh_component
    smc.set_static_mesh(mesh)
    # Engine plane is 100x100 cm; 1500 scale = 1.5 km, easily covers
    # the dev-test bowl.
    actor.set_actor_scale3d(unreal.Vector(1500.0, 1500.0, 1.0))
    if material:
        smc.set_material(0, material)
    smc.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    print("[terrain] ground plane spawned (1.5 km, jungle ground material)")
    return actor


def _spawn_hills(seed=1):
    """Place ~24 oversized rock meshes around the origin to form a
    rolling-hill basin. Random yaw + scale keeps them from reading as
    a copy-paste line."""
    rng_state = [seed & 0xFFFFFFFF]
    def rand():
        # Tiny LCG -- deterministic per seed across runs so re-running
        # gives the same landscape rather than reshuffling on every call.
        rng_state[0] = (rng_state[0] * 1103515245 + 12345) & 0x7FFFFFFF
        return rng_state[0] / float(0x7FFFFFFF)

    meshes = [m for m in [_load_static_mesh(p) for p in HILL_MESHES] if m]
    if not meshes:
        print("[terrain] no hill meshes loaded -- ScifiJungle pack missing?")
        return 0

    count = 32
    placed = 0
    # Distribute on a jittered ring + a few in the centre, between 30 m
    # and 120 m from origin so the player spawn area stays open.
    ring_min, ring_max = 3000.0, 12000.0
    for i in range(count):
        angle = (i / float(count)) * math.tau + (rand() - 0.5) * 0.6
        r = ring_min + (ring_max - ring_min) * rand()
        x = math.cos(angle) * r
        y = math.sin(angle) * r
        z = -200.0 + rand() * 100.0   # bury feet so they read as hills
        loc = unreal.Vector(x, y, z)
        rot = unreal.Rotator(0.0, rand() * 360.0, 0.0)

        actor = _spawn(unreal.StaticMeshActor, loc, rot,
                       "QR_Terrain_Hill_{:02d}".format(i))
        if not actor:
            continue
        mesh = meshes[i % len(meshes)]
        smc = actor.static_mesh_component
        smc.set_static_mesh(mesh)
        # 4-7x scale -- these were authored as smallish cliff props, so
        # they need to be sized way up to read as terrain.
        s = 4.0 + rand() * 3.0
        actor.set_actor_scale3d(unreal.Vector(s, s, s))
        smc.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        # The jungle rocks ship with their own material -- leave it.
        placed += 1

    # A small "bump" cluster near origin so something is in arm's reach
    # of the player spawn for immediate "wildlife climbs a hill" testing.
    near = [
        (   0.0,  1500.0, -120.0, 1.7),
        (-1300.0,  -700.0, -100.0, 1.4),
        ( 1400.0,  -800.0, -150.0, 2.0),
        ( -200.0,  -1900.0, -80.0, 1.2),
    ]
    for i, (x, y, z, s) in enumerate(near):
        mesh = meshes[i % len(meshes)]
        actor = _spawn(unreal.StaticMeshActor,
                       unreal.Vector(x, y, z),
                       unreal.Rotator(0.0, i * 53.0, 0.0),
                       "QR_Terrain_NearHill_{:02d}".format(i))
        if not actor:
            continue
        smc = actor.static_mesh_component
        smc.set_static_mesh(mesh)
        actor.set_actor_scale3d(unreal.Vector(s, s, s))
        smc.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        placed += 1

    print("[terrain] spawned {} hill rocks (jungle cliffs, scale 4-7x)".format(placed))
    return placed


def static_mesh_path(seed=1):
    """Always-works fallback: ground plane + jungle hill rocks. Returns
    the number of actors placed (>0 = success)."""
    print("[terrain] using static-mesh hill formation (PATH B)")

    soil = _load_material(GROUND_SOIL_MTL)
    if not soil:
        # Try the landscape material as a less-tiled but still-jungle
        # texture for the ground plane.
        soil = _load_material(JUNGLE_LANDSCAPE_MTL)
    plane_actor = _spawn_ground_plane(soil)
    hills_placed = _spawn_hills(seed=seed)

    return (1 if plane_actor else 0) + hills_placed


# ─── Top-level run() ───────────────────────────────────────────────────

def run(try_landscape=False, seed=1):
    """Drop dev-test terrain onto whatever level is currently open.

    try_landscape -- attempt the real Landscape path first. UE Python's
      Landscape API is brittle and often missing; the call is guarded
      and falls through cleanly. Default False so re-running this
      script is always fast.
    seed -- deterministic seed for the hill scatter / heightmap.
    """
    print("\n=== qr_terrain_devtest ===")
    world = _editor_world()
    if not world:
        print("[terrain] no editor world -- open a level first")
        return

    _clear_previous_terrain()

    landscape = None
    if try_landscape:
        landscape = try_landscape_path(seed=seed)

    if not landscape:
        n = static_mesh_path(seed=seed)
        if n == 0:
            print("[terrain] FAILED -- no terrain placed. Check that the")
            print("[terrain]   ScifiJungle pack is at /Game/Fabs/ScifiJungle/")
            return

    # Force the editor to redraw + flag the level dirty so saves pick
    # up the new actors.
    _try(lambda: unreal.EditorLevelLibrary.editor_set_game_view(False),
         "exit game view")
    print("[terrain] done.")
    print("[terrain] NEXT STEPS:")
    print("[terrain]   1. Save the level (Ctrl+S).")
    print("[terrain]   2. If you ran qr_dev_test_dressup before, your NavMesh")
    print("[terrain]      volume is in place -- press P in the viewport to verify")
    print("[terrain]      the green nav overlay now wraps the hill bases.")
    print("[terrain]   3. Drop a wildlife actor near a hill in PIE -- gravity-fix")
    print("[terrain]      and slope-conforming movement (commit b367afb5) will")
    print("[terrain]      walk it up and around the terrain.")


if __name__ == "__main__":
    run()
