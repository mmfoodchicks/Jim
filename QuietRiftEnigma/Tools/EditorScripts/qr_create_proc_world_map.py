"""
Quiet Rift: Enigma — create a procedural-world test map.

Builds /Game/Maps/L_ProcTest with:
  • A ground plane (engine cube scaled to 200m × 200m)
  • Directional + sky light + atmospheric fog
  • PlayerStart 200cm up
  • One AQRProceduralScatterActor covering the full ground, palette
    pre-filled with rocks + plants from the Fab packs you already have
  • One ScifiJungle BP_PCG_Manager near origin so you can compare the
    PCG approach side-by-side with our scatter actor
  • World Settings → DefaultGameMode = AQRGameMode

The scatter actor is pre-configured with Seed = 1337 and TargetCount =
500 — change either in the Details panel and click "Generate" to
re-roll. Press Play to wander the resulting world.

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_create_proc_world_map.py').read())
"""

import unreal


MAP_PATH = "/Game/Maps/L_ProcTest"

# Source palettes — confirmed-existing assets from the Fab packs already
# in the project. Empty entries are skipped (keeps the script working
# even after pack changes).
SCATTER_PALETTE = [
    # path, weight, min_scale, max_scale, z_offset, align
    ("/Game/Fabs/Rock_Collection_04/Meshes/SM_Rock01.SM_Rock01",                 2.0, 0.6, 1.4, -10.0, True),
    ("/Game/Fabs/Rock_Collection_04/Meshes/SM_Rock02.SM_Rock02",                 1.5, 0.7, 1.6, -20.0, True),
    ("/Game/Fabs/Rock_Collection_04/Meshes/SM_Rock03.SM_Rock03",                 1.0, 0.8, 1.8,   0.0, True),
    # Plants take much higher weight so they're the dominant decor.
    ("/Game/Fabs/OWD_Plants_Pack/Plants/SM_Plant_01.SM_Plant_01",                4.0, 0.7, 1.3,   0.0, False),
    ("/Game/Fabs/OWD_Plants_Pack/Plants/SM_Plant_02.SM_Plant_02",                4.0, 0.7, 1.3,   0.0, False),
    ("/Game/Fabs/ScifiJungle/Models/Trees/SM_Tree_Jungle_01.SM_Tree_Jungle_01",  0.5, 0.8, 1.5,   0.0, False),
]


def _ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _spawn(cls_path, loc=(0,0,0), rot=(0,0,0)):
    cls = unreal.load_object(None, cls_path)
    if not cls:
        print("[proc-map] couldn't load " + cls_path)
        return None
    return unreal.EditorLevelLibrary.spawn_actor_from_class(
        cls, unreal.Vector(*loc), unreal.Rotator(*rot))


def _spawn_floor(extent_m=100.0):
    """Spawn a flat ground at z=0, sized 2*extent_m on each axis."""
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    if not cube: return None
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(0, 0, -50), unreal.Rotator(0, 0, 0))
    if not a: return None
    a.static_mesh_component.set_static_mesh(cube)
    # Engine cube is 100cm; scale X/Y to extent_m * 2 in metres → * 2
    a.set_actor_scale3d(unreal.Vector(extent_m * 2.0, extent_m * 2.0, 1.0))
    a.set_actor_label("ProcGround")
    return a


def _spawn_scatter(extent_m=100.0):
    """Drop our AQRProceduralScatterActor and populate its palette."""
    cls = unreal.load_object(None, "/Script/QuietRiftEnigma.QRProceduralScatterActor")
    if not cls:
        print("[proc-map] AQRProceduralScatterActor not loaded — compile C++ first")
        return None

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        cls, unreal.Vector(0, 0, 50), unreal.Rotator(0, 0, 0))
    if not actor: return None
    actor.set_actor_label("ProcScatter_Default")

    # Resize the bounds box to cover the floor.
    box = actor.get_editor_property("bounds")
    if box:
        box.set_box_extent(unreal.Vector(extent_m * 100.0, extent_m * 100.0, 500.0))

    # Build the palette array — only include entries whose mesh exists.
    palette = []
    StructType = unreal.load_object(None, "/Script/QuietRiftEnigma.QRScatterEntry")
    for path, weight, min_s, max_s, z_off, align in SCATTER_PALETTE:
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            continue
        entry = unreal.QRScatterEntry()
        entry.set_editor_property("mesh",              unreal.load_asset(path))
        entry.set_editor_property("weight",            weight)
        entry.set_editor_property("min_scale",         min_s)
        entry.set_editor_property("max_scale",         max_s)
        entry.set_editor_property("z_offset",          z_off)
        entry.set_editor_property("b_random_yaw",      True)
        entry.set_editor_property("b_align_to_surface",align)
        palette.append(entry)

    actor.set_editor_property("palette",      palette)
    actor.set_editor_property("target_count", 500)
    actor.set_editor_property("min_spacing",  120.0)
    actor.set_editor_property("max_slope_deg",60.0)
    actor.set_editor_property("seed",         1337)
    print("[proc-map] scatter palette: {} valid entries".format(len(palette)))
    return actor


def _try_spawn_scifi_pcg():
    """Optional — drop the ScifiJungle PCG_Manager near origin so the
    designer can compare PCG vs our scatter actor side-by-side."""
    p = "/Game/Fabs/ScifiJungle/PCG/Bundle/Blueprints/BP_PCG_Manager.BP_PCG_Manager_C"
    cls = unreal.load_object(None, p)
    if not cls:
        return None
    return unreal.EditorLevelLibrary.spawn_actor_from_class(
        cls, unreal.Vector(8000, 0, 0), unreal.Rotator(0, 0, 0))


def run():
    _ensure_dir("/Game/Maps")
    if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
        print("[proc-map] new_level failed for " + MAP_PATH)
        return

    _spawn_floor(extent_m=100.0)
    _spawn("/Script/Engine.DirectionalLight", loc=(0, 0, 1500), rot=(-50, 30, 0))
    _spawn("/Script/Engine.SkyLight",         loc=(0, 0, 800))
    _spawn("/Script/Engine.AtmosphericFog",   loc=(0, 0, 0))
    _spawn("/Script/Engine.PlayerStart",      loc=(0, 0, 200))

    _spawn_scatter(extent_m=100.0)
    _try_spawn_scifi_pcg()

    # World Settings game mode.
    settings = unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
    if settings:
        gm = unreal.load_object(None, "/Script/QuietRiftEnigma.QRGameMode")
        if gm:
            try:
                settings.set_editor_property("default_game_mode", gm)
            except Exception as e:
                print("[proc-map] game mode set failed: " + str(e))

    unreal.EditorLevelLibrary.save_current_level()
    print("[proc-map] L_ProcTest saved. Press Play to walk the world.")
    print("           To re-roll: select 'ProcScatter_Default' actor,")
    print("           change Seed in Details, click Generate button.")


if __name__ == "__main__":
    run()
