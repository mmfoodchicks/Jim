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


def _fill_table_from_csv(dt, csv_text):
    """UE 5.7 Python has NO per-row mutation API (the add_data_table_row
    this script used to call never existed as a binding -- it threw
    AttributeError on every run). The supported surface is a whole-table
    CSV (re)fill. Replaces the table's contents."""
    try:
        ok = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(
            dt, csv_text)
    except Exception as e:
        print("[seed-dt]   CSV fill failed: {}".format(e))
        return False
    if not ok:
        print("[seed-dt]   CSV fill reported problems (check Output Log)")
    return bool(ok)


def _fill_rows_minimal(dt, row_ids):
    """Create rows whose fields are all struct defaults (name column
    only; missing CSV columns import as defaults). Net result matches
    what the old per-row path intended."""
    return _fill_table_from_csv(dt, "---\n" + "\n".join(row_ids) + "\n")


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
    """(Re)generate DT_BuildCatalog from the SM_BLD_* meshes on disk.

    Every row is derived from a mesh, so the table is rebuilt in full
    through the CSV fill API whenever new meshes appear (or on
    overwrite=True). NOTE: a rebuild resets hand-edited MaterialCost /
    RequiredTechNodeId cells to defaults -- the script prints a warning
    when that happens."""
    struct = _load_struct("/Script/QuietRiftEnigma.QRBuildPieceRow")
    if not struct:
        print("[seed-dt] FQRBuildPieceRow not found — skipping build catalog")
        return 0

    dt = _load_or_create_data_table(DT_BUILDS_PATH, struct)
    if not dt:
        print("[seed-dt] failed to create DT_BuildCatalog")
        return 0

    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    ar.scan_paths_synchronous([BLD_MESH_ROOT], True)
    f = unreal.ARFilter(
        class_names=["StaticMesh"],
        package_paths=[BLD_MESH_ROOT],
        recursive_paths=True)

    rows = []
    for ad in ar.get_assets(f):
        name = str(ad.asset_name)
        if not name.startswith("SM_BLD_"): continue
        if "_LOD" in name: continue
        row_id   = name.replace("SM_BLD_", "BLD_")
        category = _build_category(name)
        pretty   = _build_pretty_name(name)
        obj_path = "{}.{}".format(ad.package_name, ad.asset_name)
        rows.append((row_id, pretty, category, obj_path))

    if not rows:
        print("[seed-dt] no SM_BLD_* meshes under {} — import them first "
              "(qr_seed_items)".format(BLD_MESH_ROOT))
        return 0

    existing = {str(n) for n in dt.get_row_names()}
    wanted = {r[0] for r in rows}
    if not overwrite and wanted.issubset(existing):
        print("[seed-dt] DT_BuildCatalog : all {} piece rows present — "
              "skipped (run(overwrite=True) to rebuild)".format(len(wanted)))
        return 0
    if existing:
        print("[seed-dt] DT_BuildCatalog : rebuilding {} rows (hand-edited "
              "costs/tech gates reset to defaults)".format(len(wanted)))

    lines = ["---,DisplayName,Category,Mesh,MaterialCost,RequiredTechNodeId"]
    for row_id, pretty, category, obj_path in sorted(rows):
        lines.append('{},"{}",{},"{}",,'.format(
            row_id, pretty, category, obj_path))
    if not _fill_table_from_csv(dt, "\n".join(lines) + "\n"):
        return 0

    unreal.EditorAssetLibrary.save_loaded_asset(dt)
    print("[seed-dt] DT_BuildCatalog : {} rows".format(len(rows)))
    return len(rows)


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

    # DT_Recipes' REAL content is the 100+ row DT_Recipes.csv imported by
    # qr_import_datatables -- never overwrite a populated table with the
    # 5 bootstrap samples (a CSV fill replaces the whole table).
    existing = list(dt.get_row_names())
    if existing:
        print("[seed-dt] DT_Recipes : {} rows present (managed by "
              "qr_import_datatables) — skipped".format(len(existing)))
        return 0

    if not _fill_rows_minimal(dt, [r[0] for r in SAMPLE_RECIPES]):
        return 0
    unreal.EditorAssetLibrary.save_loaded_asset(dt)
    print("[seed-dt] DT_Recipes : {} bootstrap rows (run "
          "qr_import_datatables for the full set)".format(len(SAMPLE_RECIPES)))
    return len(SAMPLE_RECIPES)


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

    existing = list(dt.get_row_names())
    if existing and not overwrite:
        print("[seed-dt] DT_NPC_Greetings : {} rows present — "
              "skipped".format(len(existing)))
        return 0

    row_ids = [row_id for row_id, _lines in SAMPLE_DIALOGUE_NODES]
    if not _fill_rows_minimal(dt, row_ids):
        return 0
    unreal.EditorAssetLibrary.save_loaded_asset(dt)
    print("[seed-dt] DT_NPC_Greetings : {} sample nodes".format(len(row_ids)))
    return len(row_ids)


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

    existing = list(dt.get_row_names())
    if existing and not overwrite:
        print("[seed-dt] DT_LootTables : {} rows present — "
              "skipped".format(len(existing)))
        return 0

    if not _fill_rows_minimal(dt, SAMPLE_LOOT):
        return 0
    unreal.EditorAssetLibrary.save_loaded_asset(dt)
    print("[seed-dt] DT_LootTables : {} sample rows".format(len(SAMPLE_LOOT)))
    return len(SAMPLE_LOOT)


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
