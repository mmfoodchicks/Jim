"""
Quiet Rift: Enigma — seed the 14 canonical UQRBiomeProfile data assets.

Aligned with Master GDD §4 Biome Catalog and Visual World Bible v1.5
§3 Biome Visual Table. Each profile is tagged with a DepthBand
(Surface / Mid / Deep / Remnant) so trees + predator pools progress
correctly the further into the Rift the player travels.

Tree progression per GDD Visual World Bible §5:
  TRE_GLASSBARK   — Surface (BasaltShelf, MeltlineEdges, WindPlains)
  TRE_VELVETSPINE — Mid     (WindPlains, RidgeShadows)
  TRE_SLAGROOT    — Mid     (ThermalCracks, CraterFloors)
  TRE_ASTERBARK   — Deep    (MagneticRidges, HighRims)  ← premium

The script creates each biome with a palette pre-filled from
confirmed-existing Fab assets used as stand-ins (until the real
TRE_*/PLT_* meshes ship). Designer can swap meshes per row as art
lands. Already-existing profiles are updated in place (idempotent).

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_seed_biome_profiles.py').read())
"""

import unreal


BIOME_DIR = "/Game/QuietRift/Data/Biomes"


# Helpers used for the placeholder palettes. Real flora/fauna meshes
# under /Game/Meshes/{flora,wildlife}_assets/ will eventually replace
# these Fab stand-ins.
SCIFI_TREE     = "/Game/Fabs/ScifiJungle/Models/Trees/SM_Tree_Jungle_01.SM_Tree_Jungle_01"
SCIFI_TREE_2   = "/Game/Fabs/ScifiJungle/Models/Trees/SM_Tree_Jungle_02.SM_Tree_Jungle_02"
OWD_PLANT_1    = "/Game/Fabs/OWD_Plants_Pack/Plants/SM_Plant_01.SM_Plant_01"
OWD_PLANT_2    = "/Game/Fabs/OWD_Plants_Pack/Plants/SM_Plant_02.SM_Plant_02"
OWD_PLANT_3    = "/Game/Fabs/OWD_Plants_Pack/Plants/SM_Plant_03.SM_Plant_03"
ROCK_1         = "/Game/Fabs/Rock_Collection_04/Meshes/SM_Rock01.SM_Rock01"
ROCK_2         = "/Game/Fabs/Rock_Collection_04/Meshes/SM_Rock02.SM_Rock02"
ROCK_3         = "/Game/Fabs/Rock_Collection_04/Meshes/SM_Rock03.SM_Rock03"
POLAR_ICE_1    = "/Game/Fabs/Polar/Meshes/SM_Polar_Ice_01.SM_Polar_Ice_01"
POLAR_ROCK_1   = "/Game/Fabs/Polar/Meshes/SM_Polar_Rock_01.SM_Polar_Rock_01"
WINTER_PINE_1  = "/Game/Fabs/WinterTown/Meshes/SM_Tree_Pine_Snow_01.SM_Tree_Pine_Snow_01"

# Sound + sky stand-ins.
AMBIENT_WIND   = "/Game/Fabs/Free_Sounds_Pack/cue/Ambient_Wind_Loop_1_Cue.Ambient_Wind_Loop_1_Cue"
AMBIENT_BIRDS  = "/Game/Fabs/Free_Sounds_Pack/cue/Ambient_Birds_Loop_04_Cue.Ambient_Birds_Loop_04_Cue"
LANDSCAPE_AUTO = "/Game/Fabs/MWLandscapeAutoMaterial/Materials/M_MWAM_Landscape.M_MWAM_Landscape"


# Each biome entry follows GDD §4 Biome Catalog mapping. Palette is
# (asset_path, weight, min_scale, max_scale, z_offset, align_to_surface).
# Missing assets are silently skipped so the script keeps working as
# packs change.
BIOMES = {
    # ── Surface (Tier 1) — starter, readable sightlines ─────────────
    "BP_BasaltShelf": {
        "DisplayName": "Basalt Shelf",
        "Tag":         "Biome.BasaltShelf",
        "Depth":       "Surface",
        "Suggested":   {"target": 350, "spacing": 140.0, "slope": 45.0},
        "Landscape":   LANDSCAPE_AUTO,
        "Ambient":     AMBIENT_WIND,
        "Palette": [
            # TRE_GLASSBARK stand-in (Tier 1 wood)
            (SCIFI_TREE,    1.5, 0.9, 1.5,   0.0, False),
            (OWD_PLANT_1,   3.0, 0.7, 1.2,   0.0, False),
            (OWD_PLANT_2,   3.0, 0.7, 1.2,   0.0, False),
            (ROCK_1,        2.0, 0.6, 1.4, -10.0, True),
            (ROCK_2,        1.5, 0.7, 1.5, -15.0, True),
        ],
    },
    "BP_WindPlains": {
        "DisplayName": "Wind Plains",
        "Tag":         "Biome.WindPlains",
        "Depth":       "Surface",
        "Suggested":   {"target": 250, "spacing": 200.0, "slope": 35.0},
        "Landscape":   LANDSCAPE_AUTO,
        "Ambient":     AMBIENT_WIND,
        "Palette": [
            # Mostly low spiral reeds; sparse Glassbark/Velvetspine outliers
            (OWD_PLANT_3,   5.0, 0.6, 1.1,   0.0, False),
            (SCIFI_TREE,    0.6, 0.9, 1.4,   0.0, False),  # rare canopy
            (ROCK_1,        0.7, 0.5, 1.0, -5.0,  True),
        ],
    },
    "BP_MeltlineEdges": {
        "DisplayName": "Meltline Edges",
        "Tag":         "Biome.MeltlineEdges",
        "Depth":       "Surface",
        "Suggested":   {"target": 400, "spacing": 90.0, "slope": 50.0},
        "Landscape":   LANDSCAPE_AUTO,
        "Ambient":     AMBIENT_BIRDS,
        "Palette": [
            (SCIFI_TREE,    1.0, 0.9, 1.5,   0.0, False),
            (SCIFI_TREE_2,  1.0, 0.9, 1.5,   0.0, False),
            (OWD_PLANT_1,   4.0, 0.7, 1.3,   0.0, False),
            (OWD_PLANT_2,   4.0, 0.7, 1.3,   0.0, False),
            (ROCK_3,        1.0, 0.7, 1.4, -15.0, True),
        ],
    },
    "BP_CraterFloors": {
        "DisplayName": "Crater Floors",
        "Tag":         "Biome.CraterFloors",
        "Depth":       "Surface",
        "Suggested":   {"target": 220, "spacing": 250.0, "slope": 30.0},
        "Landscape":   LANDSCAPE_AUTO,
        "Ambient":     AMBIENT_WIND,
        "Palette": [
            (ROCK_1,        3.0, 0.7, 1.8, -20.0, True),
            (ROCK_2,        3.0, 0.7, 1.8, -20.0, True),
            (OWD_PLANT_3,   1.0, 0.6, 1.0,   0.0, False),
        ],
    },

    # ── Mid (Tier 2) — vertical / damp / first hazard signal ────────
    "BP_WetBasins": {
        "DisplayName": "Wet Basins",
        "Tag":         "Biome.WetBasins",
        "Depth":       "Mid",
        "Suggested":   {"target": 500, "spacing": 80.0, "slope": 55.0},
        "Landscape":   LANDSCAPE_AUTO,
        "Ambient":     AMBIENT_BIRDS,
        "Palette": [
            (OWD_PLANT_1,   5.0, 0.7, 1.4,   0.0, False),
            (OWD_PLANT_2,   5.0, 0.7, 1.4,   0.0, False),
            (OWD_PLANT_3,   4.0, 0.7, 1.4,   0.0, False),
            (SCIFI_TREE,    1.5, 1.0, 1.7,   0.0, False),  # TRE_VELVETSPINE stand-in
            (SCIFI_TREE_2,  1.0, 1.0, 1.7,   0.0, False),
            (ROCK_3,        0.8, 0.6, 1.2, -10.0, True),
        ],
    },
    "BP_ShallowFens": {
        "DisplayName": "Shallow Fens",
        "Tag":         "Biome.ShallowFens",
        "Depth":       "Mid",
        "Suggested":   {"target": 450, "spacing": 90.0, "slope": 45.0},
        "Landscape":   LANDSCAPE_AUTO,
        "Ambient":     AMBIENT_BIRDS,
        "Palette": [
            (OWD_PLANT_2,   5.0, 0.7, 1.3,   0.0, False),
            (OWD_PLANT_3,   5.0, 0.7, 1.3,   0.0, False),
            (SCIFI_TREE,    1.0, 1.0, 1.6,   0.0, False),
        ],
    },
    "BP_ThermalCracks": {
        "DisplayName": "Thermal Cracks",
        "Tag":         "Biome.ThermalCracks",
        "Depth":       "Mid",
        "Suggested":   {"target": 200, "spacing": 180.0, "slope": 60.0},
        "Landscape":   LANDSCAPE_AUTO,
        "Ambient":     AMBIENT_WIND,
        "Palette": [
            # TRE_SLAGROOT stand-in — dense structural wood
            (SCIFI_TREE_2,  1.5, 0.8, 1.4,   0.0, False),
            (ROCK_1,        3.5, 0.8, 1.6, -10.0, True),
            (ROCK_2,        3.5, 0.8, 1.6, -10.0, True),
            (OWD_PLANT_1,   0.8, 0.6, 1.0,   0.0, False),
        ],
    },
    "BP_GlassDunes": {
        "DisplayName": "Glass Dunes",
        "Tag":         "Biome.GlassDunes",
        "Depth":       "Mid",
        "Suggested":   {"target": 180, "spacing": 220.0, "slope": 40.0},
        "Landscape":   LANDSCAPE_AUTO,
        "Ambient":     AMBIENT_WIND,
        "Palette": [
            (ROCK_1,        4.0, 0.5, 1.6, -25.0, True),
            (ROCK_3,        4.0, 0.5, 1.8, -30.0, True),
            (OWD_PLANT_3,   1.0, 0.5, 0.9,   0.0, False),
        ],
    },
    "BP_MossFields": {
        "DisplayName": "Moss Fields",
        "Tag":         "Biome.MossFields",
        "Depth":       "Mid",
        "Suggested":   {"target": 600, "spacing": 70.0, "slope": 35.0},
        "Landscape":   LANDSCAPE_AUTO,
        "Ambient":     AMBIENT_BIRDS,
        "Palette": [
            (OWD_PLANT_1,   6.0, 0.8, 1.3,   0.0, False),
            (OWD_PLANT_2,   6.0, 0.8, 1.3,   0.0, False),
            (OWD_PLANT_3,   5.0, 0.8, 1.3,   0.0, False),
            (SCIFI_TREE,    0.5, 1.0, 1.6,   0.0, False),
        ],
    },

    # ── Deep (Tier 3) — premium resources, hostile, vertical ────────
    "BP_MagneticRidges": {
        "DisplayName": "Magnetic Ridges",
        "Tag":         "Biome.MagneticRidges",
        "Depth":       "Deep",
        "Suggested":   {"target": 300, "spacing": 180.0, "slope": 65.0},
        "Landscape":   LANDSCAPE_AUTO,
        "Ambient":     AMBIENT_WIND,
        "Palette": [
            # TRE_ASTERBARK stand-in — premium late-tier wood
            (SCIFI_TREE_2,  2.0, 1.1, 1.7,   0.0, False),
            (ROCK_1,        3.0, 0.7, 1.5, -10.0, True),
            (ROCK_3,        2.5, 0.8, 1.6, -15.0, True),
        ],
    },
    "BP_HighRims": {
        "DisplayName": "High Rims",
        "Tag":         "Biome.HighRims",
        "Depth":       "Deep",
        "Suggested":   {"target": 200, "spacing": 200.0, "slope": 70.0},
        "Landscape":   LANDSCAPE_AUTO,
        "Ambient":     AMBIENT_WIND,
        "Palette": [
            (SCIFI_TREE_2,  1.5, 1.0, 1.6,   0.0, False),  # Asterbark stand-in
            (ROCK_3,        4.0, 0.8, 1.8, -10.0, True),
            (POLAR_ROCK_1,  2.0, 0.7, 1.4, -10.0, True),
        ],
    },
    "BP_ColdBasins": {
        "DisplayName": "Cold Basins",
        "Tag":         "Biome.ColdBasins",
        "Depth":       "Deep",
        "Suggested":   {"target": 350, "spacing": 160.0, "slope": 50.0},
        "Landscape":   LANDSCAPE_AUTO,
        "Ambient":     AMBIENT_WIND,
        "Palette": [
            (POLAR_ICE_1,   2.0, 0.8, 1.5,   0.0, False),
            (POLAR_ROCK_1,  3.0, 0.7, 1.6, -15.0, True),
            (WINTER_PINE_1, 1.0, 0.9, 1.4,   0.0, False),
            (ROCK_2,        1.5, 0.7, 1.3, -20.0, True),
        ],
    },
    "BP_CanyonWebs": {
        "DisplayName": "Canyon Webs",
        "Tag":         "Biome.CanyonWebs",
        "Depth":       "Deep",
        "Suggested":   {"target": 280, "spacing": 110.0, "slope": 80.0},
        "Landscape":   LANDSCAPE_AUTO,
        "Ambient":     AMBIENT_BIRDS,
        "Palette": [
            (SCIFI_TREE,    1.5, 1.0, 1.6,   0.0, False),
            (SCIFI_TREE_2,  1.0, 1.0, 1.6,   0.0, False),
            (ROCK_3,        2.5, 0.8, 1.4, -15.0, True),
            (OWD_PLANT_2,   2.0, 0.7, 1.2,   0.0, False),
        ],
    },
    "BP_RidgeShadows": {
        "DisplayName": "Ridge Shadows",
        "Tag":         "Biome.RidgeShadows",
        "Depth":       "Deep",
        "Suggested":   {"target": 320, "spacing": 130.0, "slope": 60.0},
        "Landscape":   LANDSCAPE_AUTO,
        "Ambient":     AMBIENT_WIND,
        "Palette": [
            # TRE_VELVETSPINE stand-in
            (SCIFI_TREE_2,  2.0, 1.0, 1.6,   0.0, False),
            (ROCK_1,        2.0, 0.7, 1.4, -10.0, True),
            (OWD_PLANT_2,   1.5, 0.7, 1.2,   0.0, False),
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

    cls = unreal.load_object(None, "/Script/QuietRiftEnigma.QRBiomeProfile")
    if not cls:
        print("[biome] UQRBiomeProfile not loaded — compile C++ first")
        return None

    factory = unreal.DataAssetFactory()
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


_DEPTH_MAP = {
    "Surface": unreal.QRDepthBand.SURFACE,
    "Mid":     unreal.QRDepthBand.MID,
    "Deep":    unreal.QRDepthBand.DEEP,
    "Remnant": unreal.QRDepthBand.REMNANT,
}


def _populate(asset, spec):
    asset.set_editor_property("display_name",         unreal.Text(spec["DisplayName"]))
    asset.set_editor_property("biome_tag",            unreal.Name(spec["Tag"]))
    asset.set_editor_property("depth_band",           _DEPTH_MAP.get(spec["Depth"], unreal.QRDepthBand.SURFACE))

    sug = spec["Suggested"]
    asset.set_editor_property("suggested_target_count",   sug["target"])
    asset.set_editor_property("suggested_min_spacing",    sug["spacing"])
    asset.set_editor_property("suggested_max_slope_deg",  sug["slope"])

    palette = _build_palette(spec["Palette"])
    asset.set_editor_property("palette", palette)

    if spec.get("Landscape") and unreal.EditorAssetLibrary.does_asset_exist(spec["Landscape"]):
        asset.set_editor_property("landscape_material", unreal.load_asset(spec["Landscape"]))
    if spec.get("Ambient") and unreal.EditorAssetLibrary.does_asset_exist(spec["Ambient"]):
        asset.set_editor_property("ambient_loop", unreal.load_asset(spec["Ambient"]))

    print("[biome] {:<24s} depth={:<7s} palette={} entries".format(
        asset.get_name(), spec["Depth"], len(palette)))


def run():
    _ensure_dir(BIOME_DIR)

    # Delete the legacy placeholders if they're hanging around.
    for legacy in ("BP_AlienJungle", "BP_PolarTundra", "BP_DesertSand"):
        legacy_path = "{}/{}".format(BIOME_DIR, legacy)
        if unreal.EditorAssetLibrary.does_asset_exist(legacy_path):
            unreal.EditorAssetLibrary.delete_asset(legacy_path)
            print("[biome] deleted legacy " + legacy)

    for name, spec in BIOMES.items():
        asset = _make_or_load(name)
        if not asset:
            continue
        _populate(asset, spec)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
    print("[biome] done — {} canonical biome profiles".format(len(BIOMES)))


if __name__ == "__main__":
    run()
