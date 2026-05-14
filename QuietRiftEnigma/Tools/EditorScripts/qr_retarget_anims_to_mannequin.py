"""
Quiet Rift: Enigma — retarget FuturisticWarrior animations onto the UE5
Manny/Quinn skeleton.

We picked the UE5 Mannequin as the canonical player skeleton because:
  • the eventual character creator will swap meshes on it
  • Manny/Quinn is the de-facto standard, so anim packs and IK Rigs you
    grab off Fab will mostly target it
  • the FuturisticWarrior pack has the locomotion + combat anim set we
    need for v1, and they're close enough to a Mannequin proportion
    that retargeting works without rebuilding the rig

What this script does (in one run):
  1. Locates the UE5 Mannequin skeletal mesh in /Game/. If missing,
     prints clear instructions and exits without touching anything.
  2. Locates the FuturisticWarrior mesh + skeleton at the known Fab path.
  3. Creates two IKRigDefinition assets (one per skeleton) under
     /Game/QuietRift/Retarget/, populating each with standard humanoid
     retarget chains (Root, Pelvis, Spine, Head, L/R Arm, L/R Leg).
     Bone matching is fuzzy — looks for any bone whose lowercase name
     contains the standard token (spine, pelvis, head, ...) so it works
     across mixamo-style, UE-style, and custom naming.
  4. Creates an IKRetargeter asset that pairs the two rigs.
  5. Batch-retargets every AnimSequence under
     /Game/Fabs/FuturisticWarrior/Animation/ into
     /Game/QuietRift/Animations/Retargeted/, naming each
     A_QR_<original_name>.

Re-running is idempotent — already-created rigs / retargeter are loaded,
already-retargeted anims are skipped (delete the output folder if you
want to force a clean re-run).

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_retarget_anims_to_mannequin.py').read())

Or:
  import qr_retarget_anims_to_mannequin
  qr_retarget_anims_to_mannequin.run()
"""

import os
import unreal


# ─── Inputs ───────────────────────────────────────────────────────────

SOURCE_SKELETON_PATH = "/Game/Fabs/FuturisticWarrior/Mesh/SK_FuturisticWarrior_Skeleton.SK_FuturisticWarrior_Skeleton"
SOURCE_MESH_PATH     = "/Game/Fabs/FuturisticWarrior/Mesh/SK_FuturisticWarrior.SK_FuturisticWarrior"
SOURCE_ANIMS_PATH    = "/Game/Fabs/FuturisticWarrior/Animation"

# Where to look for the UE5 Mannequin in the user's project. The Third
# Person template imports under /Game/Characters/Mannequins/Meshes/. We
# search both Manny and Quinn — either is fine, they share a skeleton.
TARGET_MESH_CANDIDATES = [
    "/Game/Characters/Mannequins/Meshes/SKM_Manny",
    "/Game/Characters/Mannequins/Meshes/SKM_Quinn",
    "/Game/Characters/Mannequins/Meshes/SK_Mannequin",
    "/Game/ThirdPerson/Characters/Mannequins/Meshes/SKM_Manny",
    "/Game/ThirdPerson/Characters/Mannequins/Meshes/SKM_Quinn",
]

# Output locations.
OUTPUT_RETARGET_DIR = "/Game/QuietRift/Retarget"
OUTPUT_ANIM_DIR     = "/Game/QuietRift/Animations/Retargeted"

SOURCE_RIG_NAME = "IKR_FuturisticWarrior"
TARGET_RIG_NAME = "IKR_UE5Mannequin_QR"
RETARGETER_NAME = "RTG_FuturisticWarrior_to_Mannequin"


# Standard humanoid retarget chains. Each tuple is:
#   (chain_label, start_bone_tokens, end_bone_tokens)
# Tokens are checked case-insensitively against bone names; the first
# bone containing any token wins. None for end means "same as start".
STANDARD_CHAINS = [
    # label,        start tokens,                    end tokens
    ("Root",        ["root"],                        None),
    ("Pelvis",      ["pelvis", "hips"],              None),
    ("Spine",       ["spine_01", "spine1", "spine"], ["spine_05", "spine_04", "spine_03", "spine_02"]),
    ("Neck",        ["neck_01", "neck"],             None),
    ("Head",        ["head"],                        None),
    ("LeftArm",     ["clavicle_l", "leftshoulder"],  ["hand_l", "lefthand"]),
    ("RightArm",    ["clavicle_r", "rightshoulder"], ["hand_r", "righthand"]),
    ("LeftLeg",     ["thigh_l", "leftupleg"],        ["ball_l", "leftfoot", "foot_l"]),
    ("RightLeg",    ["thigh_r", "rightupleg"],       ["ball_r", "rightfoot", "foot_r"]),
]


# ─── Helpers ──────────────────────────────────────────────────────────

def _find_target_mesh():
    for p in TARGET_MESH_CANDIDATES:
        if unreal.EditorAssetLibrary.does_asset_exist(p):
            return p
    return None


def _ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _get_bone_names(skel_mesh):
    """Returns the bone name list for a USkeletalMesh."""
    if not skel_mesh:
        return []
    skeleton = skel_mesh.skeleton if hasattr(skel_mesh, "skeleton") else None
    if not skeleton:
        return []
    # UE5 exposes get_reference_pose / bone array via the editor only.
    # SkeletalMeshEditorSubsystem path is the most reliable.
    sms = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    if sms:
        return [str(b) for b in sms.get_bones(skel_mesh)] if hasattr(sms, "get_bones") else []
    return []


def _pick_bone(bone_names, token_list):
    """Find the first bone whose lowercased name contains any of the tokens."""
    if not bone_names or not token_list:
        return None
    lowered = [(n, n.lower()) for n in bone_names]
    for token in token_list:
        t = token.lower()
        # Prefer exact match, then suffix, then substring.
        for orig, lc in lowered:
            if lc == t:
                return orig
        for orig, lc in lowered:
            if lc.endswith("_" + t) or lc.endswith(t):
                return orig
        for orig, lc in lowered:
            if t in lc:
                return orig
    return None


def _create_or_load_asset(asset_name, package_path, asset_class, factory):
    full = "{}/{}".format(package_path, asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        return unreal.load_asset(full)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    return tools.create_asset(
        asset_name=asset_name,
        package_path=package_path,
        asset_class=asset_class,
        factory=factory)


def _build_ik_rig(rig_name, skel_mesh):
    """Create/load an IKRigDefinition and configure standard chains."""
    factory = unreal.IKRigDefinitionFactory()
    rig = _create_or_load_asset(rig_name, OUTPUT_RETARGET_DIR,
                                unreal.IKRigDefinition, factory)
    if not rig:
        print("[retarget] FAILED to create rig: " + rig_name)
        return None

    ctrl = unreal.IKRigController.get_controller(rig)
    if not ctrl:
        print("[retarget] no IKRigController for " + rig_name)
        return None

    # Bind the skeletal mesh first; chain bone lookups require it.
    try:
        ctrl.set_skeletal_mesh(skel_mesh)
    except Exception as e:
        print("[retarget] set_skeletal_mesh failed for {}: {}".format(rig_name, e))
        return None

    bone_names = _get_bone_names(skel_mesh)
    print("[retarget] {} : {} bones".format(rig_name, len(bone_names)))

    # Strip any existing chains so re-runs stay clean.
    try:
        existing = ctrl.get_retarget_chains() if hasattr(ctrl, "get_retarget_chains") else []
        for ch in existing:
            ctrl.remove_retarget_chain(ch.chain_name if hasattr(ch, "chain_name") else ch)
    except Exception:
        pass

    for label, start_tokens, end_tokens in STANDARD_CHAINS:
        start = _pick_bone(bone_names, start_tokens)
        if not start:
            print("[retarget] {} : no bone for chain '{}' (start tokens {}) — skipped".format(
                rig_name, label, start_tokens))
            continue
        end = start if not end_tokens else (_pick_bone(bone_names, end_tokens) or start)
        try:
            ctrl.add_retarget_chain(label, start, end, "None")
            print("[retarget] {} : chain {:<10s} {:>20s} -> {}".format(rig_name, label, start, end))
        except Exception as e:
            print("[retarget] {} : add_retarget_chain {} failed: {}".format(rig_name, label, e))

    unreal.EditorAssetLibrary.save_loaded_asset(rig)
    return rig


def _build_retargeter(source_rig, target_rig):
    factory = unreal.IKRetargetFactory()
    retargeter = _create_or_load_asset(RETARGETER_NAME, OUTPUT_RETARGET_DIR,
                                       unreal.IKRetargeter, factory)
    if not retargeter:
        print("[retarget] FAILED to create retargeter")
        return None

    ctrl = unreal.IKRetargeterController.get_controller(retargeter)
    if not ctrl:
        print("[retarget] no IKRetargeterController")
        return None

    # Different UE versions name the side enum differently. Try both.
    try:
        ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
        ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target_rig)
    except Exception:
        try:
            ctrl.set_ik_rig("Source", source_rig)
            ctrl.set_ik_rig("Target", target_rig)
        except Exception as e:
            print("[retarget] set_ik_rig failed: {}".format(e))
            return None

    # Auto-map any chains whose labels match on both sides.
    try:
        ctrl.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    except Exception:
        try:
            ctrl.auto_map_chains()
        except Exception as e:
            print("[retarget] auto_map_chains failed: {}".format(e))

    unreal.EditorAssetLibrary.save_loaded_asset(retargeter)
    return retargeter


def _batch_retarget(retargeter, source_anims):
    _ensure_dir(OUTPUT_ANIM_DIR)

    # IKRetargetBatchOperation is the editor-side workhorse for
    # duplicate-and-retarget. Some versions of UE expose this only via
    # Slate, but UE5.7 has a Python-callable path.
    settings = None
    try:
        settings = unreal.IKRetargetBatchOperationContext()
        settings.set_editor_property("ik_retargeter_asset", retargeter)
        settings.set_editor_property("name_rule",
            unreal.RetargetBatchSettings(
                folder_path=OUTPUT_ANIM_DIR,
                prefix="A_QR_",
                suffix="",
                find_string="",
                replace_string=""))
    except Exception:
        settings = None

    duplicated = 0
    skipped = 0

    for ad in source_anims:
        src_path = str(ad.object_path)
        src_name = str(ad.asset_name)
        out_name = "A_QR_{}".format(src_name)
        out_path = "{}/{}.{}".format(OUTPUT_ANIM_DIR, out_name, out_name)

        if unreal.EditorAssetLibrary.does_asset_exist(out_path):
            skipped += 1
            continue

        # Per-asset path: duplicate + retarget in place. UE5.7 has
        # unreal.IKRetargeterController.duplicate_and_retarget which
        # handles a single anim.
        try:
            ctrl = unreal.IKRetargeterController.get_controller(retargeter)
            new_asset = ctrl.duplicate_and_retarget(
                assets_to_retarget=[unreal.load_asset(src_path)],
                source_mesh=unreal.load_asset(SOURCE_MESH_PATH),
                target_mesh=unreal.load_asset(_find_target_mesh()),
                search="",
                replace="",
                prefix="A_QR_",
                suffix="",
                include_referenced_assets=False)
            if new_asset:
                duplicated += 1
                # Move into our output folder if it didn't land there.
                for n in new_asset:
                    if hasattr(n, "get_path_name"):
                        cur = n.get_path_name().rsplit(".", 1)[0]
                        want = "{}/{}".format(OUTPUT_ANIM_DIR, n.get_name())
                        if cur != want:
                            unreal.EditorAssetLibrary.rename_asset(cur, want)
        except Exception as e:
            print("[retarget] failed on {}: {}".format(src_name, e))

    print("[retarget] batch done — duplicated={} skipped={}".format(duplicated, skipped))


# ─── Entry point ──────────────────────────────────────────────────────

def run():
    target_path = _find_target_mesh()
    if not target_path:
        print("")
        print("=" * 72)
        print("UE5 Mannequin (Manny/Quinn) skeletal mesh not found in /Game/.")
        print("")
        print("To install it:")
        print("  Editor → Add → Add Feature or Content Pack to Project")
        print("    → choose 'Third Person' template")
        print("    → click Add to Project")
        print("")
        print("That imports SKM_Manny + SKM_Quinn under")
        print("  /Game/Characters/Mannequins/Meshes/")
        print("")
        print("Then re-run this script.")
        print("=" * 72)
        return

    print("[retarget] target mesh: " + target_path)

    if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE_MESH_PATH):
        print("[retarget] source mesh missing: " + SOURCE_MESH_PATH)
        return

    _ensure_dir(OUTPUT_RETARGET_DIR)
    _ensure_dir(OUTPUT_ANIM_DIR)

    source_mesh = unreal.load_asset(SOURCE_MESH_PATH)
    target_mesh = unreal.load_asset(target_path)

    source_rig = _build_ik_rig(SOURCE_RIG_NAME, source_mesh)
    target_rig = _build_ik_rig(TARGET_RIG_NAME, target_mesh)
    if not source_rig or not target_rig:
        return

    retargeter = _build_retargeter(source_rig, target_rig)
    if not retargeter:
        return

    # Find every AnimSequence under the FuturisticWarrior animation folder.
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    ar.scan_paths_synchronous([SOURCE_ANIMS_PATH], True)
    f = unreal.ARFilter(
        class_names=["AnimSequence"],
        package_paths=[SOURCE_ANIMS_PATH],
        recursive_paths=True,
    )
    anims = list(ar.get_assets(f))
    print("[retarget] {} source anims to retarget".format(len(anims)))

    _batch_retarget(retargeter, anims)

    print("[retarget] DONE. Retargeted anims → " + OUTPUT_ANIM_DIR)


if __name__ == "__main__":
    run()
