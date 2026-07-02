"""
qr_append_entry_tool_recipes.py -- recipe + tech-node rows for the 5
crash-entry tools created by qr_seed_entry_tools.py.

Per the recipe/research mirror rule: items and their recipe + research
rows move together in the same commit. This is the recipe half.

Design intent: the entry tools ARE the progression gates for the 6
minor crash sites. The pry bar is nearly free (T0) so the first two
wrecks open early; the torch/coupler sit behind basic tooling and
electronics; the decrypt spike is the late unlock. The med key is NOT
craftable -- it's loot-only (found at the CommandBridge hero crash),
which makes the medbay a treasure-hunt rather than a bench recipe.

Run from a shell (NOT inside UE -- writes the CSVs directly):
    cd D:\\QuietRiftEnigma\\Jim\\QuietRiftEnigma\\Tools\\EditorScripts
    python qr_append_entry_tool_recipes.py

Idempotent; --replace overwrites matching rows. Reimport DT_Recipes +
DT_TechNodes in UE afterwards.
"""

import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.normpath(os.path.join(HERE, "..", "..", "Content", "QuietRift", "Data"))

RECIPES_CSV   = os.path.join(DATA, "DT_Recipes.csv")
TECHNODES_CSV = os.path.join(DATA, "DT_TechNodes.csv")


def _r(rid, out, ingredients, time_s, station, unlock):
    return [rid, "{} x1".format(out), ingredients, "{}s".format(time_s),
            station, unlock, "Crash Entry Tools (auto-seeded)"]


def _recipe_rows():
    return [
        _r("RC_TOL_PRY_BAR",       "TOL_PRY_BAR",
           "MAT_SCRAP_METAL x2",
           45, "Camp Workbench", "T0 Primitive"),
        _r("RC_TOL_CUTTING_TORCH", "TOL_CUTTING_TORCH",
           "MAT_SCRAP_METAL x2; CMP_VALVE_SET x1; MAT_FUEL_CANISTER x1",
           180, "Machine Bench", "TN_BASIC_TOOLING"),
        _r("RC_TOL_POWER_COUPLER", "TOL_POWER_COUPLER",
           "MAT_SCRAP_METAL x1; RAW_WIRE x3; RAW_CAPACITOR x1",
           150, "Electronics Bench", "TN_ELECTRONICS_T1"),
        _r("RC_TOL_DECRYPT_SPIKE", "TOL_DECRYPT_SPIKE",
           "RAW_CIRCUIT_BOARD x1; RAW_WIRE x2; REM_ART_DATA_SHARD x1",
           240, "Electronics Bench", "TN_DECRYPTION"),
        # TOL_MED_KEY intentionally has NO recipe -- loot-only at the
        # CommandBridge hero crash. Documented here so nobody "fixes" it.
    ]


def _tech_rows():
    def T(tid, name, tier, family, prereq, points, unlocks, notes):
        return ["---", tid, '"{}"'.format(name), tier, family,
                prereq, "", points, unlocks, notes]
    return [
        T("TN_DECRYPTION", "Signal Decryption", "T2_Intermediate", "Energy",
          "TN_ELECTRONICS_T1", 220,
          "RC_TOL_DECRYPT_SPIKE",
          "Remnant-derived handshake spoofing. Unlocks the avionics "
          "wreck's decryption spike."),
    ]


def _append_csv(path, new_rows, key_col, replace):
    if not os.path.isfile(path):
        print("ERR: {} not found".format(path))
        return 0

    with open(path, "r", newline="", encoding="utf-8") as f:
        rows = list(csv.reader(f))
    existing = {r[key_col] for r in rows[1:] if r and len(r) > key_col}

    keep = [rows[0]] + [r for r in rows[1:]
                        if not replace or r[key_col] not in {nr[key_col] for nr in new_rows}]
    added = 0
    for nr in new_rows:
        if nr[key_col] in existing and not replace:
            continue
        keep.append(nr)
        added += 1

    with open(path, "w", newline="", encoding="utf-8") as f:
        csv.writer(f).writerows(keep)
    print("[entry-tools] {}: +{} rows".format(os.path.basename(path), added))
    return added


def main(replace=False):
    _append_csv(RECIPES_CSV,   _recipe_rows(), key_col=0, replace=replace)
    _append_csv(TECHNODES_CSV, _tech_rows(),   key_col=1, replace=replace)
    print("[entry-tools] done. Reimport DT_Recipes + DT_TechNodes in UE.")


if __name__ == "__main__":
    main(replace=("--replace" in sys.argv))
