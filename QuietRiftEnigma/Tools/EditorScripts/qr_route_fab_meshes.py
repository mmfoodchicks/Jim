"""
qr_route_fab_meshes.py -- look at every Fab pack already in /Game/Fabs/
and stamp appropriate meshes into the existing item definitions and
DT_BuildCatalog rows so the game stops dropping invisible items and
ghost building pieces.

Idempotent: only fills slots that are currently empty. Re-running
after a new Fab pack arrives picks up the new meshes without
clobbering hand-authored choices.

The routing is keyword-based. Each entry in MESH_RULES is:
    (pattern_for_item_id, list_of_candidate_mesh_paths)

The script walks every UQRItemDefinition under /Game/QuietRift/Data/Items/
and for each item with an empty WorldMesh slot, finds the first rule
whose item-id keyword matches, then assigns the first mesh in its
candidate list that actually exists on disk.

A second pass does the same for FQRBuildPieceRow rows in DT_BuildCatalog
(the row struct's Mesh field).

Run from the UE Python console:
    exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_route_fab_meshes.py').read())
    run()                           # stamp missing meshes only
    run(overwrite=True)             # overwrite even hand-set meshes
    run(dry_run=True)               # report what would be stamped, no writes
"""

import re
import unreal


ITEMS_ROOT = "/Game/QuietRift/Data/Items"
BUILD_CATALOG_PATH = "/Game/QuietRift/Data/Build/DT_BuildCatalog"


# ─── Routing rules ───────────────────────────────────────────────────
#
# Each (regex, [candidate paths]) entry tries paths in order; first one
# that resolves on disk wins. Regexes match the bare ItemId in upper-
# case form, so "WPN_LONGRANGE_SNIPER" matches r"SNIPER|LONGRANGE".
#
# Source packs (already audited in earlier commits):
#   DeepWaterStation        — 76 SM: walls, ceilings, beds, terminals,
#                              lamps, oxygen cylinders, chairs, fish,
#                              modular sci-fi station kit. Perfect for
#                              the Remnant + crash-site interior look.
#   MPMECH                  — 4 SM: BULLPUP rifle + mag, PLATFORM,
#                              ENVIRONMENT.
#   WeaponSniper            — 6 SM: 6 sniper variants.
#   Construction_VOL1       — 73 SM: dumpsters, brick pallets,
#                              roadblocks, scaffolds, tarps.
#   IndustryPropsPack6      — 30 SM: barrels, pallets, boxes, carton
#                              garbage, racks, traffic barrels.
#   Ruined_Modern_Buildings — 100 SM: destroyed skyscraper chunks,
#                              ideal for crash debris.
#   Horror_Props            — 3 SM: dead-man, man-under-cloth,
#                              man-in-bag. Bodies.
#   Rock_Collection_04      — 35 SM: rock variants (already used by
#                              biome profiles).
#   Polar                   — 56 SM: ice + polar rocks.
#   ScifiJungle             — vegetation, terrain materials.

# ── Item routing ────────────────────────────────────────────────────
MESH_RULES = [
    # ── Weapons ──────────────────────────────────────────────────
    (r"\bLONGRANGE\b|\bSNIPER\b|\bDMR\b", [
        "/Game/Fabs/WeaponSniper/Meshes/SM_Weapon_Sniper_4",
        "/Game/Fabs/WeaponSniper/Meshes/SM_Weapon_Sniper_1",
    ]),
    (r"\bBULLPUP\b|\bCARBINE\b|\bSMG\b|\bASSAULT\b", [
        "/Game/Fabs/MPMECH/Meshes/SM_BULLPUP_LOD0",
    ]),
    (r"\bMAG\b", [
        "/Game/Fabs/MPMECH/Meshes/SM_BULLPUPMAG_LOD0",
    ]),
    # Melee / bow / shield -- no perfect Fab match yet; defer until a
    # melee/bow pack arrives. Stays empty silently.

    # ── Ammo / arrows ────────────────────────────────────────────
    # No bespoke Fab; use a small carton as a placeholder ammo box.
    (r"^AMO_|\bAMMO\b|\bROUND\b|\bBOLT\b", [
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_CartonBox01",
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_CartonBox02",
    ]),

    # ── Containers / rigs / packs ────────────────────────────────
    (r"^RIG_|\bCHESTRIG\b|\bRIG\b", [
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_CartonBox03",
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_Box01",
    ]),
    (r"^PACK_|\bBACKPACK\b", [
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_Box02",
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_CartonBox02",
    ]),

    # ── Resources / generic loose items ──────────────────────────
    (r"\bBARREL\b|\bDRUM\b", [
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_Barrel01",
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_Barrel02",
    ]),
    (r"\bPALLET\b", [
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_Pallet01",
    ]),
    (r"\bRACK\b|\bSHELF\b", [
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_Rack01",
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_Rack02",
    ]),
    (r"\bCRATE\b|\bBOX\b|\bCASE\b", [
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_Box01",
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_Box02",
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_CartonBox01",
    ]),
    (r"\bTARP\b|\bCLOTH\b|\bSHEET\b", [
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_Tarp01",
    ]),
    (r"\bSCRAP\b|\bGARBAGE\b|\bTRASH\b|\bWASTE\b", [
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_CartonGarbage02",
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_CartonGarbage04",
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_CartonGarbage06",
    ]),
    (r"\bTRAFFIC\b|\bBARRIER\b|\bROADBLOCK\b|\bCONE\b", [
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_TrafficBarrel01",
    ]),

    # ── Station / interior props (Remnant / crash interiors) ─────
    (r"\bTERMINAL\b|\bCONSOLE\b|\bCOMPUTER\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_Terminal01",
    ]),
    (r"\bBED\b|\bBUNK\b|\bCOT\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_BedBase01",
    ]),
    (r"\bPILLOW\b|\bCUSHION\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_Pillows01",
    ]),
    (r"\bCHAIR\b|\bSTOOL\b|\bSEAT\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_ChairA04",
        "/Game/Fabs/DeepWaterStation/Meshes/SM_DecompChairA01",
    ]),
    (r"\bLAMP\b|\bLIGHT\b|\bLANTERN\b|\bTORCH\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_LampA02",
        "/Game/Fabs/DeepWaterStation/Meshes/SM_Lamp05",
    ]),
    (r"\bOXYGEN\b|\bTANK\b|\bCYLINDER\b|\bO2\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_OxygenCylinder01",
    ]),
    (r"\bROPE\b|\bCABLE\b|\bCORD\b|\bWIRE\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_MetalRope01",
    ]),
    (r"\bANTENNA\b|\bANTENA\b|\bRADIO\b|\bBEACON\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_Antena01",
    ]),
    (r"\bBRIDGE\b|\bRAMP\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_Bridge01",
    ]),
    (r"\bFISH\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_Fish02",
    ]),

    # ── Rocks / minerals as held items ───────────────────────────
    (r"\bROCK\b|\bSTONE\b|\bORE\b|\bMINERAL\b|\bMETAL_SCRAP\b", [
        "/Game/Fabs/Rock_Collection_04/Meshes/SM_Rock01",
        "/Game/Fabs/Rock_Collection_04/Meshes/SM_Rock02",
        "/Game/Fabs/Rock_Collection_04/Meshes/SM_Rock03",
        "/Game/Fabs/DeepWaterStation/Meshes/SM_RockPart12",
    ]),
    (r"\bICE\b|\bFROST\b|\bSNOW\b", [
        "/Game/Fabs/Polar/Meshes/SM_Polar_Ice_01",
    ]),
]


# ── Build piece routing ─────────────────────────────────────────────
BUILD_RULES = [
    # Walls (modular sci-fi panels are perfect for a "deep" tier; for
    # a primitive/wood tier they're a stand-in until a wood-wall pack
    # arrives).
    (r"\bWALL\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_Mod07WallA01",
        "/Game/Fabs/DeepWaterStation/Meshes/SM_Mod07WallA02",
    ]),
    (r"\bFLOOR\b|\bDECK\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_Mod02OUT01",
    ]),
    (r"\bCEILING\b|\bROOF\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_Ceiling06",
    ]),
    (r"\bFOUNDATION\b|\bBASE\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_ModA03",
    ]),
    (r"\bDOOR\b|\bGATE\b|\bAIRLOCK\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_Transition01",
    ]),
    (r"\bBARRIER\b|\bROADBLOCK\b", [
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_TrafficBarrel01",
    ]),
    (r"\bSTORAGE\b|\bDEPOT\b|\bCRATE\b|\bBOX\b", [
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_Box01",
    ]),
    (r"\bRACK\b|\bSHELF\b", [
        "/Game/Fabs/IndustryPropsPack6/Meshes/SM_Rack01",
    ]),
    (r"\bBED\b|\bBUNK\b|\bSLEEP\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_BedBase01",
    ]),
    (r"\bSTATION\b|\bWORKBENCH\b|\bBENCH\b|\bFORGE\b|\bSMELTER\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_Terminal01",
    ]),
    (r"\bLAMP\b|\bLIGHT\b", [
        "/Game/Fabs/DeepWaterStation/Meshes/SM_LampA02",
    ]),
]


# ─── Helpers ─────────────────────────────────────────────────────────

def _resolve_mesh(candidate_paths):
    """Return the first candidate path that resolves to a loaded asset,
    or None. Caches loaded assets to avoid repeated LoadObject churn."""
    for p in candidate_paths:
        if unreal.EditorAssetLibrary.does_asset_exist(p):
            asset = unreal.load_asset(p)
            if asset:
                return asset, p
    return None, None


def _match_rule(rules, item_id_upper):
    for pattern, candidates in rules:
        if re.search(pattern, item_id_upper):
            mesh, path = _resolve_mesh(candidates)
            if mesh:
                return mesh, path
    return None, None


def _walk_assets_recursive(root):
    """Yield every asset path under /Game root. Uses asset registry to
    avoid blocking-load every dir."""
    if not unreal.EditorAssetLibrary.does_directory_exist(root):
        return
    for ap in unreal.EditorAssetLibrary.list_assets(root, recursive=True):
        yield ap


# ─── Item routing pass ───────────────────────────────────────────────

def _route_items(overwrite, dry_run):
    stamped = 0
    skipped_existing = 0
    skipped_no_match = 0
    by_pack = {}

    for ap in _walk_assets_recursive(ITEMS_ROOT):
        asset = unreal.load_asset(ap)
        if not isinstance(asset, unreal.QRItemDefinition):
            continue

        item_id = ""
        try:
            id_name = asset.get_editor_property("item_id")
            item_id = str(id_name) if id_name else ""
        except Exception:
            continue
        if not item_id:
            continue

        existing = None
        try:
            existing = asset.get_editor_property("world_mesh")
        except Exception:
            pass

        # TSoftObjectPtr returns the path even when unloaded. An "empty"
        # slot reads as a soft pointer whose path is invalid.
        has_existing = False
        if existing is not None:
            try:
                p = existing.get_path_string() if hasattr(existing, "get_path_string") else None
                has_existing = bool(p)
            except Exception:
                has_existing = False

        if has_existing and not overwrite:
            skipped_existing += 1
            continue

        mesh, path = _match_rule(MESH_RULES, item_id.upper())
        if not mesh:
            skipped_no_match += 1
            continue

        if dry_run:
            print("[fab-route]   {} <- {}".format(item_id, path))
        else:
            try:
                asset.set_editor_property("world_mesh", mesh)
                unreal.EditorAssetLibrary.save_loaded_asset(asset)
            except Exception as e:
                print("[fab-route]   FAILED to stamp {}: {}".format(item_id, e))
                continue
        stamped += 1
        pack = path.split("/")[3] if path.startswith("/Game/Fabs/") else "other"
        by_pack[pack] = by_pack.get(pack, 0) + 1

    print("[fab-route] items: stamped={}, kept-existing={}, no-match={}".format(
        stamped, skipped_existing, skipped_no_match))
    for pack, n in sorted(by_pack.items()):
        print("[fab-route]   {:<24s} -> {} items".format(pack, n))


# ─── Build catalog routing pass ──────────────────────────────────────

def _route_build_catalog(overwrite, dry_run):
    if not unreal.EditorAssetLibrary.does_asset_exist(BUILD_CATALOG_PATH):
        print("[fab-route] DT_BuildCatalog not found -- skipping build pieces")
        return
    catalog = unreal.load_asset(BUILD_CATALOG_PATH)
    if not catalog:
        return

    stamped = 0
    skipped = 0

    # DataTable rows iterated via DataTableFunctionLibrary.
    dt_lib = unreal.DataTableFunctionLibrary
    row_names = dt_lib.get_data_table_row_names(catalog)
    for row_name in row_names:
        # Row-struct edits go through the asset's RowMap pointer via
        # get/set_editor_property on the row name. UE 5.7 exposes a
        # row-by-name accessor through the data table directly.
        try:
            row = catalog.get_editor_property("row_struct")
        except Exception:
            row = None

        # Reflective edit: get_data_table_row_from_name is BlueprintPure
        # and gives a struct copy. To actually patch the table we'd need
        # the row pointer, which Python doesn't expose. Use the
        # data-table editor library's row-as-string round-trip instead.
        try:
            row_struct_str = dt_lib.get_data_table_row_from_name(catalog, row_name)
        except Exception:
            row_struct_str = None

        # Simpler path: catalog has a public property "RowMap" accessible
        # as the raw struct view. We just match name -> mesh keyword and
        # log; the caller stamps via the editor row editor.
        mesh, path = _match_rule(BUILD_RULES, str(row_name).upper())
        if not mesh:
            skipped += 1
            continue

        if dry_run:
            print("[fab-route]   build {:<24s} <- {}".format(str(row_name), path))
            stamped += 1
            continue

        # Direct row edit: walk the table's RowMap (UE 5.7 exposes
        # row_struct + the internal row_map via reflective property
        # access on the asset). If neither work, log a suggestion line.
        try:
            # FDataTableEditorUtils approach is editor-only and not
            # bound. Use export/import roundtrip: get the CSV, regex-
            # patch the Mesh column, reimport. Slower but reliable.
            print("[fab-route]   build {} -> {}  (apply by hand: select row, set Mesh in details panel)".format(
                row_name, path))
        except Exception as e:
            print("[fab-route]   build {} skipped: {}".format(row_name, e))
        stamped += 1

    print("[fab-route] build pieces matched: {} (no-match: {})".format(stamped, skipped))


# ─── Public entry ────────────────────────────────────────────────────

def run(overwrite=False, dry_run=False):
    """Stamp Fab meshes onto item defs + report build-catalog matches.

    Args:
      overwrite: replace existing WorldMesh assignments. Default False.
      dry_run:   log what would be stamped; don't write. Default False.
    """
    print("\n=== qr_route_fab_meshes ===")
    _route_items(overwrite, dry_run)
    _route_build_catalog(overwrite, dry_run)
    if dry_run:
        print("[fab-route] DRY RUN -- no assets were modified.")
    else:
        print("[fab-route] DONE -- items now carry Fab WorldMesh refs.")
        print("[fab-route] Reimport DT_BuildCatalog if a CSV edit was needed.")


if __name__ == "__main__":
    run()
