"""
Quiet Rift: Enigma — bulk import FBX meshes + create UQRItemDefinition assets.

Walks every SM_*.fbx under <Project>/Content/Meshes/, auto-imports the
ones that aren't already imported, then creates a UQRItemDefinition for
each mesh under /Game/QuietRift/Data/Items/<Category>/<ItemId> with
WorldMesh already wired up. Item category, default mass, volume, stack
size, footprint, container properties, etc. are inferred from the mesh
name prefix (SM_WPN_*, SM_FOD_*, SM_RIG_*, …).

Result: the creative-mode browser in PIE lists every authored mesh as a
draggable item; dropping or holding one shows the actual 3D model.

Requirements:
  - Editor → Plugins → enable "Python Editor Script Plugin" + restart.

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_seed_items.py').read())

Or copy the file to <Project>/Content/Python/ and:
  import qr_seed_items
  qr_seed_items.run()

Idempotent — re-running skips meshes and item defs that already exist.
Pass run(overwrite=True) to recreate item defs (meshes are preserved).
Pass run(rebuild_meshes=True) to delete + re-import every static mesh.
"""

import os
import re
import unreal


# ── Filesystem layout ──────────────────────────────────────────
# The script discovers the project root by walking up from its own
# location until it finds a folder containing both Content/ and Config/.

def _project_root():
    here = os.path.dirname(os.path.abspath(__file__)) if '__file__' in globals() \
           else unreal.Paths.project_dir()
    walker = here
    for _ in range(6):
        if os.path.isdir(os.path.join(walker, 'Content')) and \
           os.path.isdir(os.path.join(walker, 'Config')):
            return walker
        walker = os.path.dirname(walker)
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


PROJECT_ROOT     = _project_root()
FBX_DISK_ROOT    = os.path.join(PROJECT_ROOT, 'Content', 'Meshes')
MESH_PKG_ROOT    = '/Game/Meshes'           # where imported StaticMesh assets live
ITEMS_PKG_ROOT   = '/Game/QuietRift/Data/Items'  # where UQRItemDefinition assets live


# ── Naming / categorization rules ──────────────────────────────
# Map the SM_XXX_ prefix to (item category, default property dict).
# Defaults can be overridden per-item later in the editor.

_CATEGORY_INTS = {
    'None': 0, 'Food': 1, 'Tool': 2, 'Weapon': 3, 'Ammo': 4, 'Attachment': 5,
    'Component': 6, 'ReferenceComp': 7, 'Resource': 8, 'Fuel': 9, 'Medicine': 10,
    'Clothing': 11, 'Blueprint_Item': 12, 'Seed': 13, 'Flora': 14, 'Wildlife': 15,
    'ChestRig': 16, 'Backpack': 17, 'Cosmetic': 18,
}
_CONTAINER_SLOT_INTS = {'None': 0, 'ChestRig': 1, 'Backpack': 2}


# prefix → (display category, fallback defaults)
PREFIX_RULES = {
    'WPN':  ('Weapon',     {'mass': 2.5,  'vol': 1.5,  'stack': 1,  'durability': 350.0, 'footprint': (4, 1)}),
    'FOD':  ('Food',       {'mass': 0.3,  'vol': 0.3,  'stack': 30}),
    'AMO':  ('Ammo',       {'mass': 0.02, 'vol': 0.01, 'stack': 200}),
    'ATT':  ('Attachment', {'mass': 0.3,  'vol': 0.2,  'stack': 5,  'durability': 100.0}),
    'RIG':  ('ChestRig',   {'mass': 1.4,  'vol': 0.6,  'stack': 1,  'footprint': (3, 3),
                            'container_slot': 'ChestRig', 'container_grid': (4, 3),
                            'container_carry_kg': 6.0, 'container_volume_l': 12.0}),
    'PACK': ('Backpack',   {'mass': 2.2,  'vol': 1.0,  'stack': 1,  'footprint': (4, 4),
                            'container_slot': 'Backpack', 'container_grid': (5, 4),
                            'container_carry_kg': 12.0, 'container_volume_l': 25.0}),
    'COS':  ('Clothing',   {'mass': 0.5,  'vol': 0.5,  'stack': 1}),
    'ANM':  ('Wildlife',   {'mass': 50.0, 'vol': 30.0, 'stack': 1}),
    'TRE':  ('Flora',      {'mass': 100.0,'vol': 60.0, 'stack': 1}),
    'PLT':  ('Flora',      {'mass': 0.5,  'vol': 0.5,  'stack': 10}),
    'REM':  ('Component',  {'mass': 1.0,  'vol': 0.5,  'stack': 5}),
    'POI':  ('Resource',   {'mass': 5.0,  'vol': 3.0,  'stack': 1}),
    'ST':   ('Component',  {'mass': 8.0,  'vol': 5.0,  'stack': 1}),
    'VAN':  ('Component',  {'mass': 3.0,  'vol': 2.0,  'stack': 1}),
    'BLD':  ('Resource',   {'mass': 2.0,  'vol': 1.0,  'stack': 50}),
    'CMP':  ('Component',  {'mass': 0.2,  'vol': 0.1,  'stack': 50}),
    'TEC':  ('Component',  {'mass': 0.3,  'vol': 0.2,  'stack': 20}),
    'CON':  ('Resource',   {'mass': 0.4,  'vol': 0.3,  'stack': 50}),
    'MAT':  ('Resource',   {'mass': 0.2,  'vol': 0.1,  'stack': 100}),
}

# Source folder → bucket name we use under /Game/Meshes and /Game/.../Items.
FOLDER_TO_BUCKET = {
    'weapons_assets':   'Weapons',
    'food_assets':      'Food',
    'attachments_ammo': 'AmmoAttachments',
    'rigs_packs':       'RigsPacks',
    'cosmetic_clothing':'Clothing',
    'wildlife':         'Wildlife',
    'Flora':            'Flora',
    'remnant_assets':   'Remnant',
    'poi_props':        'POIProps',
    'stations':         'Stations',
    'vanguard_assets':  'Vanguard',
    'walls_structures': 'Building',
    'items_handheld':   'Handheld',
    'Characters':       None,  # skip
}


# ── Helpers ────────────────────────────────────────────────────

def _category(name):
    enum = getattr(unreal, 'QRItemCategory', None)
    if enum is not None:
        attr = getattr(enum, name.upper(), None)
        if attr is not None: return attr
    return _CATEGORY_INTS[name]


def _container_slot(name):
    enum = getattr(unreal, 'QRContainerSlotType', None)
    if enum is not None:
        attr = getattr(enum, name.upper(), None)
        if attr is not None: return attr
    return _CONTAINER_SLOT_INTS[name]


def _humanize(item_id):
    """WPN_SERVICE_PISTOL → 'Service Pistol' (drop the category prefix)."""
    parts = item_id.split('_')
    if len(parts) > 1:
        parts = parts[1:]
    return ' '.join(p.capitalize() for p in parts)


def _set(obj, prop_name, value):
    try:
        obj.set_editor_property(prop_name, value)
    except Exception as e:
        unreal.log_warning(f"  could not set {prop_name}: {e}")


def _prefix_of(item_id):
    m = re.match(r'^([A-Z]+)_', item_id)
    return m.group(1) if m else ''


# ── FBX import ─────────────────────────────────────────────────

def _build_fbx_options():
    opts = unreal.FbxImportUI()
    opts.set_editor_property('import_mesh',          True)
    opts.set_editor_property('import_as_skeletal',   False)
    opts.set_editor_property('import_materials',     False)
    opts.set_editor_property('import_textures',      False)
    opts.set_editor_property('import_animations',    False)
    opts.set_editor_property('create_physics_asset', False)
    opts.set_editor_property('mesh_type_to_import',  unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm_opts = opts.static_mesh_import_data
    sm_opts.set_editor_property('combine_meshes',                  True)
    sm_opts.set_editor_property('generate_lightmap_u_vs',          True)
    sm_opts.set_editor_property('auto_generate_collision',         True)
    sm_opts.set_editor_property('remove_degenerates',              True)
    return opts


def _import_fbx(fbx_path, dest_pkg, mesh_name, fbx_opts):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename',         fbx_path)
    task.set_editor_property('destination_path', dest_pkg)
    task.set_editor_property('destination_name', mesh_name)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save',             True)
    task.set_editor_property('automated',        True)
    task.set_editor_property('options',          fbx_opts)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_tools.import_asset_tasks([task])
    asset_path = f'{dest_pkg}/{mesh_name}'
    return asset_path if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None


# ── Item definition creation ────────────────────────────────────

def _find_def_class():
    cls = getattr(unreal, 'QRItemDefinition', None)
    if cls is None:
        raise RuntimeError("unreal.QRItemDefinition not found. Rebuild the QRItems module then reopen the editor.")
    return cls


def _create_item_def(item_id, mesh_asset_path, prefix_rule, bucket, def_class, overwrite):
    cat_name, defaults = prefix_rule
    dest_dir = f'{ITEMS_PKG_ROOT}/{bucket}'
    asset_path = f'{dest_dir}/{item_id}'

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        if not overwrite:
            return False
        unreal.EditorAssetLibrary.delete_asset(asset_path)

    if not unreal.EditorAssetLibrary.does_directory_exist(dest_dir):
        unreal.EditorAssetLibrary.make_directory(dest_dir)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', def_class)
    asset = asset_tools.create_asset(item_id, dest_dir, def_class, factory)
    if not asset:
        unreal.log_error(f"  failed to create asset {asset_path}")
        return False

    mass    = defaults.get('mass',    0.1)
    vol     = defaults.get('vol',     0.1)
    stack   = defaults.get('stack',   50)
    fp_w, fp_h = defaults.get('footprint', (1, 1))
    durab   = defaults.get('durability', 0.0)

    _set(asset, 'item_id',          unreal.Name(item_id))
    _set(asset, 'display_name',     unreal.Text(_humanize(item_id)))
    _set(asset, 'description',      unreal.Text(f"{_humanize(item_id)} (auto-seeded)"))
    _set(asset, 'category',         _category(cat_name))
    _set(asset, 'mass_kg',          float(mass))
    _set(asset, 'volume_liters',    float(vol))
    _set(asset, 'max_stack_size',   int(stack))
    _set(asset, 'grid_footprint_w', int(fp_w))
    _set(asset, 'grid_footprint_h', int(fp_h))
    _set(asset, 'max_durability',   float(durab))

    # Wire the static mesh. Loading the asset and assigning lets UE
    # coerce to the TSoftObjectPtr<UStaticMesh> field.
    mesh = unreal.EditorAssetLibrary.load_asset(mesh_asset_path)
    if mesh:
        _set(asset, 'world_mesh', mesh)

    if 'container_slot' in defaults:
        cg_w, cg_h = defaults.get('container_grid', (4, 3))
        _set(asset, 'container_slot',               _container_slot(defaults['container_slot']))
        _set(asset, 'container_grid_w',             int(cg_w))
        _set(asset, 'container_grid_h',             int(cg_h))
        _set(asset, 'container_carry_bonus_kg',     float(defaults.get('container_carry_kg', 5.0)))
        _set(asset, 'container_volume_bonus_liters',float(defaults.get('container_volume_l', 10.0)))

    unreal.EditorAssetLibrary.save_asset(asset_path)
    return True


# ── Main walker ────────────────────────────────────────────────

def run(overwrite=False, rebuild_meshes=False, max_items=None):
    """Bulk-import FBXs and create UQRItemDefinitions.

    overwrite       — recreate item def assets even if they exist.
    rebuild_meshes  — delete + re-import every StaticMesh.
    max_items       — cap how many items to process (for dry-run testing).
    """
    if not os.path.isdir(FBX_DISK_ROOT):
        unreal.log_error(f"FBX root not found: {FBX_DISK_ROOT}")
        return

    def_class = _find_def_class()
    fbx_opts  = _build_fbx_options()

    imported_meshes  = 0
    skipped_meshes   = 0
    created_items    = 0
    skipped_items    = 0
    failed           = 0

    discovered = []
    for folder_name in sorted(os.listdir(FBX_DISK_ROOT)):
        bucket = FOLDER_TO_BUCKET.get(folder_name)
        if bucket is None:
            continue
        cat_dir = os.path.join(FBX_DISK_ROOT, folder_name)
        if not os.path.isdir(cat_dir):
            continue
        for fname in sorted(os.listdir(cat_dir)):
            if not fname.lower().endswith('.fbx'):
                continue
            mesh_name = os.path.splitext(fname)[0]
            if not mesh_name.startswith('SM_'):
                continue
            item_id = mesh_name[3:]  # strip SM_
            discovered.append((bucket, folder_name, mesh_name, item_id,
                               os.path.join(cat_dir, fname)))

    if max_items is not None:
        discovered = discovered[:max_items]

    total = len(discovered)
    unreal.log(f"qr_seed_items: discovered {total} FBX files")

    for idx, (bucket, folder, mesh_name, item_id, fbx_path) in enumerate(discovered):
        prefix = _prefix_of(item_id)
        rule   = PREFIX_RULES.get(prefix)
        if rule is None:
            unreal.log_warning(f"  no prefix rule for {item_id} — defaulting to Resource")
            rule = ('Resource', {'mass': 0.5, 'vol': 0.3, 'stack': 10})

        dest_pkg = f'{MESH_PKG_ROOT}/{bucket}'
        mesh_pkg_path = f'{dest_pkg}/{mesh_name}'

        # Step 1: import the mesh if needed.
        if rebuild_meshes and unreal.EditorAssetLibrary.does_asset_exist(mesh_pkg_path):
            unreal.EditorAssetLibrary.delete_asset(mesh_pkg_path)

        if unreal.EditorAssetLibrary.does_asset_exist(mesh_pkg_path):
            skipped_meshes += 1
        else:
            result = _import_fbx(fbx_path, dest_pkg, mesh_name, fbx_opts)
            if result:
                imported_meshes += 1
            else:
                unreal.log_warning(f"  import failed: {fbx_path}")
                failed += 1
                continue

        # Step 2: create the UQRItemDefinition.
        try:
            if _create_item_def(item_id, mesh_pkg_path, rule, bucket, def_class, overwrite):
                created_items += 1
            else:
                skipped_items += 1
        except Exception as e:
            unreal.log_error(f"  item def failed for {item_id}: {e}")
            failed += 1

        if (idx + 1) % 25 == 0:
            unreal.log(f"  progress: {idx + 1}/{total}")

    # Nudge the asset manager so the creative menu sees new items right away.
    try:
        am = unreal.AssetManager.get()
        am.scan_paths_synchronous([ITEMS_PKG_ROOT])
    except Exception:
        pass

    unreal.log(
        f"qr_seed_items: done. "
        f"meshes imported={imported_meshes} skipped={skipped_meshes}; "
        f"items created={created_items} skipped={skipped_items} failed={failed}"
    )


if __name__ == '__main__':
    run()
