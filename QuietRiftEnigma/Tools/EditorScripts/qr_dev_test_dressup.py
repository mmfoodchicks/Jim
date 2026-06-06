"""
qr_dev_test_dressup.py -- one-shot dressing of L_DevTest + starter
DataTable creation, so a fresh checkout can run the gameplay loop in
PIE without a full afternoon of editor work.

Does (idempotent, safe to re-run):

  1. Spawns a NavMeshBoundsVolume on the current level if missing,
     scaled to cover the dev-test play area. Without this AI can't
     path -- wildlife, NPCs, raiders all stand still.

  2. Creates DT_BuildCatalog at /Game/QuietRift/Data/Build/ with 19
     starter rows (walls/floors/doors/foundation in three tiers) so
     UQRBuildModeComponent.PieceCatalog has rows to find. Designer
     fills in the Mesh / RequiredTechNodeId fields per piece later.

  3. Creates DT_LootTables at /Game/QuietRift/Data/Loot/ with three
     tier rows (T1 / T2 / T3) so AQRLootContainer rolls produce
     items instead of failing silently.

What this script CANNOT do (manual editor follow-ups):

  - Build navigation paths after the NavMesh volume drops. UE only
    rebuilds paths on a level save or a manual Build > Build Paths.
    After saving the level once, the green nav-mesh debug overlay
    should appear when you press P in the editor viewport.

  - Author the ABP_QRPlayer Locomotion state-machine graph. UE's
    Python API does not expose Anim Graph node authoring for state
    machines (the required UEdGraphNode_StateMachineEntry / State
    nodes are editor-only and not Python-bound). Use the companion
    script qr_wire_anim_blueprint.py to ingest the locomotion anim
    set, then open ABP_QRPlayer and drag the anims into the state
    machine you author by hand. The list of anim assets to drag in
    is printed by qr_wire_anim_blueprint.py.

  - Fill in mesh references on the build-catalog rows. The 19 rows
    land with empty TSoftObjectPtr<UStaticMesh> fields. The build
    mode component checks for null and skips placement, so this
    won't crash anything -- it just means each piece type needs a
    designer to drop the right SM_BLD_* mesh in the catalog row.

Run from the UE Python console (with L_DevTest open):
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_dev_test_dressup.py').read())
"""

import unreal


# Where the new DataTables live.
BUILD_CATALOG_PKG = "/Game/QuietRift/Data/Build"
BUILD_CATALOG_NAME = "DT_BuildCatalog"
BUILD_CATALOG_PATH = "{}/{}".format(BUILD_CATALOG_PKG, BUILD_CATALOG_NAME)

LOOT_TABLES_PKG  = "/Game/QuietRift/Data/Loot"
LOOT_TABLES_NAME = "DT_LootTables"
LOOT_TABLES_PATH = "{}/{}".format(LOOT_TABLES_PKG, LOOT_TABLES_NAME)

# Row-struct script paths -- these are the UScriptStruct objects
# generated from our USTRUCT(BlueprintType) declarations in C++.
# Verified by walking QuietRiftEnigma/Source/ + module names.
BUILD_ROW_STRUCT_PATH = "/Script/QuietRiftEnigma.QRBuildPieceRow"
LOOT_ROW_STRUCT_PATH  = "/Script/QRItems.QRLootTableRow"


def _ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _try(fn, label):
    try:
        return fn()
    except Exception as e:
        print("[dressup] {} skipped: {}".format(label, e))
        return None


# -- NavMesh ------------------------------------------------------------

def ensure_navmesh_bounds_volume():
    """Spawn a NavMeshBoundsVolume scaled to cover the dev-test floor
    if one isn't already present. Designer must still click Build >
    Build Paths (or save the level) to bake the actual nav-mesh."""
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = None
    if actor_sub:
        for a in actor_sub.get_all_level_actors():
            if isinstance(a, unreal.NavMeshBoundsVolume):
                existing = a
                break
    if existing:
        print("[dressup] NavMeshBoundsVolume already present at {} -- skipping spawn"
              .format(existing.get_actor_label()))
        return existing

    # L_DevTest's floor is centred at the origin and a few thousand cm
    # across. 50 km cube around origin comfortably covers anything the
    # dev map cares about while staying well inside the 64 km world
    # bound. Designers can resize the actor in the editor afterward.
    loc = unreal.Vector(0.0, 0.0, 1000.0)
    rot = unreal.Rotator(0.0, 0.0, 0.0)
    vol = _try(lambda: unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.NavMeshBoundsVolume, loc, rot), "NavMesh spawn")
    if not vol:
        print("[dressup] failed to spawn NavMeshBoundsVolume -- spawn it manually "
              "in the editor (Place Actors > Volumes > Nav Mesh Bounds Volume).")
        return None

    vol.set_actor_label("QR_NavBounds")
    # The default brush is a 200-unit cube; scale to ~50 km cube.
    _try(lambda: vol.set_actor_scale3d(unreal.Vector(250.0, 250.0, 25.0)),
         "NavMesh scale")
    print("[dressup] NavMeshBoundsVolume QR_NavBounds spawned at origin, scale 250x250x25.")
    print("[dressup]   Save the level (Ctrl+S) to trigger a nav build,")
    print("[dressup]   or press P in the viewport to verify the green overlay.")
    return vol


# -- DataTable creation -------------------------------------------------

def _ensure_datatable(asset_path, asset_pkg, asset_name, row_struct_path):
    """Create a DataTable asset if missing. Returns the loaded DataTable."""
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        existing = unreal.load_asset(asset_path)
        if isinstance(existing, unreal.DataTable):
            print("[dressup] {} already exists -- reusing".format(asset_path))
            return existing
    _ensure_dir(asset_pkg)
    struct = unreal.load_object(None, row_struct_path)
    if not struct:
        print("[dressup] ERROR: row struct {} not found -- compile C++ first"
              .format(row_struct_path))
        return None
    factory = unreal.DataTableFactory()
    factory.struct = struct
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    table = tools.create_asset(asset_name, asset_pkg, unreal.DataTable, factory)
    if not table:
        print("[dressup] create_asset returned null for {}".format(asset_path))
        return None
    print("[dressup] created {}".format(asset_path))
    return table


def _fill_datatable_from_csv(table, csv_string):
    """Bulk-populate a DataTable from a CSV string (first column = row
    name, remaining columns = struct fields). Uses the editor function
    library that UE exposes for editor-time CSV import."""
    if not table:
        return False
    lib = getattr(unreal, "DataTableFunctionLibrary", None)
    if lib is None or not hasattr(lib, "fill_data_table_from_csv_string"):
        print("[dressup] DataTableFunctionLibrary.fill_data_table_from_csv_string "
              "not available on this UE build")
        return False
    try:
        ok = lib.fill_data_table_from_csv_string(table, csv_string)
        if not ok:
            print("[dressup] fill_data_table_from_csv_string returned false; "
                  "row struct may not match the CSV header")
        else:
            unreal.EditorAssetLibrary.save_loaded_asset(table)
        return bool(ok)
    except Exception as e:
        print("[dressup] CSV fill threw: {}".format(e))
        return False


# -- Build catalog seed data --------------------------------------------

# 19 starter rows, three tiers. Mesh + RequiredTechNodeId are left blank
# (designer wires them once SM_BLD_* assets land). Material costs are
# placeholders -- ItemId references resources that should exist in the
# items master DataTable; if an id is missing the build mode will log
# a warning but not crash.
#
# Category enum values match EQRBuildCategory in QRBuildTypes.h
# (Wall=0, Floor=1, Roof=2, Door=3, Window=4, Stair=5, Foundation=6,
# Storage=7, Defense=8, Lighting=9, Decoration=10).
BUILD_CATALOG_CSV = """\
Name,DisplayName,Category,Mesh,MaterialCost,RequiredTechNodeId
BLD_FOUNDATION_WOOD,"Wood Foundation",Foundation,"None","((ItemId=""MAT_WOOD_LOG"",Quantity=4))",
BLD_FOUNDATION_STONE,"Stone Foundation",Foundation,"None","((ItemId=""MAT_STONE_BLOCK"",Quantity=6))",TECH_MASONRY_T1
BLD_FOUNDATION_METAL,"Metal Foundation",Foundation,"None","((ItemId=""MAT_METAL_PLATE"",Quantity=4),(ItemId=""MAT_RIVET_SET"",Quantity=8))",TECH_FORGING_T1
BLD_WALL_WOOD,"Wood Wall",Wall,"None","((ItemId=""MAT_WOOD_PLANK"",Quantity=3))",
BLD_WALL_STONE,"Stone Wall",Wall,"None","((ItemId=""MAT_STONE_BLOCK"",Quantity=4))",TECH_MASONRY_T1
BLD_WALL_METAL,"Metal Wall",Wall,"None","((ItemId=""MAT_METAL_PLATE"",Quantity=2),(ItemId=""MAT_RIVET_SET"",Quantity=4))",TECH_FORGING_T1
BLD_FLOOR_WOOD,"Wood Floor",Floor,"None","((ItemId=""MAT_WOOD_PLANK"",Quantity=2))",
BLD_FLOOR_STONE,"Stone Floor",Floor,"None","((ItemId=""MAT_STONE_BLOCK"",Quantity=3))",TECH_MASONRY_T1
BLD_FLOOR_METAL,"Metal Floor",Floor,"None","((ItemId=""MAT_METAL_PLATE"",Quantity=2))",TECH_FORGING_T1
BLD_ROOF_WOOD,"Wood Roof",Roof,"None","((ItemId=""MAT_WOOD_PLANK"",Quantity=3))",
BLD_ROOF_METAL,"Metal Roof",Roof,"None","((ItemId=""MAT_METAL_PLATE"",Quantity=3))",TECH_FORGING_T1
BLD_DOOR_WOOD,"Wood Door",Door,"None","((ItemId=""MAT_WOOD_PLANK"",Quantity=2),(ItemId=""MAT_HINGE_SET"",Quantity=1))",
BLD_DOOR_METAL,"Metal Door",Door,"None","((ItemId=""MAT_METAL_PLATE"",Quantity=2),(ItemId=""MAT_HINGE_SET"",Quantity=1),(ItemId=""MAT_RIVET_SET"",Quantity=4))",TECH_FORGING_T1
BLD_WINDOW_WOOD,"Wood Window Frame",Window,"None","((ItemId=""MAT_WOOD_PLANK"",Quantity=2),(ItemId=""MAT_GLASS_PANE"",Quantity=1))",
BLD_STAIR_WOOD,"Wood Stair",Stair,"None","((ItemId=""MAT_WOOD_PLANK"",Quantity=4))",
BLD_STORAGE_CRATE,"Storage Crate",Storage,"None","((ItemId=""MAT_WOOD_PLANK"",Quantity=4),(ItemId=""MAT_HINGE_SET"",Quantity=1))",
BLD_DEFENSE_PALISADE,"Wood Palisade",Defense,"None","((ItemId=""MAT_WOOD_LOG"",Quantity=2))",
BLD_LIGHTING_TORCH,"Wall Torch",Lighting,"None","((ItemId=""MAT_WOOD_STICK"",Quantity=1),(ItemId=""MAT_TINDER"",Quantity=1))",
BLD_DECOR_BENCH,"Wood Bench",Decoration,"None","((ItemId=""MAT_WOOD_PLANK"",Quantity=3))",
"""

# Three loot tier rows. Entries reference common item ids; ids that don't
# resolve at runtime drop nothing and log a warning. Weights are
# relative within the table.
LOOT_TABLES_CSV = """\
Name,DisplayName,RollCount,bAllowDuplicates,Entries
LOOT_TIER_1,"Tier 1 (Surface Wreck)",3,False,"((ItemId=""MAT_SCRAP_METAL"",MinCount=1,MaxCount=3,Weight=40,DropChance=1.0),(ItemId=""MAT_WIRE_BUNDLE"",MinCount=1,MaxCount=2,Weight=25,DropChance=0.8),(ItemId=""FOD_RATION_PACK"",MinCount=1,MaxCount=1,Weight=15,DropChance=0.6),(ItemId=""CMP_BATTERY_AA"",MinCount=1,MaxCount=2,Weight=15,DropChance=0.7),(ItemId=""AMO_PISTOL"",MinCount=4,MaxCount=10,Weight=5,DropChance=0.4))"
LOOT_TIER_2,"Tier 2 (Mid Wreck / Cache)",4,False,"((ItemId=""MAT_METAL_PLATE"",MinCount=1,MaxCount=2,Weight=25,DropChance=1.0),(ItemId=""CMP_CIRCUIT_BOARD"",MinCount=1,MaxCount=1,Weight=20,DropChance=0.8),(ItemId=""CMP_FUEL_CELL"",MinCount=1,MaxCount=1,Weight=15,DropChance=0.6),(ItemId=""AMO_RIFLE"",MinCount=10,MaxCount=20,Weight=15,DropChance=0.6),(ItemId=""ATT_RED_DOT"",MinCount=1,MaxCount=1,Weight=10,DropChance=0.3),(ItemId=""REF_LATHE_HEAD"",MinCount=1,MaxCount=1,Weight=10,DropChance=0.15),(ItemId=""WPN_SERVICE_PISTOL"",MinCount=1,MaxCount=1,Weight=5,DropChance=0.2))"
LOOT_TIER_3,"Tier 3 (Deep / Remnant)",5,False,"((ItemId=""CMP_NANOPROC_CHIP"",MinCount=1,MaxCount=1,Weight=20,DropChance=1.0),(ItemId=""MAT_SILICATE_INGOT"",MinCount=1,MaxCount=3,Weight=20,DropChance=0.8),(ItemId=""REF_REMNANT_SLATE"",MinCount=1,MaxCount=1,Weight=15,DropChance=0.5),(ItemId=""AMO_SNIPER"",MinCount=5,MaxCount=10,Weight=15,DropChance=0.5),(ItemId=""ATT_4X_SCOPE"",MinCount=1,MaxCount=1,Weight=10,DropChance=0.3),(ItemId=""WPN_DMR"",MinCount=1,MaxCount=1,Weight=10,DropChance=0.2),(ItemId=""REF_OPTICS_LENS"",MinCount=1,MaxCount=1,Weight=10,DropChance=0.15))"
"""


def ensure_build_catalog():
    table = _ensure_datatable(BUILD_CATALOG_PATH, BUILD_CATALOG_PKG,
                              BUILD_CATALOG_NAME, BUILD_ROW_STRUCT_PATH)
    if not table:
        return None
    # Skip the bulk fill if the table already has rows (idempotent).
    existing_rows = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    if existing_rows:
        print("[dressup] {} already has {} rows -- skipping fill"
              .format(BUILD_CATALOG_PATH, len(existing_rows)))
        return table
    ok = _fill_datatable_from_csv(table, BUILD_CATALOG_CSV)
    if ok:
        print("[dressup] seeded {} with 19 starter rows".format(BUILD_CATALOG_PATH))
    return table


def ensure_loot_tables():
    table = _ensure_datatable(LOOT_TABLES_PATH, LOOT_TABLES_PKG,
                              LOOT_TABLES_NAME, LOOT_ROW_STRUCT_PATH)
    if not table:
        return None
    existing_rows = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    if existing_rows:
        print("[dressup] {} already has {} rows -- skipping fill"
              .format(LOOT_TABLES_PATH, len(existing_rows)))
        return table
    ok = _fill_datatable_from_csv(table, LOOT_TABLES_CSV)
    if ok:
        print("[dressup] seeded {} with 3 tier rows".format(LOOT_TABLES_PATH))
    return table


def ensure_wildlife_spawner():
    """Spawn AQRWildlifeSpawner if absent, pre-seeded with the v15
    species pool, cap 12, top-up every 8s. Designer can edit the actor
    in the Outliner to change cap / interval / species list."""
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not actor_sub:
        print("[dressup] no EditorActorSubsystem -- skipping wildlife spawner")
        return None

    spawner_cls = getattr(unreal, "QRWildlifeSpawner", None)
    if not spawner_cls:
        print("[dressup] AQRWildlifeSpawner C++ class not visible to Python --")
        print("[dressup]   compile the project first (it ships with the latest commit).")
        return None

    # Idempotent: skip if any spawner already exists.
    for a in actor_sub.get_all_level_actors():
        if isinstance(a, spawner_cls):
            print("[dressup] wildlife spawner already present at {} -- skipping spawn"
                  .format(a.get_actor_label()))
            return a

    loc = unreal.Vector(0.0, 0.0, 200.0)
    rot = unreal.Rotator(0.0, 0.0, 0.0)
    spawner = _try(lambda: unreal.EditorLevelLibrary.spawn_actor_from_class(
        spawner_cls, loc, rot), "wildlife spawner spawn")
    if not spawner:
        return None
    _try(lambda: spawner.set_actor_label("QR_WildlifeSpawner"), "label")

    # Wire the species pool from compiled C++ classes that ship with
    # this checkout. Skip any that aren't compiled into the current
    # build (the user may compile without all species headers).
    species_class_names = [
        "QRWildlife_AshbackBoar",
        "QRWildlife_FogleechSwarm",
        "QRWildlife_GlasshornRunner",
        "QRWildlife_HookjawStalker",
        "QRWildlife_IronstagStalker",
        "QRWildlife_NestweaverDrifter",
        "QRWildlife_PillarbackHauler",
        "QRWildlife_RidgeCourser",
        "QRWildlife_RidgebackGrazer",
        "QRWildlife_ShardbackGrazer",
        "QRWildlife_ShellmawAmbusher",
        "QRWildlife_SiltStrider",
        "QRWildlife_SutureWisp",
        "QRWildlife_ThornhideDray",
        "QRWildlife_TrenchDiggers",
        "QRWildlife_VaneRippers",
        "QRWildlife_VaultbackDray",
    ]
    pool = []
    for nm in species_class_names:
        cls = getattr(unreal, nm, None)
        if cls:
            pool.append(cls)
    if pool:
        _try(lambda: spawner.set_editor_property("species_pool", pool),
             "set species pool ({} entries)".format(len(pool)))

    # Tunables match C++ defaults but spelled out so they're greppable.
    _try(lambda: spawner.set_editor_property("max_alive", 12), "max_alive")
    _try(lambda: spawner.set_editor_property("spawn_interval_seconds", 8.0),
         "spawn_interval_seconds")
    _try(lambda: spawner.set_editor_property("spawn_radius_min", 2000.0),
         "spawn_radius_min")
    _try(lambda: spawner.set_editor_property("spawn_radius_max", 6000.0),
         "spawn_radius_max")
    _try(lambda: spawner.set_editor_property("initial_burst", 6), "initial_burst")
    _try(lambda: spawner.set_editor_property("global_cap", True), "global_cap")

    print("[dressup] QR_WildlifeSpawner placed (cap=12, interval=8s,")
    print("[dressup]   {} species in pool, initial burst 6).".format(len(pool)))
    return spawner


def run():
    print("\n=== qr_dev_test_dressup ===")
    ensure_navmesh_bounds_volume()
    ensure_build_catalog()
    ensure_loot_tables()
    ensure_wildlife_spawner()
    print("[dressup] done.")
    print("[dressup] MANUAL FOLLOW-UPS:")
    print("[dressup]   1. Save the level (Ctrl+S) to trigger the nav build.")
    print("[dressup]   2. Press P in the viewport to verify the green nav overlay.")
    print("[dressup]   3. Open DT_BuildCatalog and assign SM_BLD_* meshes per row.")
    print("[dressup]   4. UQRBuildModeComponent.PieceCatalog needs to point at")
    print("[dressup]      /Game/QuietRift/Data/Build/DT_BuildCatalog in the BP defaults.")
    print("[dressup]   5. AQRLootContainer's loot-table reference needs to point at")
    print("[dressup]      /Game/QuietRift/Data/Loot/DT_LootTables.")


if __name__ == "__main__":
    run()
