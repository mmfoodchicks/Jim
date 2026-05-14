"""
Quiet Rift: Enigma — auto-assign Fab materials to our SM_* meshes by prefix.

Walks every StaticMesh under /Game/Meshes/ (skipping /Game/Fabs/), figures
out the asset's category prefix (SM_WPN_*, SM_FOD_*, SM_BLD_*, ...), and
picks the best-matching material from /Game/Fabs/.../Materials/. Then
slots that material into every material slot on the mesh.

Why: the generated FBXes import with the engine default grey, so every
mesh looks like a checkerboard until we author or borrow materials. We
have ~1170 materials sitting in Content/Fabs already. This script wires
them up so the world stops looking grey.

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_assign_fab_materials.py').read())

Or:
  import qr_assign_fab_materials
  qr_assign_fab_materials.run()

Idempotent — re-running picks the same materials (deterministic match
order). Pass run(dry_run=True) to log what would be assigned without
modifying any assets. Pass run(force=True) to overwrite slots that
already have a non-default material.
"""

import os
import re
import unreal


# Prefix → ordered list of keyword candidates. The first Fab material
# whose name contains any keyword in the first list wins; if nothing
# matches we fall back through the next lists.
PREFIX_KEYWORDS = {
    # Weapons + attachments — gun metal, scope plastic.
    "WPN": [["rifle", "gun", "weapon", "firearm"], ["metal", "steel", "gunmetal"], ["industrial"]],
    "ATT": [["scope", "optic", "rail", "attachment"], ["metal", "steel"], ["plastic"]],

    # Buildings / walls — concrete + panel.
    "BLD": [["wall", "concrete", "panel", "construction"], ["metal", "steel"], ["industrial"]],
    "STR": [["wall", "concrete", "panel", "construction"], ["metal", "steel"]],

    # Crafting stations — industrial machines, computer consoles.
    "STN": [["console", "industrial", "machine", "computer"], ["metal", "panel", "tech"]],
    "STA": [["console", "industrial", "machine"], ["metal", "panel"]],

    # Trees + flora.
    "TRE": [["bark", "trunk", "tree"], ["wood"]],
    "FLO": [["leaf", "plant", "foliage", "moss", "grass"], ["bark", "wood"]],
    "PLT": [["leaf", "plant", "foliage", "moss"], ["grass"]],

    # Food (fruit / berry / produce / meat).
    "FOD": [["fruit", "berry", "produce", "vegetable", "food"], ["meat", "flesh"], ["leaf"]],

    # Medicine (clean white plastic / glass).
    "MED": [["plastic", "medical", "white", "bottle"], ["glass", "clean"]],

    # Wildlife / animals — skin / fur / hide / scale.
    "ANM": [["skin", "flesh", "creature", "fur", "hide", "scale"], ["leather"]],
    "WLD": [["skin", "flesh", "creature", "fur", "hide"], ["leather"]],

    # Tools / handheld — metal + wood handles.
    "TOL": [["tool", "metal", "handle"], ["wood", "steel"]],
    "HND": [["handle", "metal", "wood"], ["leather"]],

    # Resources / raw materials.
    "RAW": [["wood", "rock", "stone", "ore"], ["crate"]],

    # POI clutter / props.
    "POI": [["crate", "barrel", "box"], ["wood", "metal"]],
    "PRP": [["crate", "barrel", "box"], ["wood", "metal"]],

    # Rigs / packs / backpacks.
    "RIG": [["fabric", "cloth", "leather", "pack"], ["metal"]],
    "PCK": [["fabric", "cloth", "leather"], ["pack"]],

    # Cosmetic clothing.
    "CSM": [["cloth", "fabric", "leather"], ["wool", "linen"]],
    "CLT": [["cloth", "fabric", "leather"], ["wool"]],

    # Remnant / alien artifacts — emissive sci-fi.
    "REM": [["sci", "alien", "glow", "emissive", "energy"], ["console", "tech", "panel"]],
    "ART": [["alien", "glow", "emissive", "energy"], ["metal", "console"]],

    # Vanguard faction / military outposts.
    "VAN": [["military", "armor", "tactical"], ["metal", "steel"], ["industrial"]],
    "OUT": [["military", "armor", "tactical"], ["metal", "steel"]],
}


MESH_ROOT = "/Game/Meshes"
FAB_ROOT  = "/Game/Fabs"
DEFAULT_FALLBACK_KEYWORD = "metal"  # if nothing else fits, give it a metal mat


def _asset_registry():
    return unreal.AssetRegistryHelpers.get_asset_registry()


def _scan_paths(paths):
    ar = _asset_registry()
    for p in paths:
        ar.scan_paths_synchronous([p], True)


def _get_fab_materials():
    """Return list of (object_path:str, lowercase_asset_name:str) for every
    Material / MaterialInstanceConstant under /Game/Fabs/."""
    ar = _asset_registry()
    f = unreal.ARFilter(
        class_names=["Material", "MaterialInstanceConstant"],
        package_paths=[FAB_ROOT],
        recursive_paths=True,
    )
    out = []
    for ad in ar.get_assets(f):
        out.append((str(ad.object_path), str(ad.asset_name).lower()))
    return out


def _get_game_meshes():
    """All StaticMesh assets under /Game/Meshes that start with SM_."""
    ar = _asset_registry()
    f = unreal.ARFilter(
        class_names=["StaticMesh"],
        package_paths=[MESH_ROOT],
        recursive_paths=True,
    )
    out = []
    for ad in ar.get_assets(f):
        name = str(ad.asset_name)
        if name.startswith("SM_"):
            out.append((str(ad.object_path), name))
    return out


_PREFIX_RE = re.compile(r"^SM_([A-Z]{2,4})_")

def _prefix_of(asset_name):
    m = _PREFIX_RE.match(asset_name)
    return m.group(1) if m else None


def _pick_material(prefix, fab_materials):
    """Pick the best-matching Fab material for the given prefix. Returns
    object_path string or None."""
    keyword_tiers = PREFIX_KEYWORDS.get(prefix, [[DEFAULT_FALLBACK_KEYWORD]])
    for tier in keyword_tiers:
        for path, lname in fab_materials:
            if any(kw in lname for kw in tier):
                return path
    # Final fallback: any metal material at all.
    for path, lname in fab_materials:
        if DEFAULT_FALLBACK_KEYWORD in lname:
            return path
    return None


def run(dry_run=False, force=False):
    """Walk our SM_* meshes and slot in a Fab material per prefix.

    dry_run : log the chosen material without saving any asset.
    force   : overwrite slots even if they already have a non-default mat.
    """
    print("[fab-mat] scanning asset registry…")
    _scan_paths([MESH_ROOT, FAB_ROOT])
    fab_mats = _get_fab_materials()
    meshes   = _get_game_meshes()
    print("[fab-mat] {} Fab materials, {} game meshes".format(len(fab_mats), len(meshes)))

    if not fab_mats:
        print("[fab-mat] no Fab materials found under {} — aborting".format(FAB_ROOT))
        return

    # Cache prefix → chosen material path (single material per prefix, so
    # all WPN_* share one gun-metal mat, all BLD_* share one wall mat,
    # etc.). Keeps the world visually consistent and the asset disk
    # footprint small.
    prefix_cache = {}
    by_prefix_count = {}
    saved = 0
    skipped = 0
    no_match = 0

    for mesh_path, mesh_name in meshes:
        prefix = _prefix_of(mesh_name)
        if not prefix:
            skipped += 1
            continue

        # Resolve material for this prefix (cached).
        if prefix not in prefix_cache:
            prefix_cache[prefix] = _pick_material(prefix, fab_mats)
        mat_path = prefix_cache[prefix]
        if not mat_path:
            no_match += 1
            continue

        mat = unreal.load_asset(mat_path)
        mesh = unreal.load_asset(mesh_path)
        if not mesh or not mat:
            skipped += 1
            continue

        slot_count = mesh.get_num_sections(0) if hasattr(mesh, "get_num_sections") else 1
        # Use static_materials list to detect existing assignments. UE
        # exposes get_static_materials() returning array of FStaticMaterial.
        try:
            existing = mesh.static_materials
        except Exception:
            existing = []

        # Skip if any slot already has a non-default material and force=False.
        if not force and existing:
            has_real = any(sm.material_interface and "DefaultMaterial" not in str(sm.material_interface.get_name())
                           for sm in existing)
            if has_real:
                skipped += 1
                continue

        # Build new static materials array, one slot per existing slot.
        new_slots = []
        slot_n = max(1, len(existing))
        for i in range(slot_n):
            slot_name = existing[i].material_slot_name if i < len(existing) and existing[i].material_slot_name else "Slot{}".format(i)
            new_slots.append(unreal.StaticMaterial(
                material_interface=mat,
                material_slot_name=slot_name,
            ))

        if dry_run:
            print("[fab-mat] DRY  {:<48s} {:<5s} -> {}".format(mesh_name, prefix, os.path.basename(mat_path)))
        else:
            mesh.set_editor_property("static_materials", new_slots)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            saved += 1
            print("[fab-mat] SAVE {:<48s} {:<5s} -> {}".format(mesh_name, prefix, os.path.basename(mat_path)))

        by_prefix_count[prefix] = by_prefix_count.get(prefix, 0) + 1

    print("[fab-mat] ---- summary ----")
    print("[fab-mat] saved      : {}".format(saved))
    print("[fab-mat] skipped    : {}".format(skipped))
    print("[fab-mat] no match   : {}".format(no_match))
    print("[fab-mat] by prefix  :")
    for p, n in sorted(by_prefix_count.items()):
        mat = prefix_cache.get(p)
        print("[fab-mat]   {:<5s} x{:<4d} -> {}".format(p, n, os.path.basename(mat) if mat else "<none>"))


if __name__ == "__main__":
    run()
