# Quiet Rift: Enigma — Blueprint Implementation Guide

This document describes every Blueprint class that must be created in the Unreal Editor
to connect the C++ systems together. All C++ logic is authoritative; Blueprints provide
designer-facing orchestration, visual bindings, data wiring, and fast iteration layers.

---

## 1. GameMode / GameState Blueprints

### BP_QRGameMode (parent: AQRGameMode)
- Override `OnDayStarted`: Broadcast day event to raid scheduler, colony state tick
- Override `OnNightStarted`: Activate night lighting, push raid risk update
- Override `OnMissionCompleted`: Chain-activate next missions per the main quest table
- Wire `QuickSave`/`QuickLoad` to input actions (F5/F9 default)

### BP_QRGameState
- Add `UQRColonyStateComponent` (add in Blueprint component list)
- Add `UQRResearchComponent` (add in Blueprint component list)
- `BeginPlay`: Call `InitializeTechNodes` with all `DA_TechNode_*` data assets loaded from Asset Manager

---

## 2. Player Character Blueprint

### BP_QRCharacter (parent: AQRCharacter)
- Assign `DefaultMappingContext` → `IMC_QRDefault`
- Assign `MoveAction`, `LookAction`, `JumpAction`, `CrouchAction`, `InteractAction`, `FireAction`, `ReloadAction`, `InventoryAction`, `SprintAction` → corresponding `IA_QR_*` input actions
- Assign `ArmsMesh` skeletal mesh → `SK_FP_Arms`
- Override `OnInteractableFound`: Display interaction prompt on HUD
- Override `OnDied`: Play death animation, trigger respawn UI

### IMC_QRDefault (Input Mapping Context)
- WASD → `IA_QR_Move` (2D Axis)
- Mouse XY → `IA_QR_Look` (2D Axis)
- Space → `IA_QR_Jump`
- Ctrl → `IA_QR_Crouch`
- E → `IA_QR_Interact`
- LMB → `IA_QR_Fire`
- R → `IA_QR_Reload`
- Tab → `IA_QR_Inventory`
- L-Shift → `IA_QR_Sprint`

---

## 3. NPC Survivor Blueprint

### BP_QRNPCSurvivor (parent: AQRNPCSurvivor)
- Assign `SurvivorBehaviorTree` → `BT_Survivor`
- Override `OnMoraleCollapsed`: Trigger resignation animation; notify colony state

### BT_Survivor (Behavior Tree)
Key task sequence:
1. `BTTask_CheckRaidState` → branch on `EQRCivilianRaidState`
   - `Defending`: Move to weapon rack, equip militia kit
   - `Hiding`: Find nearest shelter actor, hide
   - `Fleeing`: Run to camp boundary, exit logic
2. `BTTask_SelectBestTask` → calls `UQRNPCRoleComponent::SelectBestTask(WorldConditions)`
3. `BTTask_ExecuteTask` → Branch by `CurrentTaskId`:
   - `TASK_GATHER_WOOD`: Move to Smokebark tree → Call Harvest → Move to LogYard → Deposit
   - `TASK_HAUL_TO_DEPOT`: Find nearest filled temporary storage → Pick up → Move to target depot → Deposit
   - `TASK_COOK`: Move to Pantry station → Call station craft queue → Wait
   - `TASK_GUARD`: Move to patrol point → Check for threats → Alert on detection
   - `TASK_REST`: Move to bedroll → Sleep until hunger/thirst threshold
4. `BTTask_UpdateSurvivalNeeds`: Check Survival component; prioritize food/water if critical

### BT_Wildlife_Prey (base predator/prey tree)
Key blackboard keys: `AIState`, `TargetActor`, `HomeLocation`, `NoiseLevelNearby`
1. Idle loop: `BTTask_Wander` in home radius
2. `BTDecorator_DetectNoise` triggers → Set `AIState=Alert`
3. Alert → `BTTask_AlertHerd` → Branch by `BehaviorRole`
4. Prey: `BTTask_Flee` at max flee speed
5. Predator: `BTTask_Stalk` → within range → `BTTask_Charge` → `BTTask_MeleeAttack`

---

## 4. Station Blueprints

### BP_ST_Workbench (parent: AQRStationBase)
- Assign `StationTag` = `Station.Workbench`
- `OnWorkTick_Implementation`: Check queue → `PullFromDepots` for recipe ingredients → advance `CurrentProgress` → on completion: `OnTaskCompleted` with output item

### BP_ST_LogYard (parent: AQRStationBase)
- `StationTag` = `Station.LogYard`
- Auto-processes `MAT_SMOKEBARK_LOG` → `MAT_SMOKEBARK_PLANK` at NPC Hauler rate

### BP_ST_Pantry (parent: AQRStationBase)
- `StationTag` = `Station.Pantry`
- Cook queue UI; accepts `FOD_*` and `MAT_*` cooking inputs
- Output stored in Pantry depot

### BP_ST_AnvilForge (parent: AQRStationBase)
- `StationTag` = `Station.AnvilForge`
- `RequiredPowerQuality` = `None` (mechanical)
- `bRequiresWorker` = True (engineer)

### BP_ST_Generator (parent: AQRStationBase)
- `StationTag` = `Station.Generator`
- Consumes `MAT_FUEL_CANISTER` per tick
- Broadcasts `EQRPowerQuality` to nearby powered stations

---

## 5. Depot Blueprints

### BP_Depot_LogYard (actor with UQRDepotComponent)
- `AcceptedCategory` = `Depot.Category.Wood`
- `PullRadiusCm` = 2500
- `MaxCapacity` = 500

### BP_Depot_Pantry (actor with UQRDepotComponent)
- `AcceptedCategory` = `Depot.Category.Food`
- `PullRadiusCm` = 2500

### BP_Depot_StonePile
- `AcceptedCategory` = `Depot.Category.Stone`

### BP_Depot_ScrapHeap
- `AcceptedCategory` = `Depot.Category.Scrap`

---

## 6. Wildlife Blueprints (per species)

### BP_Wildlife_RidgebackGrazer (parent: AQRWildlife_RidgebackGrazer)
- Assign Skeletal Mesh → `SK_RidgebackGrazer` (placeholder cube with rig)
- `BehaviorTree` → `BT_Wildlife_Prey`
- `HerdGroupId` set by spawner (1-999)
- Animation BP: Walk/Idle/Flee anims from prototype FBX

### BP_Wildlife_HookjawStalker (parent: AQRWildlife_HookjawStalker)
- `BehaviorTree` → `BT_Wildlife_Stalker` (custom: includes stalk state)
- Collision capsule tweaked for quadruped

*(Repeat pattern for all 12 species — each gets a BP subclass + animation assignment)*

---

## 7. Flora / Harvestable Blueprints

### BP_Flora_SmokebarkTree (parent: AQRFlora_SmokebarkTree)
- Static Mesh: `SM_SmokebarkTree_Full` (placeholder from Blender kit)
- On `OnDepleted`: Swap to `SM_SmokebarkTree_Stump`
- On `Fell()` called: Play fall animation, swap to `SM_SmokebarkTree_Fallen`
- Collision: Cylindrical trunk capsule for harvest interaction

### BP_Flora_RustcapFungus (parent: AQRFlora_RustcapFungus)
- Two variants of mesh: `SM_RustcapFungus_Safe` and `SM_RustcapFungus_Toxic`
- On `BeginPlay`: If `bIsToxicVariant` — switch to toxic variant mesh (client-visible!)
  - NOTE: `bIsToxicVariant` is replicated so clients see correct mesh
  - However, EdibilityState is NOT shown until player researches Rustcap in codex

---

## 8. UI / HUD Blueprints

### BP_QRHUD (parent: AQRHUD)
- Assign all widget class references
- Implement `ShowInteractionPrompt`: Play fade-in animation on WBP_InteractionPrompt
- Implement `PushNotification`: Add entry to WBP_NotificationStack with timer-driven auto-dismiss

### WBP_SurvivalHUD
- Bind to player `UQRSurvivalComponent`:
  - Health bar: `OnHealthChanged` delegate
  - Hunger/Thirst bars: Tick-bound progress bars
  - Active status icons: Driven by `ActiveStatusTags` container
  - Injury icons: Driven by `ActiveInjuries` array

### WBP_InventoryGrid
- 30-slot grid; item icons from `UQRItemDefinition::InventoryIcon`
- Drag-drop support for item stacking and equipment
- Weight bar: `GetCurrentWeightKg / MaxCarryWeightKg`

### WBP_ResearchPanel
- Tech tree visual: Node graph using Canvas panel
- Each node: Color-coded by `EQRTechNodeState`
- Micro-research queue: Vertical list with progress bars per stack

### WBP_ColonyReport
- Survivor roster: Scrollable list driven by `UQRColonyStateComponent::SurvivorRecords`
- Morale gauge: Aggregate colony morale bar
- Role assignment: Dropdown per survivor row
- Resource summary: Food days / water days remaining

### WBP_CodexEntry
- Species/item entry view
- Discovery state progress indicator
- Food safety panel (calories, spoil rate, risk) — revealed progressively

---

## 9. PCG / World Generation Setup

### PCG_BiomeFlora (PCG Graph)
- Input: World Partition cell + biome tag
- Point generation: Poisson disk sampling per species density config
- Filter by biome type tag
- Output: Spawn `BP_Flora_*` at generated points
- Biome-to-species table references `DT_Species_Flora.csv`

### PCG_WildlifeSpawner (PCG Graph)
- Input: Biome cells tagged with wildlife spawn data
- Spawns herd groups for prey species (2-6 members)
- Spawns solitary predators in correct biome pockets
- Avoids spawn overlap with fixed POI volumes

---

## 10. Save/Load Integration

### BP_QRGameMode Save Binding
- Wire `UQRSaveGameSystem::OnSaveComplete` → push "Game Saved" notification
- Wire `UQRSaveGameSystem::OnLoadComplete` → restore world state:
  1. Restore `WorldTimeSeconds` / `DayNumber`
  2. Iterate `SurvivorData` → spawn `BP_QRNPCSurvivor` per record
  3. Iterate `ColonyBuildables` → spawn station actors at saved locations
  4. Call `UQRResearchComponent::InitializeTechNodes` with saved states
  5. Iterate `ChunkDeltas` → restore harvestable node depletion states

---

## 11. Data Asset Setup

Import all CSV files as DataTable assets:
- `DT_Items_Master.csv` → `UQRItemDefinition` row struct
- `DT_Recipes.csv` → `UQRRecipeDefinition` row struct  
- `DT_TechNodes.csv` → `UQRTechNode` row struct
- `DT_Species_Wildlife.csv` → `FQRWildlifeSpeciesRow` row struct
- `DT_Species_Flora.csv` → `FQRFloraSpeciesRow` row struct
- `DT_MainQuests.csv` → `FQRMissionRow` row struct
- `DT_MicroResearch.csv` → `UQRMicroResearchDefinition` row struct

Create `DA_*` Primary Data Assets for each item, recipe, and tech node by
right-clicking DataTable rows and selecting "Create Asset."

---

## 12. First Test Map Checklist (L_DevTest)

- [ ] Place `BP_QRCharacter` start location
- [ ] Place `BP_ST_Workbench` with nearby `BP_Depot_LogYard`
- [ ] Place `BP_Flora_SmokebarkTree` x3 within depot pull radius
- [ ] Place `BP_Flora_BlackstemReed` x5 near water volume
- [ ] Place `BP_Wildlife_AshbackBoar` x2 (herd group 1)
- [ ] Place `BP_Wildlife_HookjawStalker` x1 in dense mesh area
- [ ] Place `BP_QRNPCSurvivor` x2 (assign role: Gatherer, Hauler)
- [ ] Set GameMode to `BP_QRGameMode`
- [ ] Enable World Partition with 100m streaming grid
- [ ] Test: Harvest tree → logs go to depot → NPC hauls → station processes
- [ ] Test: Boar charges player → survival component takes damage → injuries apply
- [ ] Test: Death → respawn → survival state resets correctly
- [ ] Test: Quick Save (F5) → close PIE → Quick Load (F9) → verify world state restored
