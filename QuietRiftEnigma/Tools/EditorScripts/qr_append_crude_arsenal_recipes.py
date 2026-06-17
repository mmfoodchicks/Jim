"""
qr_append_crude_arsenal_recipes.py -- append recipe + research tech-node
rows for the crude survival arsenal and arrow types added by
qr_seed_crude_arsenal.py.

Per the project policy: anytime an item is added or removed, the
corresponding research and crafting recipe rows must move with it. This
script is the recipe-half of the crude-arsenal batch.

Run from a shell (NOT inside UE -- writes the CSVs directly):
    cd D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts
    python qr_append_crude_arsenal_recipes.py

Idempotent: rows that already exist (matched by id) are skipped. Pass
--replace to overwrite. After the script writes the CSVs, re-import them
in UE (Window > DataTable > Reimport) so DT_Recipes + DT_TechNodes pick
the new rows up.
"""

import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.normpath(os.path.join(HERE, "..", "..", "Content", "QuietRift", "Data"))

RECIPES_CSV   = os.path.join(DATA, "DT_Recipes.csv")
TECHNODES_CSV = os.path.join(DATA, "DT_TechNodes.csv")


# ── Recipe rows ────────────────────────────────────────────────────
# Schema (from DT_Recipes.csv header):
#   RecipeId, Output, Inputs, Time, Building / Station Required, Unlocked By, Source Section
# Pattern matches the existing EQP_HATCHET / EQP_PICKAXE rows so the
# style stays consistent.

METALS_INGOT = {
    "FERRIC":     "MAT_SCRAP_METAL",
    "BLACKGLASS": "MAT_BLACKGLASS_PLATE",
    "MAGNET":     "MAT_MAGNET_INGOT",
    "SUNWIRE":    "MAT_SUNWIRE_INGOT",
    "SPARKSTONE": "MAT_SPARKSTONE_CORE",
    "FROSTSPARK": "MAT_FROSTSPARK_CRYSTAL",
    "REMNANT":    "REF_REMNANT_SLATE",
}


def _r(rid, out, ingredients, time_s, station, unlock):
    return [rid, "{} ×1".format(out), ingredients, "{}s".format(time_s),
            station, unlock, "Crude Arsenal (auto-seeded)"]


def _crude_melee_recipes():
    return [
        _r("RC_WPN_DAGGER",        "WPN_DAGGER",
           "MAT_SCRAP_METAL x1; MAT_BONE_MEDIUM x1; MAT_FIBER_BUNDLE x1",
           60, "Camp Workbench", "T0 Primitive"),
        _r("RC_WPN_BONE_KNIFE",    "WPN_BONE_KNIFE",
           "MAT_BONE_DENSE x1; MAT_FIBER_BUNDLE x1",
           45, "Camp Workbench", "T0 Primitive"),
        _r("RC_WPN_MACHETE",       "WPN_MACHETE",
           "MAT_SCRAP_METAL x2; MAT_FIBER_BUNDLE x1",
           90, "Anvil Forge",   "TN_BASIC_TOOLING"),
        _r("RC_WPN_HATCHET",       "WPN_HATCHET",
           "MAT_SCRAP_METAL x1; MAT_WOOD_LOG x1; MAT_FIBER_BUNDLE x1",
           90, "Anvil Forge",   "TN_BASIC_TOOLING"),
        _r("RC_WPN_STONE_AXE",     "WPN_STONE_AXE",
           "MAT_STONE_BLOCK x1; MAT_WOOD_LOG x1; MAT_FIBER_BUNDLE x1",
           60, "Camp Workbench", "T0 Primitive"),
        _r("RC_WPN_WOOD_CLUB",     "WPN_WOOD_CLUB",
           "MAT_WOOD_LOG x1",
           30, "Camp Workbench", "T0 Primitive"),
        _r("RC_WPN_STONE_SPEAR",   "WPN_STONE_SPEAR",
           "MAT_STONE_BLOCK x1; MAT_WOOD_LOG x1; MAT_FIBER_BUNDLE x1",
           60, "Camp Workbench", "T0 Primitive"),
        _r("RC_WPN_PICKAXE",       "WPN_PICKAXE",
           "MAT_SCRAP_METAL x2; MAT_WOOD_LOG x1; MAT_FIBER_BUNDLE x1",
           100, "Anvil Forge",  "TN_BASIC_TOOLING"),
        _r("RC_WPN_KNUCKLE_DUSTER","WPN_KNUCKLE_DUSTER",
           "MAT_SCRAP_METAL x1; MAT_LEATHER_ROLL x1",
           60, "Camp Workbench", "TN_BASIC_TOOLING"),
    ]


def _metal_blade_recipes():
    rows = []
    for metal, mat in METALS_INGOT.items():
        for kind, time_s in (("SWORD", 240), ("AXE", 240), ("SPEAR", 180)):
            rid = "RC_WPN_{}_{}".format(metal, kind)
            out = "WPN_{}_{}".format(metal, kind)
            station = "Anvil Forge" if metal in ("FERRIC", "MAGNET", "BLACKGLASS") \
                      else "Machine Bench" if metal in ("SUNWIRE", "SPARKSTONE", "FROSTSPARK") \
                      else "Remnant Lathe"
            unlock = {
                "FERRIC":     "TN_BASIC_TOOLING",
                "MAGNET":     "TN_METALWORK_T1",
                "BLACKGLASS": "TN_METALWORK_T2",
                "SUNWIRE":    "TN_ELECTRONICS_T1",
                "SPARKSTONE": "TN_ENERGETICS_T2",
                "FROSTSPARK": "TN_CRYO_T2",
                "REMNANT":    "TN_REMNANT_INTERFACE_T2",
            }[metal]
            ingredients = "{} x2; MAT_WOOD_LOG x1; MAT_FIBER_BUNDLE x1; CMP_RIVET_SET x1".format(mat)
            rows.append(_r(rid, out, ingredients, time_s, station, unlock))
    return rows


def _bow_recipes():
    return [
        _r("RC_WPN_SHORTBOW",     "WPN_SHORTBOW",
           "MAT_WOOD_LOG x1; MAT_FIBER_BUNDLE x2; MAT_SINEW_STRAND x1",
           75, "Camp Workbench", "TN_BASIC_TOOLING"),
        _r("RC_WPN_RECURVE_BOW",  "WPN_RECURVE_BOW",
           "MAT_WOOD_LOG x2; MAT_SINEW_STRAND x2; MAT_LEATHER_ROLL x1",
           150, "Camp Workbench", "TN_METALWORK_T1"),
        _r("RC_WPN_CROSSBOW",     "WPN_CROSSBOW",
           "MAT_WOOD_LOG x2; MAT_SCRAP_METAL x2; CMP_BEARING_SET x1; CMP_SPRING_COIL x1",
           240, "Machine Bench", "TN_METALWORK_T1"),
        _r("RC_WPN_SLING",        "WPN_SLING",
           "MAT_LEATHER_ROLL x1; MAT_FIBER_BUNDLE x2",
           30, "Camp Workbench", "T0 Primitive"),
    ]


def _shield_recipes():
    return [
        _r("RC_WPN_WOOD_SHIELD",  "WPN_WOOD_SHIELD",
           "MAT_WOOD_LOG x3; MAT_FIBER_BUNDLE x2; CMP_RIVET_SET x2",
           120, "Camp Workbench", "TN_BASIC_TOOLING"),
        _r("RC_WPN_SCRAP_SHIELD", "WPN_SCRAP_SHIELD",
           "MAT_SCRAP_METAL x4; CMP_RIVET_SET x4; MAT_LEATHER_ROLL x1",
           180, "Anvil Forge", "TN_METALWORK_T1"),
        _r("RC_WPN_RIOT_SHIELD",  "WPN_RIOT_SHIELD",
           "MAT_METAL_PLATE x2; MAT_GLASS_PANE x2; CMP_BUCKLE_SET x2; MAT_POLYMER_SHEET x2",
           300, "Machine Bench", "TN_METALWORK_T2"),
        _r("RC_WPN_PLASMA_SHIELD","WPN_PLASMA_SHIELD",
           "REF_REMNANT_SLATE x1; MAT_SPARKSTONE_CORE x1; CMP_CAPACITOR_BANK x2; CMP_HEATSINK_BLOCK x1",
           600, "Remnant Lathe", "TN_REMNANT_INTERFACE_T2"),
    ]


def _armor_recipes():
    rows = []
    for metal, mat in METALS_INGOT.items():
        unlock = {
            "FERRIC":     "TN_BASIC_TOOLING",
            "MAGNET":     "TN_METALWORK_T1",
            "BLACKGLASS": "TN_METALWORK_T2",
            "SUNWIRE":    "TN_ELECTRONICS_T1",
            "SPARKSTONE": "TN_ENERGETICS_T2",
            "FROSTSPARK": "TN_CRYO_T2",
            "REMNANT":    "TN_REMNANT_INTERFACE_T2",
        }[metal]
        station = "Anvil Forge" if metal in ("FERRIC", "MAGNET", "BLACKGLASS") \
                  else "Machine Bench" if metal in ("SUNWIRE", "SPARKSTONE", "FROSTSPARK") \
                  else "Remnant Lathe"
        for slot, time_s, qty in (("HELM", 180, 2), ("CHEST", 300, 4), ("LEGS", 240, 3)):
            rid = "RC_ARM_{}_{}".format(metal, slot)
            out = "ARM_{}_{}".format(metal, slot)
            ingredients = "{} x{}; MAT_LEATHER_ROLL x1; CMP_BUCKLE_SET x2; MAT_FIBER_BUNDLE x1".format(
                mat, qty)
            rows.append(_r(rid, out, ingredients, time_s, station, unlock))
    return rows


def _arrow_recipes():
    # Each arrow type calls out a real-world / Jovianlight ingredient that
    # justifies the payload. Stack 10 per craft.
    A = lambda rid, out, ingr, time_s, station, unlock: [
        rid, "{} ×10".format(out), ingr, "{}s".format(time_s),
        station, unlock, "Crude Arsenal (auto-seeded)"]
    return [
        A("RC_AMO_ARROW_STANDARD",      "AMO_ARROW_STANDARD",
          "MAT_WOOD_LOG x1; MAT_FEATHER x4; MAT_SCRAP_METAL x1",
          30, "Camp Workbench", "TN_BASIC_TOOLING"),
        A("RC_AMO_ARROW_BROADHEAD",     "AMO_ARROW_BROADHEAD",
          "MAT_WOOD_LOG x1; MAT_FEATHER x4; MAT_SCRAP_METAL x2",
          45, "Anvil Forge",   "TN_METALWORK_T1"),
        A("RC_AMO_ARROW_ARMOR_PIERCING","AMO_ARROW_ARMOR_PIERCING",
          "MAT_WOOD_LOG x1; MAT_FEATHER x4; MAT_BLACKGLASS_PLATE x1; MAT_MAGNET_INGOT x1",
          60, "Anvil Forge",   "TN_METALWORK_T2"),
        A("RC_AMO_ARROW_POISON",        "AMO_ARROW_POISON",
          "AMO_ARROW_STANDARD x10; MAT_PREDATOR_GLAND x1; MAT_BITTER_CRYSTAL x1",
          60, "Chem Bench",    "TN_CHEMISTRY_T1"),
        A("RC_AMO_ARROW_TRANQ",         "AMO_ARROW_TRANQ",
          "AMO_ARROW_STANDARD x10; MAT_FUNGAL_SPORE x2; MAT_RESIN_POUCH x1",
          60, "Chem Bench",    "TN_CHEMISTRY_T1"),
        A("RC_AMO_ARROW_CRYO",          "AMO_ARROW_CRYO",
          "AMO_ARROW_STANDARD x10; MAT_FROSTSPARK_CRYSTAL x1",
          75, "Chem Bench",    "TN_CRYO_T2"),
        A("RC_AMO_ARROW_EMP",           "AMO_ARROW_EMP",
          "AMO_ARROW_STANDARD x10; CMP_CAPACITOR_BANK x1; MAT_SUNWIRE_INGOT x1",
          90, "Machine Bench", "TN_ELECTRONICS_T1"),
        A("RC_AMO_ARROW_SMOKE",         "AMO_ARROW_SMOKE",
          "AMO_ARROW_STANDARD x10; MAT_SPORE_DUST x2; MAT_LIME_POWDER x1",
          45, "Chem Bench",    "TN_CHEMISTRY_T1"),
        A("RC_AMO_ARROW_TRACKER",       "AMO_ARROW_TRACKER",
          "AMO_ARROW_STANDARD x10; CMP_CIRCUIT_BOARD x1; MAT_FUNGAL_SPORE x1",
          90, "Machine Bench", "TN_ELECTRONICS_T1"),
        A("RC_AMO_ARROW_FIRE",          "AMO_ARROW_FIRE",
          "AMO_ARROW_STANDARD x10; MAT_RESIN_POUCH x1; MAT_TINDER x2",
          45, "Camp Workbench", "TN_BASIC_TOOLING"),
        A("RC_AMO_ARROW_EXPLOSIVE",     "AMO_ARROW_EXPLOSIVE",
          "AMO_ARROW_STANDARD x10; MAT_SPARKSTONE_CORE x1; CMP_FUSE_PACK x1",
          120, "Machine Bench", "TN_ENERGETICS_T2"),
        A("RC_AMO_BOLT_STANDARD",       "AMO_BOLT_STANDARD",
          "MAT_WOOD_LOG x1; MAT_SCRAP_METAL x2",
          30, "Camp Workbench", "TN_BASIC_TOOLING"),
        A("RC_AMO_BOLT_ARMOR_PIERCING", "AMO_BOLT_ARMOR_PIERCING",
          "MAT_WOOD_LOG x1; MAT_BLACKGLASS_PLATE x1; MAT_MAGNET_INGOT x1",
          60, "Anvil Forge",   "TN_METALWORK_T2"),
        A("RC_AMO_BOLT_EXPLOSIVE",      "AMO_BOLT_EXPLOSIVE",
          "AMO_BOLT_STANDARD x10; MAT_SPARKSTONE_CORE x1; CMP_FUSE_PACK x1",
          120, "Machine Bench", "TN_ENERGETICS_T2"),
    ]


# ── Tech node rows (research) ──────────────────────────────────────
# Schema: ---, TechNodeId, DisplayName, Tier, Family, Prerequisites,
#         RequiredReferenceComponentId, ResearchPointsRequired,
#         UnlockedRecipeIds, Notes
def _tech_rows():
    def T(tid, name, tier, family, prereq, points, unlocks, notes):
        return ["---", tid, '"{}"'.format(name), tier, family,
                prereq, "", points, unlocks, notes]
    return [
        T("TN_METALWORK_T1", "Metalwork Tier 1", "T1_Basic", "Materials",
          "TN_BASIC_TOOLING", 100,
          "RC_WPN_SCRAP_SHIELD+RC_ARM_FERRIC_HELM+RC_ARM_FERRIC_CHEST+RC_ARM_FERRIC_LEGS+"
          "RC_WPN_FERRIC_SWORD+RC_WPN_FERRIC_AXE+RC_WPN_FERRIC_SPEAR+"
          "RC_WPN_MAGNET_SWORD+RC_WPN_MAGNET_AXE+RC_WPN_MAGNET_SPEAR+"
          "RC_ARM_MAGNET_HELM+RC_ARM_MAGNET_CHEST+RC_ARM_MAGNET_LEGS",
          "Forge work + ferric/magnet blades + ferric armour."),
        T("TN_METALWORK_T2", "Metalwork Tier 2", "T2_Intermediate", "Materials",
          "TN_METALWORK_T1", 200,
          "RC_WPN_BLACKGLASS_SWORD+RC_WPN_BLACKGLASS_AXE+RC_WPN_BLACKGLASS_SPEAR+"
          "RC_ARM_BLACKGLASS_HELM+RC_ARM_BLACKGLASS_CHEST+RC_ARM_BLACKGLASS_LEGS+"
          "RC_WPN_RIOT_SHIELD+RC_AMO_ARROW_ARMOR_PIERCING+RC_AMO_BOLT_ARMOR_PIERCING",
          "Volcanic-glass edged weapons + riot shield + AP rounds."),
        T("TN_CHEMISTRY_T1", "Chemistry Tier 1", "T1_Basic", "Materials",
          "TN_BASIC_TOOLING", 80,
          "RC_AMO_ARROW_POISON+RC_AMO_ARROW_TRANQ+RC_AMO_ARROW_SMOKE",
          "Toxin extraction + fungal sedative + aerosol arrows."),
        T("TN_ELECTRONICS_T1", "Electronics Tier 1", "T1_Basic", "Energy",
          "TN_BASIC_TOOLING", 120,
          "RC_AMO_ARROW_EMP+RC_AMO_ARROW_TRACKER+"
          "RC_WPN_SUNWIRE_SWORD+RC_WPN_SUNWIRE_AXE+RC_WPN_SUNWIRE_SPEAR+"
          "RC_ARM_SUNWIRE_HELM+RC_ARM_SUNWIRE_CHEST+RC_ARM_SUNWIRE_LEGS",
          "Capacitor-based projectiles + sunwire alloys."),
        T("TN_CRYO_T2", "Cryogenics Tier 2", "T2_Intermediate", "Energy",
          "TN_ELECTRONICS_T1", 200,
          "RC_AMO_ARROW_CRYO+"
          "RC_WPN_FROSTSPARK_SWORD+RC_WPN_FROSTSPARK_AXE+RC_WPN_FROSTSPARK_SPEAR+"
          "RC_ARM_FROSTSPARK_HELM+RC_ARM_FROSTSPARK_CHEST+RC_ARM_FROSTSPARK_LEGS",
          "Frostspark-crystal payloads + cryo armour."),
        T("TN_ENERGETICS_T2", "Energetics Tier 2", "T2_Intermediate", "Energy",
          "TN_ELECTRONICS_T1", 250,
          "RC_AMO_ARROW_EXPLOSIVE+RC_AMO_BOLT_EXPLOSIVE+"
          "RC_WPN_SPARKSTONE_SWORD+RC_WPN_SPARKSTONE_AXE+RC_WPN_SPARKSTONE_SPEAR+"
          "RC_ARM_SPARKSTONE_HELM+RC_ARM_SPARKSTONE_CHEST+RC_ARM_SPARKSTONE_LEGS",
          "Sparkstone warheads + sparkstone armour set."),
    ]


def _append_csv(path, new_rows, key_col, replace):
    if not os.path.isfile(path):
        print("ERR: {} not found".format(path))
        return 0

    with open(path, "r", newline="", encoding="utf-8") as f:
        reader = csv.reader(f)
        rows = list(reader)
    header = rows[0]
    existing = {r[key_col] for r in rows[1:] if r and len(r) > key_col}

    added = 0
    replaced = 0
    keep = [rows[0]] + [r for r in rows[1:]
                        if replace == False or r[key_col] not in {nr[key_col] for nr in new_rows}]
    for nr in new_rows:
        if nr[key_col] in existing:
            if replace:
                replaced += 1
            else:
                continue
        keep.append(nr)
        added += 1 if nr[key_col] not in existing else 0

    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerows(keep)

    print("[recipes] {}: +{} added, {} replaced".format(
        os.path.basename(path), added, replaced))
    return added + replaced


def main(replace=False):
    recipes = (
        _crude_melee_recipes() +
        _metal_blade_recipes() +
        _bow_recipes() +
        _shield_recipes() +
        _armor_recipes() +
        _arrow_recipes()
    )
    _append_csv(RECIPES_CSV, recipes, key_col=0, replace=replace)
    _append_csv(TECHNODES_CSV, _tech_rows(), key_col=1, replace=replace)
    print("[recipes] done. Reimport DT_Recipes + DT_TechNodes in UE.")


if __name__ == "__main__":
    main(replace=("--replace" in sys.argv))
