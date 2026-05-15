# Quiet Rift: Enigma — System Cohesion Audit

End-of-session sanity sweep after the strategy / worldgen / UI build-out.
Every system below has its file paths, integration points, and known
gaps. Read this before opening the project to know what should work
and what's deferred.

---

## A. Compile-time integrity

| Check | Result |
|---|---|
| Brace balance across all `Source/QuietRiftEnigma/{Public,Private}/*` | ✅ all balanced |
| Python syntax across `Tools/EditorScripts/*.py` | ✅ all parse |
| Forward-declared classes resolve | ✅ |
| Module dependency list (`QuietRiftEnigma.Build.cs`) | ✅ Niagara + AIModule + NavigationSystem + all QR* modules declared |
| `QRCombatThreat` module has `Niagara` dep | ✅ added earlier |
| No mismatched UFUNCTION signatures vs delegate signatures | ✅ spot-checked the 25+ DECLARE_DYNAMIC delegates |

**File count this session:** 63 headers + 59 cpps in
`Source/QuietRiftEnigma`. 11 editor Python scripts.

---

## B. Data-flow integrity (system → system)

### Worldgen → Spawner → Camps → Raids → Civilian Reaction

```
AQRWorldGenSeedActor.Generate()
   ↓ feeds
UQRWorldGenSubsystem (cells + POI placements)
   ↓ read by
AQRWorldGenSpawner.SpawnAll()
   ↓ creates
AQRFactionCamp (one per FactionSatellite POI)
   ↓ ticks
UQRCampSimComponent.AdvanceGameHours()  ← driven by AQRGameMode.Tick
   ↓ decides
OnRaidLaunched(FQRRaidPlan)
   ↓ subscribed by
   ├─ AQRFactionCamp.HandleRaidLaunched → spawns AQRNPCActor +s with UQRRaidPartyAI
   └─ UQRCivilianReactionComponent.HandleRaidLaunched → Fight/Flee/Hide
```

**Verified.** `OnRaidLaunched` has two subscribers. Each raid spawn
re-uses the same plan, and the civilian-side trigger only fires when
distance to TargetLocation is within `TriggerRadiusCm`.

### Player → Codex → Mission director

```
AQRWorldItem.TryPickup() → UQRCodexSubsystem.Record(Item, Sampled)
AQRWildlifeActor first perception → Codex.Record(Species, Observed)
AQRCharacter.ApplyBiomeProfile() → Codex.Record(Biome, Observed)
AQRCharacter.QR_StudyItem(Id) → Codex.Record(Id, Researched)
   ↓ broadcasts
UQRCodexSubsystem.OnEntryUpdated
   ↓ subscribed by
UQRMissionDirector.HandleCodexUpdated → ResearchItem mission progress

UQRInventoryComponent.OnItemAdded
   ↓ subscribed by
UQRMissionDirector.HandleItemAdded → FetchItem mission progress

AQRWildlifeActor (Health <= 0)
   ↓ calls static
UQRMissionDirector::ReportSpeciesKilled → KillTarget mission progress
```

**Verified.** Three event surfaces, three director handlers, all
hooked in `UQRMissionDirector::BeginPlay` + the static helper.

### Atmosphere

```
AQRGameMode.BeginPlay() →
  auto-spawns AQRSkyManager (rotates sun based on GetDayProgress)
  auto-spawns AQRWeatherFXManager (subscribes to UQRWeatherComponent
                                   on GameState; spawns Niagara FX per
                                   weather event)
```

**Verified.** Both managers fall back to no-ops if their dependencies
aren't present — sky finds no DirectionalLight = idle; weather has no
component = idle.

### Save / load

```
AQRGameMode.BeginPlay() → if QuickSave exists, async LoadGame
  → OnLoadComplete → ApplyLoadedDataToPlayer (restores vitals + inventory
                                              + identity + location)
AQRGameMode.EndPlay  → QuickSave (autosave on quit/level travel/PIE-stop)
AQRGameMode.Logout   → QuickSave (per-PC exit)
AQRGameMode periodic timer → QuickSave every 5 min
Pause menu Save button → QuickSave
Main menu Continue → enabled iff QuickSave slot exists
```

**Verified.**

---

## C. Per-system reference

| System | File(s) | Lives on | Wired via |
|---|---|---|---|
| Survival vitals | `QRSurvivalComponent` | AQRCharacter | `Survival->OnHealthChanged` → vitals HUD |
| Vitals HUD | `QRVitalsHUDWidget` | Local PC viewport | `AQRCharacter::BeginPlay` |
| Hotbar | `QRHotbarComponent` + `QRHotbarHUDWidget` | AQRCharacter | Input map 1–9 + scroll |
| Inventory grid | `QRInventoryGridWidget` | Local PC | `I` key toggle |
| Codex | `QRCodexSubsystem` + `QRCodexWidget` | UWorld + local PC | `K` key toggle |
| Pause menu | `QRPauseMenuWidget` | Local PC | `Esc` |
| Main menu | `QRMainMenuWidget` + `QRMainMenuGameMode` | `L_MainMenu` | Game default map |
| Settings | `QRSettingsWidget` | Local PC | `QR_OpenSettings` exec from pause/main |
| Creative browser | `QRCreativeBrowserWidget` | Local PC | Backtick |
| Death screen | `QRDeathScreenWidget` | Local PC | `AQRGameMode::HandlePlayerDied` |
| Scope overlay | `QRScopeOverlayWidget` | Local PC | Bound to FPView; visible while scope ADS |
| Crafting bench | `AQRCraftingBench` + `QRCraftingWidget` | Level | F-interact opens widget |
| NPC dialogue | `AQRNPCActor` + `UQRDialogueComponent` + `QRDialogueWidget` | Level + PC | F-interact starts conversation |
| Build piece selector | `QRBuildPieceSelectorWidget` | Local PC | Auto on entering build mode |
| Wildlife actors | `AQRWildlifeActor` (FSM) | Spawner | `UQRWorldGenSpawner.SpawnFauna` |
| Wildlife death loot | `AQRWildlifeActor.Dead state` | inherited from AQRWorldItem | spawns FOD_RAW_MEAT + RAW_HIDE on death |
| Crash sites | `AQRCrashSiteActor` (hardcoded loot) | Spawner | `UQRWorldGenSpawner.SpawnPOIs` |
| Caves | `AQRCaveEntrance` | Spawner | Spawned per Nth cell w/ Caves habitat flag |
| Remnants | `AQRRemnantSite` (5-state FSM) | Spawner | Concordat research-tier escalation |
| Faction camps | `AQRFactionCamp` + `UQRCampSimComponent` | Spawner (FactionSatellite POIs) | `AQRGameMode::Tick` advances sim |
| Raid party | `UQRRaidPartyAI` | Spawned NPCs | Faction camp `OnRaidLaunched` |
| Civilian reaction | `UQRCivilianReactionComponent` | Friendly NPCs | Subscribes to every camp's `OnRaidLaunched` |
| Depots | `AQRDepotActor` (+ `UQRInventoryComponent`) | Level | Hauler queries |
| Haulers | `UQRHaulerComponent` | NPCs | FSM ticks depot→station route |
| Missions | `UQRMissionDirector` | AQRGameMode subobject | Auto-hooks events on BeginPlay |
| Day/night | `AQRSkyManager` | Auto-spawned | Tick reads `GameMode->GetDayProgress` |
| Weather VFX | `AQRWeatherFXManager` | Auto-spawned | Subscribes to `UQRWeatherComponent` on GameState |
| Concordat raids | `AQRRaidScheduler` | Designer-placed (optional) | Reflective reads of weather + concordat hostility |
| Procedural scatter | `AQRProceduralScatterActor` | Designer-placed | Optional `bUseWorldGenSubsystem` queries cell grid |
| Biome system | `UQRBiomeProfile` | Data assets | Scatter actor + AQRBiomeZone + character poll |

---

## D. Input map (final)

| Key | Action | Handler |
|---|---|---|
| WASD | Move | `AQRCharacter::Move` |
| Mouse | Look | `AQRCharacter::Look` |
| Space | Jump / Vault | `AQRCharacter::HandleJumpPressed` |
| Shift | Sprint | `AQRCharacter::StartSprint` |
| C | Crouch | engine `ToggleCrouch` |
| Q / E | Lean L/R | `AQRCharacter::LeanLeftPressed` etc |
| F | Interact | `AQRCharacter::TryInteract` |
| G | Drop / build / wildlife place | `AQRCharacter::TryDropHeld` (category dispatch) |
| LMB | Fire weapon | `UQRWeaponComponent::TryFireFromTrace` |
| RMB | Use held / ADS | `AQRCharacter::TryUseHeld` |
| 1–9 | Hotbar | `AQRCharacter::OnHotbarSlotInput` |
| Scroll | Hotbar prev/next | `OnHotbarPrev` / `OnHotbarNext` |
| I | Inventory grid | `OnInventoryPressed` |
| K | Codex | `OnCodexPressed` |
| Esc | Pause menu | `OnPausePressed` |
| Backtick | Creative browser | `OnCreativeBrowserPressed` |

---

## E. Known gaps (deferred, not bugs)

| Gap | Impact | Path to fix |
|---|---|---|
| NavMesh pathing | Wildlife / haulers / raid parties walk straight lines (won't path around obstacles) | Add NavMesh volume to test map; switch movement to `AAIController::MoveToLocation` |
| Behavior trees | Combat/idle behaviors are tick-based FSMs, not true BTs | Substantial — author BT assets per AI archetype |
| Real combat AI | Civilians "Fight" mode currently just faces threat (no shoot) | Add `UQRWeaponComponent` hookup + fire-at-target in fight tick |
| `UQRCraftingComponent` exposing CurrentDemandItem | Hauler currently hardcodes RAW_METAL_SCRAP as the demand item | One-line API expansion + tiny FSM update |
| Faction-faction alliances | Multi-camp coordination (gang up / split targets) | Camp sim already considers other camps as targets; alliance + shared planning is the next layer |
| Camp recovery after leader death | Leader killed → no rebuild path | Add a promotion-from-ranks timer that picks a new leader and slowly rebuilds skill |
| Master material authoring | All meshes use Fab MIs assigned by Python script | Designer authors project-specific master mats later |
| AnimBP state machines | `ABP_QRPlayer` shell created; state graph empty | Designer builds state machine reading `UQRPlayerAnimInstance` publics |
| DataTable row content | Python seeded 4 DataTables with row keys; field contents need designer fill | UE5.7 Python limit — designer edits in table editor |
| L_MainMenu visuals | Map functional; backdrop / skybox / mood lighting plain | Editor decoration pass |

---

## F. Manual editor steps (in order)

1. **Compile C++** — `Ctrl+B` in IDE. If errors, paste them to me.
2. **Enable plugins** — Python Editor Script + Editor Scripting Utilities + Niagara. Restart editor.
3. **Run the Python scripts** in this order (`Tools/EditorScripts/`):

```python
exec(open(r'<P>/Tools/EditorScripts/qr_seed_items.py').read())
exec(open(r'<P>/Tools/EditorScripts/qr_assign_fab_materials.py').read())
exec(open(r'<P>/Tools/EditorScripts/qr_seed_data_tables.py').read())
exec(open(r'<P>/Tools/EditorScripts/qr_seed_biome_profiles.py').read())
exec(open(r'<P>/Tools/EditorScripts/qr_create_test_maps.py').read())
exec(open(r'<P>/Tools/EditorScripts/qr_create_anim_blueprint.py').read())
exec(open(r'<P>/Tools/EditorScripts/qr_create_proc_world_map.py').read())   # optional
exec(open(r'<P>/Tools/EditorScripts/qr_generate_heightmap.py').read())      # optional
exec(open(r'<P>/Tools/EditorScripts/qr_retarget_anims_to_mannequin.py').read()) # optional
exec(open(r'<P>/Tools/EditorScripts/qr_catalog_fab_levels.py').read())      # optional
exec(open(r'<P>/Tools/EditorScripts/qr_catalog_mannequin_anims.py').read()) # optional
```

4. **In `L_DevTest`** — drop one `AQRWorldGenSeedActor` → Generate. Drop one `AQRWorldGenSpawner` → SpawnAll. Drop one `AQRProceduralScatterActor` with `bUseWorldGenSubsystem` checked + `BiomeProfileMap` populated.
5. **Press Play.**

---

## G. Smoke-test punch list

After loading L_DevTest:

- [ ] Hotbar visible bottom-center.
- [ ] Vitals (HP/STA/FOOD/H2O/OX) bottom-left.
- [ ] WASD moves, footsteps audible (per-surface if `PhysicalSurface` set).
- [ ] Shift sprints — FOV widens.
- [ ] RMB on a weapon → ADS, FOV crunches; sniper/DMR → scope overlay appears + 20° FOV.
- [ ] LMB fires — muzzle flash + tracer + impact FX + gunshot sound (Free_Sounds_Pack).
- [ ] G drops the held item / building piece enters build mode / wildlife spawns live actor.
- [ ] F interacts — bench opens crafting widget, NPC opens dialogue widget.
- [ ] I toggles inventory grid; click-pickup, R-rotate, click-place.
- [ ] K toggles Codex; pick up an item → entry appears as `Item / SAMPLED`.
- [ ] Esc pauses; menu Save persists; Continue from menu reloads.
- [ ] Take damage (`QR.Damage 50` console) — hit sound, HP drops; full damage → death screen + 3.5s respawn.
- [ ] Wait ~1 game-day in editor — sun rotates, biome ambient swaps if walking across biome boundaries.
- [ ] Approach a faction camp — its sim ticks population/military. Wait for raid (faster with `QR.AdvanceTime` cheat).
- [ ] Kill a wildlife actor — meat + hide drop on ground.

---

## H. Module health

```
QRCore         (types, math, gameplay tags)
QRItems        (UQRItemDefinition, UQRItemInstance, UQRInventoryComponent, AQRWorldItem)
QRSurvival     (UQRSurvivalComponent, UQRWeatherComponent)
QRLogistics    (route + station scaffolding)
QRCraftingResearch (UQRCraftingComponent, UQRResearchComponent, UQRMicroResearch)
QRColonyAI     (UQRNPCRoleComponent, UQRLeaderComponent, AQRVanguardColony)
QRCombatThreat (UQRWeaponComponent + Niagara FX + UQRReloadFinishNotify)
QRSaveNet      (UQRSaveGameSystem + FQRGameSaveData)
QRUI           (placeholder — actual widgets live in main module)
QuietRiftEnigma (the everything-else module)
```

**Cross-module dependencies are clean.** Game module pulls from all
others; others don't depend on the game module (good — keeps
recompile times reasonable).

---

## I. What I genuinely can't compile-check

I write code statically without compiling. The above brace + Python
checks catch syntax errors, but I can't verify:
- UFUNCTION reflection signatures match delegate types at link time
- `LoadObject<T>` paths actually resolve at runtime
- `FindFProperty` reflective lookups find their target property
- Niagara user-parameter names exist in the actual systems

If you hit a build error on `Ctrl+B`, paste it — almost all of them
will be missing-include / signature mismatch issues I can fix in one
edit each.
