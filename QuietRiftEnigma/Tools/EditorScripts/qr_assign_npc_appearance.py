"""
qr_assign_npc_appearance.py -- bulk-assign a skeletal mesh + idle/walk
animations to the AQRNPCActor (and AQRNPCColonist) class defaults so
every villager spawned by qr_spawn_starter_village walks out of the
script with skin + working animations.

Sidesteps the ABP_QRPlayer state-machine problem: the brain forces the
mesh into AnimationSingleNode mode and PlayAnimation()'s the assigned
sequence directly. No anim-graph authoring required, no Python state-
machine support needed.

How it picks assets:
  1. SKM mesh -- walks a search list of known Mannequin mesh paths
     (UE5 stock, Quinn variant, retargeted FuturisticWarrior) and
     uses the first one that resolves.
  2. Idle / Walk anims -- same approach with a candidate list. The
     UE5 stock locomotion set ships these names.
  3. If none resolve, prints a clear "no Mannequin assets found"
     message and exits gracefully -- doesn't crash the editor or
     stamp garbage onto the class defaults.

Run from the UE Python console:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_assign_npc_appearance.py').read())
  run()

Override which assets to use by passing soft paths:
  run(mesh='/Game/Custom/SKM_Survivor', idle='/Game/Anims/A_Idle')
"""

import unreal


# Mesh candidates in preference order. The script picks the first one
# that resolves. Stock UE5 Mannequin first (most likely to have stock
# anims that play correctly); then the Fab character packs the user
# has installed; then a debug cube. The Fab characters carry their own
# skeletons -- if their pack's anims aren't compatible with Mannequin's
# UAnimSequence assets, fall back to Mannequin instead.
# The Fab library ships FOUR full UE5 Mannequin copies (ControlRig,
# FreeAnimsMixPack, DynamicFalling, DeadBodies all carry Demo
# mannequins). ControlRig is preferred: it has BOTH the meshes AND the
# matching MM_/MF_ locomotion anims in one pack, so mesh + anims are
# guaranteed same-skeleton. Stock /Game/Characters/ path checked first
# in case the user adds the official feature pack later.
MESH_CANDIDATES = [
    "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple",
    "/Game/ControlRig/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple",
    "/Game/ControlRig/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple",
    "/Game/ControlRig/Characters/Mannequins/Meshes/SKM_Quinn.SKM_Quinn",
    "/Game/ControlRig/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny",
    "/Game/FreeAnimsMixPack/Demo/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple",
    "/Game/FreeAnimsMixPack/Demo/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple",
    "/Game/DynamicFalling/Demo/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple",
    # Fab character packs (own skeletons -- only via explicit override
    # so the Mannequin anims don't get paired with a foreign skeleton).
    "/Game/QuantumCharacter/Mesh/SKM_QuantumCharacter.SKM_QuantumCharacter",
    "/Engine/EngineMeshes/SkeletalCube.SkeletalCube",   # last-ditch debug placeholder
]

# Locomotion -- prefer the unified ControlRig set so all NPCs share one
# skeleton (qr_unify_mannequin_anims clones them under /Game/QuietRift/
# Animations/ControlRig). The stock /Game/Characters path is checked
# first in case the official feature pack is installed.
IDLE_CANDIDATES = [
    "/Game/QuietRift/Animations/ControlRig/A_MM_Idle.A_MM_Idle",
    "/Game/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle",
    "/Game/ControlRig/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle",
    "/Game/ControlRig/Characters/Mannequins/Animations/Quinn/MF_Idle.MF_Idle",
    # Combat-pack fallback: Standing_Idle ships with RamsterZ.
    "/Game/QuietRift/Animations/RamsterZ/A_Standing_Idle.A_Standing_Idle",
]

WALK_CANDIDATES = [
    "/Game/QuietRift/Animations/ControlRig/A_MM_Walk_Fwd.A_MM_Walk_Fwd",
    "/Game/Characters/Mannequins/Animations/Manny/MM_Walk_Fwd.MM_Walk_Fwd",
    "/Game/ControlRig/Characters/Mannequins/Animations/Manny/MM_Walk_Fwd.MM_Walk_Fwd",
    "/Game/ControlRig/Characters/Mannequins/Animations/Quinn/MF_Walk_Fwd.MF_Walk_Fwd",
    "/Game/ControlRig/Characters/Mannequins/Animations/Manny/MM_Walk_InPlace.MM_Walk_InPlace",
]

RUN_CANDIDATES = [
    "/Game/QuietRift/Animations/ControlRig/A_MM_Run_Fwd.A_MM_Run_Fwd",
    "/Game/ControlRig/Characters/Mannequins/Animations/Manny/MM_Run_Fwd.MM_Run_Fwd",
    "/Game/ControlRig/Characters/Mannequins/Animations/Quinn/MF_Run_Fwd.MF_Run_Fwd",
]

# Sleep loops -- DeadBodies has Lie poses; use one as a sleep pose.
SLEEP_CANDIDATES = [
    "/Game/QuietRift/Animations/DeadBodies/A_AS_DeadBody_Pose_Lie_05.A_AS_DeadBody_Pose_Lie_05",
    "/Game/QuietRift/Animations/DeadBodies/A_AS_DeadBody_Pose_Lie_03.A_AS_DeadBody_Pose_Lie_03",
]

# Work loop -- FreeAnimsMix has ReachingForward (working-at-table feel).
WORK_CANDIDATES = [
    "/Game/QuietRift/Animations/FreeAnimsMix/A_AS_ReachingForward.A_AS_ReachingForward",
    "/Game/QuietRift/Animations/RamsterZ/A_H2H_Idle.A_H2H_Idle",
]

# Talk loop -- emote/gesture anims work for chatting villagers.
TALK_CANDIDATES = [
    "/Game/QuietRift/Animations/FreeAnimsMix/A_AS_Emotes19.A_AS_Emotes19",
    "/Game/QuietRift/Animations/RamsterZ/A_SillyGesture01.A_SillyGesture01",
    "/Game/QuietRift/Animations/RamsterZ/A_SillyGesture02.A_SillyGesture02",
]

# Death pose -- DeadBodies has 30 variants. Any Lie pose works.
DEATH_CANDIDATES = [
    "/Game/QuietRift/Animations/DeadBodies/A_AS_DeadBody_Pose_Lie_11.A_AS_DeadBody_Pose_Lie_11",
    "/Game/QuietRift/Animations/DeadBodies/A_AS_DeadBody_Pose_Lie_16.A_AS_DeadBody_Pose_Lie_16",
    "/Game/QuietRift/Animations/FreeAnimsMix/A_AS_DyingFromWounds.A_AS_DyingFromWounds",
]

# Melee swing -- UQRRaidPartyAI flashes this through the brain's
# FlashAttack() on every attack tick. RamsterZ ships an H2H combat set
# whose exact spellings vary, so after the fixed guesses miss we sweep
# the unified pool by keyword instead of failing silently.
ATTACK_CANDIDATES = [
    "/Game/QuietRift/Animations/RamsterZ/A_H2H_Punch.A_H2H_Punch",
    "/Game/QuietRift/Animations/RamsterZ/A_Punching.A_Punching",
    "/Game/QuietRift/Animations/FreeAnimsMix/A_AS_Punch.A_AS_Punch",
]

ATTACK_KEYWORDS = ("punch", "jab", "hook", "swing", "attack", "kick", "h2h")


def _find_anim_by_keyword(keywords, root="/Game/QuietRift/Animations"):
    """First AnimSequence under root whose name contains a keyword.
    Keyword order is preference order."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path(root, recursive=True)
    for kw in keywords:
        for ad in assets:
            try:
                if str(ad.asset_class_path.asset_name) != "AnimSequence":
                    continue
            except Exception:
                continue
            name = str(ad.asset_name).lower()
            # 'idle' guard: A_H2H_Idle matches 'h2h' but is a stance, not a swing.
            if kw in name and "idle" not in name:
                return unreal.load_asset(
                    "{}.{}".format(ad.package_name, ad.asset_name))
    return None


def _first_existing(candidates):
    for path in candidates:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            return unreal.load_asset(path)
    return None


def _resolve_or(candidates, override):
    if override:
        if unreal.EditorAssetLibrary.does_asset_exist(override):
            return unreal.load_asset(override)
        print("[npc-skin]   override missing: {}".format(override))
    return _first_existing(candidates)


def _set_cdo_property(class_obj, prop, value):
    """Stamp a property on the class default object, then re-save the
    blueprint. Class defaults propagate to every instance unless that
    instance hand-overrides the field."""
    if not class_obj:
        return False
    cdo = unreal.get_default_object(class_obj)
    if not cdo:
        return False
    try:
        cdo.set_editor_property(prop, value)
    except Exception as e:
        print("[npc-skin]   set {} skipped: {}".format(prop, e))
        return False
    return True


def _stamp_class(class_path, mesh, idle, walk):
    cls = unreal.load_object(None, class_path)
    if not cls:
        print("[npc-skin]   class {} not loaded (recompile?)".format(class_path))
        return False

    name = class_path.rsplit("/", 1)[-1]

    # Mesh stamps on AQRNPCActor.DefaultSkeletalMesh; brain gets the
    # idle/walk soft refs so the swap fires on movement edges.
    ok_mesh = _set_cdo_property(cls, "default_skeletal_mesh", mesh) if mesh else False

    cdo = unreal.get_default_object(cls)
    try:
        brain = cdo.get_editor_property("brain")
    except Exception:
        brain = None
    ok_idle = False
    ok_walk = False
    if brain:
        if idle:
            try:
                brain.set_editor_property("idle_anim", idle)
                ok_idle = True
            except Exception as e:
                print("[npc-skin]   idle_anim skipped on {}: {}".format(name, e))
        if walk:
            try:
                brain.set_editor_property("walk_anim", walk)
                ok_walk = True
            except Exception as e:
                print("[npc-skin]   walk_anim skipped on {}: {}".format(name, e))

    print("[npc-skin]   {:<22s}: mesh={} idle={} walk={}".format(
        name, "Y" if ok_mesh else ".", "Y" if ok_idle else ".",
        "Y" if ok_walk else "."))
    return True


def _stamp_brain_slot(brain, prop_snake, asset, name):
    if not brain or not asset:
        return False
    try:
        brain.set_editor_property(prop_snake, asset)
        return True
    except Exception as e:
        print("[npc-skin]   {} skipped on {}: {}".format(prop_snake, name, e))
        return False


def _stamp_class_v2(class_path, mesh, anims):
    """anims = {brain_property_snake_case: asset_or_none}"""
    cls = unreal.load_object(None, class_path)
    if not cls:
        print("[npc-skin]   class {} not loaded (recompile?)".format(class_path))
        return False
    name = class_path.rsplit("/", 1)[-1]
    ok_mesh = _set_cdo_property(cls, "default_skeletal_mesh", mesh) if mesh else False

    cdo = unreal.get_default_object(cls)
    try:
        brain = cdo.get_editor_property("brain")
    except Exception:
        brain = None

    stamped = []
    for prop_snake, asset in anims.items():
        if _stamp_brain_slot(brain, prop_snake, asset, name):
            stamped.append(prop_snake)

    print("[npc-skin]   {:<22s}: mesh={} anims=[{}]".format(
        name, "Y" if ok_mesh else ".", ", ".join(stamped) if stamped else "none"))
    return True


def run(mesh=None, idle=None, walk=None, run_=None,
        sleep=None, work=None, talk=None, death=None, attack=None):
    """Resolve a mesh + full anim set and stamp onto AQRNPCActor and
    AQRNPCColonist class defaults.

    Args (all optional soft-path overrides):
      mesh, idle, walk, run_, sleep, work, talk, death, attack
    """
    print("\n=== qr_assign_npc_appearance ===")

    mesh_asset = _resolve_or(MESH_CANDIDATES, mesh)
    attack_asset = _resolve_or(ATTACK_CANDIDATES, attack)
    if not attack_asset:
        attack_asset = _find_anim_by_keyword(ATTACK_KEYWORDS)
    anims = {
        "idle_anim":  _resolve_or(IDLE_CANDIDATES,  idle),
        "walk_anim":  _resolve_or(WALK_CANDIDATES,  walk),
        "run_anim":   _resolve_or(RUN_CANDIDATES,   run_),
        "sleep_anim": _resolve_or(SLEEP_CANDIDATES, sleep),
        "work_anim":  _resolve_or(WORK_CANDIDATES,  work),
        "talk_anim":  _resolve_or(TALK_CANDIDATES,  talk),
        "death_anim": _resolve_or(DEATH_CANDIDATES, death),
        "attack_anim": attack_asset,
    }

    if not mesh_asset:
        print("[npc-skin] No Mannequin SKM mesh found. The Fab library should ship")
        print("[npc-skin] several (ControlRig, FreeAnimsMixPack, DynamicFalling).")
        print("[npc-skin] If the script can't find any, override: run(mesh='/Game/...')")
        return

    print("[npc-skin] mesh:  {}".format(mesh_asset.get_path_name()))
    for prop, asset in anims.items():
        path = asset.get_path_name() if asset else "(none)"
        print("[npc-skin] {:<10s} {}".format(prop + ":", path))

    targets = [
        "/Script/QuietRiftEnigma.QRNPCActor",
        "/Script/QuietRiftEnigma.QRNPCColonist",
    ]
    for t in targets:
        _stamp_class_v2(t, mesh_asset, anims)

    # The PLAYER third-person body uses the same mesh + locomotion set
    # via AQRCharacter's own slots (DefaultBodyMesh / TPIdle / TPWalk /
    # TPRun). Co-op partners + the player's own shadow animate through
    # single-node exactly like the villagers -- no AnimBP needed.
    player_cls = unreal.load_object(None, "/Script/QuietRiftEnigma.QRCharacter")
    if player_cls:
        cdo = unreal.get_default_object(player_cls)
        stamped = []
        for prop, asset in (
            ("default_body_mesh", mesh_asset),
            ("tp_idle_anim",      anims.get("idle_anim")),
            ("tp_walk_anim",      anims.get("walk_anim")),
            ("tp_run_anim",       anims.get("run_anim")),
        ):
            if not asset:
                continue
            try:
                cdo.set_editor_property(prop, asset)
                stamped.append(prop)
            except Exception as e:
                print("[npc-skin]   {} skipped on QRCharacter: {}".format(prop, e))
        print("[npc-skin]   {:<22s}: [{}]".format("QRCharacter", ", ".join(stamped)))

    print("[npc-skin] DONE -- new NPC spawns wear the assigned mesh + full anim set.")
    print("[npc-skin] Existing villagers in the level must be re-spawned")
    print("[npc-skin] (re-run qr_spawn_starter_village) to pick up the change.")


if __name__ == "__main__":
    run()
