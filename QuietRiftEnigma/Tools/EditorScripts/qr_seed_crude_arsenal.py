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

# Category names (resolved to the EQRItemCategory enum by name in
# _make_def -- name-based avoids the int-mismatch bug that previously set
# weapons to category 8 = Resource).
CAT_WEAPON   = "Weapon"
CAT_CLOTHING = "Clothing"
CAT_AMMO     = "Ammo"
CAT_CHESTRIG = "ChestRig"
CAT_BACKPACK = "Backpack"


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

# Arrow / projectile rounds. Bows read UQRWeaponComponent::EquippedAmmoItemId
# and map the id (via name tokens) to an EQRInjuryType + damage multiplier.
# Real-world inspirations + the Jovianlight setting:
#   POISON: native toxin-gland extract -- DOT, anti-megafauna
#   TRANQ:  sedative compound for taming or non-lethal capture
#   CRYO:   frostspark-crystal payload, slow + cold damage
#   EMP:    capacitor shock, disables Remnant-tech defenders
#   SMOKE:  signal / area-denial puff (scientifically: aerosolised mineral
#           dust loaded into a hollow shaft; reduces visibility)
#   TRACKER: spore + radio dye marker, "Marked" status enables hunter UI
#   FIRE:   pitch-impregnated head, sustained burn
#   EXPLOSIVE: sparkstone-warhead, AOE on impact (concussion)
#   BROADHEAD: heavier bleed, real big-game hunting analog
#   ARMOR_PIERC: tungsten-core for hard targets (Remnant chassis)
ARROWS = [
    "AMO_ARROW_STANDARD",
    "AMO_ARROW_BROADHEAD",
    "AMO_ARROW_ARMOR_PIERCING",
    "AMO_ARROW_POISON",
    "AMO_ARROW_TRANQ",
    "AMO_ARROW_CRYO",
    "AMO_ARROW_EMP",
    "AMO_ARROW_SMOKE",
    "AMO_ARROW_TRACKER",
    "AMO_ARROW_FIRE",
    "AMO_ARROW_EXPLOSIVE",
    # Crossbow bolts -- same payloads, bolt-shaped, used by WPN_CROSSBOW.
    "AMO_BOLT_STANDARD",
    "AMO_BOLT_ARMOR_PIERCING",
    "AMO_BOLT_EXPLOSIVE",
]

# Armour ids (Clothing category). The wear-protection system is a
# follow-up; these exist so the metals "make armour" and recipes resolve.
ARMOR = []
for m in METALS:
    ARMOR.append("ARM_{}_HELM".format(m))
    ARMOR.append("ARM_{}_CHEST".format(m))
    ARMOR.append("ARM_{}_LEGS".format(m))

# Chest rigs + backpacks. container slot: 1=ChestRig, 2=Backpack (matches
# EQRContainerSlotType). grid = the inner storage grid; carry/vol = the
# capacity bonus the container adds. These need the container fields set
# or TryEquipContainer rejects them.
# (id, category, mass, container_spec)
CONTAINERS = [
    ("RIG_SCRAP_CHESTRIG", CAT_CHESTRIG, 1.4,
     {"slot": 1, "grid": (4, 3), "carry": 6.0,  "vol": 8.0,  "fp": (3, 2)}),
    ("RIG_TACTICAL_RIG",   CAT_CHESTRIG, 1.8,
     {"slot": 1, "grid": (5, 4), "carry": 9.0,  "vol": 12.0, "fp": (3, 2)}),
    ("PACK_FIELD_BACKPACK", CAT_BACKPACK, 2.0,
     {"slot": 2, "grid": (5, 5), "carry": 14.0, "vol": 22.0, "fp": (3, 3)}),
    ("PACK_HAULER_BACKPACK", CAT_BACKPACK, 2.8,
     {"slot": 2, "grid": (6, 6), "carry": 22.0, "vol": 35.0, "fp": (3, 3)}),
]


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


def _resolve_category(cat_name):
    """EQRItemCategory enum value from its name, e.g. 'Weapon' -> the
    enum member. UE Python exposes enum members UPPERCASE."""
    enum = getattr(unreal, "QRItemCategory", None)
    if enum is not None:
        member = getattr(enum, cat_name.upper(), None)
        if member is not None:
            return member
    # Fallback ints if the enum isn't reflected (shouldn't happen).
    return {"None": 0, "Food": 1, "Tool": 2, "Weapon": 3, "Ammo": 4,
            "Clothing": 11, "ChestRig": 16, "Backpack": 17}.get(cat_name, 0)


# Two-handed weapons: holding one of these clears the offhand slot, and
# nothing can be equipped to the offhand while one is in the primary hand.
# Substring-matched against ItemId.upper() in _is_two_handed below.
TWO_HANDED_TOKENS = (
    "BOW",            # SHORTBOW / RECURVE_BOW / CROSSBOW (yes, shooting a bow needs both hands)
    "SPEAR",          # WPN_STONE_SPEAR, WPN_<METAL>_SPEAR
    "RIFLE", "SNIPER",# future long-arm ids
)


def _is_two_handed(item_id):
    upper = item_id.upper()
    return any(tok in upper for tok in TWO_HANDED_TOKENS)


# container spec: None, or dict(slot=1|2, grid=(w,h), carry=kg, vol=L)
def _make_def(item_id, bucket, cat_name, mass, def_class, overwrite, container=None):
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
    _set(asset, "category",         _resolve_category(cat_name))
    _set(asset, "mass_kg",          float(mass))
    _set(asset, "volume_liters",    float(max(mass * 0.6, 0.2)))
    _set(asset, "max_stack_size",   1)
    _set(asset, "grid_footprint_w", 2)
    _set(asset, "grid_footprint_h", 1)
    _set(asset, "max_durability",   120.0)

    # Two-handed flag drives the offhand-clear rule on TryEquipToHandSlot
    # (UQRInventoryComponent). Bows + spears need both hands; shields and
    # daggers don't, so they remain offhand-friendly.
    if cat_name == CAT_WEAPON and _is_two_handed(item_id):
        _set(asset, "is_two_handed", True)

    # Container payload (chest rig / backpack) -- without these fields set,
    # TryEquipContainer rejects the item with WrongSlot.
    if container:
        slot_enum = getattr(unreal, "QRContainerSlotType", None)
        slot_val = int(container["slot"])
        if slot_enum is not None:
            slot_name = {1: "CHEST_RIG", 2: "BACKPACK"}.get(slot_val)
            member = getattr(slot_enum, slot_name, None) if slot_name else None
            if member is not None:
                slot_val = member
        _set(asset, "container_slot",                 slot_val)
        _set(asset, "container_grid_w",               int(container["grid"][0]))
        _set(asset, "container_grid_h",               int(container["grid"][1]))
        _set(asset, "container_carry_bonus_kg",       float(container.get("carry", 0.0)))
        _set(asset, "container_volume_bonus_liters",  float(container.get("vol", 0.0)))
        _set(asset, "grid_footprint_w",               int(container.get("fp", (3, 2))[0]))
        _set(asset, "grid_footprint_h",               int(container.get("fp", (3, 2))[1]))

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
    # (id, bucket, category, mass, container_spec)
    plan = (
        [(i, "Weapons",          CAT_WEAPON,   1.2,  None) for i in CRUDE_MELEE]  +
        [(i, "Weapons",          CAT_WEAPON,   2.0,  None) for i in METAL_BLADES] +
        [(i, "Weapons",          CAT_WEAPON,   1.6,  None) for i in BOWS]         +
        [(i, "Weapons",          CAT_WEAPON,   5.0,  None) for i in SHIELDS]      +
        [(i, "Clothing",         CAT_CLOTHING, 3.0,  None) for i in ARMOR]        +
        [(i, "AmmoAttachments",  CAT_AMMO,     0.05, None) for i in ARROWS]       +
        [(i, "Containers",       cat,  m, c) for (i, cat, m, c) in CONTAINERS]
    )
    for item_id, bucket, cat, mass, container in plan:
        if _make_def(item_id, bucket, cat, mass, def_class, overwrite, container):
            made += 1
        else:
            skipped += 1

    print("[arsenal] created {} item defs, skipped {} existing.".format(made, skipped))
    print("[arsenal] {} melee, {} metal blades, {} bows, {} shields, {} armour, {} arrows, {} containers."
          .format(len(CRUDE_MELEE), len(METAL_BLADES), len(BOWS), len(SHIELDS),
                  len(ARMOR), len(ARROWS), len(CONTAINERS)))
    print("[arsenal] Equip from the creative browser (Tab). Held mesh is empty")
    print("[arsenal] until qr_generate_weapons_assets bakes one -- the weapon")
    print("[arsenal] LOGIC works now (swing/draw/block + damage).")


if __name__ == "__main__":
    run()
