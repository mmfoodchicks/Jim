"""
qr_wire_anim_blueprint.py -- finalize the ABP_QRPlayer Locomotion
state-machine wire-up as far as Python can take it.

What this script does (in one run):

  1. Ensures /Game/QuietRift/Animations/ABP_QRPlayer exists. If it
     doesn't, runs qr_create_anim_blueprint.run() to create the empty
     AnimBlueprint targeting the Mannequin skeleton with parent class
     UQRPlayerAnimInstance.

  2. Scans /Game/QuietRift/Animations/Retargeted/ (output of
     qr_retarget_anims_to_mannequin.py) for the locomotion anim set.
     If the folder is empty or missing anims, falls back to scanning
     the Fab Mannequin-rigged packs for any matching name tokens.

  3. Picks the best AnimSequence for each Locomotion-state slot:
     Idle, Walk, Run, Jump, Fall, Land, Death. First-token match
     wins (case-insensitive, scoped to anims on the Mannequin
     skeleton or a Mannequin-compatible retargeted skeleton).

  4. Prints a clear manual checklist with the resolved asset paths,
     so the designer drags each anim into the right state node when
     they author the state machine in the AnimBP editor.

What this script CANNOT do (UE Python limitation):

  - Author AnimGraph state-machine nodes in code. UE's editor-side
    Anim Graph node classes (UAnimGraphNode_StateMachine,
    UAnimGraphNode_State, transition rules) are not Python-bound in
    UE 5.x. The state machine itself MUST be drawn by hand in the
    Anim Blueprint editor. Compensating: the catalog this script
    prints lists exactly which anim to drop in each state, so the
    manual step is straight assembly, no asset hunting.

Run from the UE Python console:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_wire_anim_blueprint.py').read())

Prerequisite (run once before this script):
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_retarget_anims_to_mannequin.py').read())
"""

import unreal


ABP_PATH = "/Game/QuietRift/Animations/ABP_QRPlayer"

# Folders to scan, in priority order. The Third Person Template
# folders are first because (a) they're guaranteed-clean Mannequin
# anims that ship with the engine, (b) they include a full locomotion
# set (Idle/Walk/Run/Jump/Fall/Land/Crouch). Retargeted next because
# those are FuturisticWarrior anims we re-skinned onto the Mannequin.
# FAB packs last as fallback -- they're inconsistent (some packs
# bring partial Mannequin dependencies and break loading).
ANIM_SEARCH_PATHS = [
    "/Game/ControlRig/Characters/Mannequins/Animations",  # Mannequins Asset Pack (Fab, by Epic)
    "/Game/Characters/Mannequins/Animations",             # Third Person Template fallback
    "/Game/ThirdPerson/Blueprints",                       # UE5 TPS sometimes here too
    "/Game/QuietRift/Animations/Retargeted",              # our retarget output
    "/Game/FreeAnimsMixPack/Animation",
    "/Game/RamsterZ_FreeAnims_Volume1/AnimationSequence",
    "/Game/FuturisticWarrior/Animation",
    "/Game/DynamicFalling/Animation",
    "/Game/DeadBodies_Poses_nikoff/Animations",
]

# Locomotion state -> token list (first match wins, case-insensitive).
# Order matters: more specific tokens before generic ones (so
# "backwalk" doesn't accidentally match "Walk" before a real "walk"
# is found in a different anim).
LOCOMOTION_SLOTS = [
    # Token order: prefer the Mannequin-pack naming (MM_Idle, MM_Walk_Fwd
    # etc) over older naming. Within a single token, the first anim in
    # ANIM_SEARCH_PATHS order wins, so high-priority folders beat fallbacks.
    ("Idle",  ["mm_idle", "mf_idle", "idle1", "_idle", "idle"]),
    ("Walk",  ["mm_walk_fwd", "mf_walk_fwd", "walk1", "walk_forward", "walk"]),
    ("Run",   ["mm_run_fwd", "mf_run_fwd", "run1", "sprint", "jog", "run"]),
    ("Jump",  ["mm_jump", "mf_jump", "jump_start", "jump1", "jump"]),
    ("Fall",  ["mm_fall_loop", "mm_fall", "mf_fall", "fall_loop", "falling", "fall"]),
    ("Land",  ["mm_land", "mf_land", "land_soft", "landing", "land"]),
    ("Death", ["mm_death", "mm_die", "death1", "dying", "death", "die_"]),
]

# These tokens disqualify an anim even if its name matches a slot
# token. Keeps "BackWalk", "WalkLeft", combo strings out of the main
# Walk slot.
NEGATIVE_TOKENS = {
    "Walk": ["backwalk", "walkleft", "walkright", "walkback"],
    "Run":  ["runback", "backrun"],
}


def _list_anims_in(package_path):
    """Return all AnimSequence assets under a /Game path."""
    if not unreal.EditorAssetLibrary.does_directory_exist(package_path):
        return []
    paths = unreal.EditorAssetLibrary.list_assets(
        package_path, recursive=True, include_folder=False)
    out = []
    for p in paths:
        # list_assets returns object paths like "/Game/Foo/Bar.Bar"; we
        # want the package path for load_asset.
        pkg = p.split(".")[0] if "." in p else p
        # Hard blocklist: MPMECH ships MM_*_ANIM whose skeleton dependency
        # was never synced, so they load with skeleton == None and poison
        # the ABP. Never source anims from these packs.
        if any(bad in pkg for bad in ("/MPMECH/",)):
            continue
        a = unreal.load_asset(pkg)
        if isinstance(a, unreal.AnimSequence):
            # Belt-and-suspenders: skip any anim whose skeleton is broken/
            # missing (a wired one throws 'missing skeleton' compile
            # errors on every editor start).
            try:
                if a.get_editor_property("skeleton") is None:
                    continue
            except Exception:
                continue
            out.append((pkg, a))
    return out


def _scan_all_anims():
    """Walk every search path, return [(package_path, AnimSequence), ...]."""
    found = []
    seen = set()
    for root in ANIM_SEARCH_PATHS:
        for pkg, a in _list_anims_in(root):
            if pkg in seen:
                continue
            seen.add(pkg)
            found.append((pkg, a))
    return found


def _pick_anim_for_slot(slot_name, slot_tokens, all_anims):
    """First anim whose lowercase name contains a slot token wins,
    after the NEGATIVE_TOKENS filter for that slot."""
    negatives = NEGATIVE_TOKENS.get(slot_name, [])
    for token in slot_tokens:
        for pkg, a in all_anims:
            name = pkg.split("/")[-1].lower()
            if token not in name:
                continue
            if any(neg in name for neg in negatives):
                continue
            return (pkg, a)
    return (None, None)


def _ensure_abp():
    """Create ABP_QRPlayer if it's missing. Returns the AnimBlueprint."""
    if unreal.EditorAssetLibrary.does_asset_exist(ABP_PATH):
        return unreal.load_asset(ABP_PATH)
    # Defer to the existing creator script -- it knows how to find the
    # Mannequin skeleton and bind UQRPlayerAnimInstance.
    try:
        import qr_create_anim_blueprint
        qr_create_anim_blueprint.run()
    except Exception as e:
        print("[abp-wire] could not import qr_create_anim_blueprint: {}".format(e))
        print("[abp-wire] run it manually first:")
        print("[abp-wire]   exec(open(r'<Project>/Tools/EditorScripts/qr_create_anim_blueprint.py').read())")
        return None
    return unreal.load_asset(ABP_PATH) if unreal.EditorAssetLibrary.does_asset_exist(ABP_PATH) else None


def run():
    print("\n=== qr_wire_anim_blueprint ===")

    abp = _ensure_abp()
    if not abp:
        print("[abp-wire] ABP_QRPlayer not present and creation failed -- aborting")
        return

    print("[abp-wire] ABP_QRPlayer at: {}".format(ABP_PATH))

    all_anims = _scan_all_anims()
    print("[abp-wire] discovered {} AnimSequences across {} folders"
          .format(len(all_anims), len(ANIM_SEARCH_PATHS)))

    print("\n--- Locomotion slot resolution ---")
    resolved = {}
    for slot_name, tokens in LOCOMOTION_SLOTS:
        pkg, asset = _pick_anim_for_slot(slot_name, tokens, all_anims)
        resolved[slot_name] = pkg
        if pkg:
            print("  {:<6} -> {}".format(slot_name, pkg))
        else:
            print("  {:<6} -> NOT FOUND (tokens tried: {})".format(slot_name, tokens))

    missing = [k for k, v in resolved.items() if v is None]
    if missing:
        print("\n[abp-wire] WARNING: no anim resolved for: {}".format(", ".join(missing)))
        print("[abp-wire]   Likely cause: qr_retarget_anims_to_mannequin.py hasn't")
        print("[abp-wire]   been run, or the FuturisticWarrior pack isn't in")
        print("[abp-wire]   /Game/. Run that first, then re-run this script.")

    print("\n--- Manual follow-up checklist ---")
    print("UE 5.x Python cannot author Anim Graph state-machine nodes.")
    print("Open ABP_QRPlayer in the editor and do this once:")
    print("  1. AnimGraph tab -> right-click -> 'Add State Machine'.")
    print("  2. Rename it 'Locomotion'. Wire 'Locomotion' -> Output Pose.")
    print("  3. Double-click Locomotion. Drop seven State nodes:")
    for slot_name, _ in LOCOMOTION_SLOTS:
        pkg = resolved.get(slot_name)
        print("       - {}  (drop in anim: {})".format(slot_name, pkg or "<NONE - assign later>"))
    print("  4. Wire 'Entry' -> Idle.")
    print("  5. Transitions (read AnimInstance vars exposed by UQRPlayerAnimInstance):")
    print("       Idle -> Walk    on Speed > 10")
    print("       Walk -> Idle    on Speed < 5")
    print("       Walk -> Run     on Speed > 350 OR bIsSprinting")
    print("       Run  -> Walk    on Speed < 320 AND !bIsSprinting")
    print("       Any  -> Jump    on bJustJumped (set in C++ on jump press)")
    print("       Jump -> Fall    on bIsFalling AND duration > 0.4")
    print("       Fall -> Land    on !bIsFalling")
    print("       Land -> Idle    on duration > 0.25")
    print("       Any  -> Death   on bIsDead   (state has no exit)")
    print("  6. Compile + Save ABP. Press Play -- character should now")
    print("     idle on spawn and animate properly as you move.")


if __name__ == "__main__":
    run()
