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


def _ensure_material(name, base_color, emissive_color,
                     package_path="/Game/QuietRift/Materials/Sky"):
    """Create a flat-color emissive Material asset if it doesn't already
    exist. Returns the package path of the material for assignment.

    Verified Python API (UE 5.x):
      - AssetTools.create_asset with MaterialFactoryNew
      - MaterialEditingLibrary.create_material_expression(material,
            expression_class, x, y)
      - Constant3Vector.set_editor_property('constant', LinearColor)
      - MaterialEditingLibrary.connect_material_property(expr, '',
            MaterialProperty.MP_BASE_COLOR / MP_EMISSIVE_COLOR)
    """
    full_path = "{}/{}".format(package_path, name)
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        return full_path
    if not unreal.EditorAssetLibrary.does_directory_exist(package_path):
        unreal.EditorAssetLibrary.make_directory(package_path)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialFactoryNew()
    mat = asset_tools.create_asset(name, package_path, unreal.Material, factory)
    if not mat:
        print("[sky]   (material create failed: {})".format(full_path))
        return None

    # Base color expression.
    color_expr = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionConstant3Vector, -300, 0)
    if color_expr:
        color_expr.set_editor_property('constant', base_color)
        unreal.MaterialEditingLibrary.connect_material_property(
            color_expr, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Emissive so the body stays visible when the directional light is
    # on the wrong side (real moons are lit by Jupiter's reflected light
    # at "night"; we approximate that with a flat low-intensity emissive).
    emi_expr = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionConstant3Vector, -300, 200)
    if emi_expr:
        emi_expr.set_editor_property('constant', emissive_color)
        unreal.MaterialEditingLibrary.connect_material_property(
            emi_expr, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(full_path)
    print("[sky]   created material: {}".format(full_path))
    return full_path


def _ensure_celestial(label, mesh_path, location, scale, material_path=None):
    """Spawn-or-retune a StaticMeshActor representing a celestial body
    (Jupiter, a Galilean moon, etc). Idempotent."""
    actor = _find(unreal.StaticMeshActor, label)
    if actor:
        print("[sky] {} already present -- retuning".format(label))
    else:
        actor = _spawn(unreal.StaticMeshActor, label, location)
        print("[sky] {} spawned".format(label))
    if not actor:
        return None
    comp = getattr(actor, "static_mesh_component", None)
    mesh = unreal.load_object(None, mesh_path) if mesh_path else None
    if comp and mesh:
        _try(lambda: comp.set_mobility(unreal.ComponentMobility.MOVABLE))
        _try(lambda: comp.set_static_mesh(mesh))
        _try(lambda: comp.set_cast_shadow(False))
        if material_path:
            mat_obj = unreal.load_asset(material_path)
            if mat_obj:
                _try(lambda: comp.set_material(0, mat_obj))
    _try(lambda: actor.set_actor_location(location, False, False))
    _try(lambda: actor.set_actor_scale3d(unreal.Vector(scale, scale, scale)))
    return actor


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

    # ── Jupiter + Galilean moons — scaled for a Europa-surface view ─
    # Real geometry: from Europa (671,100 km from Jupiter), Jupiter
    # spans ~12° of sky (24x our Moon's 0.5°). The Galileans appear as
    # small but resolvable disks (Io ~0.3°, Ganymede/Callisto ~0.3°).
    #
    # Game values chosen so Jupiter reads at ~12° from the world origin:
    # placed at ~65 km game distance with scale 7000 (radius ~7 km).
    # Moon orbital radii preserve real ratios to Jupiter's body radius
    # (~Io 6 RJ, Europa 9 RJ, Ganymede 15 RJ, Callisto 26 RJ).
    JUPITER_LOC   = V(5000000.0, 1000000.0, 4000000.0)  # 50km E, 10km N, 40km up
    JUPITER_SCALE = 7000.0                              # ~12° angular at origin

    _ensure_celestial(
        label="QR_Jupiter",
        mesh_path="/Engine/BasicShapes/Sphere.Sphere",
        location=JUPITER_LOC,
        scale=JUPITER_SCALE,
        material_path=_ensure_material(
            "M_QR_Jupiter",
            unreal.LinearColor(0.88, 0.78, 0.62, 1.0),   # cream-tan gas-giant bands
            unreal.LinearColor(0.30, 0.25, 0.18, 1.0)),  # subtle emissive so it's lit at night
    )

    # Galilean moons. Orbital radii in cm; QRSkyManager Tick keeps them
    # circling Jupiter. Scales are real-proportion to Jupiter (Io is
    # ~2.6% of Jupiter's diameter, etc) so the size relationships are
    # scientifically accurate.
    galileans = [
        # label,             scale, base color (RGB),               emissive
        ("QR_Moon_Io",        200, (0.95, 0.85, 0.55), (0.40, 0.32, 0.18)),  # sulfur-yellow volcanic
        ("QR_Moon_Europa",    170, (0.95, 0.92, 0.88), (0.40, 0.38, 0.35)),  # icy white-cream
        ("QR_Moon_Ganymede",  300, (0.62, 0.58, 0.50), (0.24, 0.22, 0.18)),  # grey-brown mottled
        ("QR_Moon_Callisto",  270, (0.42, 0.40, 0.34), (0.16, 0.14, 0.12)),  # dark cratered
    ]
    # Initial positions are arbitrary -- QRSkyManager overwrites them
    # every tick from the orbit math. Just spawn them near Jupiter so
    # they're not at world-origin if the manager isn't ticking yet.
    for i, (label, scale, base_rgb, emi_rgb) in enumerate(galileans):
        offset = V(800000.0 + i * 600000.0, 0.0, 0.0)
        init_loc = V(JUPITER_LOC.x + offset.x,
                     JUPITER_LOC.y + offset.y,
                     JUPITER_LOC.z + offset.z)
        _ensure_celestial(
            label=label,
            mesh_path="/Engine/BasicShapes/Sphere.Sphere",
            location=init_loc,
            scale=float(scale),
            material_path=_ensure_material(
                "M_" + label,
                unreal.LinearColor(base_rgb[0], base_rgb[1], base_rgb[2], 1.0),
                unreal.LinearColor(emi_rgb[0],  emi_rgb[1],  emi_rgb[2],  1.0)),
        )

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
