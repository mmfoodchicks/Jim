"""
Quiet Rift: Enigma — comprehensive Fab pack audit.

Walks every /Game/Fabs/<Pack>/ (and the repointed /Game/<Pack>/ copies
if they exist), inventories the assets, and writes a per-pack report
to <Project>/Saved/qr_fab_audit.md. Read-only — never modifies, deletes,
or moves anything.

For each pack the report shows:
  - Total asset count by type (StaticMesh, SkeletalMesh, Material,
    Texture, AnimSequence, Skeleton, AnimBlueprint, SoundCue,
    SoundWave, Niagara, Blueprint, Level, other)
  - Skeleton sniff: which Skeleton anims/SK meshes in the pack target,
    so you know if it's UE5 Mannequin-compatible or custom
  - "Use as player character?" verdict (has SkeletalMesh + Skeleton +
    >=N anims)
  - "Use as world prop pack?" verdict (>=N StaticMeshes with materials)
  - Broken dependencies: number of references to packages that don't
    exist on disk (these spam the warnings you've been seeing)
  - Sample asset paths so you can hop straight to the editor

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_audit_fabs.py').read())

Output: <Project>/Saved/qr_fab_audit.md  (also echoed to Output Log)
"""

import os
import unreal
from collections import defaultdict


FABS_ROOT = "/Game/Fabs"


# ─── Pack discovery ──────────────────────────────────────────────────

def _list_packs():
    """Return list of (pack_name, content_root). Looks under /Game/Fabs/
    AND /Game/<pack>/ in case anything has been repointed back to its
    original path."""
    seen = {}
    if unreal.EditorAssetLibrary.does_directory_exist(FABS_ROOT):
        for ap in unreal.EditorAssetLibrary.list_assets(FABS_ROOT, recursive=True):
            if not ap.startswith(FABS_ROOT + "/"):
                continue
            rest = ap[len(FABS_ROOT) + 1:]
            pack = rest.split("/", 1)[0]
            if pack and pack not in seen:
                # Prefer the /Game/<Pack>/ location if it has content
                # (i.e. the repoint script moved the assets back). Else
                # use the /Game/Fabs/<Pack>/ location.
                alt = "/Game/" + pack
                if unreal.EditorAssetLibrary.does_directory_exist(alt):
                    if unreal.EditorAssetLibrary.list_assets(alt, recursive=True):
                        seen[pack] = alt
                        continue
                seen[pack] = "{}/{}".format(FABS_ROOT, pack)
    return sorted(seen.items())


# ─── Asset bucketing ─────────────────────────────────────────────────

def _classify(asset):
    """Return short type string for an asset, or None if uninteresting."""
    # Order matters — SkeletalMesh inherits from things, check specific
    # subclasses first.
    if isinstance(asset, unreal.SkeletalMesh):   return 'SkeletalMesh'
    if isinstance(asset, unreal.StaticMesh):     return 'StaticMesh'
    if isinstance(asset, unreal.AnimBlueprint):  return 'AnimBlueprint'
    if isinstance(asset, unreal.AnimSequence):   return 'AnimSequence'
    if isinstance(asset, unreal.Skeleton):       return 'Skeleton'
    if isinstance(asset, unreal.MaterialInterface): return 'Material'
    if isinstance(asset, unreal.Texture):        return 'Texture'
    if isinstance(asset, unreal.SoundCue):       return 'SoundCue'
    if isinstance(asset, unreal.SoundWave):      return 'SoundWave'
    if isinstance(asset, unreal.World):          return 'Level'
    if isinstance(asset, unreal.Blueprint):      return 'Blueprint'
    # Niagara — class name string-check because the type isn't always exposed
    cls = asset.get_class().get_name() if asset.get_class() else ''
    if 'NiagaraSystem' in cls: return 'Niagara'
    if 'NiagaraEmitter' in cls: return 'NiagaraEmitter'
    if 'IKRig' in cls: return 'IKRig'
    if 'IKRetargeter' in cls: return 'IKRetargeter'
    if 'PhysicsAsset' in cls: return 'PhysicsAsset'
    return 'other'


def _bucket_pack(content_root):
    """Walk every asset under content_root and bucket by type.
    Returns: (bucket dict, sample_paths dict, skeletons_seen set)."""
    bucket  = defaultdict(int)
    samples = defaultdict(list)
    skeletons = set()

    for ap in unreal.EditorAssetLibrary.list_assets(content_root, recursive=True):
        asset = unreal.load_asset(ap)
        if not asset: continue
        kind = _classify(asset)
        if not kind: continue
        bucket[kind] += 1
        if len(samples[kind]) < 3:
            samples[kind].append(ap)
        # Pull the target skeleton for SkeletalMesh / AnimSequence.
        if kind in ('SkeletalMesh', 'AnimSequence'):
            try:
                skel = asset.get_editor_property('skeleton')
                if skel:
                    skeletons.add(skel.get_path_name())
            except Exception:
                pass
    return (dict(bucket), dict(samples), skeletons)


# ─── Broken-dependency scan ──────────────────────────────────────────

def _safe_deps(ar, pkg_name):
    """Tolerant get_dependencies — handles UE 5.4 vs 5.7 signature drift."""
    name = unreal.Name(pkg_name)
    for attempt in [
        lambda: ar.get_dependencies(name, unreal.DependencyCategory.PACKAGE) \
            if hasattr(unreal, 'DependencyCategory') else None,
        lambda: ar.get_dependencies(name, unreal.AssetRegistryDependencyOptions()) \
            if hasattr(unreal, 'AssetRegistryDependencyOptions') else None,
        lambda: ar.get_dependencies(name),
    ]:
        try:
            r = attempt()
            if r is not None:
                return [str(d) for d in r]
        except Exception:
            continue
    return []


def _count_broken_deps(content_root):
    """Returns (broken_asset_count, missing_dep_examples)."""
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    broken_count = 0
    examples = []
    for ap in unreal.EditorAssetLibrary.list_assets(content_root, recursive=True):
        pkg = ap.split('.', 1)[0] if '.' in ap else ap
        deps = _safe_deps(ar, pkg)
        any_missing = False
        for d in deps:
            if not d.startswith('/Game/'): continue
            if not unreal.EditorAssetLibrary.does_asset_exist(d):
                any_missing = True
                if len(examples) < 5:
                    examples.append("{} -> missing {}".format(pkg.split('/')[-1], d))
                break
        if any_missing:
            broken_count += 1
    return (broken_count, examples)


# ─── Verdict heuristics ──────────────────────────────────────────────

def _player_character_verdict(bucket, skeletons):
    """Can this pack supply a playable character?"""
    sk    = bucket.get('SkeletalMesh', 0)
    anims = bucket.get('AnimSequence', 0)
    skel  = bucket.get('Skeleton', 0)
    if sk >= 1 and anims >= 5 and skel >= 1:
        return "**YES** — {} SK mesh / {} anims / {} skeleton(s)".format(sk, anims, skel)
    if sk >= 1 and skel >= 1:
        return "maybe — has SK + skeleton but only {} anims (need 5+)".format(anims)
    if sk >= 1:
        return "no — SK mesh present but no skeleton asset in pack"
    return "no — no SkeletalMesh"


def _prop_pack_verdict(bucket):
    """Can this pack supply static-mesh world props?"""
    sm  = bucket.get('StaticMesh', 0)
    mat = bucket.get('Material', 0)
    tex = bucket.get('Texture', 0)
    if sm >= 10 and mat >= 1:
        return "**YES** — {} static meshes / {} materials / {} textures".format(sm, mat, tex)
    if sm >= 1:
        return "minor — only {} meshes".format(sm)
    return "no — no StaticMesh"


def _sound_pack_verdict(bucket):
    cue = bucket.get('SoundCue', 0)
    wav = bucket.get('SoundWave', 0)
    if cue >= 5 or wav >= 5:
        return "**YES** — {} cues / {} waves".format(cue, wav)
    if cue + wav >= 1:
        return "minor — {} cues / {} waves".format(cue, wav)
    return "no"


def _vfx_pack_verdict(bucket):
    n = bucket.get('Niagara', 0)
    e = bucket.get('NiagaraEmitter', 0)
    if n >= 1: return "YES — {} Niagara systems".format(n)
    if e >= 1: return "partial — {} emitters but no systems".format(e)
    return "no"


# ─── Report writing ──────────────────────────────────────────────────

def _save_path():
    proj = unreal.Paths.project_dir()
    saved_dir = os.path.join(proj, 'Saved')
    if not os.path.isdir(saved_dir):
        os.makedirs(saved_dir, exist_ok=True)
    return os.path.join(saved_dir, 'qr_fab_audit.md')


def _write_pack_section(out, pack, root, bucket, samples, skeletons, broken_count, broken_examples):
    out.append("## {}".format(pack))
    out.append("")
    out.append("`{}`".format(root))
    out.append("")
    out.append("### Verdicts")
    out.append("- **Player character source:**  {}".format(_player_character_verdict(bucket, skeletons)))
    out.append("- **Prop / mesh pack:**         {}".format(_prop_pack_verdict(bucket)))
    out.append("- **Sound pack:**               {}".format(_sound_pack_verdict(bucket)))
    out.append("- **VFX pack:**                 {}".format(_vfx_pack_verdict(bucket)))
    out.append("")

    if bucket:
        out.append("### Inventory")
        out.append("| Type | Count |")
        out.append("|---|---:|")
        for kind in sorted(bucket.keys()):
            out.append("| {} | {} |".format(kind, bucket[kind]))
        out.append("")

    if skeletons:
        out.append("### Skeleton(s) targeted")
        for s in sorted(skeletons):
            note = ""
            ls = s.lower()
            if 'manny' in ls or 'mannequin' in ls or 'quinn' in ls:
                note = "  *(UE5 Mannequin-compatible)*"
            out.append("- `{}`{}".format(s, note))
        out.append("")

    if samples:
        out.append("### Sample assets")
        for kind in ('SkeletalMesh', 'Skeleton', 'AnimSequence', 'AnimBlueprint',
                     'StaticMesh', 'Material', 'SoundCue', 'Niagara', 'Blueprint'):
            if kind in samples:
                out.append("- **{}**:".format(kind))
                for p in samples[kind]:
                    out.append("  - `{}`".format(p))
        out.append("")

    if broken_count > 0:
        out.append("### Broken dependencies")
        out.append("- **{}** assets in this pack reference missing packages.".format(broken_count))
        for ex in broken_examples:
            out.append("  - `{}`".format(ex))
        out.append("")
    out.append("")


def run():
    out_path = _save_path()
    print("[fab-audit] writing report to {}".format(out_path))

    packs = _list_packs()
    if not packs:
        print("[fab-audit] no Fab packs found")
        return

    lines = []
    lines.append("# Quiet Rift — Fab Pack Audit")
    lines.append("")
    lines.append("Generated by `qr_audit_fabs.py`. Read-only; no assets were modified.")
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    lines.append("| Pack | Verdict | SK | Anims | StaticMesh | Sounds | VFX | Broken Deps |")
    lines.append("|---|---|---:|---:|---:|---:|---:|---:|")

    detail_lines = []
    totals = defaultdict(int)
    for pack, root in packs:
        print("[fab-audit] scanning {}".format(pack))
        bucket, samples, skeletons = _bucket_pack(root)
        broken_count, broken_examples = _count_broken_deps(root)
        for k, v in bucket.items():
            totals[k] += v

        # Top-line summary row.
        char_v = _player_character_verdict(bucket, skeletons)
        verdict_short = "PLAYER" if "YES" in char_v \
            else ("PROPS" if "YES" in _prop_pack_verdict(bucket)
                  else ("SOUNDS" if "YES" in _sound_pack_verdict(bucket)
                        else ("VFX" if "YES" in _vfx_pack_verdict(bucket) else "—")))
        lines.append("| {} | {} | {} | {} | {} | {} | {} | {} |".format(
            pack, verdict_short,
            bucket.get('SkeletalMesh', 0),
            bucket.get('AnimSequence', 0),
            bucket.get('StaticMesh', 0),
            bucket.get('SoundCue', 0) + bucket.get('SoundWave', 0),
            bucket.get('Niagara', 0),
            broken_count))

        # Full per-pack section.
        _write_pack_section(detail_lines, pack, root, bucket, samples, skeletons,
                            broken_count, broken_examples)

    lines.append("")
    lines.append("## Totals across all packs")
    lines.append("")
    for k in sorted(totals.keys()):
        lines.append("- {}: {}".format(k, totals[k]))
    lines.append("")
    lines.append("---")
    lines.append("")
    lines.append("# Per-pack detail")
    lines.append("")
    lines.extend(detail_lines)

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(lines))

    print("[fab-audit] done — {} packs in report".format(len(packs)))
    print("[fab-audit] open: {}".format(out_path))


if __name__ == "__main__":
    run()
