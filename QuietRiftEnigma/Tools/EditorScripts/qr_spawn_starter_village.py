"""
qr_spawn_starter_village.py -- drop a handful of AQRNPCActor instances
into the open dev map so the player walks into a populated colony
instead of an empty bowl.

Each spawned NPC carries the default Brain + Civilian Reaction +
Dialogue + Faction subobjects, so they wander, work in daylight, sleep
at night, socialize, and flee/hide/fight when raids land. Names + work
posts are deterministic per index so they don't shuffle between runs.

Idempotent -- re-running tears down the previous QR_Village_* actors
before placing a fresh set.

Run from the UE Python console with the dev map open:
  exec(open(r'D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts\\qr_spawn_starter_village.py').read())
  run()                     # 8 colonists in a ring around origin
  run(count=20, radius_m=80) # bigger settlement
"""

import math
import os
import unreal


VILLAGE_LABEL_PREFIX = "QR_Village_"


def _run_sibling(script_name, fn_name="run"):
    """exec another qr_*.py from the same folder."""
    here = os.path.dirname(os.path.abspath(__file__)) \
        if "__file__" in globals() else \
        os.path.normpath(os.path.join(unreal.Paths.project_dir(), "..", "Tools", "EditorScripts"))
    path = os.path.join(here, script_name)
    if not os.path.isfile(path): return
    g = {"__name__": "__qr_sub__", "__file__": path}
    exec(open(path).read(), g)
    fn = g.get(fn_name)
    if callable(fn): fn()

# Designer can mix-in real names later via DialogueTable; for now a
# small rolodex so the village reads as a place, not a clone army.
DEFAULT_NAMES = [
    "Iris Kowalski",    "Mara Sundholm",  "Theo Beckford",  "June Halpern",
    "Vance Ohira",      "Sela Voss",      "Cory Wren",      "Adrian Kells",
    "Pria Vasquez",     "Kit Renwood",    "Niall Otway",    "Brynn Halls",
    "Zev Conlan",       "Saskia Lir",     "Mira Tanaka",    "Quin Asher",
    "Dell Bartow",      "Eun-Ji Park",    "Roan Halverson", "Faye Atalay",
]

# Rotating role pool so the spawn is a real colony, not 8 unassigned
# wanderers. EQRNPCRole exposes these as UPPERCASE in Python. (No
# HUNTER role exists in the enum -- Scout is the field/recon job.)
ROLE_POOL = ["FARMER", "ENGINEER", "SCOUT", "COOK", "MEDIC", "GUARD",
             "FARMER", "BUILDER"]


def _role_enum(name):
    enum = getattr(unreal, "QRNPCRole", None)
    if not enum: return 0
    return getattr(enum, name, 0)


def _editor_world():
    sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return sub.get_editor_world() if sub else None


def _actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _level_actors():
    sub = _actor_subsystem()
    if sub:
        return sub.get_all_level_actors() or []
    try:
        return unreal.EditorLevelLibrary.get_all_level_actors() or []
    except Exception:
        return []


def _spawn(cls, loc, rot=None):
    sub = _actor_subsystem()
    r = rot or unreal.Rotator(0, 0, 0)
    if sub:
        return sub.spawn_actor_from_class(cls, loc, r)
    try:
        return unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, r)
    except Exception:
        return None


def _destroy(actor):
    if not actor: return
    sub = _actor_subsystem()
    if sub: sub.destroy_actor(actor); return
    try: unreal.EditorLevelLibrary.destroy_actor(actor)
    except Exception:
        actor.destroy_actor()


def _wipe_previous():
    removed = 0
    for a in _level_actors():
        if not a:
            continue
        try:
            label = a.get_actor_label() or ""
        except Exception:
            continue
        if label.startswith(VILLAGE_LABEL_PREFIX):
            _destroy(a)
            removed += 1
    if removed:
        print("[village] wiped {} prior villagers".format(removed))


def run(count=8, radius_m=40.0, center=(0.0, 0.0)):
    """Spawn `count` colonists around `center` (cm). Each gets a unique
    name from DEFAULT_NAMES (recycled past index 20), a wander home at
    spawn, a work post offset 6m north of home, a bed 6m south.

    Args:
      count:    number of villagers (1-30 sensible).
      radius_m: radius of the spawn ring around center.
      center:   (x, y) in cm. Defaults to origin.
    """
    print("\n=== qr_spawn_starter_village ===")
    world = _editor_world()
    if not world:
        print("[village] no editor world -- open a map first")
        return

    # Prefer the role-aware Colonist subclass; fall back to bare NPC.
    colonist_cls = getattr(unreal, "QRNPCColonist", None)
    npc_cls      = colonist_cls or getattr(unreal, "QRNPCActor", None)
    if npc_cls is None:
        print("[village] AQRNPCActor unavailable -- recompile C++")
        return

    _wipe_previous()
    # Make sure the NPC class defaults have a mesh + idle/walk anim
    # before we spawn anyone -- otherwise the village is invisible /
    # T-pose-static. Idempotent; cheap re-run.
    _run_sibling("qr_assign_npc_appearance.py")
    radius_cm = radius_m * 100.0

    placed = 0
    for i in range(count):
        angle = (i / float(max(1, count))) * 2.0 * math.pi
        x = center[0] + math.cos(angle) * radius_cm
        y = center[1] + math.sin(angle) * radius_cm
        npc = _spawn(npc_cls, unreal.Vector(x, y, 100.0))
        if not npc:
            continue
        name = DEFAULT_NAMES[i % len(DEFAULT_NAMES)]
        role = ROLE_POOL[i % len(ROLE_POOL)]
        try:
            npc.set_actor_label("{}{:02d}_{}_{}".format(
                VILLAGE_LABEL_PREFIX, i, role.lower(), name.replace(" ", "_")))
            npc.set_editor_property("display_name", unreal.Text(name))
            if colonist_cls:
                npc.set_editor_property("colonist_role", _role_enum(role))
        except Exception:
            pass

        # Give each NPC a non-zero wander home + post + bed so the brain
        # plays out a real schedule even without designer wiring. The
        # Colonist subclass overrides assigned_work_post on BeginPlay
        # with the nearest matching station -- the values set here are
        # safe defaults until BeginPlay runs.
        try:
            brain = npc.get_editor_property("brain")
            if brain:
                home = unreal.Vector(x, y, 100.0)
                brain.set_editor_property("home_position", home)
                brain.set_editor_property("assigned_work_post",
                    unreal.Vector(x + 600.0, y, 100.0))
                brain.set_editor_property("assigned_bed",
                    unreal.Vector(x - 600.0, y, 100.0))
        except Exception as e:
            print("[village]   brain wiring skipped on #{}: {}".format(i, e))
        placed += 1

    print("[village] placed {} colonists around ({:.0f},{:.0f})".format(
        placed, center[0], center[1]))

    # Colony dogs. The species class carries its own mesh + anims
    # (German_Shepherd_3D_Model pack), so they walk out of the spawn
    # skinned and animated with zero extra wiring.
    dog_cls = getattr(unreal, "QRWildlife_ColonyDog", None)
    if dog_cls:
        dogs = max(1, count // 6)
        for i in range(dogs):
            angle = (i / float(dogs)) * 2.0 * math.pi + 0.4
            x = center[0] + math.cos(angle) * radius_cm * 0.5
            y = center[1] + math.sin(angle) * radius_cm * 0.5
            dog = _spawn(dog_cls, unreal.Vector(x, y, 100.0))
            if dog:
                dog.set_actor_label("{}dog_{:02d}".format(VILLAGE_LABEL_PREFIX, i))
        print("[village] released {} colony dog(s)".format(dogs))
    else:
        print("[village] QRWildlife_ColonyDog unavailable -- recompile C++ for dogs")

    print("[village] DONE -- save the level to keep them.")


if __name__ == "__main__":
    run()
