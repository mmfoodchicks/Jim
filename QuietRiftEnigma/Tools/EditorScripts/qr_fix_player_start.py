"""
qr_fix_player_start.py — drop every PlayerStart onto the ground.

Line-traces straight down at each PlayerStart's X/Y and moves it to the
surface, lifted by SPAWN_HEIGHT so the player spawns standing on the
ground — no fall damage, no spawning buried. If the level has no
PlayerStart at all, one is created at the world origin.

Idempotent — safe to re-run any time the ground changes (notably after
importing the worldgen landscape, which shifts where "the ground" is).

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_fix_player_start.py').read())

The script does NOT save the level — press Ctrl+S afterwards to keep
the change.
"""

import unreal


# How far above the surface to place the PlayerStart, in cm. The player
# capsule's center sits here, so ~100 leaves the feet a few cm above the
# ground — a harmless settle, never fall damage.
SPAWN_HEIGHT = 100.0

# Down-trace reach (cm) above and below each PlayerStart. 1 km each way
# clears any terrain relief while staying well under sky geometry.
TRACE_SPAN = 100000.0


def _editor_world():
    return unreal.EditorLevelLibrary.get_editor_world()


def _all_level_actors():
    """Modern subsystem first, fall back to the older library."""
    try:
        sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if sub:
            return sub.get_all_level_actors()
    except Exception:
        pass
    return unreal.EditorLevelLibrary.get_all_level_actors()


def _ground_z(world, x, y, center_z):
    """Down-trace at (x, y); return the surface Z, or None if nothing hit."""
    start = unreal.Vector(x, y, center_z + TRACE_SPAN)
    end   = unreal.Vector(x, y, center_z - TRACE_SPAN)
    out = unreal.SystemLibrary.line_trace_single(
        world, start, end,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,   # Visibility channel
        False,                                     # trace_complex
        [],                                        # actors_to_ignore
        unreal.DrawDebugTrace.NONE,
        True)                                      # ignore_self
    # line_trace_single returns (bool, HitResult) — pull out the HitResult.
    hit = out
    if isinstance(out, (tuple, list)):
        hit = next((h for h in out if isinstance(h, unreal.HitResult)), None)
    if not isinstance(hit, unreal.HitResult):
        return None
    # FHitResult fields aren't reliably exposed as Python attributes —
    # break it with the Blueprint helper. Output order is stable:
    # index 0 is bBlockingHit, index 5 is ImpactPoint.
    broken = unreal.GameplayStatics.break_hit_result(hit)
    if not broken[0]:
        return None
    return float(broken[5].z)


def run():
    world = _editor_world()
    if not world:
        print("[playerstart] no editor world")
        return

    print("[playerstart] scanning level...")
    starts = [a for a in _all_level_actors()
              if isinstance(a, unreal.PlayerStart)]

    # No PlayerStart at all — create one at the world origin.
    if not starts:
        gz = _ground_z(world, 0.0, 0.0, 0.0)
        if gz is None:
            print("[playerstart] no PlayerStart, and no ground under the "
                  "origin to place one on.")
            print("[playerstart] open a level with a floor / landscape, "
                  "then re-run.")
            return
        loc = unreal.Vector(0.0, 0.0, gz + SPAWN_HEIGHT)
        ps = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.PlayerStart, loc, unreal.Rotator(0.0, 0.0, 0.0))
        if ps:
            print("[playerstart] none existed — created one at "
                  "({:.0f}, {:.0f}, {:.0f})".format(loc.x, loc.y, loc.z))
            print("[playerstart] SAVE THE LEVEL (Ctrl+S) to keep it.")
        else:
            print("[playerstart] failed to spawn a PlayerStart")
        return

    # Snap every existing PlayerStart down onto the ground.
    fixed = 0
    for ps in starts:
        loc = ps.get_actor_location()
        gz = _ground_z(world, loc.x, loc.y, loc.z)
        if gz is None:
            print("[playerstart] '{}' — no ground below its X/Y; left "
                  "as-is (move it over solid ground and re-run).".format(
                      ps.get_actor_label()))
            continue
        new_z = gz + SPAWN_HEIGHT
        ps.set_actor_location(
            unreal.Vector(loc.x, loc.y, new_z), False, True)
        print("[playerstart] '{}'  z {:.0f} -> {:.0f}  (ground {:.0f})".format(
            ps.get_actor_label(), loc.z, new_z, gz))
        fixed += 1

    print("[playerstart] done — {} of {} PlayerStart(s) snapped to ground"
          .format(fixed, len(starts)))
    print("[playerstart] SAVE THE LEVEL (Ctrl+S) to keep the change.")


if __name__ == "__main__":
    run()
