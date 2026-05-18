"""
Quiet Rift: Enigma — wire a visible character body onto AQRCharacter.

The C++ AQRCharacter inherits ACharacter, which gives it a Mesh
component (USkeletalMeshComponent), but the project never assigned a
SkeletalMesh or AnimBlueprint to it — so PIE has no visible body.

This script:
  1. Finds the best available SkeletalMesh on disk (mannequin first,
     FuturisticWarrior second, anything else third).
  2. Finds an AnimBlueprint (ABP_QRPlayer if it exists; otherwise any).
  3. Creates /Game/QuietRift/Characters/BP_QRCharacter as a Blueprint
     child of AQRCharacter.
  4. Sets the inherited Mesh component's SkeletalMesh + AnimClass.
  5. Positions the body roughly under the camera so the user isn't
     standing on their own head.

AQRGameMode is updated separately (C++ side) to prefer BP_QRCharacter
over the bare C++ class when this asset exists.

After the asset is created, animations themselves (the state machine
inside ABP_QRPlayer) still need to be authored in the Anim Blueprint
editor — there's no automatic state-machine generator. This script
just gives you a body to put anims on.

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_create_player_blueprint.py').read())
"""

import unreal


BP_DIR  = "/Game/QuietRift/Characters"
BP_NAME = "BP_QRCharacter"
BP_PATH = "{}/{}".format(BP_DIR, BP_NAME)


# Candidate skeletal-mesh paths, in priority order. First one that
# actually loads wins.
SK_CANDIDATES = [
    "/Game/Mannequins/Meshes/SKM_Manny",
    "/Game/Mannequins/Meshes/SKM_Quinn",
    "/Game/Characters/Mannequins/Meshes/SKM_Manny",
    "/Game/Characters/Mannequins/Meshes/SKM_Quinn",
    "/Game/Fabs/Mannequins/Meshes/SKM_Manny",
    "/Game/Fabs/Mannequins/Meshes/SKM_Quinn",
    "/Game/Fabs/FuturisticWarrior/Mesh/SK_FuturisticWarrior",
    "/Game/FuturisticWarrior/Mesh/SK_FuturisticWarrior",
]


ANIM_BP_CANDIDATES = [
    "/Game/QuietRift/Animations/ABP_QRPlayer",
    "/Game/Characters/Mannequins/Animations/ABP_Manny",
    "/Game/Mannequins/Animations/ABP_Manny",
    "/Game/Fabs/Mannequins/Animations/ABP_Manny",
]


def _find_first_loadable(paths, expected_type=None):
    """Walk paths in order; return the first that loads to an asset of
    expected_type (if given), else returns the first that loads to
    anything at all."""
    for p in paths:
        asset = unreal.load_asset(p)
        if not asset:
            continue
        if expected_type is None or isinstance(asset, expected_type):
            return (p, asset)
    return (None, None)


def _scan_for_any_skeletal_mesh():
    """Fall-back: scan /Game for any SkeletalMesh and return the first."""
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    f = unreal.ARFilter(
        class_names=['SkeletalMesh'],
        package_paths=['/Game'],
        recursive_paths=True)
    for ad in ar.get_assets(f):
        p = "{}.{}".format(ad.package_name, ad.asset_name)
        a = unreal.load_asset(p)
        if a: return (p, a)
    return (None, None)


def _ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _set_mesh_defaults_on_bp(bp_path, sk_mesh, anim_bp_class):
    """Open the BP's CDO and assign the inherited Mesh component's
    SkeletalMesh + AnimClass. Inherited-component defaults set on the
    CDO propagate to every spawned instance unless an instance overrides
    them."""
    bp = unreal.load_asset(bp_path)
    if not bp:
        print("[player-bp] BP didn't load after create: {}".format(bp_path))
        return False

    generated_cls = bp.generated_class()
    cdo = unreal.get_default_object(generated_cls)
    if not cdo:
        print("[player-bp] no CDO for {}".format(bp_path))
        return False

    # ACharacter's inherited Mesh component name is "CharacterMesh0".
    # Python exposes it via the 'mesh' editor property on the CDO.
    mesh = cdo.get_editor_property('mesh')
    if not mesh:
        print("[player-bp] CDO has no Mesh component? Class hierarchy unexpected.")
        return False

    mesh.set_skeletal_mesh_asset(sk_mesh)
    if anim_bp_class:
        mesh.set_anim_instance_class(anim_bp_class)
    # Drop the mesh ~90 cm so feet land at capsule base rather than
    # floating mid-air on top of the capsule.
    mesh.set_relative_location(unreal.Vector(0.0, 0.0, -90.0))
    mesh.set_relative_rotation(unreal.Rotator(0.0, 0.0, -90.0))

    # Make sure others see the body even though the local player has
    # SetOwnerNoSee(true) set on the third-person mesh in C++. (Default
    # SetOwnerNoSee=false on the BP override so the local player can
    # see their feet looking down too — toggle if you prefer Doom-style
    # first-person-only.)
    mesh.set_only_owner_see(False)
    mesh.set_owner_no_see(False)

    unreal.EditorAssetLibrary.save_asset(bp_path)
    return True


def run():
    _ensure_dir(BP_DIR)

    # 1. Find SkeletalMesh
    sk_path, sk_mesh = _find_first_loadable(SK_CANDIDATES, unreal.SkeletalMesh)
    if not sk_mesh:
        sk_path, sk_mesh = _scan_for_any_skeletal_mesh()
    if not sk_mesh:
        print("[player-bp] no SkeletalMesh found anywhere under /Game — aborting.")
        return
    print("[player-bp] using SkeletalMesh: {}".format(sk_path))

    # 2. Find AnimBlueprint (optional — body still renders without one)
    ab_path, ab = _find_first_loadable(ANIM_BP_CANDIDATES, unreal.AnimBlueprint)
    ab_class = ab.generated_class() if ab else None
    if ab:
        print("[player-bp] using AnimBlueprint: {}".format(ab_path))
    else:
        print("[player-bp] no AnimBlueprint found — body will render in default pose.")

    # 3. Locate the QRCharacter parent class
    parent_cls = getattr(unreal, 'QRCharacter', None)
    if not parent_cls:
        print("[player-bp] unreal.QRCharacter not found — compile the C++ first.")
        return

    # 4. Delete-and-create so re-runs always pick up a fresh state.
    if unreal.EditorAssetLibrary.does_asset_exist(BP_PATH):
        unreal.EditorAssetLibrary.delete_asset(BP_PATH)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent_cls)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    bp = tools.create_asset(BP_NAME, BP_DIR, None, factory)
    if not bp:
        print("[player-bp] BlueprintFactory.create_asset failed")
        return
    print("[player-bp] created {}".format(BP_PATH))

    # 5. Wire the Mesh component defaults.
    if _set_mesh_defaults_on_bp(BP_PATH, sk_mesh, ab_class):
        print("[player-bp] mesh + anim defaults wired on {}".format(BP_PATH))
    else:
        print("[player-bp] component wire-up failed — open the BP in editor "
              "to set Mesh -> SkeletalMesh manually.")

    print("[player-bp] AQRGameMode is now configured to prefer this BP "
          "over the bare C++ class. Reopen L_DevTest if it's open.")


if __name__ == "__main__":
    run()
