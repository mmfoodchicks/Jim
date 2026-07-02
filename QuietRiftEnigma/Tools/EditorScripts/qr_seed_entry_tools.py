"""
qr_seed_entry_tools.py -- create the 5 tool-gated crash-entry tool item
definitions so the MinorCrash_* sites are actually breach-able.

The worldgen names these ids as RequiredToolItemId on the small crash
sites (UQRWorldGenSubsystem::PlacePOIs):
    TOL_CUTTING_TORCH   -> MinorCrash_Armory
    TOL_MED_KEY         -> MinorCrash_MedBay
    TOL_PRY_BAR         -> MinorCrash_Galley + MinorCrash_Luggage
    TOL_DECRYPT_SPIKE   -> MinorCrash_Avionics
    TOL_POWER_COUPLER   -> MinorCrash_PowerCore

Without these assets, the unlock check counts 0 of the required item
and every gated wreck stays sealed forever.

Recipe + tech-node rows ship in qr_append_entry_tool_recipes.py (run
it from a SHELL, not inside UE) -- per the recipe/research mirror rule
both halves land in the same commit as this seeder.

Run from the UE Python console:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_seed_entry_tools.py').read())
  run()                 # create missing
  run(overwrite=True)   # rebuild all 5
"""

import unreal

ITEMS_PKG_ROOT = "/Game/QuietRift/Data/Items"

# (item_id, display, mass_kg, description)
TOOLS = [
    ("TOL_CUTTING_TORCH", "Cutting Torch", 2.4,
     "Plasma-arc cutter salvaged from engineering stores. Breaches the "
     "armory wreck's sealed bulkhead. Reusable."),
    ("TOL_MED_KEY", "Medbay Override Key", 0.1,
     "Crew-medic keycard, biolocked to triage overrides. Opens the "
     "medbay wreck's quarantine door."),
    ("TOL_PRY_BAR", "Pry Bar", 1.8,
     "Two meters of stubborn ferric leverage. Forces jammed galley and "
     "luggage-bay hatches."),
    ("TOL_DECRYPT_SPIKE", "Decryption Spike", 0.3,
     "One-shot intrusion wafer that spoofs an avionics maintenance "
     "handshake. Opens the avionics wreck's datacore shroud."),
    ("TOL_POWER_COUPLER", "Power Coupler", 1.2,
     "Rated junction coupler. Re-energizes the power-core wreck's door "
     "servos long enough to walk in."),
]


def _find_def_class():
    cls = getattr(unreal, "QRItemDefinition", None)
    if cls is None:
        raise RuntimeError("unreal.QRItemDefinition not found -- compile QRItems first.")
    return cls


def _set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
    except Exception as e:
        print("[tools]   (set {} skipped: {})".format(prop, e))


def run(overwrite=False):
    print("\n=== qr_seed_entry_tools ===")
    def_class = _find_def_class()
    tools_api = unreal.AssetToolsHelpers.get_asset_tools()
    dest_dir = "{}/Tools".format(ITEMS_PKG_ROOT)
    if not unreal.EditorAssetLibrary.does_directory_exist(dest_dir):
        unreal.EditorAssetLibrary.make_directory(dest_dir)

    tool_cat = getattr(unreal.QRItemCategory, "TOOL", None)

    made = 0
    for item_id, display, mass, desc in TOOLS:
        asset_path = "{}/{}".format(dest_dir, item_id)
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            if not overwrite:
                print("[tools]   {} exists -- skip".format(item_id))
                continue
            unreal.EditorAssetLibrary.delete_asset(asset_path)

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", def_class)
        asset = tools_api.create_asset(item_id, dest_dir, def_class, factory)
        if not asset:
            print("[tools]   FAILED {}".format(item_id))
            continue

        _set(asset, "item_id",        unreal.Name(item_id))
        _set(asset, "display_name",   unreal.Text(display))
        _set(asset, "description",    unreal.Text(desc))
        if tool_cat is not None:
            _set(asset, "category",   tool_cat)
        _set(asset, "mass_kg",        float(mass))
        _set(asset, "volume_liters",  float(max(mass * 0.5, 0.1)))
        _set(asset, "max_stack_size", 1)
        _set(asset, "max_durability", 200.0)
        # Entry tools are keys: crafting may reference them without
        # consuming (bIsReusableCraftingInput) and the crash unlock
        # never consumes them either.
        _set(asset, "is_reusable_crafting_input", True)

        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        made += 1
        print("[tools]   {} created".format(item_id))

    print("[tools] done -- {} created. Run qr_route_fab_meshes.run() so".format(made))
    print("[tools] they pick up their placeholder meshes, and reimport")
    print("[tools] DT_Recipes/DT_TechNodes after the recipe appender runs.")


if __name__ == "__main__":
    run()
