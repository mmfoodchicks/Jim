"""
Quiet Rift: Enigma — list every .umap inside /Game/Fabs with quick metrics
so we can pick which Fab demo levels to copy into our Maps folder as test
sandboxes.

For each map:
  • actor count
  • light count (DirectionalLight / SkyLight / PointLight / SpotLight)
  • has-sky / has-postprocess flags
  • a rough role hint (Demo / LevelInstance / Test / Gallery)

Run from the UE Python console:
  exec(open(r'<Project>/Tools/EditorScripts/qr_catalog_fab_levels.py').read())

Or:
  import qr_catalog_fab_levels
  qr_catalog_fab_levels.run()
"""

import os
import unreal


FAB_ROOT = "/Game/Fabs"


def _classify(map_path):
    """Heuristic role tag — based on path tokens only, so it stays fast."""
    p = map_path.lower()
    if "levelinstances" in p:    return "LevelInstance"
    if "/demo/" in p:            return "Demo"
    if "gallery" in p:           return "Gallery"
    if "test" in p:              return "Test"
    if "/maps/" in p:            return "Map"
    return "Other"


def _count_actors(world):
    """Tallies the actors of interest inside a loaded UWorld."""
    counts = {"actors": 0, "lights": 0, "sky": 0, "post_process": 0}
    if not world:
        return counts
    all_actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    counts["actors"] = len(all_actors)
    for a in all_actors:
        c = a.get_class().get_name()
        if "Light" in c:
            counts["lights"] += 1
        if "Sky" in c or "Atmospheric" in c:
            counts["sky"] += 1
        if "PostProcess" in c:
            counts["post_process"] += 1
    return counts


def run(load_each=False):
    """Walk /Game/Fabs/ for .umap assets and print a table.

    load_each : if True, actually load each map to count actors (slow,
                spawns transient editor work). Default False just lists
                paths + classifies by name — much faster.
    """
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    ar.scan_paths_synchronous([FAB_ROOT], True)

    f = unreal.ARFilter(
        class_names=["World"],
        package_paths=[FAB_ROOT],
        recursive_paths=True,
    )
    assets = list(ar.get_assets(f))
    print("[fab-maps] {} levels found under {}".format(len(assets), FAB_ROOT))

    rows = []
    for ad in assets:
        path = str(ad.object_path)
        role = _classify(path)
        rec = {"path": path, "role": role}
        if load_each:
            w = unreal.EditorAssetLibrary.load_asset(path)
            counts = _count_actors(w)
            rec.update(counts)
        rows.append(rec)

    # Group by role and print.
    by_role = {}
    for r in rows:
        by_role.setdefault(r["role"], []).append(r)

    role_priority = ["Demo", "Gallery", "Map", "Test", "LevelInstance", "Other"]
    print("")
    for role in role_priority:
        if role not in by_role: continue
        print("== {} ({} levels) ==".format(role, len(by_role[role])))
        for r in by_role[role]:
            if load_each:
                print("  {:<70s} actors={:<4d} lights={:<3d} sky={:<2d} pp={}".format(
                    r["path"], r.get("actors", 0), r.get("lights", 0),
                    r.get("sky", 0), r.get("post_process", 0)))
            else:
                print("  {}".format(r["path"]))
        print("")

    # Recommendations for which to keep as sandboxes.
    print("== suggested playable sandbox candidates ==")
    for role in ("Demo", "Gallery"):
        for r in by_role.get(role, []):
            print("  ✔  {}".format(r["path"]))
    print("== suggested PARENT-level placeables (Level Instances) ==")
    for r in by_role.get("LevelInstance", []):
        print("  ◇  {}".format(r["path"]))


if __name__ == "__main__":
    run()
