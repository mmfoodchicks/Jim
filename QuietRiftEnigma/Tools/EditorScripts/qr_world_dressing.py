"""
qr_world_dressing.py -- one-button "make the dev map look like a planet"
orchestrator.

The problem this solves: the project ships several thousand Fab assets
(ScifiJungle terrain mats, Rock_Collection_04 boulders, MWLandscape
AutoMaterial blends, Polar ice, ScifiJungle plants), the biome profile
system reads them, and the AQRProceduralScatterActor scatters them --
but L_DevTest has neither scatter actors nor proper terrain materials,
so the dev map looks like a runway with grey domes on it.

This script wires it end-to-end against the currently-open map:

  1. Calls qr_seed_biome_profiles.run() so the 14 biome data assets
     exist (idempotent re-seed).
  2. Calls qr_terrain_devtest.run() so the hill-dome terrain exists
     (engine spheres -- collision-reliable). Idempotent.
  3. Paints the half-buried engine-sphere hills with the ScifiJungle
     ROCK material so they read as boulders instead of blobs.
  4. Paints the existing engine floor with the ScifiJungle SOIL
     material (kills the white-ground problem).
  5. Drops 4 AQRProceduralScatterActor instances around the playable
     bowl, each bound to a different Surface-tier biome profile
     (BasaltShelf / WindPlains / MeltlineEdges / CraterFloors). The
     scatter actor's Generate() runs in editor so vegetation + rocks
     appear immediately on the map.

Every actor spawned by step 3-5 is labeled QR_Dress_*; re-running the
script tears them down first so the dev map stays clean.

Run from the UE Python console with the dev map open:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_world_dressing.py').read())
  run()
"""

import os
import math
import unreal


# ─── Asset paths ─────────────────────────────────────────────────────

# Biome materials -- the actual Fab content the project already ships.
SOIL_MTL = ("/Game/Fabs/ScifiJungle/Materials/Nature/Ground/"
            "MI_Soil_Jungle.MI_Soil_Jungle")
ROCK_MTL = ("/Game/Fabs/ScifiJungle/Materials/Nature/Rock/"
            "MI_Rock_Jungle_Large.MI_Rock_Jungle_Large")

# Fallback: MWLandscapeAutoMaterial example -- always present in the
# auto-material pack and works on any static-mesh ground.
FALLBACK_GROUND_MTL = ("/Game/Fabs/MWLandscapeAutoMaterial/Materials/Landscape/"
                       "MTL_MWAM_Landscape_IslandExample.MTL_MWAM_Landscape_IslandExample")

BIOME_DIR = "/Game/QuietRift/Data/Biomes"

# The four Surface-tier profiles the biome seeder authors. Each gets a
# dedicated scatter actor on the dev map so the player walks through a
# layered, varied environment instead of one uniform tile.
SURFACE_BIOMES = [
    ("BP_BasaltShelf",   (   0.0,    0.0), (12000.0, 12000.0,  600.0)),
    ("BP_WindPlains",    ( 9000.0,    0.0), ( 8000.0,  8000.0,  600.0)),
    ("BP_MeltlineEdges", (-9000.0,    0.0), ( 8000.0,  8000.0,  600.0)),
    ("BP_CraterFloors",  (   0.0, 9000.0), ( 8000.0,  8000.0,  600.0)),
]

# Label every spawned actor with this prefix so re-runs can wipe cleanly.
DRESS_LABEL_PREFIX = "QR_Dress_"


# ─── Editor helpers ──────────────────────────────────────────────────

def _editor_world():
    """Current editor world (the open level)."""
    sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if sub:
        w = sub.get_editor_world()
        if w:
            return w
    try:
        return unreal.EditorLevelLibrary.get_editor_world()
    except Exception:
        return None


def _actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _level_actors():
    sub = _actor_subsystem()
    if sub:
        return sub.get_all_level_actors() or []
    try:
        return unreal.EditorLevelLibrary.get_all_level_actors() or []
    except Exception:
        return []


def _spawn_actor(cls, location, rotation=None):
    sub = _actor_subsystem()
    rot = rotation or unreal.Rotator(0.0, 0.0, 0.0)
    if sub:
        return sub.spawn_actor_from_class(cls, location, rot)
    try:
        return unreal.EditorLevelLibrary.spawn_actor_from_class(cls, location, rot)
    except Exception:
        return None


def _destroy_actor(actor):
    if not actor:
        return
    sub = _actor_subsystem()
    if sub:
        sub.destroy_actor(actor)
        return
    try:
        unreal.EditorLevelLibrary.destroy_actor(actor)
    except Exception:
        actor.destroy_actor()


def _maybe_load(path):
    """Load an asset by path. Returns None instead of raising if missing
    so dressing keeps running with whatever's available."""
    if not path:
        return None
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        print("[dress]   missing asset: {}".format(path))
        return None
    return unreal.load_asset(path)


def _load_ground_mtl():
    """Pick the best available ground material -- prefer ScifiJungle
    soil, fall back to the MW auto-material example, accept anything
    that loads."""
    for path in (SOIL_MTL, FALLBACK_GROUND_MTL):
        m = _maybe_load(path)
        if m:
            return m, path
    return None, None


def _load_rock_mtl():
    return _maybe_load(ROCK_MTL), ROCK_MTL


# ─── Step 0: dress-actor cleanup ─────────────────────────────────────

def _wipe_previous_dressing():
    removed = 0
    for a in _level_actors():
        if not a:
            continue
        try:
            label = a.get_actor_label()
        except Exception:
            continue
        if label and label.startswith(DRESS_LABEL_PREFIX):
            _destroy_actor(a)
            removed += 1
    if removed:
        print("[dress] wiped {} prior dressing actors".format(removed))


# ─── Step 1 / 2: chain to the biome seeder + terrain script ──────────

def _here():
    """Absolute dir of this script (works under shell exec; the
    DEV_TEST_DRESSUP scripts use the same trick)."""
    try:
        return os.path.dirname(os.path.abspath(__file__))
    except NameError:
        return os.path.normpath(os.path.join(
            unreal.Paths.project_dir(), "..", "Tools", "EditorScripts"))


def _run_sibling(script_name, fn_name="run", **kwargs):
    """Source another qr_*.py from the same folder and call its run().
    Lets this orchestrator stay one file the designer types."""
    here = _here()
    path = os.path.join(here, script_name)
    if not os.path.isfile(path):
        print("[dress] {} not on disk -- skipped".format(script_name))
        return None
    g = {"__name__": "__qr_dress_sub__", "__file__": path}
    exec(open(path).read(), g)
    fn = g.get(fn_name)
    if not callable(fn):
        print("[dress] {}.{}() missing".format(script_name, fn_name))
        return None
    return fn(**kwargs)


def _seed_biomes():
    print("[dress] step 1/4: ensure biome profiles exist")
    _run_sibling("qr_seed_biome_profiles.py")


def _seed_terrain():
    print("[dress] step 2/4: ensure rolling-hills terrain exists")
    _run_sibling("qr_terrain_devtest.py")


# ─── Step 3: paint the hill spheres with the rock material ───────────

def _paint_terrain_geometry():
    print("[dress] step 3/4: paint terrain geometry with Fab materials")
    rock_mtl, rock_path = _load_rock_mtl()
    soil_mtl, soil_path = _load_ground_mtl()

    painted_rocks = 0
    painted_ground = 0

    for a in _level_actors():
        if not a:
            continue
        try:
            label = a.get_actor_label() or ""
        except Exception:
            continue

        # qr_terrain_devtest labels its hill domes QR_Terrain_Hill_* and
        # the optional ground plane QR_Terrain_Ground.
        if not label.startswith("QR_Terrain_"):
            continue

        # Walk every StaticMeshComponent on the actor and override its
        # material slots. CreateDynamicMaterialInstance isn't needed --
        # we want the real Fab material instance, not a transient copy.
        smcs = a.get_components_by_class(unreal.StaticMeshComponent)
        for smc in (smcs or []):
            mesh = smc.get_static_mesh()
            if not mesh:
                continue
            slot_count = mesh.get_num_sections(0) or 1

            is_hill = "Hill" in label or "_Mound_" in label or "_Mesa_" in label
            target_mtl = rock_mtl if is_hill else soil_mtl
            if not target_mtl:
                continue
            for slot in range(slot_count):
                smc.set_material(slot, target_mtl)
            if is_hill:
                painted_rocks += 1
            else:
                painted_ground += 1

    if rock_mtl:
        print("[dress]   rock material:   {}  ({} actors)".format(rock_path, painted_rocks))
    else:
        print("[dress]   rock material:   none available, hills stay default")
    if soil_mtl:
        print("[dress]   ground material: {}  ({} actors)".format(soil_path, painted_ground))
    else:
        print("[dress]   ground material: none available, ground stays default")


# ─── Step 4: drop scatter actors per biome ───────────────────────────

def _spawn_scatter(biome_name, center, extent):
    scatter_cls = unreal.QRProceduralScatterActor
    actor = _spawn_actor(scatter_cls,
        unreal.Vector(center[0], center[1], 0.0))
    if not actor:
        print("[dress]   FAILED to spawn scatter for {}".format(biome_name))
        return None

    actor.set_actor_label("{}{}".format(DRESS_LABEL_PREFIX, biome_name))

    # Set the biome profile reference. The scatter actor's BiomeProfile
    # field overrides its inline Palette + Suggested* defaults.
    profile = _maybe_load("{}/{}".format(BIOME_DIR, biome_name))
    if profile:
        actor.set_editor_property("biome_profile", profile)

    # Resize the bounds component so the scatter covers a real area.
    # UBoxComponent::SetBoxExtent expects centimetres.
    box = actor.get_editor_property("bounds")
    if box:
        box.set_box_extent(unreal.Vector(extent[0], extent[1], extent[2]), True)

    # Seed -- vary per biome so neighbouring biomes don't share layouts.
    actor.set_editor_property("seed", hash(biome_name) & 0x7FFFFFFF)

    # Trigger an editor-time Generate so the level is dressed
    # immediately without needing to PIE.
    try:
        actor.call_method("Generate", ())
    except Exception:
        try:
            actor.generate()
        except Exception as e:
            print("[dress]   Generate() didn't fire for {}: {}".format(biome_name, e))
    return actor


def _spawn_biome_scatters():
    print("[dress] step 4/4: drop scatter actors per biome")
    placed = 0
    for biome_name, center, extent in SURFACE_BIOMES:
        if not unreal.EditorAssetLibrary.does_asset_exist("{}/{}".format(BIOME_DIR, biome_name)):
            print("[dress]   skip {} -- profile not seeded".format(biome_name))
            continue
        if _spawn_scatter(biome_name, center, extent):
            placed += 1
    print("[dress]   placed {} scatter volumes".format(placed))


# ─── Public entry ────────────────────────────────────────────────────

def run(skip_biomes=False, skip_terrain=False):
    """Wire the open dev map for visual content.

    Args:
      skip_biomes:  pass True to skip the qr_seed_biome_profiles step
                    (saves a few seconds on repeat runs once seeded).
      skip_terrain: pass True to skip qr_terrain_devtest (likewise once
                    terrain is in the map).
    """
    print("\n=== qr_world_dressing ===")
    world = _editor_world()
    if not world:
        print("[dress] no editor world -- open a map first")
        return

    _wipe_previous_dressing()
    if not skip_biomes:  _seed_biomes()
    if not skip_terrain: _seed_terrain()
    _paint_terrain_geometry()
    _spawn_biome_scatters()

    print("[dress] DONE -- save the level to keep the dressing.")


if __name__ == "__main__":
    run()
