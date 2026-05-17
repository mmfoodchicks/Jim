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
    """Sets the world settings' DefaultGameMode to the given class."""
    cls = unreal.load_object(None, class_path)
    if not cls:
        print("[maps] couldn't load game-mode class " + class_path)
        return
    # Set DefaultGameMode directly on the AWorldSettings actor. The old
    # EditorLevelLibrary.get_game_mode_settings_for_current_level() helper
    # was removed in UE 5.7; this path is what's left and is sufficient.
    settings = unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
    if settings:
        try:
            settings.set_editor_property("default_game_mode", cls)
        except Exception as e:
            print("[maps] world settings game mode set failed: " + str(e))


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
