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
JUNGLE_LANDSCAPE_MTL = ("/Game/ScifiJungle/Materials/Nature/Landscape/"
                        "MI_LandscapeJungle.MI_LandscapeJungle")
# Backup landscape material from the MW auto-material pack -- works if
# the jungle one isn't in the project for some reason.
FALLBACK_LANDSCAPE_MTL = ("/Game/MWLandscapeAutoMaterial/Materials/Landscape/"
                          "MTL_MWAM_Landscape_IslandExample.MTL_MWAM_Landscape_IslandExample")
# Soil instance for the base floor in the static-mesh fallback path.
GROUND_SOIL_MTL = ("/Game/ScifiJungle/Materials/Nature/Ground/"
                   "MI_Soil_Jungle.MI_Soil_Jungle")

# Hill meshes -- ENGINE BASIC SHAPES only. The ScifiJungle rock meshes
# are Git-LFS pointers in this repo and frequently aren't resolved in the
# asset registry, so relying on them left the map flat. Engine basic
# shapes are always cooked + registered, so hills always appear. A
# half-buried Sphere makes a smooth, walkable dome; the Cylinder makes a
# flat-topped mesa.
ENGINE_SPHERE = "/Engine/BasicShapes/Sphere.Sphere"
ENGINE_CYLINDER = "/Engine/BasicShapes/Cylinder.Cylinder"
ENGINE_CUBE = "/Engine/BasicShapes/Cube.Cube"
HILL_MESHES = [ENGINE_SPHERE, ENGINE_CYLINDER]

# UE-engine basic shapes for the (optional) ground plane.
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
    # UClass objects don't expose get_name() as a bound method here;
    # use __name__ for the debug label.
    cls_label = getattr(actor_class, "__name__", str(actor_class))
    actor = _try(
        lambda: unreal.EditorLevelLibrary.spawn_actor_from_class(
            actor_class, location, rotation),
        "spawn {}".format(cls_label))
    if actor and label:
        _try(lambda: actor.set_actor_label(label), "label")
    return actor


def _load_static_mesh(pkg):
    asset = unreal.EditorAssetLibrary.load_asset(pkg)
    if isinstance(asset, unreal.StaticMesh):
        return asset
    return None


_MATERIAL_LOAD_WARNED = False


def _load_material(pkg):
    """Load a material by package path. Returns None on any failure
    (asset registry miss, type mismatch, etc.) and prints a one-time
    actionable hint the first time anything goes missing."""
    global _MATERIAL_LOAD_WARNED
    # does_asset_exist is asset-registry-driven, so it will return False
    # if the asset is on disk but the registry hasn't scanned it yet.
    try:
        if not unreal.EditorAssetLibrary.does_asset_exist(pkg):
            if not _MATERIAL_LOAD_WARNED:
                print("[terrain] material not in asset registry: {}".format(pkg))
                print("[terrain]   if the .uasset is on disk, force a rescan via")
                print("[terrain]   Window > Asset Registry > Refresh, OR close + reopen")
                print("[terrain]   the editor once after the Fab pack landed on disk.")
                print("[terrain]   The script falls back to the default material on the mesh.")
                _MATERIAL_LOAD_WARNED = True
            return None
    except Exception:
        pass
    try:
        asset = unreal.EditorAssetLibrary.load_asset(pkg)
    except Exception:
        return None
    if isinstance(asset, unreal.MaterialInterface):
        return asset
    return None


def _load_first_available_material(*pkgs):
    """Return the first material that resolves from a list of candidate
    package paths."""
    for p in pkgs:
        m = _load_material(p)
        if m:
            return m
    return None


def _clear_previous_terrain():
    """Remove every actor labelled QR_Terrain_* so re-running this
    script leaves a single fresh terrain set, not a stack of them.
    Also purges the legacy StarterHill_* cube blocks that
    qr_create_test_maps used to hardcode into L_DevTest before the
    dome hills existed -- maps saved with those cubes get cleaned up
    the next time this script runs."""
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not actor_sub:
        return 0
    killed = 0
    for a in actor_sub.get_all_level_actors():
        try:
            label = a.get_actor_label()
            if (label.startswith("QR_Terrain_")
                    or label.startswith("StarterHill_")):
                _try(lambda act=a: unreal.EditorLevelLibrary.destroy_actor(act),
                     "destroy stale terrain actor")
                killed += 1
        except Exception:
            pass
    if killed:
        print("[terrain] removed {} stale terrain/starter-hill actors".format(killed))
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

def _spawn_dome(label, x, y, diameter_m, mesh, expose_frac=0.42, yaw=0.0):
    """Spawn one half-buried sphere as a smooth, walkable hill.

    Uses USE_COMPLEX_AS_SIMPLE collision so the VISUAL triangles ARE the
    collision surface. Without this the engine Sphere mesh has a small
    default simple sphere that doesn't match the scaled-up visual, and
    the player walks through hill territory ('I can walk into the made
    hills'). Complex-as-simple guarantees collision == visual at every
    scale.

    diameter_m  -- full sphere diameter in metres.
    expose_frac -- fraction of the RADIUS that pokes above z=0 (0.42 ~=
                   a broad rolling hill; 1.0 would be a full hemisphere).
    """
    actor = _spawn(unreal.StaticMeshActor,
                   unreal.Vector(x, y, 0.0),
                   unreal.Rotator(0.0, yaw, 0.0), label)
    if not actor:
        return None
    smc = actor.static_mesh_component
    smc.set_static_mesh(mesh)
    # Engine sphere is 100 cm diameter at scale 1, so scale = diameter_m.
    s = float(diameter_m)
    actor.set_actor_scale3d(unreal.Vector(s, s, s))
    radius_cm = s * 50.0
    exposed_cm = radius_cm * float(expose_frac)
    bury_z = -(radius_cm - exposed_cm)   # sink centre so the cap pokes out
    actor.set_actor_location(unreal.Vector(x, y, bury_z), False, False)
    smc.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)

    # Force the visual mesh triangles to BE the collision surface. The
    # engine Sphere ships with a small default simple collision that
    # doesn't tightly match the visible sphere at this scale, which is
    # why the player walked through hills before. Setting the body
    # instance to USE_COMPLEX_AS_SIMPLE makes the triangulated sphere
    # itself the collision -- exact match, no walk-through.
    body = smc.get_editor_property("body_instance")
    if body:
        try:
            body.set_editor_property(
                "collision_complexity",
                unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            smc.set_editor_property("body_instance", body)
        except Exception as e:
            print("[terrain]   (collision_complexity skip: {})".format(e))
    # Belt-and-suspenders: force a BlockAll-style profile so nothing in
    # the project's collision matrix accidentally lets pawns walk through.
    try:
        smc.set_collision_profile_name("BlockAll")
    except Exception:
        pass
    return actor


def _spawn_hills(seed=1):
    """Build a basin of smooth domes from half-buried, UNIFORMLY-scaled
    engine spheres. No coplanar ground plane (that z-fought the existing
    dev floor and made the ground flash). Returns the count placed."""
    rng_state = [seed & 0xFFFFFFFF]
    def rand():
        rng_state[0] = (rng_state[0] * 1103515245 + 12345) & 0x7FFFFFFF
        return rng_state[0] / float(0x7FFFFFFF)

    sphere = _load_static_mesh(ENGINE_SPHERE)
    if not sphere:
        print("[terrain] engine Sphere mesh missing -- cannot build hills")
        return 0

    placed = 0
    count = 20
    # Ring of big rolling hills 30 m..120 m out, keeping the spawn area
    # at the centre open.
    ring_min, ring_max = 3000.0, 12000.0
    for i in range(count):
        angle = (i / float(count)) * math.tau + (rand() - 0.5) * 0.5
        r = ring_min + (ring_max - ring_min) * rand()
        x = math.cos(angle) * r
        y = math.sin(angle) * r
        # Big, broad spheres exposed only a little: a low expose_frac means
        # the dome's rim slope is gentle (a sphere is steepest at the rim,
        # so a shallow cap keeps the whole walkable surface under the
        # player's ~52° walkable-floor limit). 0.12-0.26 ~= 28-43° rim.
        diameter_m = 45.0 + rand() * 55.0   # 45-100 m broad hills
        expose = 0.12 + rand() * 0.14       # shallow, climbable domes
        if _spawn_dome("QR_Terrain_Hill_{:02d}".format(i),
                       x, y, diameter_m, sphere, expose_frac=expose,
                       yaw=rand() * 360.0):
            placed += 1

    # A few smaller mounds near the spawn so there's a slope in arm's
    # reach for immediate "wildlife climbs a hill" testing.
    near = [
        (   0.0,  1800.0, 26.0),
        (-1600.0,  -900.0, 22.0),
        ( 1700.0, -1000.0, 32.0),
    ]
    for i, (x, y, d) in enumerate(near):
        if _spawn_dome("QR_Terrain_NearHill_{:02d}".format(i),
                       x, y, d, sphere, expose_frac=0.20, yaw=i * 47.0):
            placed += 1

    print("[terrain] spawned {} smooth domes (uniform-scaled engine spheres)".format(placed))
    return placed


def static_mesh_path(seed=1):
    """Always-works path: half-buried engine-sphere hills on top of the
    level's existing floor. No Fab dependency, no coplanar plane."""
    print("[terrain] using engine-shape hill formation (PATH B)")
    return _spawn_hills(seed=seed)


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
            print("[terrain] FAILED -- no terrain placed (engine Sphere mesh")
            print("[terrain]   couldn't load, which should never happen).")
            return

    print("[terrain] done.")
    print("[terrain] NEXT STEPS:")
    print("[terrain]   1. Save the level (Ctrl+S) to rebuild the NavMesh over")
    print("[terrain]      the new hills.")
    print("[terrain]   2. Press P in the viewport to verify the green nav")
    print("[terrain]      overlay now drapes the dome slopes.")
    print("[terrain]   3. Drop / spawn a wildlife actor near a hill in PIE --")
    print("[terrain]      gravity + slope-conforming movement walks it up and")
    print("[terrain]      over the domes.")


if __name__ == "__main__":
    run()
