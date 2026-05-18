"""
Quiet Rift: Enigma — delete Fab sound cues whose source wavs are missing.

Several Fab packs (Free_Sounds_Pack, Bodycam_VHS_Effect/Sounds,
NiagaraExamples impacts, FuturisticWarrior, Chaotic_Skies, etc.) shipped
SoundCue assets that reference SoundWave assets at /Game/<Pack>/... paths.
Some of those wavs were never imported, or were pruned out of the project.

Loading any of those cues at PIE start spams hundreds of warnings like:
  LogStreaming: Warning: LoadPackage: SkipPackage:
    /Game/Free_Sounds_Pack/wav/Ambient_Wind_Loop_1 - does not exist on disk

This script walks every SoundCue under /Game/Fabs/ and /Game/<Pack>/
locations, inspects each cue's dependency list, and deletes any cue
whose dependency tree references a missing package. The wav files that
DO exist are left alone — only the cues with broken refs go.

Dry-run mode (default) just reports counts; pass dry_run=False to
actually delete.

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_purge_broken_cues.py').read())

  # preview only:
  import qr_purge_broken_cues
  qr_purge_broken_cues.run(dry_run=True)

  # actually delete:
  qr_purge_broken_cues.run(dry_run=False)
"""

import unreal


def _list_all_cue_paths():
    """Return every SoundCue asset path in /Game/."""
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    f = unreal.ARFilter(
        class_names=['SoundCue'],
        package_paths=['/Game'],
        recursive_paths=True)
    paths = []
    for ad in ar.get_assets(f):
        paths.append("{}.{}".format(ad.package_name, ad.asset_name))
    return paths


def _cue_has_missing_dep(cue_package_path, asset_registry):
    """Return list of missing dependency package names, or [] if all are
    present on disk. cue_package_path is the /Game/.../Asset path."""
    # Strip the .Asset suffix if present
    pkg = cue_package_path.split('.', 1)[0] if '.' in cue_package_path else cue_package_path

    # UE 5.7 Python doesn't expose EditorAssetLibrary.get_package_dependencies;
    # the correct path is the asset registry's get_dependencies.
    deps = asset_registry.get_dependencies(unreal.Name(pkg)) or []
    missing = []
    for dep in deps:
        dep_str = str(dep)
        # Only check /Game/ deps — those are the ones that can vanish.
        if not dep_str.startswith('/Game/'):
            continue
        if not unreal.EditorAssetLibrary.does_asset_exist(dep_str):
            missing.append(dep_str)
    return missing


def run(dry_run=True):
    print("[purge-cues] {} mode".format("DRY-RUN" if dry_run else "LIVE DELETE"))

    all_cues = _list_all_cue_paths()
    print("[purge-cues] scanning {} SoundCue assets...".format(len(all_cues)))

    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    broken = []   # list of (cue_path, [missing_deps...])
    for cp in all_cues:
        missing = _cue_has_missing_dep(cp, asset_registry)
        if missing:
            broken.append((cp, missing))

    print("[purge-cues] found {} cue(s) with at least one missing dependency"
          .format(len(broken)))

    if not broken:
        print("[purge-cues] nothing to do.")
        return

    # Show a sample so the user can sanity-check before live mode.
    for cp, missing in broken[:8]:
        print("[purge-cues]   {}  -> missing {}".format(
            cp.split('.')[0], ", ".join(missing[:3]) + ("…" if len(missing) > 3 else "")))
    if len(broken) > 8:
        print("[purge-cues]   ... and {} more".format(len(broken) - 8))

    if dry_run:
        print("[purge-cues] DRY-RUN — call run(dry_run=False) to actually delete.")
        return

    deleted = 0
    failed  = 0
    for cp, _ in broken:
        pkg = cp.split('.', 1)[0]
        ok = unreal.EditorAssetLibrary.delete_asset(pkg)
        if ok:
            deleted += 1
        else:
            failed += 1
            print("[purge-cues]   delete failed: {}".format(pkg))

    print("[purge-cues] done — deleted {} / {} ({} failed)".format(
        deleted, len(broken), failed))
    if deleted > 0:
        print("[purge-cues] tip: right-click /Game in the content browser ->")
        print("[purge-cues]      Fix Up Redirectors In Folder, then Save All.")


if __name__ == "__main__":
    run()
