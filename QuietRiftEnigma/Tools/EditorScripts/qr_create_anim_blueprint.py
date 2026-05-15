"""
Quiet Rift: Enigma — create ABP_QRPlayer animation Blueprint.

Builds a minimal AnimBlueprint asset at /Game/QuietRift/Animations/
ABP_QRPlayer with:
  • AnimInstance class      = UQRPlayerAnimInstance  (our C++ class)
  • TargetSkeleton          = first found SK_Mannequin (or whatever
                              skeleton lives under the player mesh path)
  • Default AnimGraph slot  = empty (returns SourcePose -> identity)

Why so light: the Anim Blueprint state machine — locomotion blendspaces,
combat layer, additive sway, IK — is a real authoring task that doesn't
benefit from being programmatically generated. What we CAN automate is
the boilerplate: create the asset, bind the right C++ AnimInstance,
target the right skeleton. Designer then opens the asset in editor and
drops state-machine nodes that read the variables our AnimInstance
already publishes (Speed, Direction, bIsFalling, bIsAiming, …).

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_create_anim_blueprint.py').read())

Or:
  import qr_create_anim_blueprint
  qr_create_anim_blueprint.run()

Idempotent — if /Game/QuietRift/Animations/ABP_QRPlayer already exists,
the script reports and exits. Pass run(overwrite=True) to recreate.
"""

import unreal


OUTPUT_DIR  = "/Game/QuietRift/Animations"
ABP_NAME    = "ABP_QRPlayer"
ABP_PATH    = "{}/{}".format(OUTPUT_DIR, ABP_NAME)

# Where to look for a Mannequin skeleton inside the project. Same list
# as qr_retarget_anims_to_mannequin.py — most Fab anim packs bundle the
# Mannequin asset under their Demo folder.
SKELETON_CANDIDATES = [
    "/Game/Characters/Mannequins/Meshes/SK_Mannequin",
    "/Game/Fabs/FreeAnimsMixPack/Demo/Mannequins/Meshes/SK_Mannequin",
    "/Game/Fabs/DynamicFalling/Demo/Characters/Mannequins/Meshes/SK_Mannequin",
    "/Game/Fabs/RamsterZ_FreeAnims_Volume1/Demo/Mannequin/Character/Mesh/SK_Mannequin",
    "/Game/Fabs/DeadBodies_Poses_nikoff/Demo/Mannequins/Meshes/SK_Mannequin",
]


def _ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _find_skeleton():
    """First existing Skeleton asset from SKELETON_CANDIDATES."""
    for p in SKELETON_CANDIDATES:
        if unreal.EditorAssetLibrary.does_asset_exist(p):
            ast = unreal.load_asset(p)
            if isinstance(ast, unreal.Skeleton):
                return ast
    # Maybe the candidates point at SkeletalMeshes — pull skeleton off them.
    for p in SKELETON_CANDIDATES:
        if unreal.EditorAssetLibrary.does_asset_exist(p):
            ast = unreal.load_asset(p)
            if isinstance(ast, unreal.SkeletalMesh):
                return ast.skeleton
    return None


def run(overwrite=False):
    _ensure_dir(OUTPUT_DIR)

    if unreal.EditorAssetLibrary.does_asset_exist(ABP_PATH):
        if not overwrite:
            print("[abp] " + ABP_PATH + " already exists — pass run(overwrite=True) to recreate")
            return
        unreal.EditorAssetLibrary.delete_asset(ABP_PATH)

    skel = _find_skeleton()
    if not skel:
        print("[abp] no Mannequin skeleton found in /Game/. Install the")
        print("      Third Person template or one of the Fab anim packs")
        print("      that bundles the Mannequin (FreeAnimsMixPack /")
        print("      DynamicFalling / RamsterZ / DeadBodies_Poses).")
        return

    # AnimInstance class — our C++ subclass.
    parent_cls_path = "/Script/QuietRiftEnigma.QRPlayerAnimInstance"
    parent_cls = unreal.load_object(None, parent_cls_path)
    if not parent_cls:
        print("[abp] couldn't load " + parent_cls_path + " — compile your C++ first")
        return

    # Build the asset via AnimBlueprintFactory.
    factory = unreal.AnimBlueprintFactory()
    factory.set_editor_property("target_skeleton", skel)
    factory.set_editor_property("parent_class",   parent_cls)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = tools.create_asset(
        asset_name=ABP_NAME,
        package_path=OUTPUT_DIR,
        asset_class=unreal.AnimBlueprint,
        factory=factory)

    if not asset:
        print("[abp] AnimBlueprint create_asset returned null")
        return

    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    print("[abp] created " + ABP_PATH)
    print("[abp]   target skeleton : " + skel.get_path_name())
    print("[abp]   parent class    : " + parent_cls_path)
    print("[abp] now open the asset and build the state machine using")
    print("      the variables UQRPlayerAnimInstance already exposes:")
    print("        Speed, Direction, bIsFalling, bIsCrouched, bIsSprinting,")
    print("        bIsAiming, bIsReloading, bIsJammed, WeaponAlpha,")
    print("        bIsDead, HealthPct, LeanAlpha")


if __name__ == "__main__":
    run()
