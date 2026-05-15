# Quiet Rift: Enigma — Manual Editor Tasks

Step-by-step list of things that **must** be done inside the UE editor
(can't be automated via C++ or Python). Cross items off with `[x]` as
you complete them.

> Anything not on this list is either already shipped in C++ or runs
> automatically via the editor Python scripts under `Tools/EditorScripts/`.

---

## Phase 0 — One-time project bootstrap

Do these once, the first time you open the project after pulling.

- [ ] **Generate project files**. Right-click `QuietRiftEnigma.uproject`
      in your file manager → `Generate Visual Studio project files` (or
      run `UnrealEditor.exe -Generate` on Mac/Linux).
- [ ] **Compile C++** (`Ctrl+B` in your IDE, or click `Compile` in the
      UE toolbar). Wait for "Compile complete!" in the Output Log.
      If you see errors, paste them to me — they're almost always
      missing-module or header-include issues I can fix.
- [ ] **Enable required plugins** (Edit → Plugins):
  - [ ] Python Editor Script Plugin (required for every Python script
        in this checklist).
  - [ ] Editor Scripting Utilities (required by some Python APIs).
  - [ ] Niagara (already a dependency of QRCombatThreat but make sure
        the plugin checkbox is on).
- [ ] **Restart the editor** after enabling plugins.

---

## Phase 1 — Run the Python scripts (in order)

Open `Window → Output Log`, switch the input dropdown from `Cmd` to
`Python`, and run each in turn. All scripts are idempotent — re-running
them is safe.

- [ ] `qr_seed_items.py` — bulk-imports every SM_*.fbx under
      `Content/Meshes/` and creates a `UQRItemDefinition` per mesh.
      _(may take 5-10 minutes on a fresh project — 200+ FBX imports)_
- [ ] `qr_assign_fab_materials.py` — walks all `SM_*` meshes and
      assigns a fitting Fab material per category prefix. Greybox →
      textured in one pass.
- [ ] `qr_seed_data_tables.py` — creates `DT_BuildCatalog`,
      `DT_Recipes`, `DT_NPC_Greetings`, `DT_LootTables` with starter
      row keys. You still have to open each and fill in field values
      (see Phase 4).
- [ ] `qr_create_test_maps.py` — creates `/Game/Maps/L_MainMenu` and
      `/Game/Maps/L_DevTest`. L_DevTest has a floor + lights +
      pre-placed crafting bench + NPC spawner + wildlife actor.
- [ ] `qr_create_anim_blueprint.py` — creates `ABP_QRPlayer.uasset`
      with `UQRPlayerAnimInstance` parent + Mannequin skeleton bound.
      AnimGraph is empty — see Phase 3.
- [ ] `qr_retarget_anims_to_mannequin.py` — duplicates FuturisticWarrior
      anims onto the Mannequin skeleton (only needed if you want the
      FW anims; the Mannequin-rigged Fab anims work as-is).
- [ ] `qr_catalog_fab_levels.py` — _optional_, prints which Fab demo
      maps are worth copying into your Maps folder.
- [ ] `qr_catalog_mannequin_anims.py` — _optional_, prints AnimBP slot
      recommendations from your current anim pack inventory.

How to run each:
```python
exec(open(r'<full-path>/Tools/EditorScripts/<scriptname>.py').read())
```

---

## Phase 2 — Project Settings sanity check

Edit → Project Settings.

- [ ] **Maps & Modes**:
  - [ ] Editor Startup Map = `L_MainMenu`
  - [ ] Game Default Map   = `L_MainMenu`
  - [ ] Server Default Map = `L_DevTest`
  - [ ] Default GameMode   = `AQRGameMode` (BP_QRGameMode_C if you
        subclass in BP later)
  - These should already be set by the INI edit, but verify.
- [ ] **Physics → Physical Surfaces** — verify the 10 entries
      (SurfaceType1=Concrete through SurfaceType10=Wood) are present.
      Also set by INI edit, but check.
- [ ] **Engine → General Settings → Frame Rate**:
  - [ ] Smooth Frame Rate ✓
  - [ ] Use Fixed Frame Rate ✗
- [ ] **Engine → Rendering**:
  - [ ] Forward Shading: leave OFF (deferred is fine for now).
  - [ ] Lumen / Nanite: on if your GPU can handle it; otherwise
        switch global illumination to "Screen Space".

---

## Phase 3 — AnimBP state machine (the big one)

The Python script created `/Game/QuietRift/Animations/ABP_QRPlayer` and
bound it to the right skeleton + parent C++ class. The state machine
itself you build manually.

Open `ABP_QRPlayer`.

- [ ] **Event Graph** is fine empty — `UQRPlayerAnimInstance::Native
      UpdateAnimation` writes all the variables.
- [ ] **AnimGraph** — replace the empty Output Pose with a State
      Machine:
  - [ ] Right-click → **Add New State Machine**, call it `Locomotion`.
  - [ ] Plug `Locomotion` into Output Pose.
  - [ ] Open it. Inside:
    - [ ] Add states: `Idle`, `Walk`, `Run`, `Jump`, `Fall`, `Death`.
    - [ ] Each state's body plays an AnimSequence. Use the Mannequin-
          rigged anims from `FreeAnimsMixPack` / `RamsterZ` for
          combat / death; use FuturisticWarrior retargeted output
          (`/Game/QuietRift/Animations/Retargeted/A_QR_*`) for the
          locomotion set.
    - [ ] Transitions read the AnimInstance variables:
      - Idle → Walk : `Speed > 10`
      - Walk → Run  : `Speed > 250 && bIsSprinting`
      - Any  → Jump : `bIsFalling` (and not bIsDead)
      - Jump → Fall : Time > 0.2s
      - Fall → Idle : `!bIsFalling`
      - Any  → Death: `bIsDead`
- [ ] **Save + compile** the AnimBP.

Tip: if you want a quicker MVP, plug a single Idle AnimSequence
directly into Output Pose, skip the state machine, and iterate
once the game is playable.

---

## Phase 4 — DataTable contents

The Python seeder created the assets and row keys; you fill in fields.

### DT_BuildCatalog (`/Game/QuietRift/Data/DT_BuildCatalog`)
- [ ] Open the table. 19 rows pre-populated (BLD_WALL_WOOD, etc.).
- [ ] For each row:
  - [ ] **DisplayName**: set a player-readable name.
  - [ ] **Category**: verify (auto-set; correct if any are wrong).
  - [ ] **Mesh**: drag the matching `SM_BLD_*` mesh into the slot.
  - [ ] **MaterialCost**: add 1-2 ingredient entries (e.g. WALL_WOOD
        costs `RAW_WOOD_PLANK x 4`).

### DT_Recipes (`/Game/QuietRift/Data/DT_Recipes`)
- [ ] Open the table. 5 starter row keys.
- [ ] For each row:
  - [ ] DisplayName, OutputItemId, OutputQuantity, CraftTimeSeconds.
  - [ ] Ingredient1..N (ItemId + Quantity).
  - [ ] Optional: RequiredStationTag, RequiredTechNodeId.
- [ ] **Add more recipes** as gameplay grows.

### DT_NPC_Greetings (`/Game/QuietRift/Data/DT_NPC_Greetings`)
- [ ] Open the table. 3 sample row keys.
- [ ] For each row, add Lines array entries: `(Speaker, Text)`.
- [ ] Text supports tokens: `{name}`, `{he}/{she}/{they}`,
      `{is}/{are}`, `{has}/{have}` — the pronoun library substitutes
      based on initiating player's identity.
- [ ] Optional: `NextNodeId` for multi-node conversations.

### DT_LootTables (`/Game/QuietRift/Data/DT_LootTables`)
- [ ] Open. 3 tier row keys (LOW / MEDIUM / HIGH).
- [ ] Per row, add ItemRolls with weights summing to 1.0.

---

## Phase 5 — Map polish

L_MainMenu and L_DevTest are functional but plain.

### L_MainMenu (already loads + shows menu widget)
- [ ] Add a backdrop static mesh (a crashed ship hull, distant
      mountain) — visible behind the menu widget.
- [ ] Set ambient color + post-process volume for mood.
- [ ] Optional: looping ambient track (SoundCue from Free_Sounds_Pack
      `Ambient_Wind_Loop_1` works).

### L_DevTest (already has floor + bench + NPC + wildlife)
- [ ] Add `NavMeshBoundsVolume` covering the floor — without it the
      NPC AI can't path.
- [ ] Press P to verify nav mesh covers the playable area (green
      overlay).
- [ ] Drop more `AQRWorldItem` actors via the creative-browser
      (open with `\`` in PIE, click items).
- [ ] Optional: add a directional light + skybox for outdoor feel.

---

## Phase 6 — Smoke tests (after every change)

- [ ] Press Play in editor. Verify:
  - [ ] Hotbar visible bottom-center.
  - [ ] Vitals visible bottom-left (5 bars).
  - [ ] HP / Stamina / Hunger / Thirst / Oxygen all draining or
        regenerating sensibly.
  - [ ] `WASD` moves, `Space` jumps, `Shift` sprints, `C` crouches.
  - [ ] `\`` opens creative browser, can drag items to hotbar slots.
  - [ ] `1..9` selects hotbar slot, held item appears in hand.
  - [ ] `G` drops held item (or enters build mode if a BLD_ item).
  - [ ] `LMB` fires when a weapon is held.
  - [ ] `RMB` aims down sights (weapons) / consumes (food / medicine).
  - [ ] `F` interacts with bench (opens crafting widget) and NPC
        (opens dialogue widget).
  - [ ] `I` toggles the inventory grid.
  - [ ] `Esc` toggles pause menu.
  - [ ] Walking plays footstep sounds (surface defaults to concrete
        if no PhysMat is on the floor mesh).
- [ ] Damage yourself in console: `QR.Damage 50` — verify HP bar
      drops and hit SFX plays.
- [ ] Damage to zero — verify death screen + respawn after 3.5s.
- [ ] Save + quit + relaunch — verify QuickLoad on level restart.

---

## Phase 7 — Procedural world generation

Detailed setup lives in [`PROCEDURAL_WORLD_PLAN.md`](PROCEDURAL_WORLD_PLAN.md).
Quick checklist:

- [ ] Run `qr_seed_biome_profiles.py` — creates three starter
      `UQRBiomeProfile` data assets (Alien Jungle / Polar Tundra /
      Desert Sand).
- [ ] Run `qr_create_proc_world_map.py` — creates `/Game/Maps/L_ProcTest`
      with a floor + one scatter actor.
- [ ] Open `L_ProcTest`. Select `ProcScatter_Default`. Details panel
      → set **BiomeProfile = BP_AlienJungle** → click **Generate**.
      Verify 500ish scattered plants + rocks appear.
- [ ] Optional: replace the flat floor with a real Landscape actor
      painted using MWLandscapeAutoMaterial's master material.
- [ ] Optional: drop additional scatter actors for other biomes
      (Polar Tundra, Desert Sand).
- [ ] Optional: drop ScifiJungle's `BP_PCG_Manager` for dense
      forest layers alongside the scatter actor.

---

## Things I cannot do

These genuinely need human judgment:

- **Aesthetic level design** — where mountains / trees / NPCs sit for
  a specific gameplay beat.
- **Recipe balance** — how many planks for a wall, how long to craft.
- **Story content** — NPC dialogue lines, mission text, codex entries.
- **AnimBP state machine layout** — the design decision _is_ which
  states exist and how they transition.
- **Material node graphs** for custom shaders (we're using Fab MIs
  so this isn't needed).
- **Sound design** — choosing which weapon sound matches which gun.

Everything else either ships in C++ or runs from the Python scripts
above.
