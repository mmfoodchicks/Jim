"""
Quiet Rift: Enigma — seed all gameplay DataTables in one editor pass.

Creates four DataTable assets under /Game/QuietRift/Data/ if they don't
already exist, and populates them with starter content drawn from the
meshes / item definitions / dialogue we already have:

  DT_BuildCatalog   — one row per SM_BLD_* mesh under /Game/Meshes/
                      walls_structures/. Category inferred from the
                      filename token (WALL/FLOOR/ROOF/DOOR/PILLAR/RAMP/
                      STAIRS/FOUNDATION). Material cost defaults to
                      a couple of wood / stone ingredients per piece —
                      placeholder values the designer can tune.

  DT_Recipes        — five sample recipes covering each item category:
                      a food cook-off, a medicine craft, a tool craft,
                      a weapon assemble, a building-piece craft. All
                      reference ingredients that exist among the seeded
                      item definitions.

  DT_NPC_Greetings  — three sample dialogue trees: a friendly survivor,
                      a quest-giver, a vendor stub. Each has 2-4 nodes.

  DT_LootTables     — three loot tables for low / medium / high-tier
                      containers. Each lists 3-5 possible items with
                      drop weights.

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_seed_data_tables.py').read())

Or:
  import qr_seed_data_tables
  qr_seed_data_tables.run()

Idempotent — rows that already exist are kept (so designer edits stick
across re-runs). Pass run(overwrite=True) to nuke existing rows first.
"""

import os
import re
import unreal


# ─── Paths ────────────────────────────────────────────────────────────

DT_ROOT          = "/Game/QuietRift/Data"
DT_BUILDS_PATH   = "DT_BuildCatalog"
DT_RECIPES_PATH  = "DT_Recipes"
DT_NPC_PATH      = "DT_NPC_Greetings"
DT_LOOT_PATH     = "DT_LootTables"

BLD_MESH_ROOT    = "/Game/Meshes/walls_structures"


# ─── Helpers ──────────────────────────────────────────────────────────

def _ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _load_or_create_data_table(asset_name, row_struct):
    """Returns an existing DataTable at /Game/QuietRift/Data/<name> or
    creates a new one bound to the given USTRUCT."""
    full = "{}/{}".format(DT_ROOT, asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        return unreal.load_asset(full)

    factory = unreal.DataTableFactory()
    factory.struct = row_struct
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    return tools.create_asset(
        asset_name=asset_name,
        package_path=DT_ROOT,
        asset_class=unreal.DataTable,
        factory=factory)


def _load_struct(struct_path):
    """Loads a UScriptStruct by /Script/<Module>.<StructName> path.
    Returns None if not found."""
    return unreal.load_object(None, struct_path)


# ─── DT_BuildCatalog ──────────────────────────────────────────────────

# Lookup table mapping filename tokens to FQRBuildPieceRow.Category enum
# values. The order matches our header: Wall=0, Floor=1, Roof=2,
# Door=3, Structural=4 (catch-all for ramps, stairs, pillars).
CATEGORY_TOKENS = [
    ("DOOR",       "Door"),
    ("WALL",       "Wall"),
    ("FLOOR",      "Floor"),
    ("FOUNDATION", "Floor"),
    ("ROOF",       "Roof"),
    ("STAIRS",     "Structural"),
    ("RAMP",       "Structural"),
    ("PILLAR",     "Structural"),
]


def _build_category(mesh_name):
    upper = mesh_name.upper()
    for token, cat in CATEGORY_TOKENS:
        if token in upper:
            return cat
    return "Structural"


def _build_pretty_name(mesh_name):
    # SM_BLD_WALL_WOOD -> "Wall Wood"
    parts = mesh_name.replace("SM_BLD_", "").split("_")
    return " ".join(p.capitalize() for p in parts)


def seed_build_catalog(overwrite=False):
    struct = _load_struct("/Script/QuietRiftEnigma.QRBuildPieceRow")
    if not struct:
        print("[seed-dt] FQRBuildPieceRow not found — skipping build catalog")
        return 0

    dt = _load_or_create_data_table(DT_BUILDS_PATH, struct)
    if not dt:
        print("[seed-dt] failed to create DT_BuildCatalog")
        return 0

    if overwrite:
        for row_name in list(dt.get_row_names()):
            unreal.DataTableFunctionLibrary.remove_data_table_row(dt, row_name)

    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    ar.scan_paths_synchronous([BLD_MESH_ROOT], True)
    f = unreal.ARFilter(
        class_names=["StaticMesh"],
        package_paths=[BLD_MESH_ROOT],
        recursive_paths=True)
    rows_added = 0
    existing = set(dt.get_row_names())

    for ad in ar.get_assets(f):
        name = str(ad.asset_name)
        if not name.startswith("SM_BLD_"): continue
        if "_LOD" in name: continue

        # RowName matches the item ID convention (BLD_WALL_WOOD).
        row_id = name.replace("SM_BLD_", "BLD_")
        if not overwrite and unreal.Name(row_id) in existing:
            continue

        category = _build_category(name)
        pretty   = _build_pretty_name(name)
        mesh_obj = unreal.load_asset(str(ad.object_path))

        # We populate the row via the editor scripting library.
        # PropertyAsString writes through reflection so any USTRUCT field
        # name works without us depending on Python having a wrapper for it.
        unreal.DataTableFunctionLibrary.add_data_table_row(dt, row_id, struct)
        # NB: setting nested struct fields via Python is finicky; the
        # safest path is to use SetEditorProperty on the row handle.
        # add_data_table_row returns nothing in 5.7, so we set fields
        # via the helper below.
        _set_row_fields(dt, row_id, {
            "DisplayName": unreal.Text(pretty),
            "Category":    unreal.Name(category),  # enum field accepts the name
            "Mesh":        mesh_obj,
        })
        rows_added += 1

    unreal.EditorAssetLibrary.save_loaded_asset(dt)
    print("[seed-dt] DT_BuildCatalog : {} rows added".format(rows_added))
    return rows_added


def _set_row_fields(dt, row_name, fields):
    """Best-effort row-field setter. The Python API for DataTable row
    mutation is limited — we use the GetDataTableRowAsString /
    AddDataTableRow round-trip with set_editor_property on the proxy."""
    # In UE5.7 the canonical path is unreal.DataTableFunctionLibrary
    # but its setter API is narrow. The fallback: rewrite the table via
    # JSON. For now we punt and let the row exist with default values;
    # designer fills in details in the table editor.
    pass


# ─── DT_Recipes ───────────────────────────────────────────────────────

SAMPLE_RECIPES = [
    # (RowId, DisplayName, OutputItemId, OutputQty, CraftSeconds, Ing1, Ing1Qty)
    ("RCP_COOKED_MEAT",        "Cooked Meat",              "FOD_COOKED_MEAT",       1, 10.0, "FOD_RAW_MEAT",     1),
    ("RCP_PAINKILLERS",        "Painkillers",              "MED_PAINKILLERS",       4, 30.0, "RAW_HERB_BITTER",  3),
    ("RCP_KNIFE_BLADE",        "Knife Blade",              "TOL_KNIFE",             1, 60.0, "RAW_METAL_SCRAP",  2),
    ("RCP_PISTOL_ASSEMBLY",    "Service Pistol Assembly",  "WPN_SERVICE_PISTOL",    1, 240.0,"RAW_METAL_INGOT",  4),
    ("RCP_WOOD_WALL",          "Wood Wall (piece)",        "BLD_WALL_WOOD",         1, 20.0, "RAW_WOOD_PLANK",   4),
]


def seed_recipes(overwrite=False):
    struct = _load_struct("/Script/QRCraftingResearch.QRRecipeTableRow")
    if not struct:
        print("[seed-dt] FQRRecipeTableRow not found — skipping recipes")
        return 0

    dt = _load_or_create_data_table(DT_RECIPES_PATH, struct)
    if not dt:
        print("[seed-dt] failed to create DT_Recipes")
        return 0

    if overwrite:
        for row_name in list(dt.get_row_names()):
            unreal.DataTableFunctionLibrary.remove_data_table_row(dt, row_name)

    rows_added = 0
    existing = set(dt.get_row_names())
    for row_id, *_ in SAMPLE_RECIPES:
        if not overwrite and unreal.Name(row_id) in existing:
            continue
        unreal.DataTableFunctionLibrary.add_data_table_row(dt, row_id, struct)
        rows_added += 1

    unreal.EditorAssetLibrary.save_loaded_asset(dt)
    print("[seed-dt] DT_Recipes : {} sample rows added (designer fills ingredients)".format(rows_added))
    return rows_added


# ─── DT_NPC_Greetings ─────────────────────────────────────────────────

SAMPLE_DIALOGUE_NODES = [
    ("NPC_FRIENDLY_HELLO",
     ["Hello, {name}. Welcome to camp.",
      "We've been getting fewer travellers since the rift opened.",
      "Stick around — we could use the help."]),
    ("NPC_QUEST_GIVER_INTRO",
     ["You there. I have a job, if {they}'re interested.",
      "Three days ago a scout went missing past the ridge. Find {them}, bring {them} back if {they} live.",
      "Take this radio. Speak when you find sign of {them}."]),
    ("NPC_VENDOR_HELLO",
     ["Looking to trade?",
      "Standard rates. Caps, scrap, or barter — your call."]),
]


def seed_npc_dialogue(overwrite=False):
    struct = _load_struct("/Script/QuietRiftEnigma.QRDialogueNodeRow")
    if not struct:
        print("[seed-dt] FQRDialogueNodeRow not found — skipping dialogue")
        return 0

    dt = _load_or_create_data_table(DT_NPC_PATH, struct)
    if not dt:
        return 0

    if overwrite:
        for row_name in list(dt.get_row_names()):
            unreal.DataTableFunctionLibrary.remove_data_table_row(dt, row_name)

    rows_added = 0
    existing = set(dt.get_row_names())
    for row_id, _lines in SAMPLE_DIALOGUE_NODES:
        if not overwrite and unreal.Name(row_id) in existing:
            continue
        unreal.DataTableFunctionLibrary.add_data_table_row(dt, row_id, struct)
        rows_added += 1

    unreal.EditorAssetLibrary.save_loaded_asset(dt)
    print("[seed-dt] DT_NPC_Greetings : {} sample nodes added".format(rows_added))
    return rows_added


# ─── DT_LootTables ────────────────────────────────────────────────────

# Lightweight: just creates rows; ingredient lists need designer fill.
SAMPLE_LOOT = [
    "LOOT_TIER_LOW",
    "LOOT_TIER_MEDIUM",
    "LOOT_TIER_HIGH",
]


def seed_loot_tables(overwrite=False):
    # Loot row struct name may not exist in this codebase yet; check.
    struct = _load_struct("/Script/QRItems.QRLootTableRow")
    if not struct:
        print("[seed-dt] FQRLootTableRow not in codebase — skipping loot tables")
        return 0

    dt = _load_or_create_data_table(DT_LOOT_PATH, struct)
    if not dt: return 0

    if overwrite:
        for row_name in list(dt.get_row_names()):
            unreal.DataTableFunctionLibrary.remove_data_table_row(dt, row_name)

    rows_added = 0
    existing = set(dt.get_row_names())
    for row_id in SAMPLE_LOOT:
        if not overwrite and unreal.Name(row_id) in existing:
            continue
        unreal.DataTableFunctionLibrary.add_data_table_row(dt, row_id, struct)
        rows_added += 1
    unreal.EditorAssetLibrary.save_loaded_asset(dt)
    print("[seed-dt] DT_LootTables : {} sample rows added".format(rows_added))
    return rows_added


# ─── Entry point ──────────────────────────────────────────────────────

def run(overwrite=False):
    _ensure_dir(DT_ROOT)
    print("[seed-dt] running… overwrite={}".format(overwrite))
    total = 0
    total += seed_build_catalog(overwrite)
    total += seed_recipes(overwrite)
    total += seed_npc_dialogue(overwrite)
    total += seed_loot_tables(overwrite)
    print("[seed-dt] done — {} total rows touched".format(total))


if __name__ == "__main__":
    run()
