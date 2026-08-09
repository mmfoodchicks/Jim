"""
qr_import_datatables.py -- create UDataTable assets from every CSV in
Content/QuietRift/Data/, paired with the correct C++ row struct. No
editor dialogs, no struct-roulette: each table is created with its
known struct so the rows parse cleanly.

Why this exists: clicking "Reimport" in the editor on a CSV that has no
existing .uasset (the situation we're actually in) drops a "Choose Row
Type" dropdown listing every USTRUCT in the project. Picking wrong
corrupts the table. This script knows the right answer for each CSV.

Tables it knows about:
  DT_ArmoryAmmo        -> FQRArmoryAmmoRow
  DT_ArmoryWeapons     -> FQRArmoryWeaponRow
  DT_ArmoryAttachments -> FQRArmoryAttachmentRow
  DT_TechNodes         -> FQRTechNodeRow
  DT_Recipes           -> FQRRecipeTableRow

Anything else in the Data folder is skipped (those CSVs are reference
docs / design appendices, not runtime tables -- they intentionally
don't have row structs).

Run from the UE Python console:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_import_datatables.py').read())
  run()                    # create missing, leave existing
  run(overwrite=True)      # delete + recreate every table
"""

import csv
import io
import os
import re
import unreal


DATA_PKG  = "/Game/QuietRift/Data"

# NOTE: when this file is run via `exec(open(...).read())` in the UE Python
# console, __file__ is NOT defined, so os.path.dirname(__file__) throws
# NameError. Derive the data folder from the project content dir instead --
# that always resolves regardless of how the script was launched.
DATA_DISK = os.path.normpath(
    os.path.join(unreal.Paths.project_content_dir(), "QuietRift", "Data"))


# (csv_basename, asset_name, row_struct_python_class_name, owning_module)
# Module is needed because UE 5.7's CSVImportSettings.import_row_struct
# wants the UScriptStruct object (load_object via /Script/<Module>.<F>Name),
# NOT the Python-exposed type (getattr(unreal, '<Name>')). The two look
# similar but the property's nativizer rejects the bare type with a
# "Cannot nativize ... as Object (allowed Class type: ScriptStruct)" error.
TABLES = [
    ("DT_ArmoryAmmo.csv",        "DT_ArmoryAmmo",        "QRArmoryAmmoRow",        "QRCombatThreat"),
    ("DT_ArmoryWeapons.csv",     "DT_ArmoryWeapons",     "QRArmoryWeaponRow",      "QRCombatThreat"),
    ("DT_ArmoryAttachments.csv", "DT_ArmoryAttachments", "QRArmoryAttachmentRow",  "QRCombatThreat"),
    ("DT_TechNodes.csv",         "DT_TechNodes",         "QRTechNodeRow",          "QRCombatThreat"),
    ("DT_Recipes.csv",           "DT_Recipes",           "QRRecipeTableRow",       "QRCraftingResearch"),
]


# ─────────────────────────────────────────────────────────────────────────
# DT_Recipes.csv is authored in the DESIGN format
#   RecipeId,Output,Inputs,Time,Building / Station Required,Unlocked By,...
# with display names ("Fiber Bundle ×2; Cloth Patch ×2") and "60s" times,
# while FQRRecipeTableRow wants id-keyed struct columns (Ingredient1..6,
# OutputItemId, CraftTimeSeconds...). Importing the design CSV directly
# produced 243 EMPTY rows — every field defaulted, so runtime crafting had
# no outputs, no ingredients, and 5s times. The converter below rewrites
# the design CSV into a struct-format temp CSV at import time:
#   • id-shaped tokens (AMO_ARROW_FIRE) pass through as-is — many exist
#     only as seeded UQRItemDefinition assets, not in DT_Items_Master.csv
#   • display names resolve via DT_Items_Master.csv's Item column
#   • RequiredTechNodeId comes from DT_TechNodes.csv's UnlockedRecipeIds
#     reverse mapping (the "+"-separated mirror-rule column)
#   • unresolvable outputs drop the row; unresolvable inputs drop the
#     ingredient — both land in the conversion report
# ─────────────────────────────────────────────────────────────────────────

_ID_TOKEN_RE  = re.compile(r'^[A-Z][A-Z0-9]{1,5}_[A-Z0-9_]+$')
_QTY_TOKEN_RE = re.compile(r'^(.*?)\s*[×xX]\s*(\d+)\s*$')


def _parse_time_seconds(raw):
    raw = (raw or "").strip().lower()
    m = re.match(r'^(?:(\d+)m)?\s*(?:(\d+)s?)?$', raw)
    if not m or (m.group(1) is None and m.group(2) is None):
        return 5.0
    return float(m.group(1) or 0) * 60.0 + float(m.group(2) or 0)


def _convert_recipes_to_struct_csv(src_path, items_csv, technodes_csv, out_path):
    """Pure-python transform (no unreal API) — testable outside the editor.
    Returns (row_count, report_lines)."""
    items_by_name = {}
    if os.path.isfile(items_csv):
        with io.open(items_csv, newline='', encoding='utf-8-sig') as f:
            for row in csv.DictReader(f):
                rid = (row.get('ItemId') or '').strip()
                dn  = (row.get('Item') or '').strip().lower()
                if rid and dn:
                    items_by_name.setdefault(dn, rid)

    recipe_to_tech = {}
    if os.path.isfile(technodes_csv):
        with io.open(technodes_csv, newline='', encoding='utf-8-sig') as f:
            for row in csv.DictReader(f):
                tid = (row.get('TechNodeId') or '').strip()
                for rc in (row.get('UnlockedRecipeIds') or '').split('+'):
                    rc = rc.strip()
                    if rc.startswith('RC_') and tid:
                        recipe_to_tech[rc] = tid

    def resolve_token(tok):
        m = _QTY_TOKEN_RE.match(tok.strip())
        name, qty = ((m.group(1), int(m.group(2))) if m else (tok.strip(), 1))
        name = name.strip()
        if not name:
            return None, 0
        if _ID_TOKEN_RE.match(name):
            return name, qty
        return items_by_name.get(name.lower()), qty

    report = []
    out_rows = []
    with io.open(src_path, newline='', encoding='utf-8-sig') as f:
        for row in csv.DictReader(f):
            rid = (row.get('RecipeId') or '').strip()
            if not rid:
                continue
            out_id, out_qty = resolve_token(row.get('Output') or '')
            if not out_id:
                report.append("DROPPED {}: unresolvable output '{}'".format(
                    rid, row.get('Output')))
                continue

            ings = []
            for tok in (row.get('Inputs') or '').split(';'):
                if not tok.strip():
                    continue
                iid, iqty = resolve_token(tok)
                if iid:
                    ings.append((iid, iqty))
                else:
                    report.append("{}: dropped unresolvable input '{}'".format(
                        rid, tok.strip()))
            ings = ings[:6]

            rec = {
                '---':                    rid,
                'DisplayName':            (row.get('Output') or '').split('×')[0].strip(),
                'RequiredStation':        '',
                'RequiredTier':           'T0_Primitive',
                'RequiredTechNodeId':     recipe_to_tech.get(rid, ''),
                'RequiredReferenceComponentId': '',
                'OutputItemId':           out_id,
                'OutputQty':              out_qty,
                'Output2ItemId':          '',
                'Output2Qty':             0,
                'Output2YieldChance':     0.0,
                'CraftTimeSeconds':       _parse_time_seconds(row.get('Time')),
                'bNPCOnly':               'False',
            }
            for i in range(6):
                iid, iqty = (ings[i] if i < len(ings) else ('', 0))
                rec['Ingredient{}'.format(i + 1)]         = iid
                rec['Ingredient{}Qty'.format(i + 1)]      = iqty
                rec['Ingredient{}Reusable'.format(i + 1)] = 'False'
            out_rows.append(rec)

    header = ['---', 'DisplayName', 'RequiredStation', 'RequiredTier',
              'RequiredTechNodeId', 'RequiredReferenceComponentId']
    for i in range(6):
        header += ['Ingredient{}'.format(i + 1), 'Ingredient{}Qty'.format(i + 1),
                   'Ingredient{}Reusable'.format(i + 1)]
    header += ['OutputItemId', 'OutputQty', 'Output2ItemId', 'Output2Qty',
               'Output2YieldChance', 'CraftTimeSeconds', 'bNPCOnly']

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with io.open(out_path, 'w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=header)
        w.writeheader()
        for rec in out_rows:
            w.writerow(rec)

    return len(out_rows), report


def _resolve_struct(struct_name, module):
    """Return the UScriptStruct OBJECT for /Script/<module>.F<name>. UE
    5.7's CSVImportSettings.import_row_struct expects a ScriptStruct
    pointer, which load_object hands back; getattr(unreal, name) returns
    the Python TYPE which the nativizer rejects."""
    path = "/Script/{}.{}".format(module, struct_name)
    obj = unreal.load_object(None, path)
    if obj is None:
        print("[dt-import] WARN: struct {} not found at {}. Recompile "
              "the module that owns it, then reopen the editor.".format(struct_name, path))
    return obj


def _delete_asset_if_exists(asset_path):
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)


def _import_one(csv_basename, asset_name, struct_name, module, overwrite):
    csv_path = os.path.join(DATA_DISK, csv_basename)
    if not os.path.isfile(csv_path):
        print("[dt-import] SKIP {} (CSV not on disk)".format(csv_basename))
        return False

    # Recipes need the design→struct conversion pass (see comment above).
    if csv_basename == "DT_Recipes.csv":
        gen_dir  = os.path.join(DATA_DISK, "_generated")
        gen_path = os.path.join(gen_dir, "DT_Recipes_struct.csv")
        n, report = _convert_recipes_to_struct_csv(
            csv_path,
            os.path.join(DATA_DISK, "DT_Items_Master.csv"),
            os.path.join(DATA_DISK, "DT_TechNodes.csv"),
            gen_path)
        report_path = os.path.join(gen_dir, "DT_Recipes_conversion_report.txt")
        with io.open(report_path, 'w', encoding='utf-8') as f:
            f.write("\n".join(report))
        print("[dt-import] recipes converted: {} rows, {} warnings -> {}".format(
            n, len(report), report_path))
        for line in report[:10]:
            print("[dt-import]   " + line)
        if len(report) > 10:
            print("[dt-import]   ... ({} more in the report file)".format(len(report) - 10))
        csv_path = gen_path

    asset_path = "{}/{}".format(DATA_PKG, asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        if not overwrite:
            print("[dt-import] {} already exists -- skipping (use overwrite=True)".format(asset_name))
            return False
        _delete_asset_if_exists(asset_path)

    struct = _resolve_struct(struct_name, module)
    if struct is None:
        return False

    # CSVImportFactory drives DataTable creation from CSV. The factory's
    # AutomatedAssetImportData carries the destination path and row struct.
    factory = unreal.CSVImportFactory()
    opts = factory.get_editor_property("automated_import_settings")
    opts.import_row_struct = struct

    task = unreal.AssetImportTask()
    task.set_editor_property("filename",         csv_path)
    task.set_editor_property("destination_path", DATA_PKG)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save",             True)
    task.set_editor_property("automated",        True)
    task.set_editor_property("factory",          factory)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    tools.import_asset_tasks([task])

    if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        print("[dt-import] FAILED to create {}".format(asset_path))
        return False

    print("[dt-import] {} <- {}".format(asset_name, csv_basename))
    return True


def run(overwrite=False):
    print("\n=== qr_import_datatables ===")
    print("[dt-import] data folder: {}".format(DATA_DISK))
    created = 0
    skipped = 0
    for csv_name, asset_name, struct_name, module in TABLES:
        if _import_one(csv_name, asset_name, struct_name, module, overwrite):
            created += 1
        else:
            skipped += 1
    print("[dt-import] done. created/replaced: {}, skipped: {}.".format(created, skipped))
    print("[dt-import] Reference / design CSVs (DT_LeaderConditions, DT_MainQuests,")
    print("[dt-import]   DT_FoodNutritionStats, etc.) are intentionally not imported")
    print("[dt-import]   -- they don't have a runtime row struct yet.")


if __name__ == "__main__":
    run()
