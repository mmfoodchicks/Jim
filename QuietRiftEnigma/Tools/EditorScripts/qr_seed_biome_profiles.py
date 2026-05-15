"""
Quiet Rift: Enigma — create three starter UQRBiomeProfile data assets.

Creates:
  /Game/QuietRift/Data/Biomes/BP_AlienJungle    — ScifiJungle + OWD plants
  /Game/QuietRift/Data/Biomes/BP_PolarTundra    — Polar + WinterTown
  /Game/QuietRift/Data/Biomes/BP_DesertSand     — ROCKY_SAND_PACK + rocks

Each profile pre-fills the Palette array with assets confirmed to exist
in the project, plus suggested defaults for spacing / slope / count.
After running, designer drops an AQRProceduralScatterActor in a level,
sets its BiomeProfile to one of these, and clicks Generate.

Caveat: the Python API can't fully populate USTRUCT arrays via
set_editor_property in some 5.7 builds. The script creates the asset
with the right name + class — designer adds palette rows in the Details
panel if the bulk-fill path silently no-ops.

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_seed_biome_profiles.py').read())
"""

import unreal


BIOME_DIR = "/Game/QuietRift/Data/Biomes"


# Per-biome palette sources. (asset_path, weight, min_scale, max_scale,
# z_offset, align_to_surface). Entries with missing assets are silently
# skipped so the script keeps working as packs change.
BIOMES = {
    "BP_AlienJungle": {
        "DisplayName": "Alien Jungle",
        "Tag":         "Biome.AlienJungle",
        "Suggested":   {"target": 600, "spacing": 90.0, "slope": 55.0},
        "Landscape":   "/Game/Fabs/MWLandscapeAutoMaterial/Materials/M_MWAM_Landscape.M_MWAM_Landscape",
        "Sky":         "/Game/Fabs/Chaotic_Skies/Materials/M_Sky_Default.M_Sky_Default",
        "Ambient":     "/Game/Fabs/Free_Sounds_Pack/cue/Ambient_Birds_Loop_04_Cue.Ambient_Birds_Loop_04_Cue",
        "Palette": [
            # heavy plants
            ("/Game/Fabs/OWD_Plants_Pack/Plants/SM_Plant_01.SM_Plant_01",                   4.0, 0.7, 1.4,  0.0, False),
            ("/Game/Fabs/OWD_Plants_Pack/Plants/SM_Plant_02.SM_Plant_02",                   4.0, 0.7, 1.4,  0.0, False),
            ("/Game/Fabs/OWD_Plants_Pack/Plants/SM_Plant_03.SM_Plant_03",                   3.0, 0.7, 1.4,  0.0, False),
            # canopy trees
            ("/Game/Fabs/ScifiJungle/Models/Trees/SM_Tree_Jungle_01.SM_Tree_Jungle_01",     1.0, 0.9, 1.6,  0.0, False),
            ("/Game/Fabs/ScifiJungle/Models/Trees/SM_Tree_Jungle_02.SM_Tree_Jungle_02",     1.0, 0.9, 1.6,  0.0, False),
            # rocks tucked low
            ("/Game/Fabs/Rock_Collection_04/Meshes/SM_Rock01.SM_Rock01",                    1.5, 0.6, 1.4,-15.0, True),
            ("/Game/Fabs/Rock_Collection_04/Meshes/SM_Rock03.SM_Rock03",                    1.0, 0.7, 1.6,-25.0, True),
        ],
    },
    "BP_PolarTundra": {
        "DisplayName": "Polar Tundra",
        "Tag":         "Biome.PolarTundra",
        "Suggested":   {"target": 350, "spacing": 180.0, "slope": 40.0},
        "Landscape":   "/Game/Fabs/Polar/Materials/M_Polar_Snow.M_Polar_Snow",
        "Sky":         "/Game/Fabs/Chaotic_Skies/Materials/M_Sky_Overcast.M_Sky_Overcast",
        "Ambient":     "/Game/Fabs/Free_Sounds_Pack/cue/Ambient_Wind_Loop_1_Cue.Ambient_Wind_Loop_1_Cue",
        "Palette": [
            # frozen rocks
            ("/Game/Fabs/Polar/Meshes/SM_Polar_Rock_01.SM_Polar_Rock_01",                   2.0, 0.6, 1.5, -10.0, True),
            ("/Game/Fabs/Polar/Meshes/SM_Polar_Rock_02.SM_Polar_Rock_02",                   2.0, 0.7, 1.6, -15.0, True),
            # ice spikes
            ("/Game/Fabs/Polar/Meshes/SM_Polar_Ice_01.SM_Polar_Ice_01",                     1.0, 0.7, 1.5,   0.0, False),
            # winter tree silhouettes
            ("/Game/Fabs/WinterTown/Meshes/SM_Tree_Pine_Snow_01.SM_Tree_Pine_Snow_01",      0.7, 0.9, 1.4,   0.0, False),
            ("/Game/Fabs/WinterTown/Meshes/SM_Tree_Pine_Snow_02.SM_Tree_Pine_Snow_02",      0.7, 0.9, 1.4,   0.0, False),
        ],
    },
    "BP_DesertSand": {
        "DisplayName": "Desert Sand",
        "Tag":         "Biome.DesertSand",
        "Suggested":   {"target": 250, "spacing": 250.0, "slope": 50.0},
        "Landscape":   "/Game/Fabs/ROCKY_SAND_PACK/materials/M_RSP_Sand.M_RSP_Sand",
        "Sky":         "/Game/Fabs/Chaotic_Skies/Materials/M_Sky_Default.M_Sky_Default",
        "Ambient":     "/Game/Fabs/Free_Sounds_Pack/cue/Ambient_Wind_Loop_1_Cue.Ambient_Wind_Loop_1_Cue",
        "Palette": [
            # sand-buried rocks dominate
            ("/Game/Fabs/Rock_Collection_04/Meshes/SM_Rock01.SM_Rock01",                    3.0, 0.5, 1.8, -20.0, True),
            ("/Game/Fabs/Rock_Collection_04/Meshes/SM_Rock02.SM_Rock02",                    3.0, 0.6, 1.9, -25.0, True),
            ("/Game/Fabs/Rock_Collection_04/Meshes/SM_Rock03.SM_Rock03",                    2.0, 0.7, 2.0, -30.0, True),
        ],
    },
}


def _ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _make_or_load(name):
    full = "{}/{}".format(BIOME_DIR, name)
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        return unreal.load_asset(full)

    factory = unreal.DataAssetFactory()
    cls     = unreal.load_object(None, "/Script/QuietRiftEnigma.QRBiomeProfile")
    if not cls:
        print("[biome] UQRBiomeProfile not loaded — compile C++ first")
        return None
    factory.data_asset_class = cls

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    return tools.create_asset(
        asset_name=name,
        package_path=BIOME_DIR,
        asset_class=unreal.QRBiomeProfile,
        factory=factory)


def _build_palette(specs):
    out = []
    for path, w, mn, mx, z, align in specs:
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            continue
        e = unreal.QRScatterEntry()
        e.set_editor_property("mesh",               unreal.load_asset(path))
        e.set_editor_property("weight",             w)
        e.set_editor_property("min_scale",          mn)
        e.set_editor_property("max_scale",          mx)
        e.set_editor_property("z_offset",           z)
        e.set_editor_property("b_random_yaw",       True)
        e.set_editor_property("b_align_to_surface", align)
        out.append(e)
    return out


def _populate(asset, spec):
    asset.set_editor_property("display_name", unreal.Text(spec["DisplayName"]))
    asset.set_editor_property("biome_tag",    unreal.Name(spec["Tag"]))

    sug = spec["Suggested"]
    asset.set_editor_property("suggested_target_count",   sug["target"])
    asset.set_editor_property("suggested_min_spacing",    sug["spacing"])
    asset.set_editor_property("suggested_max_slope_deg", sug["slope"])

    palette = _build_palette(spec["Palette"])
    asset.set_editor_property("palette", palette)
    print("[biome] {} : {} palette entries valid".format(asset.get_name(), len(palette)))

    # Optional terrain / sky / ambient.
    for prop, key in (("landscape_material", "Landscape"),
                      ("sky_material",       "Sky"),
                      ("ambient_loop",       "Ambient")):
        path = spec.get(key)
        if path and unreal.EditorAssetLibrary.does_asset_exist(path):
            asset.set_editor_property(prop, unreal.load_asset(path))


def run():
    _ensure_dir(BIOME_DIR)
    for name, spec in BIOMES.items():
        asset = _make_or_load(name)
        if not asset:
            continue
        _populate(asset, spec)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
    print("[biome] done")


if __name__ == "__main__":
    run()
