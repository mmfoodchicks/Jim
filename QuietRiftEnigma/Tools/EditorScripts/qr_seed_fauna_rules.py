"""
Quiet Rift: Enigma — seed UQRFaunaSpawnRule data assets per biome.

AQRWorldGenSpawner.FaunaRulesPerBiome is a TMap<FName, UQRFaunaSpawnRule*>.
Without entries, the spawner falls through to a 5% sparse fallback that
puts ~26 wildlife in an 8 km world — the "empty world" you noticed.

This script creates one UQRFaunaSpawnRule asset per canonical biome
under /Game/QuietRift/Data/FaunaRules/ and pre-populates each with the
existing AQRWildlife_* C++ classes, weighted by depth band:

  Surface  → grazers + scavengers, no predators
  Mid      → grazers + a stalker or two
  Deep     → mixed pack predators + heavy grazers
  Remnant  → high-tier predators, dense

After this runs, drop an AQRWorldGenSpawner into your level and the
companion qr_create_test_maps update wires the map → rule lookup for you.

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_seed_fauna_rules.py').read())
"""

import unreal


RULES_DIR = "/Game/QuietRift/Data/FaunaRules"

# Wildlife class path -> short name for SpeciesId. Verified against
# /Source/QuietRiftEnigma/Public/Wildlife/.
WILDLIFE = {
    "AshbackBoar":       "/Script/QuietRiftEnigma.QRWildlife_AshbackBoar",
    "FogleechSwarm":     "/Script/QuietRiftEnigma.QRWildlife_FogleechSwarm",
    "GlasshornRunner":   "/Script/QuietRiftEnigma.QRWildlife_GlasshornRunner",
    "HookjawStalker":    "/Script/QuietRiftEnigma.QRWildlife_HookjawStalker",
    "IronstagStalker":   "/Script/QuietRiftEnigma.QRWildlife_IronstagStalker",
    "NestweaverDrifter": "/Script/QuietRiftEnigma.QRWildlife_NestweaverDrifter",
    "PillarbackHauler":  "/Script/QuietRiftEnigma.QRWildlife_PillarbackHauler",
    "RidgeCourser":      "/Script/QuietRiftEnigma.QRWildlife_RidgeCourser",
    "RidgebackGrazer":   "/Script/QuietRiftEnigma.QRWildlife_RidgebackGrazer",
    "ShardbackGrazer":   "/Script/QuietRiftEnigma.QRWildlife_ShardbackGrazer",
    "ShellmawAmbusher":  "/Script/QuietRiftEnigma.QRWildlife_ShellmawAmbusher",
    "SiltStrider":       "/Script/QuietRiftEnigma.QRWildlife_SiltStrider",
    "SutureWisp":        "/Script/QuietRiftEnigma.QRWildlife_SutureWisp",
    "ThornhideDray":     "/Script/QuietRiftEnigma.QRWildlife_ThornhideDray",
    "TrenchDiggers":     "/Script/QuietRiftEnigma.QRWildlife_TrenchDiggers",
    "VaneRippers":       "/Script/QuietRiftEnigma.QRWildlife_VaneRippers",
    "VaultbackDray":     "/Script/QuietRiftEnigma.QRWildlife_VaultbackDray",
}


# Per-biome roster. (species_key, weight, min_group, max_group, predator).
# Names match qr_seed_biome_profiles BIOMES dict; the BiomeTag is what
# AQRWorldGenSpawner.FaunaRulesPerBiome looks up.
ROSTERS = {
    # ── Surface ───────────────────────────────────────────────
    "BasaltShelf":     [("ShardbackGrazer", 4.0, 2, 5, False),
                        ("RidgebackGrazer", 3.0, 2, 4, False),
                        ("AshbackBoar",     2.0, 1, 3, False),
                        ("FogleechSwarm",   1.0, 1, 1, False)],
    "WindPlains":      [("RidgebackGrazer", 5.0, 3, 8, False),
                        ("GlasshornRunner", 4.0, 2, 6, False),
                        ("ShardbackGrazer", 2.0, 1, 3, False)],
    "MeltlineEdges":   [("AshbackBoar",     3.0, 1, 3, False),
                        ("ShardbackGrazer", 2.0, 1, 2, False),
                        ("FogleechSwarm",   2.0, 1, 1, False),
                        ("HookjawStalker",  0.8, 1, 1, True)],
    "CraterFloors":    [("AshbackBoar",     3.0, 1, 3, False),
                        ("SiltStrider",     2.0, 1, 2, False)],

    # ── Mid ───────────────────────────────────────────────────
    "WetBasins":       [("ShellmawAmbusher", 2.5, 1, 2, True),
                        ("AshbackBoar",      2.0, 1, 2, False),
                        ("NestweaverDrifter",1.5, 1, 1, False)],
    "ThermalCracks":   [("HookjawStalker",   2.0, 1, 1, True),
                        ("RidgeCourser",     2.5, 1, 2, False),
                        ("ThornhideDray",    1.5, 1, 1, False)],
    "RidgeShadows":    [("IronstagStalker",  2.0, 1, 1, True),
                        ("PillarbackHauler", 1.5, 1, 1, False),
                        ("RidgeCourser",     2.5, 1, 2, False)],
    "MagneticRidges":  [("VaultbackDray",    2.0, 1, 1, False),
                        ("PillarbackHauler", 2.0, 1, 1, False),
                        ("RidgeCourser",     1.5, 1, 2, False)],

    # ── Deep ──────────────────────────────────────────────────
    "HighRims":        [("IronstagStalker",  2.5, 1, 2, True),
                        ("ThornhideDray",    2.0, 1, 1, False),
                        ("VaneRippers",      1.5, 1, 2, True)],
    "TrenchSystems":   [("TrenchDiggers",    3.0, 2, 4, False),
                        ("ShellmawAmbusher", 2.0, 1, 2, True),
                        ("HookjawStalker",   1.5, 1, 1, True)],
    "ResonantCaverns": [("SutureWisp",       3.0, 1, 3, True),
                        ("FogleechSwarm",    2.0, 1, 1, False),
                        ("NestweaverDrifter",2.5, 1, 2, False)],
    "AshShelves":      [("ThornhideDray",    2.5, 1, 1, False),
                        ("AshbackBoar",      2.0, 1, 2, False),
                        ("VaneRippers",      1.0, 1, 1, True)],

    # ── Remnant ───────────────────────────────────────────────
    "HazardBelt":      [("SutureWisp",       3.0, 2, 4, True),
                        ("VaneRippers",      2.5, 1, 3, True),
                        ("IronstagStalker",  2.0, 1, 2, True)],
    "RiftEdge":        [("SutureWisp",       4.0, 2, 5, True),
                        ("VaneRippers",      3.0, 2, 4, True)],
}


def _ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _create_rule(biome_tag, roster):
    asset_name = "FR_{}".format(biome_tag)
    asset_path = "{}/{}".format(RULES_DIR, asset_name)

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.QRFaunaSpawnRule)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    rule = tools.create_asset(asset_name, RULES_DIR, unreal.QRFaunaSpawnRule, factory)
    if not rule:
        print("[fauna] failed to create " + asset_path)
        return None

    rule.set_editor_property('biome_tag', unreal.Name(biome_tag))

    entries = []
    for species_key, weight, mn, mx, predator in roster:
        cls_path = WILDLIFE.get(species_key)
        if not cls_path:
            print("[fauna]   unknown species '{}' for {}".format(species_key, biome_tag))
            continue
        cls = unreal.load_class(None, cls_path)
        if not cls:
            print("[fauna]   class not loadable: {}".format(cls_path))
            continue
        entry = unreal.QRFaunaEntry()
        entry.set_editor_property('species_id', unreal.Name(species_key))
        entry.set_editor_property('actor_class', unreal.SoftClassPath(cls_path))
        entry.set_editor_property('weight', float(weight))
        entry.set_editor_property('min_group_size', int(mn))
        entry.set_editor_property('max_group_size', int(mx))
        entry.set_editor_property('b_is_predator', bool(predator))
        entries.append(entry)

    rule.set_editor_property('entries', entries)
    rule.set_editor_property('spawn_density_multiplier', 1.0)
    rule.set_editor_property('max_predator_fraction', 0.25)

    unreal.EditorAssetLibrary.save_asset(asset_path)
    print("[fauna] created {} ({} entries)".format(asset_name, len(entries)))
    return rule


def run():
    _ensure_dir(RULES_DIR)
    print("[fauna] seeding {} fauna rules under {}".format(len(ROSTERS), RULES_DIR))
    created = 0
    for biome_tag, roster in ROSTERS.items():
        if _create_rule(biome_tag, roster):
            created += 1
    print("[fauna] done — {} / {} rule assets created".format(created, len(ROSTERS)))


if __name__ == "__main__":
    run()
