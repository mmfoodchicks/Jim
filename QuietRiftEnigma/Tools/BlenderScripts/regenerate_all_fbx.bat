@echo off
REM ============================================================================
REM Regenerate every Quiet Rift FBX via Blender CLI (headless).
REM
REM Run from anywhere — the script cd's into its own directory.
REM
REM If Blender isn't on your PATH, edit BLENDER_EXE below to the full path
REM (e.g. "C:\Program Files\Blender Foundation\Blender 4.2\blender.exe").
REM
REM To run a single script instead, comment out the others in the list, or
REM run it directly:
REM     "%BLENDER_EXE%" --background --python qr_generate_weapons_assets_assets.py
REM
REM After this finishes, reimport in UE — Output Log -> Python:
REM     exec(open(r'<Project>/Tools/EditorScripts/qr_seed_items.py').read())
REM   then call run(rebuild_meshes=True) to delete + re-import every mesh.
REM ============================================================================

setlocal
set BLENDER_EXE=blender

set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

echo === Regenerating all Quiet Rift FBXs ===
echo Using Blender : %BLENDER_EXE%
echo Script dir    : %SCRIPT_DIR%
echo.

for %%S in (
    qr_generate_weapons_assets_assets.py
    qr_generate_wildlife_assets.py
    qr_generate_flora_assets.py
    qr_generate_food_assets_assets.py
    qr_generate_items_handheld_assets.py
    qr_generate_attachments_ammo_assets.py
    qr_generate_rigs_packs_assets.py
    qr_generate_cosmetic_clothing_assets.py
    qr_generate_walls_structures.py
    qr_generate_poi_props.py
    qr_generate_stations.py
    qr_generate_remnant_assets.py
    qr_generate_vanguard_assets.py
    qr_generate_player_base.py
    qr_generate_player_rigged.py
    qr_generate_player_morphs.py
) do (
    echo --- Running %%S ---
    "%BLENDER_EXE%" --background --python "%%S"
    if errorlevel 1 (
        echo.
        echo FAILED on %%S  ^(check the Blender output above^)
        endlocal
        exit /b 1
    )
    echo.
)

echo === All FBXs regenerated successfully ===
echo Next: open UE, Output Log -> Python, run:
echo   exec(open(r'%SCRIPT_DIR%..\EditorScripts\qr_seed_items.py').read())
echo and then call run(rebuild_meshes=True) to reimport every mesh.
endlocal
