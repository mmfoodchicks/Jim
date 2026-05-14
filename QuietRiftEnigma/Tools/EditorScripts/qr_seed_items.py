"""
Quiet Rift: Enigma — seed UQRItemDefinition assets.

Run from inside the Unreal Editor. One-shot script that creates ~30
starter items under /Game/QuietRift/Data/Items so the creative-mode
browser (Tab in PIE) has things to drag onto the hotbar.

Requirements:
  - Editor → Plugins → enable "Python Editor Script Plugin" and
    "Editor Scripting Utilities" (both ship with UE, just toggled off).
  - Restart the editor.

How to run:
  1. Window → Developer Tools → Python Console.
  2. Paste this whole file into the console and press Enter, OR:
     - Save this file somewhere UE Python can see it
       (e.g. <ProjectDir>/Content/Python/qr_seed_items.py).
     - In the Python console run:
         import qr_seed_items
         qr_seed_items.run()

Re-running is safe — existing assets are skipped, not overwritten,
unless you pass overwrite=True to run().

What you get: an item per row in the ITEMS list below, saved at
/Game/QuietRift/Data/Items/<ItemId>.uasset. All fields are pre-filled
with sensible placeholder values. WorldMesh + InventoryIcon stay None
until you assign real assets in the editor (the C++ held-item mesh
falls back to "no mesh" gracefully — the slot still works).
"""

import unreal


PACKAGE_ROOT = '/Game/QuietRift/Data/Items'


# ── Item category enum bridge ─────────────────────────────────
# UE Python exposes UENUM(BlueprintType) types as unreal.<NameWithoutE>.
# Fall back to integer values if the binding isn't generated for some
# reason (rare — happens if the module wasn't compiled with the editor).
_CATEGORY_INTS = {
    'None':           0,
    'Food':           1,
    'Tool':           2,
    'Weapon':         3,
    'Ammo':           4,
    'Attachment':     5,
    'Component':      6,
    'ReferenceComp':  7,
    'Resource':       8,
    'Fuel':           9,
    'Medicine':       10,
    'Clothing':       11,
    'Blueprint_Item': 12,
    'Seed':           13,
    'Flora':          14,
    'Wildlife':       15,
    'ChestRig':       16,
    'Backpack':       17,
    'Cosmetic':       18,
}

_CONTAINER_SLOT_INTS = {
    'None':     0,
    'ChestRig': 1,
    'Backpack': 2,
}


def _category(name):
    enum_type = getattr(unreal, 'QRItemCategory', None)
    if enum_type is not None:
        return getattr(enum_type, name.upper(), _CATEGORY_INTS[name])
    return _CATEGORY_INTS[name]


def _container_slot(name):
    enum_type = getattr(unreal, 'QRContainerSlotType', None)
    if enum_type is not None:
        return getattr(enum_type, name.upper(), _CONTAINER_SLOT_INTS[name])
    return _CONTAINER_SLOT_INTS[name]


# ── Item manifest ─────────────────────────────────────────────
# Defaults: mass 0.1, volume 0.1, max stack 50, footprint 1×1, no durability.
# Override any field by including it in the entry. ContainerSlot+grid fields
# only apply to ChestRig / Backpack categories.

ITEMS = [
    # ── Materials / resources ────────────────────────────
    {'id': 'MAT_SCRAP_METAL',   'name': 'Scrap Metal',      'cat': 'Resource',  'mass': 0.4, 'vol': 0.2, 'stack': 100},
    {'id': 'MAT_FIBER',         'name': 'Native Fiber',     'cat': 'Resource',  'mass': 0.05,'vol': 0.1, 'stack': 100},
    {'id': 'MAT_REGOLITH',      'name': 'Refined Regolith', 'cat': 'Resource',  'mass': 0.8, 'vol': 0.3, 'stack': 100},
    {'id': 'MAT_POLYMER',       'name': 'Salvaged Polymer', 'cat': 'Resource',  'mass': 0.15,'vol': 0.2, 'stack': 100},
    {'id': 'MAT_CIRCUITRY',     'name': 'Loose Circuitry',  'cat': 'Component', 'mass': 0.1, 'vol': 0.15,'stack': 50},
    {'id': 'MAT_FUEL_CELL',     'name': 'Fuel Cell',        'cat': 'Fuel',      'mass': 0.6, 'vol': 0.4, 'stack': 20},

    # ── Tools ───────────────────────────────────────────
    {'id': 'TOOL_AXE',          'name': 'Hatchet',          'cat': 'Tool',  'mass': 1.1, 'vol': 0.5, 'stack': 1, 'durability': 100, 'footprint': (2, 1)},
    {'id': 'TOOL_PICKAXE',      'name': 'Pickaxe',          'cat': 'Tool',  'mass': 1.6, 'vol': 0.6, 'stack': 1, 'durability': 120, 'footprint': (2, 1)},
    {'id': 'TOOL_MULTITOOL',    'name': 'Field Multitool',  'cat': 'Tool',  'mass': 0.4, 'vol': 0.2, 'stack': 1, 'durability': 80},
    {'id': 'TOOL_FLASHLIGHT',   'name': 'Helmet Light',     'cat': 'Tool',  'mass': 0.2, 'vol': 0.15,'stack': 1, 'durability': 40},

    # ── Weapons ─────────────────────────────────────────
    {'id': 'WPN_PISTOL_BASIC',  'name': 'Salvaged Pistol',  'cat': 'Weapon','mass': 0.9, 'vol': 0.4, 'stack': 1, 'durability': 250, 'footprint': (2, 1)},
    {'id': 'WPN_RIFLE_SCRAP',   'name': 'Scrapgun Rifle',   'cat': 'Weapon','mass': 3.4, 'vol': 1.8, 'stack': 1, 'durability': 400, 'footprint': (4, 1)},
    {'id': 'WPN_SHOTGUN_PUMP',  'name': 'Pump Shotgun',     'cat': 'Weapon','mass': 3.1, 'vol': 1.6, 'stack': 1, 'durability': 350, 'footprint': (4, 1)},
    {'id': 'WPN_KNIFE_COMBAT',  'name': 'Combat Knife',     'cat': 'Weapon','mass': 0.3, 'vol': 0.1, 'stack': 1, 'durability': 200},

    # ── Ammo ───────────────────────────────────────────
    {'id': 'AMMO_9MM',          'name': '9mm Round',        'cat': 'Ammo',  'mass': 0.01,'vol': 0.005,'stack': 200},
    {'id': 'AMMO_RIFLE',        'name': 'Rifle Cartridge',  'cat': 'Ammo',  'mass': 0.025,'vol': 0.01,'stack': 200},
    {'id': 'AMMO_SHELL',        'name': '12-Gauge Shell',   'cat': 'Ammo',  'mass': 0.05,'vol': 0.02,'stack': 100},

    # ── Food (ship rations marked SafeKnown, native food unknown) ──
    {'id': 'FOOD_RATION_BAR',   'name': 'Ship Ration Bar',  'cat': 'Food',  'mass': 0.2, 'vol': 0.15,'stack': 30},
    {'id': 'FOOD_WATER_FLASK',  'name': 'Water Flask',      'cat': 'Food',  'mass': 0.6, 'vol': 0.5, 'stack': 10},
    {'id': 'FOOD_MEAT_RAW',     'name': 'Raw Native Meat',  'cat': 'Food',  'mass': 0.5, 'vol': 0.4, 'stack': 10},
    {'id': 'FOOD_FUNGUS',       'name': 'Glowcap Fungus',   'cat': 'Food',  'mass': 0.1, 'vol': 0.1, 'stack': 20},

    # ── Medicine ────────────────────────────────────────
    {'id': 'MED_BANDAGE',       'name': 'Field Bandage',    'cat': 'Medicine','mass': 0.05,'vol': 0.05,'stack': 30},
    {'id': 'MED_PAINKILLER',    'name': 'Painkiller Tabs',  'cat': 'Medicine','mass': 0.02,'vol': 0.02,'stack': 50},
    {'id': 'MED_STIMPACK',      'name': 'Stim Injector',    'cat': 'Medicine','mass': 0.1, 'vol': 0.05,'stack': 10},

    # ── Containers (worn) ───────────────────────────────
    {'id': 'CONT_CHESTRIG_BASIC','name': 'Salvaged Chest Rig','cat': 'ChestRig','mass': 1.4, 'vol': 0.6, 'stack': 1,
     'container_slot': 'ChestRig', 'container_grid': (4, 3), 'container_carry_kg': 6.0, 'container_volume_l': 12.0, 'footprint': (3, 3)},
    {'id': 'CONT_BACKPACK_HIKER','name': 'Hiker Backpack',   'cat': 'Backpack','mass': 2.2, 'vol': 1.0, 'stack': 1,
     'container_slot': 'Backpack', 'container_grid': (5, 4), 'container_carry_kg': 12.0, 'container_volume_l': 25.0, 'footprint': (4, 4)},

    # ── Cosmetics / clothing ────────────────────────────
    {'id': 'COS_JACKET_FIELD',  'name': 'Field Jacket',     'cat': 'Clothing','mass': 1.0, 'vol': 0.8, 'stack': 1},
    {'id': 'COS_CAP_CREW',      'name': 'Crew Cap',         'cat': 'Cosmetic','mass': 0.1, 'vol': 0.2, 'stack': 1},

    # ── Flora / wildlife products ───────────────────────
    {'id': 'FLR_SEED_BASIC',    'name': 'Edible Plant Seed','cat': 'Seed',     'mass': 0.02,'vol': 0.02,'stack': 50},
    {'id': 'WLD_HIDE_RAW',      'name': 'Raw Animal Hide',  'cat': 'Wildlife', 'mass': 0.7, 'vol': 0.6, 'stack': 10},

    # ── Misc components ────────────────────────────────
    {'id': 'CMP_REFCOMP_NAV',   'name': 'Navigation Reference Comp', 'cat': 'ReferenceComp', 'mass': 0.3, 'vol': 0.2, 'stack': 5},
    {'id': 'CMP_BLUEPRINT_HUT', 'name': 'Blueprint: Shelter Hut',    'cat': 'Blueprint_Item','mass': 0.05,'vol': 0.05,'stack': 10},
]


# ── Asset creation ────────────────────────────────────────────

def _ensure_package_dir():
    if not unreal.EditorAssetLibrary.does_directory_exist(PACKAGE_ROOT):
        unreal.EditorAssetLibrary.make_directory(PACKAGE_ROOT)


def _find_def_class():
    """Resolve UQRItemDefinition via the runtime registry."""
    cls = getattr(unreal, 'QRItemDefinition', None)
    if cls is None:
        raise RuntimeError(
            "unreal.QRItemDefinition not found. Build the QRItems module in "
            "the editor target and reload the editor before re-running."
        )
    return cls


def _set(obj, prop_name, value):
    """Set a property with graceful fallback if the engine renamed it."""
    try:
        obj.set_editor_property(prop_name, value)
    except Exception as e:
        unreal.log_warning(f"  could not set {prop_name}: {e}")


def _create_one(spec, def_class, overwrite=False):
    item_id = spec['id']
    asset_path = f'{PACKAGE_ROOT}/{item_id}'

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        if not overwrite:
            unreal.log(f"skip (exists): {asset_path}")
            return None
        unreal.EditorAssetLibrary.delete_asset(asset_path)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', def_class)
    new_asset = asset_tools.create_asset(item_id, PACKAGE_ROOT, def_class, factory)
    if not new_asset:
        unreal.log_error(f"failed to create {asset_path}")
        return None

    # Pull fields with defaults.
    mass    = spec.get('mass',    0.1)
    vol     = spec.get('vol',     0.1)
    stack   = spec.get('stack',   50)
    fp_w, fp_h = spec.get('footprint', (1, 1))
    durab   = spec.get('durability', 0.0)

    _set(new_asset, 'item_id',          unreal.Name(item_id))
    _set(new_asset, 'display_name',     unreal.Text(spec['name']))
    _set(new_asset, 'description',      unreal.Text(spec.get('description', f"{spec['name']} (placeholder)")))
    _set(new_asset, 'category',         _category(spec['cat']))
    _set(new_asset, 'mass_kg',          float(mass))
    _set(new_asset, 'volume_liters',    float(vol))
    _set(new_asset, 'max_stack_size',   int(stack))
    _set(new_asset, 'grid_footprint_w', int(fp_w))
    _set(new_asset, 'grid_footprint_h', int(fp_h))
    _set(new_asset, 'max_durability',   float(durab))
    _set(new_asset, 'b_is_bulk_item',   bool(spec.get('bulk', False)))

    if spec['cat'] in ('ChestRig', 'Backpack'):
        cg_w, cg_h = spec.get('container_grid', (4, 3))
        _set(new_asset, 'container_slot',              _container_slot(spec.get('container_slot', spec['cat'])))
        _set(new_asset, 'container_grid_w',            int(cg_w))
        _set(new_asset, 'container_grid_h',            int(cg_h))
        _set(new_asset, 'container_carry_bonus_kg',    float(spec.get('container_carry_kg', 5.0)))
        _set(new_asset, 'container_volume_bonus_liters',float(spec.get('container_volume_l', 10.0)))

    unreal.EditorAssetLibrary.save_asset(asset_path)
    unreal.log(f"created: {asset_path}")
    return new_asset


def run(overwrite=False):
    """Entry point. Pass overwrite=True to recreate existing assets."""
    _ensure_package_dir()
    def_class = _find_def_class()

    created = 0
    for spec in ITEMS:
        if _create_one(spec, def_class, overwrite=overwrite):
            created += 1
    unreal.log(f"done. created/updated {created} item definitions under {PACKAGE_ROOT}")

    # Nudge the asset registry so the creative-mode browser picks them up
    # without an editor restart.
    am = unreal.AssetManager.get()
    try:
        am.scan_paths_synchronous([PACKAGE_ROOT])
    except Exception:
        pass


if __name__ == '__main__':
    run()
