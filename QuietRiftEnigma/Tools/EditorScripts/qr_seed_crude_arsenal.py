"""
qr_seed_crude_arsenal.py -- create UQRItemDefinition assets for the
survival melee / primitive arsenal, metal-tier blades, bows, and the
shield ladder, so they're equippable from the creative browser
immediately (no mesh bake required -- the held mesh is just empty until
qr_generate_weapons_assets bakes one).

Every id here is recognised by UQRWeaponComponent::ConfigureForWeaponId
(name-based), so equipping it makes the weapon behave correctly:
  - DAGGER/SWORD/AXE/SPEAR/PICKAXE/... -> melee sphere-sweep swing
  - <metal>_SWORD                       -> damage scaled by the metal
  - BOW/CROSSBOW/SLING                  -> drawn, precise on ADS
  - SHIELD / RIOT_SHIELD / PLASMA_SHIELD-> raise (RMB) to block

The exotic metals (FERRIC, BLACKGLASS, MAGNET, SUNWIRE, SPARKSTONE,
FROSTSPARK, REMNANT) are the planet's canonical materials -- see
EXOTIC_METALS_ARSENAL.md. They also feed the armour ids seeded here.

Run from the UE Python console:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_seed_crude_arsenal.py').read())
  run()                 # create everything (skips existing)
  run(overwrite=True)   # rebuild every def
"""

import unreal

ITEMS_PKG_ROOT = "/Game/QuietRift/Data/Items"
MESH_PKG_ROOT  = "/Game/Meshes/weapons_assets"   # where a baked SM_<id> would live

# Category enum ints (match EQRItemCategory). Weapon=8, Clothing=11.
CAT_WEAPON   = 8
CAT_CLOTHING = 11


# ── The arsenal ────────────────────────────────────────────────────
# (item_id, bucket, mass_kg)  -- bucket is the /Items subfolder.

CRUDE_MELEE = [
    "WPN_DAGGER", "WPN_BONE_KNIFE", "WPN_MACHETE",
    "WPN_HATCHET", "WPN_STONE_AXE", "WPN_WOOD_CLUB",
    "WPN_STONE_SPEAR", "WPN_PICKAXE", "WPN_KNUCKLE_DUSTER",
]

# Metal-tier blades. Each metal scales blade damage in ConfigureForWeaponId.
METALS = ["FERRIC", "BLACKGLASS", "MAGNET", "SUNWIRE",
          "SPARKSTONE", "FROSTSPARK", "REMNANT"]
METAL_BLADES = []
for m in METALS:
    METAL_BLADES.append("WPN_{}_SWORD".format(m))
    METAL_BLADES.append("WPN_{}_AXE".format(m))
    METAL_BLADES.append("WPN_{}_SPEAR".format(m))

BOWS = ["WPN_SHORTBOW", "WPN_RECURVE_BOW", "WPN_CROSSBOW", "WPN_SLING"]

SHIELDS = [
    "WPN_WOOD_SHIELD", "WPN_SCRAP_SHIELD",
    "WPN_RIOT_SHIELD", "WPN_PLASMA_SHIELD",
]

# Armour ids (Clothing category). The wear-protection system is a
# follow-up; these exist so the metals "make armour" and recipes resolve.
ARMOR = []
for m in METALS:
    ARMOR.append("ARM_{}_HELM".format(m))
    ARMOR.append("ARM_{}_CHEST".format(m))
    ARMOR.append("ARM_{}_LEGS".format(m))


def _find_def_class():
    cls = getattr(unreal, "QRItemDefinition", None)
    if cls is None:
        raise RuntimeError(
            "unreal.QRItemDefinition not found -- compile the QRItems module "
            "and reopen the editor first.")
    return cls


def _set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as e:
        print("[arsenal]   (set {} skipped: {})".format(prop, e))
        return False


def _humanize(item_id):
    s = item_id
    for pfx in ("WPN_", "ARM_"):
        if s.startswith(pfx):
            s = s[len(pfx):]
    return s.replace("_", " ").title()


def _make_def(item_id, bucket, cat_int, mass, def_class, overwrite):
    dest_dir = "{}/{}".format(ITEMS_PKG_ROOT, bucket)
    asset_path = "{}/{}".format(dest_dir, item_id)

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        if not overwrite:
            return False
        unreal.EditorAssetLibrary.delete_asset(asset_path)

    if not unreal.EditorAssetLibrary.does_directory_exist(dest_dir):
        unreal.EditorAssetLibrary.make_directory(dest_dir)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", def_class)
    asset = tools.create_asset(item_id, dest_dir, def_class, factory)
    if not asset:
        print("[arsenal]   FAILED to create {}".format(asset_path))
        return False

    _set(asset, "item_id",          unreal.Name(item_id))
    _set(asset, "display_name",     unreal.Text(_humanize(item_id)))
    _set(asset, "description",      unreal.Text("{} (crude arsenal)".format(_humanize(item_id))))
    # category is an enum; try the int then the enum object.
    if not _set(asset, "category", cat_int):
        enum = getattr(unreal, "QRItemCategory", None)
        if enum is not None:
            name = "WEAPON" if cat_int == CAT_WEAPON else "CLOTHING"
            _set(asset, "category", getattr(enum, name, cat_int))
    _set(asset, "mass_kg",          float(mass))
    _set(asset, "volume_liters",    float(max(mass * 0.6, 0.2)))
    _set(asset, "max_stack_size",   1)
    _set(asset, "grid_footprint_w", 2)
    _set(asset, "grid_footprint_h", 1)
    _set(asset, "max_durability",   120.0)

    # Wire a baked mesh if one happens to exist (firearms-style pipeline).
    mesh_path = "{}/SM_{}".format(MESH_PKG_ROOT, item_id)
    if unreal.EditorAssetLibrary.does_asset_exist(mesh_path):
        mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
        if mesh:
            _set(asset, "world_mesh", mesh)

    unreal.EditorAssetLibrary.save_asset(asset_path)
    return True


def run(overwrite=False):
    print("\n=== qr_seed_crude_arsenal ===")
    def_class = _find_def_class()

    made = 0
    skipped = 0
    plan = (
        [(i, "Weapons", 1.2) for i in CRUDE_MELEE] +
        [(i, "Weapons", 2.0) for i in METAL_BLADES] +
        [(i, "Weapons", 1.6) for i in BOWS] +
        [(i, "Weapons", 5.0) for i in SHIELDS] +
        [(i, "Clothing", 3.0) for i in ARMOR]
    )
    for item_id, bucket, mass in plan:
        cat = CAT_CLOTHING if bucket == "Clothing" else CAT_WEAPON
        if _make_def(item_id, bucket, cat, mass, def_class, overwrite):
            made += 1
        else:
            skipped += 1

    print("[arsenal] created {} item defs, skipped {} existing.".format(made, skipped))
    print("[arsenal] {} melee, {} metal blades, {} bows, {} shields, {} armour."
          .format(len(CRUDE_MELEE), len(METAL_BLADES), len(BOWS), len(SHIELDS), len(ARMOR)))
    print("[arsenal] Equip from the creative browser (Tab). Held mesh is empty")
    print("[arsenal] until qr_generate_weapons_assets bakes one -- the weapon")
    print("[arsenal] LOGIC works now (swing/draw/block + damage).")


if __name__ == "__main__":
    run()
