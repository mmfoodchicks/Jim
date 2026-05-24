"""
qr_setup_sky.py — Jovian-moon sky + lighting for the current level.

The test levels ship with no lighting, so editor fly-around and night
are pitch black. This sets up a sky that is always lit:

  - SkyAtmosphere        — makes the sky render
  - DirectionalLight     — the key light, Movable so it lights instantly
                           with no lighting build (warm, distant-sun)
  - SkyLight             — ambient fill, Movable + real-time capture;
                           this is what keeps the world from ever going
                           fully dark, including at night
  - ExponentialHeightFog — atmospheric depth
  - QR_Jupiter           — a large sphere high in the sky standing in for
                           the gas giant (assign a textured material for
                           the final look)

Idempotent — re-running re-tunes the existing actors instead of adding
duplicates. Does NOT save the level — press Ctrl+S afterward.

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_setup_sky.py').read())
"""

import unreal


def _all_actors():
    try:
        sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if sub:
            return sub.get_all_level_actors()
    except Exception:
        pass
    return unreal.EditorLevelLibrary.get_all_level_actors()


def _find(actor_class, label=None):
    for a in _all_actors():
        if isinstance(a, actor_class):
            if label is None or a.get_actor_label() == label:
                return a
    return None


def _spawn(actor_class, label, location):
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class, location, unreal.Rotator(0.0, 0.0, 0.0))
    if a:
        a.set_actor_label(label)
    return a


def _try(fn):
    """Run a tuning call; never let one bad property abort the setup."""
    try:
        fn()
    except Exception as e:
        print("[sky]   (tune skipped: {})".format(e))


def run():
    V = unreal.Vector

    # ── SkyAtmosphere — makes the sky render ──────────────────────
    if _find(unreal.SkyAtmosphere):
        print("[sky] SkyAtmosphere already present")
    else:
        _spawn(unreal.SkyAtmosphere, "QR_SkyAtmosphere", V(0.0, 0.0, 0.0))
        print("[sky] SkyAtmosphere spawned")

    # ── DirectionalLight — key light, Movable (no build needed) ───
    sun = _find(unreal.DirectionalLight)
    if sun:
        print("[sky] DirectionalLight already present — retuning")
    else:
        sun = _spawn(unreal.DirectionalLight, "QR_KeyLight", V(0.0, 0.0, 30000.0))
        print("[sky] DirectionalLight spawned")
    if sun:
        _try(lambda: sun.set_actor_rotation(unreal.Rotator(-38.0, -45.0, 0.0), False))
        comp = getattr(sun, "directional_light_component", None)
        if comp:
            _try(lambda: comp.set_mobility(unreal.ComponentMobility.MOVABLE))
            # 10.0 is the UE 5.x DirectionalLight default in lux units --
            # 4.0 was the prior value and read as 'night' under PIE's
            # auto-exposure, leaving the scene black even with SkyLight on.
            _try(lambda: comp.set_intensity(10.0))
            _try(lambda: comp.set_light_color(unreal.LinearColor(1.0, 0.93, 0.82, 1.0)))
            # Bind this DirectionalLight to the SkyAtmosphere so the sky
            # actually picks up its colour and disk. Without this flag the
            # SkyAtmosphere has no sun, the sky renders near-black, and
            # the SkyLight (which captures the sky in real-time) captures
            # that blackness as its ambient -- compounding the PIE darkness.
            _try(lambda: comp.set_editor_property("atmosphere_sun_light", True))

    # ── SkyLight — ambient fill; keeps the world from going black ─
    skyl = _find(unreal.SkyLight)
    if skyl:
        print("[sky] SkyLight already present — retuning")
    else:
        skyl = _spawn(unreal.SkyLight, "QR_SkyLight", V(0.0, 0.0, 30000.0))
        print("[sky] SkyLight spawned")
    if skyl:
        comp = getattr(skyl, "light_component", None)
        if comp:
            _try(lambda: comp.set_mobility(unreal.ComponentMobility.MOVABLE))
            _try(lambda: comp.set_editor_property("real_time_capture", True))
            _try(lambda: comp.set_editor_property("lower_hemisphere_is_black", False))
            _try(lambda: comp.set_intensity(3.0))
            _try(lambda: comp.recapture_sky())

    # ── ExponentialHeightFog — atmospheric depth ──────────────────
    if _find(unreal.ExponentialHeightFog):
        print("[sky] ExponentialHeightFog already present")
    else:
        _spawn(unreal.ExponentialHeightFog, "QR_HeightFog", V(0.0, 0.0, 0.0))
        print("[sky] ExponentialHeightFog spawned")

    # ── Jupiter — big sphere high in the sky (placeholder) ────────
    jupiter = _find(unreal.StaticMeshActor, "QR_Jupiter")
    if jupiter:
        print("[sky] Jupiter already present — retuning")
    else:
        jupiter = _spawn(unreal.StaticMeshActor, "QR_Jupiter", V(90000.0, 45000.0, 70000.0))
        print("[sky] Jupiter spawned")
    if jupiter:
        comp = getattr(jupiter, "static_mesh_component", None)
        sphere = unreal.load_object(None, "/Engine/BasicShapes/Sphere.Sphere")
        if comp and sphere:
            _try(lambda: comp.set_mobility(unreal.ComponentMobility.MOVABLE))
            _try(lambda: comp.set_static_mesh(sphere))
            _try(lambda: comp.set_cast_shadow(False))
        _try(lambda: jupiter.set_actor_location(V(90000.0, 45000.0, 70000.0), False, False))
        _try(lambda: jupiter.set_actor_scale3d(V(350.0, 350.0, 350.0)))

    # ── PostProcessVolume — pin manual exposure ───────────────────
    # PIE's default auto-exposure can drag the scene to near-black on
    # a Movable-only lighting setup. Pin it to a fixed manual value so
    # what you see in the editor viewport is what you get in Play.
    # Unbound=True means the volume applies everywhere, no need to
    # contain the player inside its bounds.
    ppv = _find(unreal.PostProcessVolume, "QR_Exposure")
    if ppv:
        print("[sky] PostProcessVolume already present — retuning")
    else:
        ppv = _spawn(unreal.PostProcessVolume, "QR_Exposure", V(0.0, 0.0, 0.0))
        print("[sky] PostProcessVolume spawned")
    if ppv:
        _try(lambda: ppv.set_editor_property("unbound", True))
        settings = ppv.get_editor_property("settings")
        if settings:
            _try(lambda: setattr(settings, "override_auto_exposure_method", True))
            _try(lambda: setattr(settings, "auto_exposure_method",
                                  unreal.AutoExposureMethod.AEM_MANUAL))
            _try(lambda: setattr(settings, "override_auto_exposure_bias", True))
            # auto_exposure_bias is logarithmic: 0 = neutral, 1 = 2x brighter.
            # 1.0 gives a daylit outdoor reading; raise to 2.0 if still dim.
            _try(lambda: setattr(settings, "auto_exposure_bias", 1.0))
            _try(lambda: ppv.set_editor_property("settings", settings))

    print("[sky] done — re-run any time, it re-tunes instead of duplicating.")
    print("[sky] SAVE THE LEVEL (Ctrl+S). QR_Jupiter is a plain sphere —")
    print("[sky] assign a gas-giant material to it for the final look.")


if __name__ == "__main__":
    run()
