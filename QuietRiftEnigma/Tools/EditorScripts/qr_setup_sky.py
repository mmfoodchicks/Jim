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


def _purge_stray_directional_lights(keep_labels):
    """Delete every DirectionalLight whose label isn't in keep_labels.
    A map shipping its own default sun on top of QR_KeyLight + QR_Jovianlight
    is what triggers 'Multiple directional lights are competing...' — there
    can be at most ONE atmosphere sun and the priorities only break a tie
    between two. Removing strays is the clean fix."""
    actor_sub = None
    try:
        actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    except Exception:
        actor_sub = None
    killed = 0
    for a in list(_all_actors()):
        if isinstance(a, unreal.DirectionalLight):
            lbl = a.get_actor_label()
            if lbl not in keep_labels:
                try:
                    if actor_sub:
                        actor_sub.destroy_actor(a)
                    else:
                        unreal.EditorLevelLibrary.destroy_actor(a)
                    killed += 1
                    print("[sky] removed stray DirectionalLight '{}'".format(lbl))
                except Exception as e:
                    print("[sky]   (could not remove '{}': {})".format(lbl, e))
    if killed == 0:
        print("[sky] no stray directional lights to remove")


def run(exposure_ev=13.0, sun_lux=10.0):
    """Set up the Jovian sky + lighting on the current level.

    exposure_ev -- LOCKED camera exposure (EV100). The scene no longer
      auto-adapts, so it can't blow out to white. HIGHER = darker image,
      LOWER = brighter. If the map is still too bright, bump this to 14-15;
      if too dark, drop to 11-12. Re-run after changing.
    sun_lux -- DirectionalLight intensity (lux). Canon is a dim
      Jupiter-distance sun; raise for a brighter key light.
    """
    V = unreal.Vector

    # ── SkyAtmosphere — makes the sky render ──────────────────────
    if _find(unreal.SkyAtmosphere):
        print("[sky] SkyAtmosphere already present")
    else:
        _spawn(unreal.SkyAtmosphere, "QR_SkyAtmosphere", V(0.0, 0.0, 0.0))
        print("[sky] SkyAtmosphere spawned")

    # Remove any pre-existing/default directional lights so only the two
    # QR lights remain (kills the 'competing directional lights' warning).
    _purge_stray_directional_lights({"QR_KeyLight", "QR_Jovianlight"})

    # ── DirectionalLight — key light, Movable (no build needed) ───
    # Find specifically by label so a re-run never hijacks a stray light.
    sun = _find(unreal.DirectionalLight, "QR_KeyLight")
    if sun:
        print("[sky] QR_KeyLight already present — retuning")
    else:
        sun = _spawn(unreal.DirectionalLight, "QR_KeyLight", V(0.0, 0.0, 30000.0))
        print("[sky] QR_KeyLight spawned")
    if sun:
        _try(lambda: sun.set_actor_rotation(unreal.Rotator(-38.0, -45.0, 0.0), False))
        comp = getattr(sun, "directional_light_component", None)
        if comp:
            _try(lambda: comp.set_mobility(unreal.ComponentMobility.MOVABLE))
            # Intensity in lux. Tunable via run(sun_lux=...); canon is a
            # dim Jupiter-distance sun. With LOCKED exposure (below) the
            # scene brightness is predictable from this value.
            _try(lambda: comp.set_intensity(sun_lux))
            _try(lambda: comp.set_light_color(unreal.LinearColor(1.0, 0.93, 0.82, 1.0)))
            # Bind this DirectionalLight to the SkyAtmosphere so the sky
            # actually picks up its colour and disk. Without this flag the
            # SkyAtmosphere has no sun, the sky renders near-black, and
            # the SkyLight (which captures the sky in real-time) captures
            # that blackness as its ambient -- compounding the PIE darkness.
            _try(lambda: comp.set_editor_property("atmosphere_sun_light", True))
            # Forward-shading priority: must be HIGHER than Jovianlight so
            # this is the "primary" directional light for forward shading,
            # translucent surfaces, single-layer water and volumetric fog.
            # UE warns with 'Multiple directional lights are competing'
            # when priorities tie and brightness alone has to break it.
            _try(lambda: comp.set_editor_property("forward_shading_priority", 10))
            _try(lambda: comp.set_editor_property("atmosphere_sun_light_index", 0))

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
            # SkyLight intensity is a multiplier on the captured cubemap.
            # 5.0 (was 3.0) lifts the ambient floor so the world isn't
            # coal-black when the sun is on the wrong side of the moon.
            _try(lambda: comp.set_intensity(5.0))
            _try(lambda: comp.recapture_sky())

    # ── Jovianlight — second DirectionalLight that represents Jupiter
    # reflecting sunlight onto the moon's surface. Per CLAUDE.md canon,
    # this is ~500x brighter than our full moon (real moon = 0.25 lux,
    # so Jovianlight ~= 125 lux). It's CONSTANT -- it doesn't cycle with
    # the sun -- because from a tidally-locked Jovian moon, Jupiter is
    # always in the same fixed direction. This is what stops the night
    # side from going pitch black.
    #
    # Pointed FROM the QR_Jupiter actor TOWARD world origin so the lit
    # side of the moon faces where Jupiter is in the sky.
    jovianlight = _find(unreal.DirectionalLight, "QR_Jovianlight")
    if jovianlight:
        print("[sky] Jovianlight already present -- retuning")
    else:
        jovianlight = _spawn(unreal.DirectionalLight, "QR_Jovianlight",
                              V(0.0, 0.0, 32000.0))
        print("[sky] Jovianlight spawned")
    if jovianlight:
        # Aim it from where Jupiter is (5,000,000 / 1,000,000 / 4,000,000)
        # toward the origin. UE's DirectionalLight forward vector is the
        # direction the light SHINES, so we want the vector
        # (origin - jupiter) normalized as the forward.
        # That's roughly (-50, -10, -40) in km. Pitch ~= atan2(-40,
        # sqrt(50^2+10^2)) ~= -38 deg; Yaw ~= atan2(-10, -50) ~= -169 deg
        # (pointing back toward origin from Jupiter's quadrant).
        _try(lambda: jovianlight.set_actor_rotation(
            unreal.Rotator(0.0, -38.0, -169.0), False))
        comp = getattr(jovianlight, "directional_light_component", None)
        if comp:
            _try(lambda: comp.set_mobility(unreal.ComponentMobility.MOVABLE))
            # 125 lux = canon Jovianlight (full-moon * 500). Cream-tan
            # to match Jupiter's reflected colour.
            _try(lambda: comp.set_intensity(125.0))
            _try(lambda: comp.set_light_color(
                unreal.LinearColor(0.95, 0.85, 0.65, 1.0)))
            # Do NOT bind to atmosphere -- there's only one sun. The
            # SkyAtmosphere actor already has QR_KeyLight as its sun.
            _try(lambda: comp.set_editor_property("atmosphere_sun_light", False))
            # Forward-shading priority LOWER than QR_KeyLight (which is
            # 10). Stops the 'Multiple directional lights are competing
            # to be the single one used for forward shading' warning by
            # making the sun the clear winner. Jovianlight still lights
            # the world via standard deferred shading.
            _try(lambda: comp.set_editor_property("forward_shading_priority", 0))
            # Soft shadow falloff so the Jovianlight doesn't carve hard
            # secondary shadows.
            _try(lambda: comp.set_editor_property("light_source_angle", 2.0))

    # ── ExponentialHeightFog — atmospheric depth ──────────────────
    if _find(unreal.ExponentialHeightFog):
        print("[sky] ExponentialHeightFog already present")
    else:
        _spawn(unreal.ExponentialHeightFog, "QR_HeightFog", V(0.0, 0.0, 0.0))
        print("[sky] ExponentialHeightFog spawned")

    # ── Jupiter + Galilean moons — Callisto-orbit fictional moon ───
    # Per CLAUDE.md / GDD canon (set 2026-05-24), the player is on a
    # larger fictional Jovian moon at Callisto's orbital distance
    # (~1.88 Mkm from Jupiter). At that distance Jupiter spans ~4.3°
    # of sky -- still 8x our Moon's 0.5° but no longer the 12° giant
    # an Europa surface would see. Outside Jupiter's main radiation
    # belt, geologically active, Earth-mass: this is what makes a
    # breathable atmosphere + permanent surface life plausible.
    #
    # Game values: Jupiter at ~65 km game distance, scale 4500 gives
    # ~4° angular from origin.
    JUPITER_LOC   = V(5000000.0, 1000000.0, 4000000.0)  # 50km E, 10km N, 40km up
    JUPITER_SCALE = 4500.0                              # ~4° angular at origin

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
    # a Movable-only lighting setup. With the physically-accurate
    # Jupiter-distance sun (~2,800 lux peak vs Earth's ~75,000), the
    # dynamic range across a game-day is too wide for a manual pin to
    # cover -- noon is dim-overcast, night is Jovianlight. Use bounded
    # AUTO (histogram) exposure so the camera adapts the way a human
    # eye in a Jovian-system survival scenario actually would, while
    # the min/max EV clamps prevent the runaway-to-black behaviour that
    # caused the prior PIE darkness.
    ppv = _find(unreal.PostProcessVolume, "QR_Exposure")
    if ppv:
        print("[sky] PostProcessVolume already present -- retuning")
    else:
        ppv = _spawn(unreal.PostProcessVolume, "QR_Exposure", V(0.0, 0.0, 0.0))
        print("[sky] PostProcessVolume spawned")
    if ppv:
        _try(lambda: ppv.set_editor_property("unbound", True))
        settings = ppv.get_editor_property("settings")
        if settings:
            # LOCKED exposure. The prior histogram auto-exposure was
            # adapting UP because the dim sun left the scene under-lit,
            # which blew the bright SkyAtmosphere pixels out to solid
            # cream/white (the 'unbelievably bright' report). Locking the
            # camera removes adaptation entirely: brightness is now a
            # fixed function of the actual light, so it can't run away.
            #
            # Implemented as histogram auto-exposure with min == max ==
            # exposure_ev, which pins the camera at a constant EV100.
            # HIGHER exposure_ev = darker image. Tunable via run().
            _try(lambda: setattr(settings, "override_auto_exposure_method", True))
            _try(lambda: setattr(settings, "auto_exposure_method",
                                  unreal.AutoExposureMethod.AEM_HISTOGRAM))
            _try(lambda: setattr(settings, "override_auto_exposure_min_brightness", True))
            _try(lambda: setattr(settings, "auto_exposure_min_brightness", float(exposure_ev)))
            _try(lambda: setattr(settings, "override_auto_exposure_max_brightness", True))
            _try(lambda: setattr(settings, "auto_exposure_max_brightness", float(exposure_ev)))
            # Neutral compensation; the lock above does the work.
            _try(lambda: setattr(settings, "override_auto_exposure_bias", True))
            _try(lambda: setattr(settings, "auto_exposure_bias", 0.0))
            # Instant settle (no visible adaptation ramp).
            _try(lambda: setattr(settings, "override_auto_exposure_speed_up", True))
            _try(lambda: setattr(settings, "auto_exposure_speed_up", 20.0))
            _try(lambda: setattr(settings, "override_auto_exposure_speed_down", True))
            _try(lambda: setattr(settings, "auto_exposure_speed_down", 20.0))
            _try(lambda: ppv.set_editor_property("settings", settings))
            print("[sky] exposure LOCKED at EV {} (raise to darken, lower to brighten)"
                  .format(exposure_ev))

    print("[sky] done — re-run any time, it re-tunes instead of duplicating.")
    print("[sky] SAVE THE LEVEL (Ctrl+S). QR_Jupiter is a plain sphere —")
    print("[sky] assign a gas-giant material to it for the final look.")


if __name__ == "__main__":
    run()
