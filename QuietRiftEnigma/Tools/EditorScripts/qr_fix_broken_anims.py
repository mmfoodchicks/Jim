"""
qr_fix_broken_anims.py -- purge corrupt AnimSequence assets whose
target USKeleton is missing (<None>).

The MPMECH Fab pack shipped MM_*_ANIM sequences that reference a
skeleton (/Game/Fabs/.../UE4_Mannequin_Skeleton) that was never synced
into the project. UE loads them with skeleton == None, and anything
that references one -- e.g. a Sequence Player node placed in
ABP_QRPlayer -- throws a hard 'references Anim Sequence Base that uses
a missing skeleton' compile error on every editor start.

These assets are dead weight (the game animates via single-node
PlayAnimation, not this AnimBP). Deleting them removes the error at
the source: the referencing nodes go null, which is a harmless empty
Sequence Player instead of a broken one.

Run from the UE Python console:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_fix_broken_anims.py').read())
  run()              # report only
  run(delete=True)   # delete the broken anims

Idempotent. Safe: only touches AnimSequences whose skeleton is None.
"""

import unreal

# Packs known to ship skeleton-orphaned anims. Assets outside these
# roots are reported but never auto-deleted (safety), unless
# aggressive=True.
BROKEN_ROOTS = ("/Game/MPMECH/",)


def _all_anim_sequences():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/Game"], True)
    f = unreal.ARFilter(class_names=["AnimSequence"], recursive_paths=True)
    return registry.get_assets(f)


def run(delete=False, aggressive=False):
    print("\n=== qr_fix_broken_anims ===")
    broken = []
    for ad in _all_anim_sequences():
        obj_path = "{}.{}".format(ad.package_name, ad.asset_name)
        anim = unreal.load_asset(obj_path)
        if not anim:
            continue
        try:
            skel = anim.get_editor_property("skeleton")
        except Exception:
            skel = None
        if skel is None:
            broken.append(str(ad.package_name))

    if not broken:
        print("[fix-anims] no skeleton-orphaned anims found -- clean.")
        return

    print("[fix-anims] {} broken anim(s):".format(len(broken)))
    for p in broken:
        in_known = any(p.startswith(r) for r in BROKEN_ROOTS)
        print("   {}{}".format(p, "" if in_known else "  (outside known packs)"))

    if not delete:
        print("[fix-anims] report only. Re-run run(delete=True) to purge.")
        return

    purged = 0
    for p in broken:
        in_known = any(p.startswith(r) for r in BROKEN_ROOTS)
        if not in_known and not aggressive:
            print("[fix-anims]   SKIP {} (use aggressive=True to force)".format(p))
            continue
        try:
            if unreal.EditorAssetLibrary.delete_asset(p):
                purged += 1
        except Exception as e:
            print("[fix-anims]   delete {} failed: {}".format(p, e))
    print("[fix-anims] purged {} broken anim(s). Recompile ABP_QRPlayer "
          "(open it, it should compile clean now).".format(purged))


if __name__ == "__main__":
    run()
