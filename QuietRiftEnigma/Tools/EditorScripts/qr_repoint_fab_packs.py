"""
Quiet Rift: Enigma — repoint Fab pack contents back to their original paths.

When Fab packs are imported, the assets land under /Game/Fabs/<Pack>/...
but their internal hard references (cue -> wav, niagara system -> material,
mesh -> material, etc.) still point to /Game/<Pack>/... where the pack was
originally authored. Loading any of those assets in PIE spams hundreds of
"dependent package not available" warnings, and the actual gunshot/wind/
muzzle-flash/etc never plays.

This script walks every asset under /Game/Fabs/<Pack>/ and renames it to
/Game/<Pack>/<same subpath> — moving the file to where dependencies
expect it. UE auto-creates a redirector at the old Fab path so anything
that intentionally references the Fab path (the biome-profile palettes,
the create-test-maps starter hills, etc.) still resolves.

After running, the Output Log should stop showing the
  "LoadErrors: ... dependent package /Game/<Pack>/... was not available"
warnings on PIE start, and the previously-silent gunshot / muzzle flash /
ambient audio should actually play.

Safe to re-run: target paths that already have an asset are skipped, so
nothing gets clobbered.

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_repoint_fab_packs.py').read())
"""

import unreal


FABS_ROOT = "/Game/Fabs"


def _list_fab_pack_dirs():
    """Return list of immediate subdirectories under /Game/Fabs (i.e. one
    entry per imported pack)."""
    if not unreal.EditorAssetLibrary.does_directory_exist(FABS_ROOT):
        return []
    out = []
    # list_assets at root level returns asset paths, not dir paths. Use the
    # asset-registry directory walker via EditorUtilityLibrary instead.
    # Simpler: list every asset recursively under /Game/Fabs and extract
    # the immediate child segment, then dedup.
    asset_paths = unreal.EditorAssetLibrary.list_assets(FABS_ROOT, recursive=True)
    seen = set()
    prefix = FABS_ROOT + "/"
    for ap in asset_paths:
        if not ap.startswith(prefix):
            continue
        rest = ap[len(prefix):]
        pack = rest.split("/", 1)[0]
        if pack and pack not in seen:
            seen.add(pack)
            out.append(pack)
    return sorted(out)


def _strip_object_suffix(asset_path):
    """list_assets returns /Game/Path/Asset.Asset — strip the .Asset for
    rename_asset which wants the package path only."""
    if "." in asset_path:
        return asset_path.split(".", 1)[0]
    return asset_path


def repoint_pack(pack_name):
    fab_root    = "{}/{}".format(FABS_ROOT, pack_name)
    target_root = "/Game/{}".format(pack_name)

    if not unreal.EditorAssetLibrary.does_directory_exist(fab_root):
        return (0, 0, 0)

    asset_paths = unreal.EditorAssetLibrary.list_assets(fab_root, recursive=True)
    if not asset_paths:
        print("[repoint] {} — empty pack, skipping".format(pack_name))
        return (0, 0, 0)

    renamed  = 0
    skipped  = 0   # target already has an asset there
    failed   = 0

    for path in asset_paths:
        src = _strip_object_suffix(path)
        if not src.startswith(fab_root + "/"):
            continue
        # /Game/Fabs/Pack/Sub/Asset  ->  /Game/Pack/Sub/Asset
        dst = target_root + src[len(fab_root):]

        # Skip if already at destination (target path occupied).
        if unreal.EditorAssetLibrary.does_asset_exist(dst):
            skipped += 1
            continue

        ok = unreal.EditorAssetLibrary.rename_asset(src, dst)
        if ok:
            renamed += 1
        else:
            failed += 1
            unreal.log_warning("[repoint]   rename failed: {} -> {}".format(src, dst))

    print("[repoint] {:<32s}  renamed={:<5d}  skipped={:<5d}  failed={:<5d}".format(
        pack_name, renamed, skipped, failed))
    return (renamed, skipped, failed)


def run(packs=None):
    """packs: optional list of pack names to repoint. Default = every pack
    under /Game/Fabs/ that has any assets."""
    if packs is None:
        packs = _list_fab_pack_dirs()
    if not packs:
        print("[repoint] no Fab packs found under {} — nothing to do".format(FABS_ROOT))
        return

    print("[repoint] processing {} packs:".format(len(packs)))
    for p in packs:
        print("[repoint]   - {}".format(p))
    print("[repoint]")

    total_renamed = total_skipped = total_failed = 0
    for pack in packs:
        r, s, f = repoint_pack(pack)
        total_renamed += r
        total_skipped += s
        total_failed  += f

    print("[repoint]")
    print("[repoint] done — renamed={}, skipped (target occupied)={}, failed={}"
          .format(total_renamed, total_skipped, total_failed))
    if total_renamed > 0:
        print("[repoint] tip: right-click /Game/Fabs in content browser -> "
              "Fix Up Redirectors In Folder to collapse the redirector trail")


if __name__ == "__main__":
    run()
