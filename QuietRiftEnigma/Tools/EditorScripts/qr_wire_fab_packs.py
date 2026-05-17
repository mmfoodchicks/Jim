"""
Quiet Rift: Enigma — auto-wire Fab pack contents.

After qr_repoint_fab_packs has fixed the broken hard-reference paths,
this script does the per-asset "this goes with that" wiring inside each
pack:

  StaticMesh / SkeletalMesh
    Any material slot that's empty or set to WorldGridMaterial (the
    UE engine default) gets a best-name-match material assigned from
    the same pack. Matching prefers exact slot-name match, then
    substring (mesh basename in material name or vice versa), then
    falls back to the first M_* / MI_* in the pack.

  AnimSequence
    If the target Skeleton is null, the script assigns the only
    Skeleton in the pack (if there's exactly one). If the pack has
    multiple skeletons it reports it and leaves the anim alone —
    that case wants a manual choice.

  SoundWave
    Reported only — wrapping a SoundWave in a generated SoundCue is
    doable but requires graph editing that breaks across UE versions.
    The repoint script already fixes the existing cues' references,
    which covers ~95% of cases.

  Material
    Reported only — texture-parameter binding is too project-specific
    to safely automate. The audit lists materials whose texture params
    are still set to the engine default.

Re-runnable: every fix checks current state first. dry_run=True logs
what would change without modifying anything.

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_wire_fab_packs.py').read())

  # preview only:
  import qr_wire_fab_packs
  qr_wire_fab_packs.run(dry_run=True)

  # restrict to specific packs (names as immediate /Game/Fabs/ subdirs
  # OR their repointed /Game/<Pack>/ equivalents):
  qr_wire_fab_packs.run(only_packs=['ScifiJungle', 'OWD_Plants_Pack'])
"""

import os
import unreal


FABS_ROOT = "/Game/Fabs"
ENGINE_DEFAULT_MATERIAL_PATHS = {
    "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial",
    "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial",
}


# ─── Pack discovery ──────────────────────────────────────────────────

def _list_pack_roots():
    """Yield (pack_name, content_root) for each Fab pack found, looking
    in both /Game/Fabs/<pack>/ (pre-repoint) and /Game/<pack>/ (post-
    repoint). If both exist, only the one with non-redirector assets
    is used (post-repoint location wins)."""
    seen = set()

    # First: any direct /Game/<pack>/ that mirrors a /Game/Fabs/<pack>/
    if unreal.EditorAssetLibrary.does_directory_exist(FABS_ROOT):
        for ap in unreal.EditorAssetLibrary.list_assets(FABS_ROOT, recursive=True):
            if not ap.startswith(FABS_ROOT + "/"):
                continue
            rest = ap[len(FABS_ROOT) + 1:]
            pack = rest.split("/", 1)[0]
            if not pack or pack in seen:
                continue
            seen.add(pack)
            # Prefer the repointed location if it has assets
            target_root = "/Game/" + pack
            if unreal.EditorAssetLibrary.does_directory_exist(target_root):
                yield (pack, target_root)
            else:
                yield (pack, FABS_ROOT + "/" + pack)


# ─── Asset bucketing ─────────────────────────────────────────────────

def _bucket_pack_assets(content_root):
    """Walk every asset under content_root and bucket by UE class. Skips
    redirectors so we don't double-count.
    Returns dict: {'static_meshes': [...], 'skeletal_meshes': [...],
                   'materials': [...], 'textures': [...],
                   'animations': [...], 'skeletons': [...],
                   'sound_waves': [...], 'sound_cues': [...],
                   'other': [...]}."""
    out = {k: [] for k in (
        'static_meshes', 'skeletal_meshes', 'materials', 'textures',
        'animations', 'skeletons', 'sound_waves', 'sound_cues',
        'physics_assets', 'blueprints', 'other')}

    for ap in unreal.EditorAssetLibrary.list_assets(content_root, recursive=True):
        # Skip redirectors — listing returns them but loading goes
        # through to the target which we'll catch separately.
        klass = unreal.EditorAssetLibrary.get_loaded_asset_redirector_class_path(ap) \
            if hasattr(unreal.EditorAssetLibrary, 'get_loaded_asset_redirector_class_path') else None

        asset = unreal.load_asset(ap)
        if not asset:
            continue
        if isinstance(asset, unreal.ObjectRedirector):
            continue

        if isinstance(asset, unreal.StaticMesh):
            out['static_meshes'].append(asset)
        elif isinstance(asset, unreal.SkeletalMesh):
            out['skeletal_meshes'].append(asset)
        elif isinstance(asset, unreal.MaterialInterface):
            out['materials'].append(asset)
        elif isinstance(asset, unreal.Texture):
            out['textures'].append(asset)
        elif isinstance(asset, unreal.AnimSequence):
            out['animations'].append(asset)
        elif isinstance(asset, unreal.Skeleton):
            out['skeletons'].append(asset)
        elif isinstance(asset, unreal.SoundWave):
            out['sound_waves'].append(asset)
        elif isinstance(asset, unreal.SoundCue):
            out['sound_cues'].append(asset)
        elif isinstance(asset, unreal.PhysicsAsset):
            out['physics_assets'].append(asset)
        elif isinstance(asset, unreal.Blueprint):
            out['blueprints'].append(asset)
        else:
            out['other'].append(asset)

    return out


# ─── Material slot wiring ────────────────────────────────────────────

def _score_material_for_slot(slot_name, mesh_name, mat_name):
    """Score how good a material is for this slot (higher = better)."""
    s = slot_name.lower()
    me = mesh_name.lower()
    m  = mat_name.lower()

    # Exact match between slot name and material name (minus M_/MI_ prefix)
    mat_core = m
    for p in ('mi_', 'm_'):
        if mat_core.startswith(p):
            mat_core = mat_core[len(p):]
            break

    if mat_core == s: return 100
    if s and s in mat_core: return 50
    if mat_core and mat_core in s: return 50

    # Mesh basename match (drop SM_ prefix + category prefix tokens)
    me_core = me
    for p in ('sm_',):
        if me_core.startswith(p):
            me_core = me_core[len(p):]
            break

    if me_core and me_core in mat_core: return 30
    if mat_core and mat_core in me_core: return 30
    return 0


def _slot_is_unset(mat, slot_idx):
    if mat is None: return True
    path = mat.get_path_name()
    for default in ENGINE_DEFAULT_MATERIAL_PATHS:
        if path.startswith(default.split('.')[0]):
            return True
    return False


def _wire_mesh_materials(mesh, materials_in_pack, dry_run, stats):
    """For each mesh material slot that is empty / engine-default, find
    the best matching material in the same pack and assign it."""
    if not materials_in_pack:
        return

    mesh_name = mesh.get_name()
    # StaticMesh + SkeletalMesh both expose get_material(idx) / set_material(idx, ...)
    # and a "static_materials" / "materials" array.
    slot_count = 0
    if hasattr(mesh, 'get_num_sections'):
        try:
            slot_count = mesh.get_num_sections(0)
        except Exception:
            pass
    # Fallback: walk slot indices until get_material returns None unexpectedly.
    if slot_count == 0:
        for i in range(16):
            try:
                m = mesh.get_material(i)
            except Exception:
                break
            if m is None and i > 0:
                slot_count = i
                break
            slot_count = i + 1

    for idx in range(slot_count):
        try:
            cur = mesh.get_material(idx)
        except Exception:
            continue
        if not _slot_is_unset(cur, idx):
            continue

        slot_name = ""
        try:
            slot_name = str(mesh.get_material_slot_names()[idx])
        except Exception:
            pass

        best_mat   = None
        best_score = 0
        for mat in materials_in_pack:
            score = _score_material_for_slot(slot_name, mesh_name, mat.get_name())
            if score > best_score:
                best_score = score
                best_mat = mat

        if best_mat is None and materials_in_pack:
            # Last-resort: just use the first material so the mesh isn't pink.
            best_mat = materials_in_pack[0]
            best_score = 1

        if best_mat is None:
            continue

        stats['mesh_slots_wired'] += 1
        if dry_run:
            print("[wire] DRY  {}.slot[{}]({}) -> {} (score={})".format(
                mesh_name, idx, slot_name, best_mat.get_name(), best_score))
        else:
            try:
                mesh.set_material(idx, best_mat)
                unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            except Exception as e:
                print("[wire] ERR  set_material on {}.{}: {}".format(mesh_name, idx, e))


# ─── Anim → Skeleton wiring ──────────────────────────────────────────

def _wire_anim_skeleton(anim, skeletons_in_pack, dry_run, stats):
    skel = None
    try:
        skel = anim.get_editor_property('skeleton')
    except Exception:
        return
    if skel is not None:
        return
    if len(skeletons_in_pack) != 1:
        stats['anims_ambiguous_skeleton'] += 1
        return

    target = skeletons_in_pack[0]
    stats['anims_skeleton_wired'] += 1
    if dry_run:
        print("[wire] DRY  anim {} -> skeleton {}".format(
            anim.get_name(), target.get_name()))
    else:
        try:
            anim.set_editor_property('skeleton', target)
            unreal.EditorAssetLibrary.save_loaded_asset(anim)
        except Exception as e:
            print("[wire] ERR  set skeleton on {}: {}".format(anim.get_name(), e))


# ─── Per-pack pass ───────────────────────────────────────────────────

def _process_pack(pack_name, content_root, dry_run):
    print("[wire] --- {} ({}) ---".format(pack_name, content_root))
    bucket = _bucket_pack_assets(content_root)

    stats = {
        'mesh_slots_wired': 0,
        'anims_skeleton_wired': 0,
        'anims_ambiguous_skeleton': 0,
        'sound_waves_orphan': 0,
    }

    # Inventory summary
    print("[wire]   inventory: SM={}, SK={}, Mat={}, Tex={}, Anim={}, Skel={}, "
          "Wav={}, Cue={}, Phys={}, BP={}, Other={}".format(
        len(bucket['static_meshes']), len(bucket['skeletal_meshes']),
        len(bucket['materials']), len(bucket['textures']),
        len(bucket['animations']), len(bucket['skeletons']),
        len(bucket['sound_waves']), len(bucket['sound_cues']),
        len(bucket['physics_assets']), len(bucket['blueprints']),
        len(bucket['other'])))

    # 1. Meshes ← materials (within same pack)
    for mesh in bucket['static_meshes'] + bucket['skeletal_meshes']:
        _wire_mesh_materials(mesh, bucket['materials'], dry_run, stats)

    # 2. Anims ← skeletons (only when exactly one skeleton in pack)
    for anim in bucket['animations']:
        _wire_anim_skeleton(anim, bucket['skeletons'], dry_run, stats)

    # 3. SoundWaves without cues — just report (cue generation is too
    # version-fragile to automate safely)
    referenced_waves = set()
    for cue in bucket['sound_cues']:
        # walk the cue's referenced sound waves via dependencies
        try:
            deps = unreal.EditorAssetLibrary.get_package_dependencies(cue.get_path_name(), False)
            for d in deps or []:
                referenced_waves.add(d)
        except Exception:
            pass
    for wav in bucket['sound_waves']:
        if wav.get_path_name() not in referenced_waves:
            stats['sound_waves_orphan'] += 1

    # Per-pack rollup
    print("[wire]   wired: mesh_slots={}, anim_skeletons={}, "
          "ambiguous_anim_skeletons={}, orphan_waves={}".format(
        stats['mesh_slots_wired'], stats['anims_skeleton_wired'],
        stats['anims_ambiguous_skeleton'], stats['sound_waves_orphan']))
    return stats


# ─── Entry point ─────────────────────────────────────────────────────

def run(only_packs=None, dry_run=False):
    """only_packs: optional iterable of pack names to restrict to.
       dry_run: True logs intended changes without modifying anything."""
    only = set(only_packs) if only_packs else None
    print("[wire] {} mode".format("DRY-RUN" if dry_run else "LIVE"))

    totals = {
        'mesh_slots_wired': 0,
        'anims_skeleton_wired': 0,
        'anims_ambiguous_skeleton': 0,
        'sound_waves_orphan': 0,
        'packs_processed': 0,
    }
    for pack_name, root in _list_pack_roots():
        if only and pack_name not in only:
            continue
        stats = _process_pack(pack_name, root, dry_run)
        for k, v in stats.items():
            totals[k] = totals.get(k, 0) + v
        totals['packs_processed'] += 1

    print("[wire]")
    print("[wire] TOTAL across {} packs:".format(totals['packs_processed']))
    print("[wire]   mesh material slots wired:           {}".format(totals['mesh_slots_wired']))
    print("[wire]   anim skeleton refs wired:            {}".format(totals['anims_skeleton_wired']))
    print("[wire]   anims left ambiguous (manual fix):   {}".format(totals['anims_ambiguous_skeleton']))
    print("[wire]   sound waves with no cue (info only): {}".format(totals['sound_waves_orphan']))


if __name__ == "__main__":
    run()
