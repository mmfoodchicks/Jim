"""
Quiet Rift: Enigma -- Photoreal finishing toolkit (Blender 4.x/5.x).

The original pipeline shipped flat-colour Principled materials, which
read as untextured plastic in UE. This module is the fidelity layer
used by the overhaul pass (qr_photoreal_overhaul.py) and the rebuilt
tree/rock generators:

  pbr_material(name, kind)   full procedural Cycles node graph per
                             surface family (bark, rock, worn metal,
                             plank wood, animal hide, soil, alien leaf,
                             crystal, fabric). Noise-driven albedo
                             variation, roughness maps, bump detail,
                             pointiness edge-wear on metals, cavity
                             dirt on rock.
  refine(obj, ...)           subdivision + multi-octave noise
                             displacement for organic silhouettes.
                             Hard-surface assets skip displacement and
                             keep their machined edges.
  bake_pbr(obj, name, dir)   flattens the procedural shading into
                             BaseColor / Roughness / Normal PNG maps
                             (UV atlas per asset) so the result works
                             in ANY engine renderer -- UE imports the
                             PNGs and the QR material library wires
                             them into instances.

Everything is deterministic (fixed noise seeds/locations) so re-runs
produce identical maps.
"""

import bpy
import math
import os


# ─── Node helpers ────────────────────────────────────────────────────

def _new_mat(name):
    mat = bpy.data.materials.get(name)
    if mat:
        return mat, None
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    return mat, bsdf


def _n(nt, kind, x, y, **props):
    node = nt.nodes.new(kind)
    node.location = (x, y)
    for k, v in props.items():
        setattr(node, k, v)
    return node


def _ramp(nt, x, y, stops):
    """stops = [(pos, (r,g,b,a)), ...]"""
    node = _n(nt, "ShaderNodeValToRGB", x, y)
    elems = node.color_ramp.elements
    while len(elems) > len(stops):
        elems.remove(elems[-1])
    while len(elems) < len(stops):
        elems.new(0.5)
    for elem, (pos, col) in zip(elems, stops):
        elem.position = pos
        elem.color = col
    return node


def _bsdf_in(bsdf, *names):
    """First existing input socket -- Blender 4/5 renamed several."""
    for nm in names:
        if nm in bsdf.inputs:
            return bsdf.inputs[nm]
    return None


# ─── Procedural PBR families ─────────────────────────────────────────

def pbr_material(name, kind, tint=(0.5, 0.5, 0.5), tint2=None,
                 rough=0.7, emissive=None, emissive_strength=2.0,
                 scale=1.0):
    """Build (or fetch) a full procedural PBR material.

    kind: bark | rock | metal_worn | wood_planks | hide | soil |
          leaf_alien | crystal | fabric | flat
    tint/tint2: primary/secondary albedo; tint2 defaults to a darker
    tint. scale multiplies all noise frequencies (bigger asset ->
    smaller scale so detail stays real-world sized)."""
    mat, bsdf = _new_mat(name)
    if bsdf is None:          # already built on a prior call
        return mat
    nt = mat.node_tree
    t2 = tint2 or tuple(c * 0.45 for c in tint)
    c1 = (tint[0], tint[1], tint[2], 1.0)
    c2 = (t2[0], t2[1], t2[2], 1.0)

    if kind == "flat":
        bsdf.inputs["Base Color"].default_value = c1
        bsdf.inputs["Roughness"].default_value = rough
        return mat

    coord = _n(nt, "ShaderNodeTexCoord", -1200, 0)

    if kind in ("bark", "wood_planks"):
        wave = _n(nt, "ShaderNodeTexWave", -900, 100)
        wave.inputs["Scale"].default_value = (2.2 if kind == "bark" else 4.5) * scale
        wave.inputs["Distortion"].default_value = 6.0 if kind == "bark" else 2.4
        wave.inputs["Detail"].default_value = 3.0
        if kind == "bark":
            wave.wave_type = 'BANDS'
            wave.bands_direction = 'Z'
        nt.links.new(coord.outputs["Object"], wave.inputs["Vector"])
        noise = _n(nt, "ShaderNodeTexNoise", -900, -200)
        noise.inputs["Scale"].default_value = 14.0 * scale
        noise.inputs["Detail"].default_value = 8.0
        nt.links.new(coord.outputs["Object"], noise.inputs["Vector"])
        mixf = _n(nt, "ShaderNodeMath", -650, 0, operation='MULTIPLY')
        nt.links.new(wave.outputs["Fac"], mixf.inputs[0])
        nt.links.new(noise.outputs["Fac"], mixf.inputs[1])
        ramp = _ramp(nt, -450, 100, [(0.05, c2), (0.55, c1),
                                     (0.95, tuple(min(1, c * 1.25) for c in tint) + (1.0,))])
        nt.links.new(mixf.outputs[0], ramp.inputs["Fac"])
        nt.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
        rramp = _ramp(nt, -450, -250, [(0.0, (rough * 1.15,) * 3 + (1,)),
                                       (1.0, (rough * 0.75,) * 3 + (1,))])
        nt.links.new(noise.outputs["Fac"], rramp.inputs["Fac"])
        nt.links.new(rramp.outputs["Color"], bsdf.inputs["Roughness"])
        bump = _n(nt, "ShaderNodeBump", -250, -450)
        bump.inputs["Strength"].default_value = 0.6 if kind == "bark" else 0.25
        nt.links.new(mixf.outputs[0], bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

    elif kind in ("rock", "soil"):
        vor = _n(nt, "ShaderNodeTexVoronoi", -900, 150)
        vor.inputs["Scale"].default_value = (3.5 if kind == "rock" else 9.0) * scale
        nt.links.new(coord.outputs["Object"], vor.inputs["Vector"])
        noise = _n(nt, "ShaderNodeTexNoise", -900, -150)
        noise.inputs["Scale"].default_value = 11.0 * scale
        noise.inputs["Detail"].default_value = 10.0
        noise.inputs["Roughness"].default_value = 0.7
        nt.links.new(coord.outputs["Object"], noise.inputs["Vector"])
        blend = _n(nt, "ShaderNodeMix", -650, 50, data_type='FLOAT')
        blend.inputs[0].default_value = 0.5
        nt.links.new(vor.outputs["Distance"], blend.inputs[2])
        nt.links.new(noise.outputs["Fac"], blend.inputs[3])
        # Cavity dirt: geometry pointiness darkens crevices.
        geo = _n(nt, "ShaderNodeNewGeometry", -900, 400)
        cav = _ramp(nt, -650, 400, [(0.35, (0.35, 0.30, 0.25, 1)),
                                    (0.55, (1, 1, 1, 1))])
        nt.links.new(geo.outputs["Pointiness"], cav.inputs["Fac"])
        ramp = _ramp(nt, -450, 50, [(0.05, tuple(c * 0.30 for c in tint) + (1.0,)),
                                    (0.35, c2), (0.65, c1),
                                    (0.92, tuple(min(1, c * 1.15) for c in tint) + (1.0,))])
        nt.links.new(blend.outputs[0], ramp.inputs["Fac"])
        tintmix = _n(nt, "ShaderNodeMix", -220, 150, data_type='RGBA',
                     blend_type='MULTIPLY')
        tintmix.inputs[0].default_value = 1.0
        nt.links.new(ramp.outputs["Color"], tintmix.inputs[6])
        nt.links.new(cav.outputs["Color"], tintmix.inputs[7])
        nt.links.new(tintmix.outputs[2], bsdf.inputs["Base Color"])
        rramp = _ramp(nt, -450, -250, [(0.0, (0.95,) * 3 + (1,)),
                                       (1.0, (0.65,) * 3 + (1,))])
        nt.links.new(noise.outputs["Fac"], rramp.inputs["Fac"])
        nt.links.new(rramp.outputs["Color"], bsdf.inputs["Roughness"])
        bump = _n(nt, "ShaderNodeBump", -250, -450)
        bump.inputs["Strength"].default_value = 0.8 if kind == "rock" else 0.35
        nt.links.new(blend.outputs[0], bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

    elif kind == "metal_worn":
        bsdf.inputs["Metallic"].default_value = 1.0
        geo = _n(nt, "ShaderNodeNewGeometry", -900, 300)
        wear = _ramp(nt, -650, 300, [(0.45, (0, 0, 0, 1)), (0.62, (1, 1, 1, 1))])
        nt.links.new(geo.outputs["Pointiness"], wear.inputs["Fac"])
        noise = _n(nt, "ShaderNodeTexNoise", -900, -100)
        noise.inputs["Scale"].default_value = 22.0 * scale
        noise.inputs["Detail"].default_value = 6.0
        nt.links.new(coord.outputs["Object"], noise.inputs["Vector"])
        grime = _ramp(nt, -650, -100, [(0.35, c2), (0.6, c1)])
        nt.links.new(noise.outputs["Fac"], grime.inputs["Fac"])
        edge = _n(nt, "ShaderNodeMix", -350, 150, data_type='RGBA')
        nt.links.new(wear.outputs["Color"], edge.inputs[0])
        nt.links.new(grime.outputs["Color"], edge.inputs[6])
        edge.inputs[7].default_value = tuple(min(1, c * 1.9) for c in tint) + (1.0,)
        nt.links.new(edge.outputs[2], bsdf.inputs["Base Color"])
        rmix = _n(nt, "ShaderNodeMix", -350, -200, data_type='FLOAT')
        nt.links.new(wear.outputs["Color"], rmix.inputs[0])
        rmix.inputs[2].default_value = rough
        rmix.inputs[3].default_value = max(0.08, rough * 0.35)
        nt.links.new(rmix.outputs[0], bsdf.inputs["Roughness"])
        bump = _n(nt, "ShaderNodeBump", -250, -450)
        bump.inputs["Strength"].default_value = 0.10
        nt.links.new(noise.outputs["Fac"], bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

    elif kind == "hide":
        noise = _n(nt, "ShaderNodeTexNoise", -900, 100)
        noise.inputs["Scale"].default_value = 8.0 * scale
        noise.inputs["Detail"].default_value = 9.0
        nt.links.new(coord.outputs["Object"], noise.inputs["Vector"])
        fine = _n(nt, "ShaderNodeTexVoronoi", -900, -200)
        fine.inputs["Scale"].default_value = 55.0 * scale   # skin cells
        nt.links.new(coord.outputs["Object"], fine.inputs["Vector"])
        ramp = _ramp(nt, -600, 100, [(0.2, c2), (0.55, c1),
                                     (0.9, tuple(min(1, c * 1.18) for c in tint) + (1.0,))])
        nt.links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
        nt.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = rough
        bump = _n(nt, "ShaderNodeBump", -250, -450)
        bump.inputs["Strength"].default_value = 0.22
        nt.links.new(fine.outputs["Distance"], bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

    elif kind in ("leaf_alien", "crystal"):
        noise = _n(nt, "ShaderNodeTexNoise", -900, 100)
        noise.inputs["Scale"].default_value = (5.0 if kind == "leaf_alien" else 3.0) * scale
        noise.inputs["Detail"].default_value = 6.0
        nt.links.new(coord.outputs["Object"], noise.inputs["Vector"])
        ramp = _ramp(nt, -600, 100, [(0.25, c2), (0.75, c1)])
        nt.links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
        nt.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.35 if kind == "leaf_alien" else 0.08
        tr = _bsdf_in(bsdf, "Transmission Weight", "Transmission")
        if tr and kind == "crystal":
            tr.default_value = 0.55
        if emissive:
            em = _bsdf_in(bsdf, "Emission Color", "Emission")
            es = _bsdf_in(bsdf, "Emission Strength")
            if em:
                # Veins only: high-contrast noise mask drives emission.
                vein = _ramp(nt, -600, -250,
                             [(0.47, (0, 0, 0, 1)), (0.53, emissive + (1.0,))])
                nt.links.new(noise.outputs["Fac"], vein.inputs["Fac"])
                nt.links.new(vein.outputs["Color"], em)
                if es:
                    es.default_value = emissive_strength
        sss = _bsdf_in(bsdf, "Subsurface Weight", "Subsurface")
        if sss and kind == "leaf_alien":
            sss.default_value = 0.25

    elif kind == "fabric":
        wave = _n(nt, "ShaderNodeTexWave", -900, 100)
        wave.inputs["Scale"].default_value = 90.0 * scale
        nt.links.new(coord.outputs["Object"], wave.inputs["Vector"])
        ramp = _ramp(nt, -600, 100, [(0.3, c2), (0.7, c1)])
        nt.links.new(wave.outputs["Fac"], ramp.inputs["Fac"])
        nt.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.92
        bump = _n(nt, "ShaderNodeBump", -250, -450)
        bump.inputs["Strength"].default_value = 0.15
        nt.links.new(wave.outputs["Fac"], bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

    return mat


# ─── Geometry refinement ─────────────────────────────────────────────

def _displace_tex(kind, scale):
    """Deterministic shared displacement texture. 'angular' = cellular
    voronoi (rock facets); 'puff' = musgrave (organic swell); 'fine' =
    high-octave noise (surface grain)."""
    name = "QR_Disp_{}_{:.2f}".format(kind, scale)
    tex = bpy.data.textures.get(name)
    if tex:
        return tex
    if kind == "angular":
        tex = bpy.data.textures.new(name, 'VORONOI')
        tex.distance_metric = 'DISTANCE_SQUARED'
        tex.noise_scale = scale
        tex.contrast = 1.4
    elif kind == "fine":
        tex = bpy.data.textures.new(name, 'CLOUDS')
        tex.noise_scale = scale
        tex.noise_depth = 6
    else:
        tex = bpy.data.textures.new(name, 'MUSGRAVE')
        tex.noise_scale = scale
        tex.octaves = 6.0
    return tex


def clean_mesh(obj):
    """Remove the degenerate geometry that displacement/subdivision can
    leave behind -- the source of UE's 'nearly zero tangents/normals'
    and 'degenerate tangent bases' import warnings. Merges coincident
    verts, dissolves zero-area faces, and recomputes consistent
    outward normals. Safe on any mesh; no-op if already clean."""
    if obj is None or obj.type != 'MESH':
        return obj
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.remove_doubles(threshold=0.0001)
    # Dissolve degenerate (zero-area/zero-length) elements.
    try:
        bpy.ops.mesh.dissolve_degenerate(threshold=0.0002)
    except Exception:
        pass
    bpy.ops.mesh.delete_loose()
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    return obj


def refine(obj, subdiv=2, displace=None, smooth_angle=40.0):
    """Subdivision + layered displacement, applied destructively, then
    a degenerate-geometry cleanup so the export imports without tangent
    warnings.

    displace: list of (kind, scale, strength_m) layers, e.g. a rock is
        [("angular", 0.9, 0.30), ("fine", 0.18, 0.04)]
    Legacy single-layer calls can pass displace=[("puff", s, m)].
    Hard-surface assets pass displace=None and keep machined edges."""
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)

    if subdiv > 0:
        mod = obj.modifiers.new("QR_Subsurf", 'SUBSURF')
        mod.levels = subdiv
        mod.render_levels = subdiv

    for i, (kind, scale, strength) in enumerate(displace or []):
        if strength <= 0.0:
            continue
        mod = obj.modifiers.new("QR_Displace{}".format(i), 'DISPLACE')
        mod.texture = _displace_tex(kind, scale)
        mod.strength = strength
        mod.texture_coords = 'GLOBAL'

    for mod in list(obj.modifiers):
        try:
            bpy.ops.object.modifier_apply(modifier=mod.name)
        except Exception as e:
            print("  refine: apply {} failed: {}".format(mod.name, e))

    clean_mesh(obj)

    try:
        bpy.ops.object.shade_smooth_by_angle(angle=math.radians(smooth_angle))
    except Exception:
        bpy.ops.object.shade_smooth()
    return obj


# ─── Texture baking ──────────────────────────────────────────────────

BAKE_PASSES = (
    ("D", "DIFFUSE", "sRGB"),
    ("R", "ROUGHNESS", "Non-Color"),
    ("N", "NORMAL", "Non-Color"),
)


def bake_pbr(obj, name, out_dir, size=1024, samples=8, margin=8):
    """Bake the object's procedural shading to T_<name>_{D,R,N}.png and
    replace its materials with ONE baked Principled ('<name>_Baked') so
    the exported FBX + PNGs work in any renderer. Requires UVs."""
    os.makedirs(out_dir, exist_ok=True)
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = samples
    scene.render.bake.margin = margin
    scene.render.bake.use_pass_direct = False
    scene.render.bake.use_pass_indirect = False

    for o in bpy.context.selected_objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj

    baked = {}
    for suffix, bake_type, cspace in BAKE_PASSES:
        img = bpy.data.images.new("T_{}_{}".format(name, suffix),
                                  size, size, alpha=False)
        img.colorspace_settings.name = cspace
        tex_nodes = []
        for mat in obj.data.materials:
            if not mat or not mat.use_nodes:
                continue
            node = mat.node_tree.nodes.new("ShaderNodeTexImage")
            node.image = img
            mat.node_tree.nodes.active = node
            tex_nodes.append((mat, node))
        try:
            bpy.ops.object.bake(type=bake_type)
        except Exception as e:
            print("  bake {} failed for {}: {}".format(bake_type, name, e))
            for mat, node in tex_nodes:
                mat.node_tree.nodes.remove(node)
            continue
        img.filepath_raw = os.path.join(out_dir, "T_{}_{}.png".format(name, suffix))
        img.file_format = 'PNG'
        img.save()
        baked[suffix] = img
        for mat, node in tex_nodes:
            mat.node_tree.nodes.remove(node)
        print("  baked {}".format(os.path.basename(img.filepath_raw)))

    if not baked:
        return False

    # Swap to a single baked material.
    baked_mat = bpy.data.materials.new("{}_Baked".format(name))
    baked_mat.use_nodes = True
    nt = baked_mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    if "D" in baked:
        n = _n(nt, "ShaderNodeTexImage", -500, 250, image=baked["D"])
        nt.links.new(n.outputs["Color"], bsdf.inputs["Base Color"])
    if "R" in baked:
        n = _n(nt, "ShaderNodeTexImage", -500, -50, image=baked["R"])
        n.image.colorspace_settings.name = "Non-Color"
        nt.links.new(n.outputs["Color"], bsdf.inputs["Roughness"])
    if "N" in baked:
        n = _n(nt, "ShaderNodeTexImage", -500, -350, image=baked["N"])
        n.image.colorspace_settings.name = "Non-Color"
        nm = _n(nt, "ShaderNodeNormalMap", -220, -350)
        nt.links.new(n.outputs["Color"], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])

    obj.data.materials.clear()
    obj.data.materials.append(baked_mat)
    return True


# ─── Preview rendering (container-side QA loop) ──────────────────────

def render_preview(filepath, ortho_scale=None, res=512, samples=24,
                   look_at_z=None):
    """Render the current scene contents from a 3/4 angle to PNG."""
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = samples
    scene.render.resolution_x = res
    scene.render.resolution_y = res
    # Judge raw albedo/roughness, not Blender's AgX look -- UE runs its
    # own tonemapper, so preview through the neutral transform.
    try:
        scene.view_settings.view_transform = 'Standard'
    except Exception:
        pass

    # Frame everything.
    import mathutils
    mins = mathutils.Vector((1e9,) * 3)
    maxs = mathutils.Vector((-1e9,) * 3)
    for o in scene.objects:
        if o.type != 'MESH' or o.name.startswith(("UCX_",)) or "_LOD" in o.name:
            continue
        for corner in o.bound_box:
            wc = o.matrix_world @ mathutils.Vector(corner)
            mins = mathutils.Vector(map(min, mins, wc))
            maxs = mathutils.Vector(map(max, maxs, wc))
    center = (mins + maxs) * 0.5
    radius = max((maxs - mins).length * 0.6, 0.5)

    cam_data = bpy.data.cameras.new("QRPreviewCam")
    cam = bpy.data.objects.new("QRPreviewCam", cam_data)
    bpy.context.collection.objects.link(cam)
    offset = mathutils.Vector((1.0, -1.0, 0.65)).normalized() * radius * 2.2
    cam.location = center + offset
    direction = center - cam.location
    cam.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam

    sun_data = bpy.data.lights.new("QRPreviewSun", 'SUN')
    sun_data.energy = 2.2
    sun = bpy.data.objects.new("QRPreviewSun", sun_data)
    bpy.context.collection.objects.link(sun)
    sun.rotation_euler = (0.75, 0.15, 0.6)

    if scene.world is None:
        scene.world = bpy.data.worlds.new("QRPreviewWorld")
    try:
        scene.world.use_nodes = True
        scene.world.node_tree.nodes["Background"].inputs[0].default_value = \
            (0.28, 0.35, 0.45, 1.0)
    except Exception:
        pass

    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    scene.render.filepath = filepath
    bpy.ops.render.render(write_still=True)

    bpy.data.objects.remove(cam)
    bpy.data.objects.remove(sun)
    return filepath
