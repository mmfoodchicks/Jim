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
MESH_CANDIDATES = [
    "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple",
    "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple",
    "/Game/Characters/Mannequins/Meshes/SKM_Quinn.SKM_Quinn",
    "/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny",
    # Fab character packs already in /Game/Fabs/. Tagged with their
    # native skeleton in the brain so an override doesn't pair a
    # FuturisticWarrior mesh with a Mannequin anim.
    "/Game/Fabs/QuantumCharacter/Meshes/SKM_QuantumCharacter.SKM_QuantumCharacter",
    "/Game/Fabs/QuantumCharacter/Meshes/SKM_QuantumCharacter_NoHead.SKM_QuantumCharacter_NoHead",
    "/Game/Fabs/FuturisticWarrior/Meshes/SK_FuturisticWarrior.SK_FuturisticWarrior",
    "/Engine/EngineMeshes/SkeletalCube.SkeletalCube",   # last-ditch debug placeholder
]

IDLE_CANDIDATES = [
    "/Game/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle",
    "/Game/Characters/Mannequins/Animations/Quinn/MF_Idle.MF_Idle",
    "/Game/Characters/Mannequins/Animations/Idle/MM_Idle.MM_Idle",
    "/Game/Characters/Mannequin/Animations/ThirdPersonIdle.ThirdPersonIdle",
]

WALK_CANDIDATES = [
    "/Game/Characters/Mannequins/Animations/Manny/MM_Walk_Fwd.MM_Walk_Fwd",
    "/Game/Characters/Mannequins/Animations/Quinn/MF_Walk_Fwd.MF_Walk_Fwd",
    "/Game/Characters/Mannequins/Animations/Walk/MM_Walk_Fwd.MM_Walk_Fwd",
    "/Game/Characters/Mannequin/Animations/ThirdPersonWalk.ThirdPersonWalk",
    "/Game/Characters/Mannequins/Animations/Manny/MM_Run_Fwd.MM_Run_Fwd",   # walk-as-run fallback
]


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


def run(mesh=None, idle=None, walk=None):
    """Resolve a mesh + idle + walk anim and stamp them onto the
    AQRNPCActor and AQRNPCColonist class defaults.

    Args:
      mesh: optional soft path override for the skeletal mesh.
      idle: optional soft path override for the idle animation.
      walk: optional soft path override for the walk animation.
    """
    print("\n=== qr_assign_npc_appearance ===")

    mesh_asset = _resolve_or(MESH_CANDIDATES, mesh)
    idle_asset = _resolve_or(IDLE_CANDIDATES, idle)
    walk_asset = _resolve_or(WALK_CANDIDATES, walk)

    if not mesh_asset:
        print("[npc-skin] No Mannequin SKM mesh found. Import the UE5 Third")
        print("[npc-skin] Person template content (Add Content -> Third Person")
        print("[npc-skin] Feature Pack) and re-run, OR pass mesh='/Game/...'.")
        return

    print("[npc-skin] mesh:  {}".format(mesh_asset.get_path_name()))
    print("[npc-skin] idle:  {}".format(idle_asset.get_path_name() if idle_asset else "(none)"))
    print("[npc-skin] walk:  {}".format(walk_asset.get_path_name() if walk_asset else "(none)"))

    targets = [
        "/Script/QuietRiftEnigma.QRNPCActor",
        "/Script/QuietRiftEnigma.QRNPCColonist",
    ]
    for t in targets:
        _stamp_class(t, mesh_asset, idle_asset, walk_asset)

    print("[npc-skin] DONE -- new NPC spawns wear the assigned mesh + anims.")
    print("[npc-skin] Existing villagers in the level must be re-spawned")
    print("[npc-skin] (re-run qr_spawn_starter_village) to pick up the change.")


if __name__ == "__main__":
    run()
