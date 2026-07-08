# Quiet Rift: Enigma — Claude session pointer

UE 5.7 first-person survival sim. Solo/co-op, 64km finite world on a Jovian
moon (2057 crash). C++ heavy — Blueprints are subclassable but the project
plays without WBP authoring because every widget is built in C++.

**If you're a fresh session: read this file, then `SYSTEM_COHESION_AUDIT.md`
and `GDD_IMPLEMENTATION_STATUS.md` before touching code.** Don't make the
user re-explain the project.

---

## World canon (set 2026-05-24, Tyson-defensible)

Player is on a **fictional larger Jovian moon** at roughly Callisto's
orbital distance (~1.88 million km from Jupiter). Earth-mass, breathable
N₂/O₂ atmosphere (geological replenishment + retention), **outside
Jupiter's main radiation belt** so long-term habitation is plausible.
This is what justifies:

- **Earth-like blue sky** (Rayleigh scattering of N₂/O₂ molecules works
  the same anywhere). Dimmer than Earth (~1/27 sunlight at 5.2 AU)
  but still blue.
- **Jupiter ~4° in sky** (8× our Moon) — dramatic but not a 12° giant
  like Europa's view. Cream-tan banded.
- **"Jovianlight"** at night when Jupiter reflects sunlight onto the
  surface (~500× brighter than our full moon).
- **Crystalline flora/fauna** — high mineral content, silicate-
  reinforced organic tissues. Leaves are prismatic cellulose with
  crystalline light-pipe veins (look like stained glass, function
  like normal leaves). Don't try to justify the chemistry too hard.
- **Sun ~2,800 lux peak** in QRSkyManager defaults (1/27 of Earth's
  ~75,000), bounded auto-exposure in the QR_Exposure PostProcessVolume
  handles the dynamic range.
- **O₂ HUD hides when full** — atmosphere is breathable so the meter
  only appears in caves, underwater, or hazard zones.

Other planets (Saturn, Uranus, etc) would render as bright/faint
stars in the sky from this distance, not as resolved disks — belongs
on the SkyAtmosphere's star field if added.

---

## Working branch

**`claude/unreal-cpp-blueprint-project-0F07i`** — the long-running dev branch.
All work lands here by default. Do not push to `main`, do not open PRs, do
not switch branches without the user explicitly saying so.

The harness may put new sessions on a fresh `claude/*` branch — switch back
to the dev branch unless the user says otherwise.

### Pulling the latest work to your local machine

After Claude pushes commits, pull them into your local UE project with:

```
git checkout claude/unreal-cpp-blueprint-project-0F07i
git pull origin claude/unreal-cpp-blueprint-project-0F07i
```

If git refuses because of local editor-generated changes, `git stash`
before the pull and `git stash pop` after. The SessionStart hook prints
these same two lines at the top of every session.

---

## Local paths on the user's machine

Use these literal paths when handing the user commands — don't reach
for `<Project>` placeholders, the user has to substitute them by hand
and it gets old. Repo root:

```
D:\QuietRiftEnigma\Jim
```

Common subpaths:

- Editor Python scripts: `D:\QuietRiftEnigma\Jim\QuietRiftEnigma\Tools\EditorScripts\qr_*.py`
- Blender scripts:       `D:\QuietRiftEnigma\Jim\QuietRiftEnigma\Tools\BlenderScripts\qr_*.py`
- Regenerate-all .bat:   `D:\QuietRiftEnigma\Jim\QuietRiftEnigma\Tools\BlenderScripts\regenerate_all_fbx.bat`
- Blender install:       `C:\Program Files\Blender Foundation\Blender 5.1\blender.exe`

UE Python console one-liners the user reaches for often (paste verbatim):

```
exec(open(r'D:\QuietRiftEnigma\Jim\QuietRiftEnigma\Tools\EditorScripts\qr_seed_items.py').read())
run(rebuild_meshes=True)
```

```
exec(open(r'D:\QuietRiftEnigma\Jim\QuietRiftEnigma\Tools\EditorScripts\qr_setup_sky.py').read())
run()
```

Exposure is LOCKED (no auto-exposure blowout). If the map is too bright
or too dark, nudge the EV knob — higher = darker:

```
run(exposure_ev=15.0)   # darker than the 13.0 default
```

Add rolling hills (half-buried engine spheres — no Fab dependency) to
the currently-open dev map so wildlife gravity / slope-conformance has
something to climb:

```
exec(open(r'D:\QuietRiftEnigma\Jim\QuietRiftEnigma\Tools\EditorScripts\qr_terrain_devtest.py').read())
run()
```

**Dress the dev map with real Fab content** in one button — biomes,
landscape material, rock material on the hill domes, four scatter
actors that paint the playable bowl with ScifiJungle plants +
Rock_Collection_04 boulders:

```
exec(open(r'D:\QuietRiftEnigma\Jim\QuietRiftEnigma\Tools\EditorScripts\qr_world_dressing.py').read())
run()
```

For a **full worldgen-tiled dressing** (uses UQRWorldGenSubsystem to
pick the right biome profile per cell across a configurable playable
radius — every tile knows which biome it sits in and scatters
accordingly):

```
exec(open(r'D:\QuietRiftEnigma\Jim\QuietRiftEnigma\Tools\EditorScripts\qr_world_dressing.py').read())
run_full()                                      # default 5km zone, 500m tiles
run_full(playable_radius_m=8000.0, tile_m=800)  # 16km zone, larger tiles
```

**Spawn a starter village** of brain-driven NPCs (wander / work /
sleep / socialize / flee on raid alarm):

```
exec(open(r'D:\QuietRiftEnigma\Jim\QuietRiftEnigma\Tools\EditorScripts\qr_spawn_starter_village.py').read())
run()             # 8 colonists in a 40 m ring around origin
run(count=20)     # bigger settlement
```

**Fresh-checkout one-button** — sky, nav, biomes, terrain, materials,
scatter, AND the starter village in one call:

```
exec(open(r'D:\QuietRiftEnigma\Jim\QuietRiftEnigma\Tools\EditorScripts\qr_world_dressing.py').read())
everything()
```

---

## Read-first docs (project state lives in these, not in your head)

1. `SYSTEM_COHESION_AUDIT.md` — every system: file paths, integration
   points, data-flow diagrams, known gaps, input map. Source of truth for
   "does X exist and how is it wired?"
2. `GDD_IMPLEMENTATION_STATUS.md` — full GDD ↔ code cross-reference with
   ✅ / 🟡 / 🔧 / ❌ per system. §N lists the 10 biggest open gaps in
   priority order (worldgen pipeline, AI BTs, hauler logic, mission
   generator, codex aggregator, etc.).
3. `MANUAL_EDITOR_TASKS.md` — what humans must do in the editor that
   Claude cannot automate (AnimBP state machine, DataTable field content,
   NavMesh volume placement, plugin enable).
4. `PROCEDURAL_WORLD_PLAN.md` — worldgen pipeline + Fab pack roles.
5. `QuietRiftEnigma/GAME_OVERVIEW.md` — design vision + lore.
6. `GDD_Dictionaries/` — canonical .docx/.xlsx design docs (Master GDD
   v1.3 / v1.5, Visual World Bible, Mission & Leadership Bible, etc.).
   **Read-only — never modify.** Reference if a status doc is ambiguous.

When updating any of these docs, keep them current — they're how the next
session catches up.

---

## Module layout

```
Source/
  QRCore               — types, gameplay tags, math
  QRItems              — UQRItemDefinition, inventory, world items
  QRSurvival           — vitals, weather
  QRLogistics          — routes, stations
  QRCraftingResearch   — crafting, research, micro-research
  QRColonyAI           — NPCs, leaders, Vanguard colony
  QRCombatThreat       — weapons, Niagara FX, raid scheduler, satellite outpost
  QRSaveNet            — save game system + save types
  QRUI                 — (placeholder; widgets live in main module)
  QuietRiftEnigma      — game module, pulls from all others
```

Cross-module deps are one-directional (game module → other modules, never
the reverse). Keep it that way.

---

## Code conventions

- **Commit messages** use a short scope prefix:
  - `qr_game_mode: split ternary that confused TSubclassOf<APawn>`
  - `Add qr_audit_fabs.py — comprehensive Fab pack inventory`
  - `Fix held-mesh scale, creative hotbar wildlife equip, broken-cue purge API`
- **UE 5.7 Python has API drift** — `Package.set_dirty_flag` is gone,
  `TSoftClassPtr` setter wants a loaded Class not a SoftClassPath, bool
  UPROPERTYs drop the `b` prefix in Python. Check recent commits for the
  pattern when something errors.
- **Programmatic UMG**: every widget is constructed via `WidgetTree` in C++
  so the project plays without designer WBP authoring. Designer can
  subclass to swap in polished WBP_ later.
- **Save lifecycle**: `AQRGameMode` autosaves on BeginPlay (load),
  Logout, EndPlay, and every 5 min. Pause menu Save button + Main menu
  Continue both use the QuickSave slot.
- **Save extension rule** (since save v2, 2026-06-11): all capture/apply
  logic lives in `FQRSaveSnapshot` (`QRSaveNet/QRSaveSnapshotLibrary`).
  A system persists iff it has a Capture/Apply pair there — never
  hand-copy fields in `QuickSave` again; that's how research, armour
  slots, injuries, and grid layout silently fell out of v1 saves. Bump
  `QRCurrentSaveVersion` + add a `MigrateToCurrentVersion` step when
  the structs change.

---

## Recipe + research mirror rule (HARD policy)

**Whenever an item is added or removed from the game, its crafting
recipe AND its tech-node research entry must move with it in the same
commit.** No floating items. Apply recursively — every input ingredient
the item references must itself already be a real id (or get added too).

Tables to update:
- `Content/QuietRift/Data/DT_Recipes.csv` — recipe rows
  (`RecipeId, Output, Inputs, Time, Station, Unlocked By, Source Section`)
- `Content/QuietRift/Data/DT_TechNodes.csv` — research nodes that
  unlock the recipe (`UnlockedRecipeIds` column is `+`-separated)
- If the item gives a status / consumes a buff, update
  `EQRInjuryType` (`QRTypes.h`) too

Working pattern: write a `qr_append_<feature>_recipes.py` script in
`Tools/EditorScripts/` that appends the rows idempotently and ships
alongside the item seeder. The crude arsenal pass
(`qr_append_crude_arsenal_recipes.py`) is the reference.

After running the appender, the user reimports the data tables in UE
(Window → DataTable → Reimport) so the new rows go live.

When removing items: drop the recipe rows AND scrub every
`UnlockedRecipeIds` cell that listed them, so research nodes don't
reference dangling recipes.

---

## Don't touch

- **Fab pack layout (since 2026-06-11):** the full library lives at
  `Content/<PackName>/` (i.e. `/Game/<PackName>` in asset paths) — the
  user re-uploaded everything to content root. `Content/Fabs/` holds
  only `LED_Generator` + `StampIt` (no root copies). Don't recreate
  the old `Content/Fabs/<Pack>` layout; all qr_*.py scripts and
  QRUISound.cpp reference root paths (the C++ sound loader retries
  the legacy Fabs prefix as fallback). Pack *contents* are
  borrowed assets — reference them, don't edit them.
- `GDD_Dictionaries/*.docx`, `*.xlsx`, `*.pdf` — source-of-truth design
  artifacts owned by the human.
- `.gitignore`, `.gitattributes` — LFS config; don't reconfigure.

---

## Editor Python script pipeline

`Tools/EditorScripts/qr_*.py` — 11+ scripts that automate FBX import,
material assignment, anim retarget, DataTable seeding, map creation,
biome profile authoring, Fab wiring. **All idempotent** — safe to re-run.
Order documented in `MANUAL_EDITOR_TASKS.md` Phase 1.

---

## What I cannot verify (limits of static editing)

I edit code without compiling. I can check brace balance, Python syntax,
forward decls, header includes. I cannot verify:
- UFUNCTION reflection signatures match delegate types at link time
- `LoadObject<T>` paths resolve at runtime
- `FindFProperty` reflective lookups find their target
- Niagara user-parameter names exist in the actual systems
- The editor renders what I think it renders

If `Ctrl+B` errors after my changes, paste the errors — almost all are
missing-include or signature mismatches fixable in one edit each.

---

## Active priorities (reconciled 2026-07-08)

**Maintenance rule:** when a big gap closes, update this list AND
`GDD_IMPLEMENTATION_STATUS.md` §N in the same commit. The previous
priority list went stale by months because closed gaps stayed listed
as "missing", which wastes future-session time chasing ghosts. Both
docs must move together.

Genuinely-missing gaps, in priority order (see §N of
`GDD_IMPLEMENTATION_STATUS.md` for the full reconciled audit):

1. **AI behavior trees** — 🟡 mostly closed 2026-07-08. Wildlife:
   `AQRWildlifeAIController` code-only FSM (4Hz think, NavMesh pathing,
   role branching, herd alert, per-species sizing/gravity/two-way
   damage — 2026-06-05). **NPC job AI (2026-07-08):** colonists execute
   their roles — farmers harvest/replant `AQRFarmPlotActor` tiles,
   deposit yield to depots, feed husbandry animals (gated on
   `FeedIntervalHours`); guards patrol build-piece waypoint loops;
   medics triage + heal the most-wounded in claim range. Raiders: the
   villager brain yields movement to `UQRRaidPartyAI` (lazy re-probe —
   the FSM attaches post-spawn) while still painting anims; melee
   swings flash via `UQRNPCBrainComponent::FlashAttack`. Wildlife can
   carry skinned bodies + single-node anims (`DefaultBodyMesh` +
   Idle/Walk/Run/DeathAnim slots on `AQRWildlifeBase`); first user is
   `AQRWildlife_ColonyDog` — a fully skinned German Shepherd spawned
   with the starter village (`qr_spawn_starter_village.run()`).
   Still missing: leader-level BTs, herd-route data driving, predator
   pressure-pull weighting, dog follow/guard behavior.
2. **Mission generator + RewardSourceValidation** — ✅ closed 2026-06-11.
   `InstantiateMission` builds live instances with the
   `MissionLocationFallbackRule` cascade; `GrantRewards` enforces the
   No-Pocket-OP law per `EQRRewardSource` (Knowledge → codex,
   FactionStockpile → real depot withdrawal, SiteContainer →
   `AQRWorldItem` drops, NPCPersonal capped at 3, Morale/Infrastructure
   strip items). HUD `UQRMissionHUDWidget` surfaces active missions.
3. **Hauler de-hardcode** — ✅ v1 closed 2026-06-11
   (`GetCurrentDemandItem` drives demand; v2 = deficit weighting
   across multiple stalled stations).
4. **Long-range optics v8** — ✅ closed 2026-06-11. `WPN_LONGRANGE_SNIPER`
   already in `ConfigureForWeaponId`; `UQRFPViewComponent::ScopeZoomMultiplier`
   + name-based detection in `RefreshHeldItemMesh` give 8X/16X tier
   magnification.
5. **Cross-contamination farming mutation pipeline** — ✅ closed 2026-06-11.
   `AQRFarmPlotActor` runs `UQRMath::CrossContamExposure` per grow cycle,
   branches the yield to `MUT_<original>`, propagates SporeLoad to
   neighbors.
6. **Mount husbandry loop** — ✅ closed 2026-06-11.
   `UQRMountHusbandryComponent` owns BaseTameDays, P_tameFailPerDay,
   CurrentStressPool, PanicThreshold; Courser + Dray subclassed.
7. **Leader directive chains + Moral Compass vectors** — ✅ closed
   2026-06-11 (chain side). `UQRLeaderComponent::OnQuestIssued` fires
   at the Escalating→QuestIssued FSM edge; mission director's
   `IssueDirectiveMission` materializes a family-matched mission.
   Moral Compass vector math already lived in `UpdateCampAlignment`.
8. **Faction raid leader experience bands** — ✅ closed 2026-06-11.
   `FQRRaidPlan::Experience` set by `DetermineRaidTier`; the raid AI
   restamps perception/speed/damage/retreat per tier.
9. **Civilian Fight mode** — ✅ closed 2026-06-11. Fight state acquires
   the nearest live raider and fires on `FireIntervalSeconds`.
10. **Codex save persistence** — 🟡 partial 2026-06-11: research-side
    codex states persist via save v2 (`FQRSaveSnapshot`); the game-
    module `UQRCodexSubsystem::Entries` map still doesn't.
11. **Co-op transaction-ID safety net.**
12. **Programmatic Landscape import.**
13. **World partition streaming + chunk delta saves** — 🟡 visual side
    closed 2026-07-08: `AQRDressingStreamer` maintains a moving
    scatter-tile bubble around the player (one tile per 0.25s,
    deterministic per-tile seeds), so the full 64km map reads dressed
    at ~9k live instances. Enable with
    `qr_world_dressing.enable_streaming()` (also fixes the worldgen
    tiles' empty-palette early-return that made `run_full` tiles spawn
    nothing). Actor streaming + chunk delta saves still open.

Build-blockers (urgent — gameplay fails without these):
- NavMesh on `L_DevTest` (now scriptable via `qr_dev_test_dressup.py`).
- `ABP_QRPlayer` state machine empty (graph authoring is manual).
  **Deferred 2026-06-05:** game is first-person and the third-person
  body is hidden from the owning player (`OwnerNoSee=true`), so the
  player locomotion state machine is invisible in single-player.
  Revisit when co-op is being tested.
- ~~Buildable + looted-container save/load glue~~ — ✅ closed 2026-06-11
  via `UQRBuildModeComponent::RestoreFromSave` +
  `FQRGameSaveData::LootedContainerIds` wiring.
- 4 starter DataTables (now bulk-seeded by `qr_seed_starter_datatables.py`).

Already-built — DO NOT add these back to the priority list without
checking code first: Codex (subsystem + K-key widget), Mission director
(runtime), Remnant FSM (5-state), Raid scheduler, Faction component,
Satellite outposts, Civilian reaction component, Hauler component.
