"""
Quiet Rift: Enigma — Fab content prune script.

Deletes the bloat from your Content/Fabs/ collection:
  Phase 1: 11 entire packs that don't fit the sci-fi survival tone
  Phase 2: demo / showcase / map subfolders inside the 24 kept packs

Defaults to DRY RUN (preview only). Add --execute to actually delete.
You'll be asked to type YES before any deletion happens.

Usage:
    cd C:\\path\\to\\Jim
    python prune_fabs.py                # preview — nothing deleted
    python prune_fabs.py --execute      # delete after confirmation

This script ONLY touches Content/Fabs/. It will refuse to run if
that folder isn't found (so you can't accidentally run it from the
wrong directory). It does NOT touch:
  - __ExternalActors__ / __ExternalObjects__ (UE5 partition data)
  - Anything outside Content/Fabs/
  - Git state (Fabs is already gitignored)

Safety features:
  - Defaults to dry run
  - Refuses to run unless the working directory contains the expected
    QuietRiftEnigma project structure
  - Confirmation prompt before any deletion
  - Per-folder size preview so you see exactly what's going
  - Errors on individual files don't abort the whole run

Estimated savings: 30-50 GB depending on your pack versions.
"""

import argparse
import shutil
import sys
from pathlib import Path

# ── Configuration ────────────────────────────────────────────────────────────

# Entire packs to delete (genre / setting mismatch).
PACKS_TO_DELETE_ENTIRELY = [
    "Deko_MatrixDemo",
    "Dungeon_Modular_V1",
    "ElfArden",
    "Fantasy_Props",
    "MedCastle",
    "Mixed_Magic_VFX_Pack",
    "Necropolis",
    "OldWest",
    "StylizedlogCabin_A1",
    "WesternDesertTown",
    "Wood_Monster",
    "Maps",  # standalone "Maps" pack — 1 file, just an Overview.umap
]

# Subfolders to delete inside each kept pack. Everything else stays.
PACK_BLOAT = {
    "Bodycam_VHS_Effect":     ["DEMO", "Maps"],
    "CombatMagicAnims":       ["Demo", "Map"],
    "Construction_VOL1":      ["Maps"],
    "DeepWaterStation":       ["Maps"],
    "FreeAnimationsPack":     ["Demo", "Map"],
    "FuturisticWarrior":      ["Demo", "Maps"],
    "IndustryPropsPack6":     ["Maps"],
    "LED_Generator":          ["Map"],
    "LensFlareVFX":           ["Levels"],
    "MPMECH":                 ["Maps", "Manny", "Quinn", "UE5MANNEQUIN", "HDRI"],
    "MWLandscapeAutoMaterial":["Maps"],
    "NiagaraExamples":        ["Gallery"],
    "OWD_Plants_Pack":        ["Maps"],
    "Polar":                  ["Maps"],
    "QuantumCharacter":       ["Demo", "Map"],
    "Ruined_Modern_Buildings":["Maps"],
    "ScifiJungle":            ["Demo", "ExamplePlayer", "Maps"],
    "Vefects":                ["Demo"],
    "WoodenProps":            ["PreviewScene"],
    "PhoneSystem":            ["Demo", "Movie"],
    "Chaotic_Skies":          ["Maps"],
    "Horror_Props":           ["Maps"],
    "WinterTown":             ["Maps"],
    "ModernBridges":          ["Maps"],
}

FABS_RELATIVE_PATH = Path("QuietRiftEnigma") / "Content" / "Fabs"


# ── Helpers ──────────────────────────────────────────────────────────────────

def folder_size_bytes(path: Path) -> int:
    """Total size of all files under a folder, recursive. Ignores errors."""
    total = 0
    try:
        for entry in path.rglob("*"):
            try:
                if entry.is_file():
                    total += entry.stat().st_size
            except (OSError, PermissionError):
                pass
    except (OSError, PermissionError):
        pass
    return total


def fmt_size(n: int) -> str:
    """Human-readable byte count."""
    f = float(n)
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if f < 1024.0:
            return f"{f:>7.1f} {unit}"
        f /= 1024.0
    return f"{f:>7.1f} PB"


def safe_rmtree(path: Path) -> tuple[bool, str]:
    """Delete a directory tree. Returns (success, error_message)."""
    try:
        shutil.rmtree(path)
        return True, ""
    except (OSError, PermissionError) as e:
        return False, str(e)


def find_fabs_root() -> Path | None:
    """Locate Content/Fabs/ from the current working directory. Refuses to
    return a path unless it sits inside a recognizable QuietRiftEnigma layout
    (i.e. a sibling .uproject file)."""
    cwd = Path.cwd()
    candidate = cwd / FABS_RELATIVE_PATH
    uproject = cwd / "QuietRiftEnigma" / "QuietRiftEnigma.uproject"
    if candidate.exists() and uproject.exists():
        return candidate.resolve()
    # Try one level up in case user is inside QuietRiftEnigma/
    candidate = cwd.parent / FABS_RELATIVE_PATH
    uproject = cwd.parent / "QuietRiftEnigma" / "QuietRiftEnigma.uproject"
    if candidate.exists() and uproject.exists():
        return candidate.resolve()
    return None


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Prune Fab content packs for Quiet Rift: Enigma.",
        epilog="Defaults to dry run. Add --execute to delete.",
    )
    parser.add_argument(
        "--execute",
        action="store_true",
        help="Actually delete files (otherwise just preview).",
    )
    parser.add_argument(
        "--no-confirm",
        action="store_true",
        help="Skip the YES confirmation prompt. Combined with --execute, "
             "deletes immediately. Use with caution.",
    )
    args = parser.parse_args()

    fabs_root = find_fabs_root()
    if fabs_root is None:
        print("ERROR: could not find QuietRiftEnigma/Content/Fabs/ from this directory.")
        print(f"  Current directory: {Path.cwd()}")
        print("  Run this script from the project root (the folder containing")
        print("  QuietRiftEnigma/QuietRiftEnigma.uproject).")
        sys.exit(1)

    mode = "EXECUTE (will delete files)" if args.execute else "DRY RUN (preview only — nothing deleted)"
    print()
    print("=" * 78)
    print(f"  Quiet Rift: Enigma — Fab Pack Prune")
    print("=" * 78)
    print(f"  Target:  {fabs_root}")
    print(f"  Mode:    {mode}")
    print("=" * 78)
    print()

    # Inventory pass — compute everything we'd delete, including sizes, BEFORE
    # touching anything. This way the user can review the full plan.
    print("Inventorying packs to delete entirely...")
    plan_phase1: list[tuple[str, Path, int]] = []
    for pack in PACKS_TO_DELETE_ENTIRELY:
        p = fabs_root / pack
        if not p.exists():
            print(f"  [skip] {pack:<28}  (not on disk)")
            continue
        size = folder_size_bytes(p)
        plan_phase1.append((pack, p, size))

    print()
    print("Inventorying bloat subfolders inside kept packs...")
    plan_phase2: list[tuple[str, Path, int]] = []
    for pack, subs in PACK_BLOAT.items():
        pack_path = fabs_root / pack
        if not pack_path.exists():
            print(f"  [skip] {pack:<28}  (pack not on disk)")
            continue
        for sub in subs:
            sp = pack_path / sub
            if not sp.exists():
                continue
            size = folder_size_bytes(sp)
            plan_phase2.append((f"{pack}/{sub}", sp, size))

    # Show the plan.
    print()
    print("─" * 78)
    print(" PHASE 1 — Entire packs (genre / tone mismatch)")
    print("─" * 78)
    total1 = 0
    for label, _, size in plan_phase1:
        print(f"  {label:<32} {fmt_size(size)}")
        total1 += size
    print(f"  {'-' * 50}")
    print(f"  Phase 1 total: {len(plan_phase1)} packs, {fmt_size(total1)}")

    print()
    print("─" * 78)
    print(" PHASE 2 — Bloat subfolders (demo / showcase / preview)")
    print("─" * 78)
    total2 = 0
    for label, _, size in plan_phase2:
        print(f"  {label:<48} {fmt_size(size)}")
        total2 += size
    print(f"  {'-' * 66}")
    print(f"  Phase 2 total: {len(plan_phase2)} folders, {fmt_size(total2)}")

    grand = total1 + total2
    print()
    print("=" * 78)
    print(f"  TOTAL TO FREE: {fmt_size(grand)}  ({len(plan_phase1)} packs + {len(plan_phase2)} subfolders)")
    print("=" * 78)
    print()

    if not args.execute:
        print("This was a DRY RUN — no files were touched.")
        print("Re-run with --execute to actually delete.")
        return

    if not args.no_confirm:
        print("This WILL permanently delete the above files.")
        print("Type 'YES' (in capitals) to proceed, anything else to abort.")
        try:
            response = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nAborted.")
            return
        if response != "YES":
            print("Aborted — nothing deleted.")
            return

    # Execute.
    print()
    print("Deleting...")
    deleted_size = 0
    deleted_count = 0
    errors: list[tuple[str, str]] = []

    for label, path, size in plan_phase1 + plan_phase2:
        ok, err = safe_rmtree(path)
        if ok:
            print(f"  [ok]   {label}")
            deleted_size += size
            deleted_count += 1
        else:
            print(f"  [ERR]  {label} — {err}")
            errors.append((label, err))

    print()
    print("=" * 78)
    print(f"  Done.")
    print(f"  Deleted: {deleted_count} items, {fmt_size(deleted_size)} freed.")
    if errors:
        print(f"  Errors:  {len(errors)} items could not be deleted.")
        for label, err in errors[:10]:
            print(f"    - {label}: {err}")
        if len(errors) > 10:
            print(f"    ... and {len(errors) - 10} more")
        print()
        print("  Common causes:")
        print("    - Unreal Editor is open with one of those packs loaded")
        print("    - File is read-only or owned by another user")
        print("    - Antivirus has a file locked")
        print("  Close UE5 and re-run if needed.")
    print("=" * 78)


if __name__ == "__main__":
    main()
