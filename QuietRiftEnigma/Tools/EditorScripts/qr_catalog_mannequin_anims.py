"""
Quiet Rift: Enigma — survey every AnimSequence in the project, group by
underlying skeleton, and report which ones land on the UE5 Mannequin.

Why: after the May-2026 Fab haul we have ~50 mannequin-rigged anims
across FreeAnimsMixPack, RamsterZ_FreeAnims_Volume1, DynamicFalling,
DeadBodies_Poses_nikoff, plus the existing FuturisticWarrior pack on
its own skeleton. Before we author ABP_QRPlayer we need to know which
animations are already on the target skeleton (no retarget needed) and
which still need to go through the retarget pipeline.

Output is a console table:
  • per-skeleton anim count + a few sample paths
  • a "buckets" recommendation that classifies each anim by role
    (Idle / Walk / Run / Jump / Reload / Attack / Death / Roll / Misc)
    based on naming tokens, so we can drop them straight into AnimBP
    state machine slots.

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_catalog_mannequin_anims.py').read())

Or:
  import qr_catalog_mannequin_anims
  qr_catalog_mannequin_anims.run()
"""

import os
import re
import unreal


# Anim role buckets, keyed by lowercased token match against the anim
# asset name. First match wins. The unordered fallback bucket is "Misc".
ROLE_TOKENS = [
    ("Idle",   ["idle", "_idle", "stand_pose", "stance"]),
    ("Walk",   ["walk"]),
    ("Run",    ["run", "sprint", "jog"]),
    ("Jump",   ["jump"]),
    ("Land",   ["land", "fall"]),
    ("Roll",   ["roll", "dodge"]),
    ("Crouch", ["crouch", "prone"]),
    ("Reload", ["reload"]),
    ("Equip",  ["equip", "takeout", "takeoutgun", "draw"]),
    ("Fire",   ["fire", "shoot", "gunshot"]),
    ("Melee",  ["punch", "kick", "swing", "slash", "stab", "combo", "h2h", "1hm", "2hm"]),
    ("Hit",    ["damage", "hit", "wound", "stagger"]),
    ("Death",  ["death", "die", "dying", "deadbody", "dead_body"]),
    ("Emote",  ["emote", "dance", "gesture", "kiss", "hug", "talk", "rest"]),
]


def _role_of(name):
    lower = name.lower()
    for label, tokens in ROLE_TOKENS:
        if any(t in lower for t in tokens):
            return label
    return "Misc"


def _format_path(p):
    # Drop the object suffix (.<AssetName>) for display.
    return p.split(".", 1)[0]


def run():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    print("[anim-catalog] scanning /Game/ for AnimSequence assets…")
    ar.scan_paths_synchronous(["/Game/"], True)

    f = unreal.ARFilter(
        class_names=["AnimSequence"],
        package_paths=["/Game/"],
        recursive_paths=True,
    )
    anims = list(ar.get_assets(f))
    print("[anim-catalog] found {} anim sequences total".format(len(anims)))

    # Group by skeleton's package path. AssetData carries a Skeleton tag
    # in its tag-and-values dictionary (engine writes it on import).
    by_skeleton = {}
    by_role     = {}

    for ad in anims:
        # Skeleton tag is stored as "Skeleton" → "/Path/To/Skel.Skel"
        try:
            skel = str(ad.get_tag_value("Skeleton"))
        except Exception:
            skel = ""
        skel = skel or "<unknown>"

        path = str(ad.object_path)
        name = str(ad.asset_name)
        role = _role_of(name)

        by_skeleton.setdefault(skel, []).append((path, name, role))
        by_role.setdefault(role, []).append((path, name, skel))

    # ── Print per-skeleton summary ────────────────────────────────────
    print("")
    print("== per-skeleton totals ==")
    skel_sorted = sorted(by_skeleton.items(), key=lambda kv: -len(kv[1]))
    for skel, items in skel_sorted:
        print("  {:>4d}  {}".format(len(items), _format_path(skel)))

    # ── Identify Mannequin-rigged anims ───────────────────────────────
    mannequin_skel_keys = [k for k in by_skeleton.keys() if "mannequin" in k.lower() or "manny" in k.lower() or "quinn" in k.lower()]
    mannequin_items = []
    for k in mannequin_skel_keys:
        mannequin_items.extend(by_skeleton[k])

    print("")
    print("== Mannequin-rigged anims ({}) — drop-in for ABP_QRPlayer ==".format(len(mannequin_items)))
    by_role_manny = {}
    for path, name, role in mannequin_items:
        by_role_manny.setdefault(role, []).append((path, name))

    role_order = ["Idle", "Walk", "Run", "Jump", "Land", "Roll", "Crouch",
                  "Reload", "Equip", "Fire", "Melee", "Hit", "Death", "Emote", "Misc"]
    for role in role_order:
        bucket = by_role_manny.get(role, [])
        if not bucket: continue
        print("  -- {} ({}) --".format(role, len(bucket)))
        for path, name in sorted(bucket):
            print("     {:<48s} {}".format(name, _format_path(path)))

    # ── Non-Mannequin anims (retarget candidates) ─────────────────────
    other_skel_keys = [k for k in by_skeleton.keys() if k not in mannequin_skel_keys and k != "<unknown>"]
    print("")
    print("== anims on other skeletons (retarget candidates) ==")
    for k in other_skel_keys:
        items = by_skeleton[k]
        print("  {:>4d} on {}".format(len(items), _format_path(k)))
        # Show role distribution for this skeleton.
        roles = {}
        for _, _, r in items:
            roles[r] = roles.get(r, 0) + 1
        role_str = ", ".join("{}:{}".format(r, n) for r, n in sorted(roles.items()))
        print("        roles: " + role_str)

    # ── AnimBP slot recommendations ───────────────────────────────────
    print("")
    print("== AnimBP slot recommendations ==")
    print("  (assign these AnimSequence assets to states inside ABP_QRPlayer)")
    rec = {
        "LocomotionIdle": _first_match(by_role_manny.get("Idle", []), ["h2h_idle", "standing_idle", "1hm_idle"]),
        "LocomotionWalk": _first_match(by_role_manny.get("Walk", []), ["walk"]),
        "LocomotionRun":  _first_match(by_role_manny.get("Run", []),  ["run", "sprint", "jog"]),
        "JumpStart":      _first_match(by_role_manny.get("Jump", []), ["jump"]),
        "Roll":           _first_match(by_role_manny.get("Roll", []), ["roll_front", "rm_roll_front"]),
        "Reload":         _first_match(by_role_manny.get("Reload", []),[]),
        "Fire":           _first_match(by_role_manny.get("Fire", []), ["takeoutgunshoot", "fire"]),
        "Death":          _first_match(by_role_manny.get("Death", []),["deadbody_pose_lie_01", "dying", "death"]),
        "MeleePunch":     _first_match(by_role_manny.get("Melee", []),["punchcombo01", "punch_combo", "punch"]),
        "MeleeKick":      _first_match(by_role_manny.get("Melee", []),["kick01", "kick"]),
    }
    for slot, val in rec.items():
        if val:
            path, name = val
            print("  {:<18s} -> {}".format(slot, _format_path(path)))
        else:
            print("  {:<18s} -> (no match yet — needs retarget or new asset)".format(slot))


def _first_match(items, prefer_tokens):
    """Pick the first asset whose lowercased name contains one of the
    preferred tokens. Falls back to the first item overall if nothing
    matches but the list is non-empty."""
    if not items: return None
    if not prefer_tokens: return items[0]
    for tok in prefer_tokens:
        for path, name in items:
            if tok in name.lower():
                return (path, name)
    return items[0]


if __name__ == "__main__":
    run()
