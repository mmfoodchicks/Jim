"""
qr_seed_drop_items.py — create UQRItemDefinition assets for every item id
that gameplay code DROPS or SCATTERS but that no other pipeline creates.

The 2026-08-09 full-game audit cross-checked every hardcoded item id in
C++ (wildlife DeathDrops, flora harvest yields, crash-site loot tables,
POI loot lists) against DT_Items_Master.csv, every qr_seed_*.py script,
and the SM_<Id> mesh library. 112 ids exist nowhere — so wildlife loot,
flora harvests, and wreck loot resolve to null definitions and the items
either vanish or spawn as meshless ghosts.

This seeder creates a minimal, functional definition per id (category
from the prefix, prettified display name, sane mass/stack defaults) into
the standard bucket layout /Game/QuietRift/Data/Items/<Bucket>/<Id>.
No recipes are added: these are raw drops/harvests (recipe mirror rule
applies to craftables; raw inputs need no recipe rows).

Idempotent — existing assets are left alone unless run(overwrite=True).

Run from the UE Python console:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_seed_drop_items.py').read())
  run()
"""

import unreal

ITEMS_PKG_ROOT = "/Game/QuietRift/Data/Items"

# id -> (bucket, category enum name, mass_kg, max_stack)
# Category names must match EQRItemCategory entries.
PREFIX_RULES = {
    "FOD_": ("Food",      "FOOD",      0.4, 20),
    "MAT_": ("Materials", "MATERIAL",  0.8, 50),
    "RAW_": ("Materials", "MATERIAL",  0.5, 50),
    "PLT_": ("Flora",     "MATERIAL",  0.3, 20),
    "MED_": ("Medical",   "MEDICINE",  0.2, 10),
    "TOL_": ("Handheld",  "TOOL",      1.0, 1),
    "ATT_": ("AmmoAttachments", "ATTACHMENT", 0.5, 1),
    "AMM_": ("AmmoAttachments", "AMMO",       0.02, 120),
    "REM_": ("Remnant",   "MATERIAL",  0.6, 10),
}

DROP_ITEM_IDS = [
    "ATT_OPTIC_RDS", "ATT_OPTIC_SCOPE",
    "FOD_BOAR_MEAT", "FOD_DRAY_MEAT_LARGE", "FOD_DRIED_FRUIT",
    "FOD_DRIFTER_MEAT", "FOD_GRAZER_MEAT_LARGE", "FOD_HAULER_MEAT_LARGE",
    "FOD_IRONSTAG_MEAT", "FOD_LANTERN_BERRY_RIPE", "FOD_MELTPOD_GEL",
    "FOD_MRE_ASSORTED", "FOD_RATION_BAR", "FOD_REED_GRAIN",
    "FOD_RIPPER_MEAT", "FOD_RUNNER_MEAT", "FOD_RUSTCAP_CAP",
    "FOD_SHARDBACK_MEAT", "FOD_SHELLMAW_MEAT", "FOD_STALKER_MEAT",
    "FOD_STRIDER_MEAT",
    "MAT_ASTERBARK_FLECK", "MAT_BOAR_HIDE", "MAT_BOAR_TUSK",
    "MAT_BONE_DENSE", "MAT_BONE_HEAVY", "MAT_BONE_LARGE", "MAT_BONE_MEDIUM",
    "MAT_BRANCH", "MAT_BUOYANT_SAC", "MAT_CERAMIC_PLATE",
    "MAT_CHEST_PLATE_IRON", "MAT_CINDER_THORN_ASH", "MAT_COURSER_HIDE",
    "MAT_DIGGER_CARAPACE", "MAT_DRAY_HIDE", "MAT_EMBER_THORN",
    "MAT_FERRIC_ANTLER", "MAT_FERRIC_PETAL", "MAT_FIBER_LONG",
    "MAT_FILAMENT_CORD", "MAT_FUNGAL_SPORE", "MAT_GLASSBARK_FLAKE",
    "MAT_GLASSHORN", "MAT_GRAZER_HIDE", "MAT_HAULER_HIDE_HEAVY",
    "MAT_HOLLOW_SHELL", "MAT_HOOKJAW_FANG", "MAT_IRONBRINE",
    "MAT_IRONSTAG_HIDE", "MAT_LANTERN_SEED", "MAT_LEECH_MEMBRANE",
    "MAT_MAGNETIC_DUST", "MAT_MAWCAP_CAP", "MAT_MEMBRANE_FIN",
    "MAT_MINERAL_CRUST", "MAT_MINERAL_SHARD", "MAT_MINERAL_TOOTH",
    "MAT_MUSHROOM_MYCELIUM", "MAT_PILLAR_BONE", "MAT_PREDATOR_GLAND",
    "MAT_REED_FIBER", "MAT_REED_STALK", "MAT_REED_THATCH",
    "MAT_RESIN_BLOCK", "MAT_RUNNER_HIDE", "MAT_SHELLMAW_PLATE",
    "MAT_SLAGROOT_CINDER", "MAT_SMOKEBARK_BARK", "MAT_SMOKEBARK_LOG",
    "MAT_SMOKEBARK_PLANK", "MAT_SMOKEBARK_RESIN", "MAT_SPIRAL_FIBER",
    "MAT_SPORE_DUST", "MAT_STALKER_HIDE", "MAT_THORN_QUILL",
    "MAT_VANE_BLADE", "MAT_VANE_QUILL", "MAT_VAULT_HIDE",
    "MAT_VELVETSPINE_SPINE", "MAT_WISP_RIBBON",
    "MAT_ASTERBARK_LOG", "MAT_GLASSBARK_LOG", "MAT_SLAGROOT_LOG",
    "MAT_VELVETSPINE_LOG",
    "MED_ANTIBIOTIC", "MED_BANDAGE", "MED_BLOOD_BAG", "MED_NULLMINT",
    "MED_SUTURE_KIT",
    "PLT_FERRIC_BLOOM", "PLT_MELTPOD_RIND", "PLT_NULLMINT_NODES",
    "PLT_RESIN_CHIMNEY",
    "RAW_ANTENNA_PARTS", "RAW_BATTERY", "RAW_CAPACITOR",
    "RAW_CIRCUIT_BOARD", "RAW_DUCT_TAPE", "RAW_FABRIC", "RAW_GLUE",
    "RAW_GRAIN", "RAW_HIDE", "RAW_POWER_CELL", "RAW_REGULATOR",
    "RAW_SALT", "RAW_WIRE",
    "REM_ART_DATA_SHARD", "REM_ART_POWER_CELL",
    "TOL_CLEANING_KIT", "TOL_COOKWARE", "TOL_LIGHTER", "TOL_SCALPEL",
    "TOL_SCREWDRIVER_SET", "TOL_SOLDERING_IRON", "TOL_WRENCH",
    # Crash-site loot table entries flagged by the 2026-08-13 playtest log.
    "AMM_556", "TOL_DECRYPT_SPIKE", "TOL_MED_KEY", "TOL_POWER_COUPLER",
    "RAW_METAL_INGOT",
]


def _humanize(item_id):
    return " ".join(w.capitalize() for w in item_id.split("_")[1:])


def _resolve_category(name):
    enum = getattr(unreal, "QRItemCategory", None)
    if enum is None:
        return None
    for candidate in (name, name.capitalize(), name.title()):
        member = getattr(enum, candidate, None)
        if member is not None:
            return member
    return None


def _set(asset, prop, value):
    if value is None:
        return
    try:
        asset.set_editor_property(prop, value)
    except Exception as e:  # noqa: BLE001 — UE property drift is per-version
        print("[drops]   set {} failed on {}: {}".format(prop, asset.get_name(), e))


def _rule_for(item_id):
    for prefix, rule in PREFIX_RULES.items():
        if item_id.startswith(prefix):
            return rule
    return ("Materials", "MATERIAL", 0.5, 20)


def run(overwrite=False):
    print("\n=== qr_seed_drop_items ===")
    def_class = unreal.load_object(None, "/Script/QRItems.QRItemDefinition")
    if def_class is None:
        print("[drops] QRItemDefinition class not found — compile QRItems first.")
        return

    created = skipped = failed = 0
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for item_id in DROP_ITEM_IDS:
        bucket, cat, mass, stack = _rule_for(item_id)
        dest_dir  = "{}/{}".format(ITEMS_PKG_ROOT, bucket)
        asset_path = "{}/{}".format(dest_dir, item_id)

        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            if not overwrite:
                skipped += 1
                continue
            unreal.EditorAssetLibrary.delete_asset(asset_path)

        if not unreal.EditorAssetLibrary.does_directory_exist(dest_dir):
            unreal.EditorAssetLibrary.make_directory(dest_dir)

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", def_class)
        asset = tools.create_asset(item_id, dest_dir, def_class, factory)
        if not asset:
            print("[drops]   FAILED {}".format(asset_path))
            failed += 1
            continue

        _set(asset, "item_id",        unreal.Name(item_id))
        _set(asset, "display_name",   unreal.Text(_humanize(item_id)))
        _set(asset, "description",
             unreal.Text("{} — field drop / harvest yield.".format(_humanize(item_id))))
        _set(asset, "category",       _resolve_category(cat))
        _set(asset, "mass_kg",        float(mass))
        _set(asset, "volume_liters",  float(max(mass * 0.6, 0.1)))
        _set(asset, "max_stack_size", int(stack))
        _set(asset, "grid_footprint_w", 1)
        _set(asset, "grid_footprint_h", 1)
        unreal.EditorAssetLibrary.save_asset(asset_path)
        created += 1

    print("[drops] created {}, skipped {}, failed {} (of {})".format(
        created, skipped, failed, len(DROP_ITEM_IDS)))
    print("[drops] Wildlife/flora/wreck drops now resolve to real items.")


if __name__ == "__main__":
    run()
