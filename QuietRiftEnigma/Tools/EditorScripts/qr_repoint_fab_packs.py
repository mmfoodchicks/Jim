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
ambient audio / footstep wavs should actually play.

Modes:
  run()                                # live repoint + post-pass verify
  run(dry_run=True)                    # report only, no disk changes
  run(packs=['ScifiJungle'])           # narrow to one pack
  verify()                             # standalone scan: how many broken deps remain

Safe to re-run: target paths that hold a stale redirector get cleared
and replaced; target paths that hold a real asset get skipped (reported
as "blocked") so the user can decide manually.

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_repoint_fab_packs.py').read())
"""

import unreal


FABS_ROOT = "/Game/Fabs"


# ─── Asset-registry helpers ──────────────────────────────────────────

def _list_fab_pack_dirs():
    """Return list of immediate subdirectories under /Game/Fabs (i.e. one
    entry per imported pack)."""
    if not unreal.EditorAssetLibrary.does_directory_exist(FABS_ROOT):
        return []
    out = []
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


def _is_redirector(package_path):
    """True if the asset at `package_path` is an ObjectRedirector. We use
    the asset registry's class metadata rather than load_asset() because
    redirectors silently follow on load and would give the wrong answer."""
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    asset_name = package_path.rsplit("/", 1)[-1]
    obj_path = "{}.{}".format(package_path, asset_name)
    try:
        ad = ar.get_asset_by_object_path(obj_path)
    except Exception:
        return False
    if not ad or not ad.is_valid():
        return False
    # UE 5.7 exposes asset_class_path (TopLevelAssetPath); older versions
    # have asset_class. Try both rather than tying to one version.
    try:
        if str(ad.asset_class_path.asset_name) == "ObjectRedirector":
            return True
    except Exception:
        pass
    try:
        return str(ad.asset_class) == "ObjectRedirector"
    except Exception:
        return False


def _safe_get_dependencies(asset_registry, pkg_name):
    """Wrapper around asset_registry.get_dependencies that handles the
    multiple signatures UE 5.x Python has shipped. Returns [] on failure
    rather than raising. Matches the pattern in qr_purge_broken_cues.py."""
    name = unreal.Name(pkg_name)
    try:
        cat = getattr(unreal, 'DependencyCategory', None)
        if cat is not None:
            return list(asset_registry.get_dependencies(name, cat.PACKAGE) or [])
    except Exception:
        pass
    try:
        opts_cls = getattr(unreal, 'AssetRegistryDependencyOptions', None)
        if opts_cls is not None:
            return list(asset_registry.get_dependencies(name, opts_cls()) or [])
    except Exception:
        pass
    try:
        return list(asset_registry.get_dependencies(name) or [])
    except Exception:
        return []


def _force_gc():
    """Force a garbage-collection pass to release loaded assets.

    A long blocking Python script never yields back to the editor tick,
    so the editor's own periodic GC never runs. Memory from every asset
    rename_asset() loads then accumulates across thousands of assets
    until the GPU runs dry — the "Out of video memory" fatal error.
    Calling this every N assets keeps the working set bounded.

    UE 5.7 API drift: collect_garbage lives on SystemLibrary. Wrapped
    so a missing symbol degrades to a no-op instead of aborting."""
    try:
        unreal.SystemLibrary.collect_garbage()
    except Exception as e:
        unreal.log_warning("[repoint]   GC call failed (non-fatal): {}".format(e))


# ─── Repoint ─────────────────────────────────────────────────────────

def repoint_pack(pack_name, dry_run=False, gc_every=80):
    """Move every asset under /Game/Fabs/<pack>/ to /Game/<pack>/. Returns
    a stats dict, or None if the pack directory doesn't exist / is empty.

    Every per-asset operation is wrapped in try/except: a broken asset
    (e.g. an AnimSequence whose skeleton was deleted) makes rename_asset
    raise a RuntimeError rather than return False. That must NOT abort
    the whole 35-pack run — the asset is counted as a failure and the
    pass continues.

    gc_every: force a garbage-collection pass every N assets to keep
    memory bounded (0 disables). Without it a big pack exhausts VRAM."""
    fab_root    = "{}/{}".format(FABS_ROOT, pack_name)
    target_root = "/Game/{}".format(pack_name)

    if not unreal.EditorAssetLibrary.does_directory_exist(fab_root):
        return None

    asset_paths = unreal.EditorAssetLibrary.list_assets(fab_root, recursive=True)
    if not asset_paths:
        return None

    stats = {
        "renamed":              0,
        "already_done":         0,
        "would_rename":         0,
        "redirector_replaced":  0,
        "blocked_real":         0,
        "failed":               0,
    }

    processed = 0
    total = len(asset_paths)
    for path in asset_paths:
        try:
            src = _strip_object_suffix(path)
            if not src.startswith(fab_root + "/"):
                continue
            # /Game/Fabs/Pack/Sub/Asset  ->  /Game/Pack/Sub/Asset
            dst = target_root + src[len(fab_root):]

            # If the SOURCE is already a redirector, a previous run
            # migrated this asset. Nothing to do — the redirector itself
            # gets collapsed by "Fix Up Redirectors In Folder" later.
            if _is_redirector(src):
                stats["already_done"] += 1
                continue

            if unreal.EditorAssetLibrary.does_asset_exist(dst):
                if _is_redirector(dst):
                    # Stale redirector from a previous partial run. Clear
                    # it so the rename can proceed instead of getting
                    # stuck on the same blocker every re-run.
                    if dry_run:
                        stats["redirector_replaced"] += 1
                        continue
                    if not unreal.EditorAssetLibrary.delete_asset(dst):
                        stats["failed"] += 1
                        unreal.log_warning(
                            "[repoint]   couldn't clear redirector at {}".format(dst))
                        continue
                    stats["redirector_replaced"] += 1
                    # fall through to the rename below
                else:
                    # Real asset already occupies the target. Skip and
                    # report so the user can resolve manually.
                    stats["blocked_real"] += 1
                    continue

            if dry_run:
                stats["would_rename"] += 1
                continue

            if unreal.EditorAssetLibrary.rename_asset(src, dst):
                stats["renamed"] += 1
            else:
                stats["failed"] += 1
                unreal.log_warning(
                    "[repoint]   rename failed: {} -> {}".format(src, dst))

        except Exception as e:
            # Broken asset (missing skeleton, corrupt data model, etc.).
            # Count it and move on — never abort the whole pass. These
            # cluster in the packs FAB_PRUNE_LIST.md marks for deletion.
            stats["failed"] += 1
            unreal.log_warning(
                "[repoint]   skipped (exception) {}: {}".format(path, e))

        finally:
            # Periodic GC + progress line: a long pack neither exhausts
            # memory ("Out of video memory" crash) nor looks frozen.
            processed += 1
            if gc_every > 0 and processed % gc_every == 0:
                _force_gc()
                print("[repoint]   {} ... {}/{} processed".format(
                    pack_name, processed, total))

    return stats


# ─── Verify (post-pass dep scan) ────────────────────────────────────

def verify():
    """Walk every asset under /Game/ and tally per-pack broken-dependency
    counts. Pure read-only — safe any time. Useful before/after a repoint
    pass to see the delta. Takes a minute or two on a fat project."""
    print("[verify] scanning /Game/ for assets with missing dependencies...")
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    f = unreal.ARFilter(package_paths=["/Game"], recursive_paths=True)

    per_pack = {}
    total_broken = 0
    total_assets = 0

    for ad in ar.get_assets(f):
        total_assets += 1
        pkg = str(ad.package_name)
        # First path segment after /Game/ (or /Game/Fabs/) is the pack.
        rest = pkg[len("/Game/"):] if pkg.startswith("/Game/") else pkg
        if rest.startswith("Fabs/"):
            rest = rest[len("Fabs/"):]
        pack = rest.split("/", 1)[0] if "/" in rest else rest

        deps = _safe_get_dependencies(ar, pkg)
        broken = 0
        for dep in deps:
            dep_str = str(dep)
            if not dep_str.startswith("/Game/"):
                continue
            if not unreal.EditorAssetLibrary.does_asset_exist(dep_str):
                broken += 1
        if broken > 0:
            per_pack[pack] = per_pack.get(pack, 0) + broken
            total_broken += broken

    print("[verify] scanned {} assets, found {} broken refs across {} packs"
          .format(total_assets, total_broken, len(per_pack)))

    if per_pack:
        for pack in sorted(per_pack.keys(), key=lambda k: -per_pack[k]):
            print("[verify]   {:<32s} {}".format(pack, per_pack[pack]))

    return total_broken


# ─── Entry point ─────────────────────────────────────────────────────

def run(packs=None, dry_run=False, verify_before=False, verify_after=True,
        gc_every=80):
    """Repoint every pack under /Game/Fabs/ (or the supplied subset).

    Args:
      packs:         Optional list of pack names to limit the pass to.
      dry_run:       If True, report what would happen without renaming.
      verify_before: If True, scan /Game/ for broken deps before repointing.
      verify_after:  If True (default), scan again after — shows the delta.
      gc_every:      Force a GC pass every N assets (0 disables). Default
                     80 keeps VRAM bounded on big packs. Lower it (e.g.
                     30) if the editor still runs out of video memory.
    """
    if packs is None:
        packs = _list_fab_pack_dirs()
    if not packs:
        print("[repoint] no Fab packs found under {} — nothing to do".format(FABS_ROOT))
        return

    mode = "DRY-RUN" if dry_run else "LIVE"
    print("[repoint] {} mode — {} packs".format(mode, len(packs)))
    for p in packs:
        print("[repoint]   - {}".format(p))
    print("")

    if verify_before:
        print("[repoint] === pre-pass verify ===")
        verify()
        print("")

    grand = {"renamed": 0, "already_done": 0, "would_rename": 0,
             "redirector_replaced": 0, "blocked_real": 0, "failed": 0}

    fmt = ("{:<28s}  renamed={:<4d} done={:<4d} would={:<4d} "
           "redir={:<3d} blocked={:<3d} failed={:<4d}")
    for pack in packs:
        s = repoint_pack(pack, dry_run=dry_run, gc_every=gc_every)
        if s is None:
            print("[repoint] {:<28s}  empty / not found".format(pack))
            continue
        print("[repoint] " + fmt.format(
            pack, s["renamed"], s["already_done"], s["would_rename"],
            s["redirector_replaced"], s["blocked_real"], s["failed"]))
        for k in grand:
            grand[k] += s.get(k, 0)
        # Release this pack's working set before starting the next one.
        if not dry_run:
            _force_gc()

    print("")
    print("[repoint] " + fmt.format(
        "TOTAL", grand["renamed"], grand["already_done"], grand["would_rename"],
        grand["redirector_replaced"], grand["blocked_real"], grand["failed"]))

    if grand["blocked_real"] > 0:
        print("[repoint]")
        print("[repoint] {} assets were BLOCKED because the target path "
              "already holds a real (non-redirector) asset.".format(
                  grand["blocked_real"]))
        print("[repoint] Likely cause: a previous import populated /Game/<Pack>/")
        print("[repoint] directly. Decide per-asset whether to keep the existing")
        print("[repoint] one or delete it and re-run this script.")

    if grand["failed"] > 0:
        print("[repoint]")
        print("[repoint] {} assets failed to move — usually broken assets "
              "(missing skeleton, corrupt data model).".format(grand["failed"]))
        print("[repoint] If they cluster in packs from FAB_PRUNE_LIST.md's "
              "DELETE list, just delete those packs — the failures are expected.")

    if not dry_run and grand["renamed"] > 0:
        print("[repoint]")
        print("[repoint] tip: right-click /Game/Fabs in Content Browser -> "
              "Fix Up Redirectors In Folder")
        print("[repoint] to collapse the redirector trail before committing.")

    if verify_after:
        print("")
        _force_gc()
        print("[repoint] === post-pass verify ===")
        verify()


if __name__ == "__main__":
    run()
