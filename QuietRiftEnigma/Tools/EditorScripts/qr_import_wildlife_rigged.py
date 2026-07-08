"""
qr_import_wildlife_rigged.py -- import the rigged wildlife FBX set
(Tools/BlenderScripts/qr_generate_wildlife_rigged.py output) as
SkeletalMeshes + AnimSequences, then stamp every AQRWildlife_* class
default with its skinned body + Idle/Walk/Run/Death anims.

After this runs, every spawned wildlife actor animates through
AQRWildlifeBase's single-node swap (velocity-driven Idle/Walk/Run,
death pose on kill) -- same pattern as NPCs and the colony dog.
Species without a C++ class (CSV-only roster) still get their assets
imported for future use.

Run from the UE Python console AFTER regenerate_all_fbx.bat:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_import_wildlife_rigged.py').read())
  run()                  # import missing + stamp classes
  run(reimport=True)     # force reimport all FBX

Idempotent -- skips FBX whose SkeletalMesh already exists unless
reimport=True; stamping is safe to re-run.
"""

import os
import unreal

FBX_DIR = os.path.normpath(os.path.join(
    unreal.Paths.project_dir(), "Content", "Meshes", "wildlife_rigged"))
DEST = "/Game/Meshes/WildlifeRigged"

# SpeciesId -> C++ class stub (None = no gameplay class yet; import only).
SPECIES_CLASS = {
    "ANM_RidgebackGrazer":       "QRWildlife_RidgebackGrazer",
    "ANM_GlasshornRunner":       "QRWildlife_GlasshornRunner",
    "ANM_AshbackBoar":           "QRWildlife_AshbackBoar",
    "ANM_HookjawStalker":        "QRWildlife_HookjawStalker",
    "ANM_ThornhideDray":         "QRWildlife_ThornhideDray",
    "ANM_MireOx":                None,
    "ANM_LanternMiteSwarm":      None,
    "ANM_BurrowEel":             None,
    "ANM_IronmantleBeetle":      None,
    "ANM_PaleRafter":            None,
    "ANM_StonebellyTortoise":    None,
    "ANM_EmbermaneAlpha":        None,
    "ANI_PEBBLE_SKITTER":        None,
    "ANI_GLEAM_LARVER":          None,
    "ANI_SHARDBACK_GRAZER":      "QRWildlife_ShardbackGrazer",
    "ANI_SILT_STRIDER":          "QRWildlife_SiltStrider",
    "ANI_PILLARBACK_HAULER":     "QRWildlife_PillarbackHauler",
    "ANI_BASIN_TREADER":         None,
    "ANI_NESTWEAVER_DRIFTER":    "QRWildlife_NestweaverDrifter",
    "ANI_MILKBLADDER_HERDLING":  None,
    "ANI_BONE_LANTERN":          None,
    "ANI_LATCHFIN_MITE":         None,
    "ANI_CRACKRUNNER":           None,
    "ANI_RIDGE_COURSER":         "QRWildlife_RidgeCourser",
    "ANI_VAULTBACK_DRAY":        "QRWildlife_VaultbackDray",
    "ANI_TETHERBACK_PACKGRAZER": None,
    "PRD_SUTURE_WISP":           "QRWildlife_SutureWisp",
    "PRD_NEEDLE_MAW":            None,
    "PRD_DRIFT_STALKER":         None,
    "PRD_VANE_RIPPERS":          "QRWildlife_VaneRippers",
    "PRD_GLASSJAW_CLUSTER":      None,
    "PRD_SILT_HOUNDS":           None,
    "PRD_TRENCH_DIGGERS":        "QRWildlife_TrenchDiggers",
    "PRD_CARRION_CHOIR":         None,
    "PRD_FOGLEECH_SWARM":        "QRWildlife_FogleechSwarm",
    "PRD_IRONSTAG_STALKER":      "QRWildlife_IronstagStalker",
    "PRD_SHELLMAW_AMBUSHER":     "QRWildlife_ShellmawAmbusher",
    # ANM_ColonyDog intentionally absent -- it uses the Fab pack's
    # SK_GermanShepherd_01, wired directly in its constructor.
}

ANIM_SLOTS = [("idle_anim", "A_Idle"), ("walk_anim", "A_Walk"),
              ("run_anim", "A_Run"), ("death_anim", "A_Death")]


def _import_one(fbx_path, dest_dir, reimport):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("create_physics_asset", False)
    ui.set_editor_property("mesh_type_to_import",
                           unreal.FBXImportType.FBXIT_SKELETAL_MESH)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", fbx_path)
    task.set_editor_property("destination_path", dest_dir)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", reimport)
    task.set_editor_property("options", ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return list(task.get_editor_property("imported_object_paths") or [])


def _dest_dir_for(sid):
    return "{}/{}".format(DEST, sid)


def _find_asset(dest_dir, class_name, name_contains):
    """First asset of class under dest_dir whose name contains the token."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for ad in registry.get_assets_by_path(dest_dir, recursive=True):
        try:
            if str(ad.asset_class_path.asset_name) != class_name:
                continue
        except Exception:
            continue
        if name_contains.lower() in str(ad.asset_name).lower():
            return unreal.load_asset("{}.{}".format(ad.package_name, ad.asset_name))
    return None


def _stamp_class(cls_stub, sid, dest_dir):
    cls = unreal.load_object(None, "/Script/QuietRiftEnigma.{}".format(cls_stub))
    if not cls:
        print("[wl-rig]   {} not loaded (recompile C++?)".format(cls_stub))
        return False
    cdo = unreal.get_default_object(cls)

    sk = _find_asset(dest_dir, "SkeletalMesh", "SK_{}".format(sid))
    if not sk:
        print("[wl-rig]   {}: no SkeletalMesh found under {}".format(sid, dest_dir))
        return False
    stamped = []
    try:
        cdo.set_editor_property("default_body_mesh", sk)
        stamped.append("mesh")
    except Exception as e:
        print("[wl-rig]   default_body_mesh failed on {}: {}".format(cls_stub, e))

    for prop, token in ANIM_SLOTS:
        seq = _find_asset(dest_dir, "AnimSequence", token)
        if not seq:
            continue
        try:
            cdo.set_editor_property(prop, seq)
            stamped.append(prop)
        except Exception as e:
            print("[wl-rig]   {} failed on {}: {}".format(prop, cls_stub, e))

    print("[wl-rig]   {:<28s} <- {} [{}]".format(cls_stub, sid, ", ".join(stamped)))
    return True


def run(reimport=False):
    print("\n=== qr_import_wildlife_rigged ===")
    if not os.path.isdir(FBX_DIR):
        print("[wl-rig] {} missing -- run qr_generate_wildlife_rigged.py "
              "in Blender first (regenerate_all_fbx.bat does it).".format(FBX_DIR))
        return

    imported = 0
    stamped = 0
    for sid, cls_stub in SPECIES_CLASS.items():
        fbx = os.path.join(FBX_DIR, "SK_{}.fbx".format(sid))
        if not os.path.isfile(fbx):
            print("[wl-rig]   SK_{}.fbx missing on disk -- skipped".format(sid))
            continue
        dest_dir = _dest_dir_for(sid)
        # Import each species into its own folder so anim takes can be
        # matched by name without cross-species collisions.
        have = unreal.EditorAssetLibrary.does_asset_exist(
            "{}/SK_{}".format(dest_dir, sid))
        if reimport or not have:
            _import_one(fbx, dest_dir, reimport=True)
            imported += 1
        if cls_stub and _stamp_class(cls_stub, sid, dest_dir):
            stamped += 1

    print("[wl-rig] done -- {} imported, {} classes stamped.".format(
        imported, stamped))
    print("[wl-rig] Run qr_build_qr_materials.run() next so the skinned")
    print("[wl-rig] bodies pick up their per-species materials.")


if __name__ == "__main__":
    run()
