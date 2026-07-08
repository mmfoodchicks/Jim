@echo off
REM ============================================================================
REM Regenerate every Quiet Rift FBX via Blender CLI (headless).
REM
REM Double-click this file, or run it from a cmd / powershell window.
REM Do NOT run it from inside Blender's Scripting tab -- it shells OUT to
REM Blender via --background --python for each script.
REM
REM Auto-detects Blender at the standard install paths. If it can't find
REM blender.exe, edit BLENDER_EXE below to the full path
REM (e.g. "C:\Program Files\Blender Foundation\Blender 4.2\blender.exe").
REM
REM After this finishes, reimport in UE -- Output Log -> Python:
REM     exec(open(r'<Project>/Tools/EditorScripts/qr_seed_items.py').read())
REM   then call run(rebuild_meshes=True) to delete + re-import every mesh.
REM ============================================================================

setlocal EnableDelayedExpansion

REM -- 1. Locate Blender ------------------------------------------------------
REM Enumerates any "Blender X.Y" folder under Program Files (Blender
REM Foundation), then Steam, then PATH. /o-n sorts newest-first so the
REM highest version wins.
set BLENDER_EXE=

set BLENDER_ROOT=C:\Program Files\Blender Foundation
if exist "%BLENDER_ROOT%" (
    for /f "delims=" %%D in ('dir /b /ad /o-n "%BLENDER_ROOT%\Blender *" 2^>nul') do (
        if not defined BLENDER_EXE if exist "%BLENDER_ROOT%\%%D\blender.exe" (
            set "BLENDER_EXE=%BLENDER_ROOT%\%%D\blender.exe"
        )
    )
)

REM Steam install path (separate because it's not under Blender Foundation).
REM Quote the SET assignment because (x86) contains parens that break cmd parsing.
if not defined BLENDER_EXE if exist "C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe" set "BLENDER_EXE=C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe"

REM Fall back to PATH lookup if no install path matched.
if not defined BLENDER_EXE (
    where blender >nul 2>&1
    if !errorlevel! equ 0 set BLENDER_EXE=blender
)

if not defined BLENDER_EXE (
    echo.
    echo ERROR: Could not find blender.exe.
    echo.
    echo Checked: any "Blender *" folder under %BLENDER_ROOT%
    echo          C:\Program Files ^(x86^)\Steam\steamapps\common\Blender
    echo          'blender' on PATH
    echo.
    echo Fix: edit this .bat and set BLENDER_EXE to the full path to your
    echo blender.exe, e.g.:
    echo     set BLENDER_EXE=D:\Apps\Blender\blender.exe
    echo.
    pause
    endlocal
    exit /b 1
)

set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

echo === Regenerating all Quiet Rift FBXs ===
echo Using Blender : %BLENDER_EXE%
echo Script dir    : %SCRIPT_DIR%
echo.

set FAILED_SCRIPT=

for %%S in (
    qr_generate_weapons_assets_assets.py
    qr_generate_wildlife_assets.py
    qr_generate_wildlife_rigged.py
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
    if not defined FAILED_SCRIPT (
        if not exist "%%S" (
            echo --- SKIP %%S  ^(file not found^) ---
        ) else (
            echo --- Running %%S ---
            "%BLENDER_EXE%" --background --python "%%S"
            if errorlevel 1 (
                echo.
                echo FAILED on %%S  ^(check the Blender output above^)
                set FAILED_SCRIPT=%%S
            )
            echo.
        )
    )
)

echo.
if defined FAILED_SCRIPT (
    echo === FAILED on !FAILED_SCRIPT! ===
    echo Scroll up to see the Blender error output for that script.
    pause
    endlocal
    exit /b 1
)

echo === All FBXs regenerated successfully ===
echo Next: open UE, Output Log -^> Python, run:
echo   exec(open(r'%SCRIPT_DIR%..\EditorScripts\qr_seed_items.py').read())
echo and then call run(rebuild_meshes=True) to reimport every mesh.
echo.
pause
endlocal
