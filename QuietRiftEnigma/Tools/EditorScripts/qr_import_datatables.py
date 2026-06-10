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

import os
import unreal


DATA_PKG  = "/Game/QuietRift/Data"

# NOTE: when this file is run via `exec(open(...).read())` in the UE Python
# console, __file__ is NOT defined, so os.path.dirname(__file__) throws
# NameError. Derive the data folder from the project content dir instead --
# that always resolves regardless of how the script was launched.
DATA_DISK = os.path.normpath(
    os.path.join(unreal.Paths.project_content_dir(), "QuietRift", "Data"))


# (csv_basename, asset_name, row_struct_python_class_name)
# struct names are the C++ struct without the leading F.
TABLES = [
    ("DT_ArmoryAmmo.csv",        "DT_ArmoryAmmo",        "QRArmoryAmmoRow"),
    ("DT_ArmoryWeapons.csv",     "DT_ArmoryWeapons",     "QRArmoryWeaponRow"),
    ("DT_ArmoryAttachments.csv", "DT_ArmoryAttachments", "QRArmoryAttachmentRow"),
    ("DT_TechNodes.csv",         "DT_TechNodes",         "QRTechNodeRow"),
    ("DT_Recipes.csv",           "DT_Recipes",           "QRRecipeTableRow"),
]


def _resolve_struct(struct_name):
    """Find a USTRUCT by its Python-exposed name. UE exposes USTRUCTs as
    unreal.<StructName> (no leading F)."""
    cls = getattr(unreal, struct_name, None)
    if cls is None:
        print("[dt-import] WARN: struct {} not visible to Python. Recompile "
              "the module that owns it, then reopen the editor.".format(struct_name))
    return cls


def _delete_asset_if_exists(asset_path):
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)


def _import_one(csv_basename, asset_name, struct_name, overwrite):
    csv_path = os.path.join(DATA_DISK, csv_basename)
    if not os.path.isfile(csv_path):
        print("[dt-import] SKIP {} (CSV not on disk)".format(csv_basename))
        return False

    asset_path = "{}/{}".format(DATA_PKG, asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        if not overwrite:
            print("[dt-import] {} already exists -- skipping (use overwrite=True)".format(asset_name))
            return False
        _delete_asset_if_exists(asset_path)

    struct = _resolve_struct(struct_name)
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
    for csv_name, asset_name, struct_name in TABLES:
        if _import_one(csv_name, asset_name, struct_name, overwrite):
            created += 1
        else:
            skipped += 1
    print("[dt-import] done. created/replaced: {}, skipped: {}.".format(created, skipped))
    print("[dt-import] Reference / design CSVs (DT_LeaderConditions, DT_MainQuests,")
    print("[dt-import]   DT_FoodNutritionStats, etc.) are intentionally not imported")
    print("[dt-import]   -- they don't have a runtime row struct yet.")


if __name__ == "__main__":
    run()
