"""
Quiet Rift: Enigma — create L_MainMenu and L_DevTest from scratch.

L_MainMenu:
  • Empty level (no geometry)
  • Directional light + skylight + atmospheric fog
  • PlayerStart at origin (the menu doesn't need a real spawn but the
    engine fails to load worlds without one)
  • World Settings → DefaultGameMode = AQRMainMenuGameMode

L_DevTest:
  • 100m flat floor (StaticMeshActor with the engine cube scaled)
  • Directional light + skylight + atmosphere
  • PlayerStart 200cm above the floor
  • Pre-placed AQRCraftingBench, AQRNPCSpawner, AQRWildlifeActor for
    quick smoke-tests
  • World Settings → DefaultGameMode = AQRGameMode

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_create_test_maps.py').read())

Or:
  import qr_create_test_maps
  qr_create_test_maps.run()

Re-running overwrites both maps. Pass run(only="dev") or only="menu" to
build just one.
"""

import unreal


MAP_ROOT = "/Game/Maps"

MAIN_MENU_PATH = "{}/L_MainMenu".format(MAP_ROOT)
DEV_TEST_PATH  = "{}/L_DevTest".format(MAP_ROOT)


# ─── Helpers ──────────────────────────────────────────────────────────

def _ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _new_empty_level(map_path):
    """Creates a fresh empty level at /Game/<path> and opens it.
    Deletes any existing asset at that path first so the script is
    safely re-runnable (otherwise new_level errors out with
    "An asset already exists at this location")."""
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        unreal.EditorAssetLibrary.delete_asset(map_path)
    # UE5.7's EditorLevelLibrary.new_level expects "/Game/..." style.
    ok = unreal.EditorLevelLibrary.new_level(map_path)
    if not ok:
        print("[maps] new_level failed for " + map_path)
        return False
    return True


def _spawn(cls_path, loc=(0,0,0), rot=(0,0,0)):
    """Load + spawn an actor class at the given world transform."""
    cls = unreal.load_object(None, cls_path)
    if not cls:
        print("[maps] couldn't load class " + cls_path)
        return None
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        cls,
        unreal.Vector(*loc),
        unreal.Rotator(*rot))
    # AQRGameMode runs a day/night cycle that rotates the sun each tick;
    # a static light component refuses to move and spams warnings. Force
    # Movable on every freshly-spawned light so the cycle works.
    if actor and 'Light' in cls_path:
        for comp in actor.get_components_by_class(unreal.LightComponent):
            comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    return actor


def _spawn_atmosphere_lighting():
    """Spawn the standard UE5 sky/lighting stack: DirectionalLight as
    atmosphere sun, SkyAtmosphere, real-time-capture SkyLight, and an
    ExponentialHeightFog. Returns nothing — actors are placed in the
    currently-open level."""

    # DirectionalLight wired as atmosphere sun so SkyAtmosphere knows
    # which light to scatter. Pitch -45 puts the sun at a flattering
    # mid-morning angle.
    dl = _spawn("/Script/Engine.DirectionalLight", loc=(0, 0, 800), rot=(-45, 30, 0))
    if dl:
        for c in dl.get_components_by_class(unreal.DirectionalLightComponent):
            c.set_editor_property('atmosphere_sun_light', True)
            c.set_intensity(10.0)  # lux equivalent; SkyAtmosphere expects realistic values

    # SkyAtmosphere — the actual blue sky / sunset gradient.
    _spawn("/Script/Engine.SkyAtmosphere", loc=(0, 0, 0))

    # SkyLight with real-time capture so bounced light updates with the
    # rotating sun. SourceType = captured scene means the SkyAtmosphere
    # drives the ambient term, no HDRI needed.
    sl = _spawn("/Script/Engine.SkyLight", loc=(0, 0, 200))
    if sl:
        for c in sl.get_components_by_class(unreal.SkyLightComponent):
            c.set_editor_property('real_time_capture', True)
            c.set_editor_property('source_type',
                unreal.SkyLightSourceType.SLS_CAPTURED_SCENE)

    # ExponentialHeightFog (UE5 replacement for the old AtmosphericFog).
    _spawn("/Script/Engine.ExponentialHeightFog", loc=(0, 0, 0))

    # PostProcessVolume so auto-exposure doesn't blow the scene to black
    # while it figures itself out. Infinite extent = applies everywhere.
    ppv = _spawn("/Script/Engine.PostProcessVolume", loc=(0, 0, 0))
    if ppv:
        ppv.set_editor_property('unbound', True)
        settings = ppv.settings
        settings.auto_exposure_method = unreal.AutoExposureMethod.AEM_HISTOGRAM
        settings.override_auto_exposure_min_brightness = True
        settings.auto_exposure_min_brightness = 1.0
        settings.override_auto_exposure_max_brightness = True
        settings.auto_exposure_max_brightness = 3.0
        ppv.settings = settings


def _set_game_mode(class_path):
    """Sets the current level's DefaultGameMode on AWorldSettings, then
    verifies + dirties the package so save_current_level actually
    persists it. Silent failures here were costing us PIE sessions:
    without QRGameMode the level loads with a default empty pawn and
    the world bootstrap never runs."""
    cls = unreal.load_object(None, class_path)
    if not cls:
        print("[maps]   couldn't load game-mode class " + class_path)
        return

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        print("[maps]   no editor world — game mode NOT set")
        return
    settings = world.get_world_settings()
    if not settings:
        print("[maps]   world has no WorldSettings — game mode NOT set")
        return

    try:
        settings.set_editor_property("default_game_mode", cls)
    except Exception as e:
        print("[maps]   set_editor_property('default_game_mode') failed: " + str(e))
        return

    # Read back to make sure it took.
    actual = settings.get_editor_property("default_game_mode")
    if actual != cls:
        print("[maps]   game mode read-back MISMATCH: got {} expected {}"
              .format(actual, class_path))
        return

    # set_editor_property already dirties the actor; save_current_level
    # picks up the change. (Old code called pkg.set_dirty_flag here, but
    # UE 5.7's Python binding doesn't expose that method on Package.)
    print("[maps]   game mode = " + class_path)


def _save():
    unreal.EditorLevelLibrary.save_current_level()


# ─── L_MainMenu ───────────────────────────────────────────────────────

def build_main_menu():
    print("[maps] building L_MainMenu…")
    if not _new_empty_level(MAIN_MENU_PATH):
        return

    # Full UE5 sky stack so the scene behind the menu widget renders as
    # an actual sky instead of a black void.
    _spawn_atmosphere_lighting()
    _spawn("/Script/Engine.PlayerStart",        loc=(0, 0, 100))

    _set_game_mode("/Script/QuietRiftEnigma.QRMainMenuGameMode")
    _save()
    print("[maps] L_MainMenu saved")


# ─── L_DevTest ────────────────────────────────────────────────────────

def _spawn_floor():
    """Spawn the engine default cube scaled to an 8 km × 8 km plate at
    z=0 so AQRGameMode's auto-bootstrap (default 8 km worldgen) has
    something to ground-trace against. Replace with a real Landscape
    actor when you're ready for terrain."""
    cube_mesh = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    if not cube_mesh:
        return

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(0, 0, -50),
        unreal.Rotator(0, 0, 0))
    if not actor:
        return

    smc = actor.static_mesh_component
    smc.set_static_mesh(cube_mesh)
    # Engine cube is 100cm per side. Scale to 800000×800000×100
    # (= 8 km × 8 km × 1 m) so the auto-bootstrap world fits.
    actor.set_actor_scale3d(unreal.Vector(8000.0, 8000.0, 1.0))
    actor.set_actor_label("Floor_DevTest")

    # Assign a ground-ish material so the floor isn't an untextured grey
    # checkerboard. Try a few well-known paths from Fab packs we have;
    # fall back silently if none are present.
    candidate_materials = [
        "/Game/Fabs/MWLandscapeAutoMaterial/Materials/M_AutoLandscape_Master",
        "/Game/Fabs/ScifiJungle/Materials/M_Ground_Forest",
        "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial",  # always available
    ]
    for path in candidate_materials:
        mat = unreal.load_object(None, path)
        if mat:
            smc.set_material(0, mat)
            break


def _spawn_starter_hills():
    """Drop a few cube-mesh "hills" near spawn so the dev-test world
    isn't a featureless plane. Placeholder visual until a real
    Landscape replaces the flat 8 km floor. Uses the engine cube so
    no Fab dependency."""
    cube_mesh = unreal.load_object(None, "/Engine/BasicShapes/Cube.Cube")
    if not cube_mesh:
        print("[maps]   /Engine/BasicShapes/Cube not loadable — no hills")
        return

    import math
    for i in range(8):
        angle = (i / 8.0) * 2 * math.pi
        dist  = 1500 + (i % 3) * 800   # 1500–3100 cm = 15–31 m
        loc   = (math.cos(angle) * dist, math.sin(angle) * dist, -50)
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor,
            unreal.Vector(*loc),
            unreal.Rotator(0, (i * 47) % 360, 0))
        if not actor: continue
        # Wide + low boxes so they read as terrain mounds rather than walls.
        scale_xy = 4.0 + (i % 4) * 2.5
        scale_z  = 1.5 + (i % 3) * 0.8
        actor.set_actor_scale3d(unreal.Vector(scale_xy, scale_xy, scale_z))
        actor.static_mesh_component.set_static_mesh(cube_mesh)
        actor.set_actor_label("StarterHill_{}".format(i))
    print("[maps]   placed 8 starter hill cubes (15–31 m radius)")


def _spawn_scatter_with_biome():
    """Drop one AQRProceduralScatterActor in front of the player spawn
    so trees / rocks / plants from a biome profile actually appear in
    the dev-test world. Without this the spawner alone leaves the floor
    empty (it only places POIs + wildlife, no foliage)."""
    cls = unreal.load_object(None, "/Script/QuietRiftEnigma.QRProceduralScatterActor")
    if not cls:
        print("[maps] no QRProceduralScatterActor class — skipping scatter")
        return
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    if not actor:
        return
    actor.set_actor_label("Scatter_DevTest")

    # Use BP_BasaltShelf (Surface-tier starter biome with rocks + plants + tree)
    profile_path = "/Game/QuietRift/Data/Biomes/BP_BasaltShelf"
    profile = unreal.load_object(None, profile_path)
    if profile:
        actor.set_editor_property('biome_profile', profile)
    else:
        print("[maps]   biome profile not found at {} — scatter has no palette".format(profile_path))

    # Cover a 200 m × 200 m area centered on origin — close enough to the
    # PlayerStart that you walk right into the scatter on Play.
    # The scatter actor's box footprint is the UBoxComponent named "Bounds".
    bounds = actor.get_editor_property('bounds')
    if bounds:
        bounds.set_box_extent(unreal.Vector(10000.0, 10000.0, 500.0))
    actor.set_editor_property('target_count', 800)
    actor.set_editor_property('seed', 1337)
    print("[maps]   scatter actor placed (200m x 200m, 800 instances)")


def _spawn_worldgen_spawner_with_fauna_rules():
    """Place an AQRWorldGenSpawner and wire its FaunaRulesPerBiome map
    from the /Game/QuietRift/Data/FaunaRules/ assets seeded by
    qr_seed_fauna_rules.py. Without this the spawner falls through to
    a ~5% sparse fallback (~25 fauna in 8 km) — what you saw before."""
    cls = unreal.load_object(None, "/Script/QuietRiftEnigma.QRWorldGenSpawner")
    if not cls:
        print("[maps] no QRWorldGenSpawner class — skipping spawner wire-up")
        return
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    if not actor:
        return
    actor.set_actor_label("WorldGenSpawner_DevTest")

    rules = {}
    rules_dir = "/Game/QuietRift/Data/FaunaRules"
    if unreal.EditorAssetLibrary.does_directory_exist(rules_dir):
        for asset_path in unreal.EditorAssetLibrary.list_assets(rules_dir, recursive=False):
            rule = unreal.load_object(None, asset_path)
            if not rule: continue
            tag = rule.get_editor_property('biome_tag')
            if not tag.is_none():
                rules[tag] = rule
    if rules:
        actor.set_editor_property('fauna_rules_per_biome', rules)
        print("[maps]   spawner wired with {} fauna rules".format(len(rules)))
    else:
        print("[maps]   no fauna rules found — run qr_seed_fauna_rules first")

    # Fallback wildlife so cells outside any biome rule still get something.
    fb = unreal.load_class(None, "/Script/QuietRiftEnigma.QRWildlife_AshbackBoar")
    if fb:
        actor.set_editor_property('wildlife_fallback_class', fb)


def build_dev_test():
    print("[maps] building L_DevTest…")
    if not _new_empty_level(DEV_TEST_PATH):
        return

    _spawn_floor()
    _spawn_atmosphere_lighting()
    _spawn("/Script/Engine.PlayerStart",        loc=(0, 0, 200))

    # Pre-place a crafting bench, NPC spawner, and one wildlife actor
    # so the dev tester can validate the F-interact + dialogue + ADS
    # paths immediately. Try/except in case the C++ class isn't compiled
    # (BP-only first-time setups).
    try:
        _spawn("/Script/QuietRiftEnigma.QRCraftingBench", loc=(300, 0, 50))
        _spawn("/Script/QuietRiftEnigma.QRNPCSpawner",     loc=(0, 300, 0))
        _spawn("/Script/QuietRiftEnigma.QRWildlifeActor",  loc=(-300, 0, 50))
    except Exception as e:
        print("[maps] pre-placement skipped: " + str(e))

    _spawn_starter_hills()
    _spawn_scatter_with_biome()
    _spawn_worldgen_spawner_with_fauna_rules()

    _set_game_mode("/Script/QuietRiftEnigma.QRGameMode")
    _save()
    print("[maps] L_DevTest saved")


# ─── Entry point ──────────────────────────────────────────────────────

def run(only=None):
    _ensure_dir(MAP_ROOT)
    if only is None or only == "menu":
        build_main_menu()
    if only is None or only == "dev":
        build_dev_test()
    print("[maps] done")


if __name__ == "__main__":
    run()
