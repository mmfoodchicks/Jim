"""
qr_unify_mannequin_anims.py -- consolidate every Mannequin-targeting
AnimSequence in the Fab library onto ONE canonical UE Mannequin
skeleton so the NPC brain can play any of them on any spawned NPC.

The library ships four copies of SK_Mannequin (DeadBodies, FreeAnims
MixPack, DynamicFalling, RamsterZ) plus ControlRig's. They're all the
same UE4 Mannequin skeleton with identical bone hierarchies -- the
copies exist because each Fab demo pack bundles its own. The duplicate
skeletons keep anims from being playable across packs.

This script:
  1. Picks ControlRig's SK_Mannequin as the canonical skeleton (it's
     the most-shared and ships alongside SKM_Manny / SKM_Quinn meshes
     plus locomotion MM_/MF_ anims).
  2. Walks every AnimSequence asset under each pack's animation folder.
  3. For each anim: duplicates it to /Game/QuietRift/Animations/<Pack>/
     A_<original_name>, then repoints the duplicate's Skeleton property
     to the canonical one. The source asset is left untouched.

After running this, every NPC mesh that uses the canonical skeleton
(SKM_Quinn_Simple / SKM_Manny etc.) can play ANY of the unified anims
via single-node PlayAnimation -- no per-pack mesh juggling.

Re-running is idempotent -- assets that already exist at the unified
path are skipped (pass overwrite=True to force re-clone).

Run from the UE Python console:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_unify_mannequin_anims.py').read())
  run()                    # unify everything
  run(overwrite=True)      # force re-clone
"""

import unreal


# ─── Canonical target ────────────────────────────────────────────────

TARGET_SKELETON = "/Game/ControlRig/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"

# Output root. One subdir per source pack.
OUTPUT_ROOT = "/Game/QuietRift/Animations"


# ─── Source packs ────────────────────────────────────────────────────
#
# (label, content_root). The script walks each content_root recursively
# for AnimSequence assets and clones them. Packs whose root doesn't
# exist are skipped.
PACKS = [
    ("DeadBodies",    "/Game/DeadBodies_Poses_nikoff"),
    ("FreeAnimsMix",  "/Game/FreeAnimsMixPack"),
    ("DynamicFall",   "/Game/DynamicFalling"),
    ("RamsterZ",      "/Game/RamsterZ_FreeAnims_Volume1"),
    # ControlRig has the canonical skeleton already; including it
    # mirrors the MM_/MF_ locomotion anims under the unified root for
    # consistent naming, but the skeleton repoint is a no-op.
    ("ControlRig",    "/Game/ControlRig/Characters/Mannequins/Animations"),
]


# ─── Helpers ─────────────────────────────────────────────────────────

def _ensure_dir(p):
    if not unreal.EditorAssetLibrary.does_directory_exist(p):
        unreal.EditorAssetLibrary.make_directory(p)


def _list_anim_assets(root):
    """Return paths to every AnimSequence asset under root."""
    if not unreal.EditorAssetLibrary.does_directory_exist(root):
        return []
    out = []
    for ap in unreal.EditorAssetLibrary.list_assets(root, recursive=True):
        # list_assets returns the package path; verify the class.
        data = unreal.EditorAssetLibrary.find_asset_data(ap)
        if not data.is_valid():
            continue
        cls = data.asset_class_path.asset_name if hasattr(data, "asset_class_path") \
              else str(data.get_class().get_name())
        if str(cls) == "AnimSequence":
            out.append(ap.split(".")[0])   # strip class suffix
    return out


def _unify_one(src_path, label, target_skel, overwrite):
    """Clone src to /Game/QuietRift/Animations/<label>/A_<name>, then
    set its Skeleton to target_skel. Returns True on success."""
    src_name = src_path.rsplit("/", 1)[-1]
    dst_name = "A_{}".format(src_name) if not src_name.startswith("A_") else src_name
    dst_dir  = "{}/{}".format(OUTPUT_ROOT, label)
    dst_path = "{}/{}".format(dst_dir, dst_name)

    if unreal.EditorAssetLibrary.does_asset_exist(dst_path):
        if not overwrite:
            return False
        unreal.EditorAssetLibrary.delete_asset(dst_path)

    _ensure_dir(dst_dir)

    cloned = unreal.EditorAssetLibrary.duplicate_asset(src_path, dst_path)
    if not cloned:
        print("[unify]   FAIL duplicate {} -> {}".format(src_path, dst_path))
        return False

    # Repoint the skeleton. UE 5.7 exposes this through the editor
    # property "skeleton" on UAnimSequence. The bone hierarchies are
    # identical (all four packs ship the same UE4 SK_Mannequin) so the
    # rebind has no animation-data loss.
    try:
        cloned.set_editor_property("skeleton", target_skel)
    except Exception as e:
        print("[unify]   skeleton repoint failed for {}: {}".format(dst_name, e))

    unreal.EditorAssetLibrary.save_loaded_asset(cloned)
    return True


# ─── Public entry ────────────────────────────────────────────────────

def run(overwrite=False):
    """Unify every Fab pack's Mannequin anims under one skeleton."""
    print("\n=== qr_unify_mannequin_anims ===")

    target = unreal.load_asset(TARGET_SKELETON)
    if not target:
        print("[unify] Canonical skeleton missing at {}".format(TARGET_SKELETON))
        print("[unify] (ControlRig pack not installed?)")
        return

    print("[unify] target skeleton: {}".format(TARGET_SKELETON))

    total_unified = 0
    for label, root in PACKS:
        if not unreal.EditorAssetLibrary.does_directory_exist(root):
            print("[unify]   {} not on disk -- skipped".format(root))
            continue
        anims = _list_anim_assets(root)
        if not anims:
            print("[unify]   {} -- no AnimSequence assets found".format(label))
            continue
        n = 0
        for src in anims:
            if _unify_one(src, label, target, overwrite):
                n += 1
        print("[unify]   {:<14s} {:3d} of {} unified".format(label + ":", n, len(anims)))
        total_unified += n

    print("[unify] DONE -- {} anims now under {}".format(total_unified, OUTPUT_ROOT))
    print("[unify] Re-run qr_assign_npc_appearance to point NPC defaults")
    print("[unify] at the unified set.")


if __name__ == "__main__":
    run()
