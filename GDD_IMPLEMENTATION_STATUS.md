# Quiet Rift: Enigma — GDD ↔ Implementation Status

Cross-reference of every system in the GDD Dictionaries against what's
actually built in code. Generated 2026-05-15 after reading all 14 docs
in `GDD_Dictionaries/` plus `GAME_OVERVIEW.md` and
`Content/QuietRift/BLUEPRINT_GUIDE.md`.

Legend: ✅ shipped · 🟡 partial · 🔧 scaffolded only · ❌ missing
"Post" = added in implementation, not yet in GDD.

---

## A. Identity, lore, campaign — Master GDD §2, GAME_OVERVIEW

| GDD item | Implementation | Status |
|---|---|---|
| Setting: Jovian moon, 2057 crash | Codified in `GAME_OVERVIEW.md`; no in-game lore plumbing | 🔧 |
| Opening pod sequence (MQ_000 Cryo Wake) | DT_MissionsMainQuestline.csv has the row; no scripted scene | 🟡 |
| Three endings (Activate / Transmit / Destroy / Stay) | Stored as `EQREndingPath` in `FQRGameSaveData` | 🔧 |
| Solo vs co-op starting NPC count (3/2/1/0) | `AQRGameMode::GetStartingNPCCount` returns the math | ✅ |
| Pre-existing leaders found in the world | `UQRLeaderComponent` + `DT_LeaderConditions.csv` | 🟡 (data exists, no spawner) |

---

## B. World map, biomes, generation — Master GDD §4

| GDD item | Implementation | Status |
|---|---|---|
| 64 km finite world + hazard belt + hard wall | `UQRWorldGenSubsystem` builds the biome cell grid with an outer HazardBelt ring; `AQRWorldGenSeedActor` drives it from an editor button | ✅ |
| WorldSeed → BiomeSeed / POISeed / EcologySeed / FactionSeed / LootSeed | `UQRWorldGenSubsystem` derives all 5 `FRandomStream`s from one WorldSeed | ✅ |
| Macro terrain generation pipeline | `qr_generate_heightmap.py` bakes a heightmap + per-biome ground weightmaps from the cell grid; Landscape import is editor-assisted | 🟡 |
| Macro / Micro / Habitat / Context biome tag layering | `FQRWorldCell` carries MacroBiome + MicroBiome + HabitatFlags + DepthBand per cell | ✅ |
| Canonical biome list (BasaltShelf, WindPlains, WetBasins, etc.) | All 14 created by `qr_seed_biome_profiles.py`; the subsystem assigns them per depth band | ✅ |
| Traversal validation from PlayerStart to each depth band | None | ❌ |
| Depth-band gating (surface → mid → deep → remnant) | `EQRDepthBand` per cell in concentric rings; gameplay gating on the band is partial | 🟡 |
| Place order: Remnant → faction capital → wrecks → minor POIs → fauna | `UQRWorldGenSubsystem::PlacePOIs` + `AQRWorldGenSpawner` place in canonical order | ✅ |
| Navmesh + chunk IDs + discovery + save deltas | World partition / save deltas: missing | ❌ |

**Worldgen Phase 1 + 2 are built** (post-dates the 2026-05-15 audit).
`UQRWorldGenSubsystem` generates the biome-tagged cell grid + POI plan;
`AQRWorldGenSeedActor` drives it; `AQRWorldGenSpawner` spawns POIs /
wrecks / caves / fauna; `AQRProceduralScatterActor` handles flora;
`qr_generate_heightmap.py` bakes the terrain heightmap + biome
weightmaps. Open gaps: programmatic Landscape import, traversal
validation, world-partition streaming.

### Canonical biome list (GDD §4 table) — implementation alignment

All 14 canonical biomes ship as `UQRBiomeProfile` assets created by
`qr_seed_biome_profiles.py` under `/Game/QuietRift/Data/Biomes/`:

- **Surface** — BasaltShelf, WindPlains, MeltlineEdges, CraterFloors
- **Mid** — WetBasins, ShallowFens, ThermalCracks, GlassDunes, MossFields
- **Deep** — MagneticRidges, HighRims, ColdBasins, CanyonWebs, RidgeShadows

Each profile carries its depth band, a scatter palette (tier-correct
trees + themed plants), suggested density, and a landscape material.
The legacy placeholders (AlienJungle / PolarTundra / DesertSand) are
deleted on seeder run.

---

## C. Flora & fauna — Visual World Bible §5–6 + GDD §14

### Trees (depth-progressed wood sources)

| GDD species | Biome | Tier / role | Implementation |
|---|---|---|---|
| Glassbark | BasaltShelf, MeltlineEdges, WindPlains | Surface / general wood | 🟡 `SM_TRE_GLASSBARK` mesh in Surface biome palettes; no item def |
| Velvetspine | WindPlains, RidgeShadows | Mid / fibrous long-grain | ❌ no mesh |
| Slagroot | ThermalCracks, CraterFloors | Mid / dense structural | 🟡 `SM_TRE_SLAGROOT` mesh in Mid biome palettes; no item def |
| Asterbark | MagneticRidges, HighRims | **Deep / premium** | 🟡 `SM_TRE_ASTERBARK` mesh in Deep biome palettes; no item def |

The "trees get progressively different as you go deeper" thread —
canonically Glassbark→Velvetspine→Slagroot→Asterbark with palette
shift visible at distance. Three of the four tree meshes now exist
(`SM_TRE_GLASSBARK / SLAGROOT / ASTERBARK`) and `qr_seed_biome_profiles.py`
wires them into the biome scatter palettes by depth band; Velvetspine
has no mesh yet. Still missing: `UQRItemDefinition`s so harvested wood
becomes an inventory item.

### Flora pool (Visual World Bible §5, 17 plants + 4 trees)

Lattice Bulb / Spiral Reed / Meltpod Rind / Ember Lace / Nullmint Nodes
/ Knifeleaf Fan / Resin Chimney / Silk Cyst Vine / Mawcap Bloom /
Threadmold Sheet / Ferric Bloom / Crystal Lichen / Cinder Thorn /
Ironbrine Cups / (+ trees above).

| Implementation | Status |
|---|---|
| `DT_Species_Flora.csv` exists w/ 9 plants (Smokebark, Blackstem, etc.) | 🟡 legacy names from pre-v1.5 |
| Item definitions for each as `UQRItemDefinition` | ❌ |
| Edibility state machine (Unknown → Observed → Sampled → Researched) | `EQREdibilityState` enum exists; no UI / state advance code | 🔧 |

### Fauna (Visual World Bible §6, ~17 animals + 9 predators + 4 mounts)

| GDD species | Implementation |
|---|---|
| Pebble Skitter, Gleam Larver, Shardback Grazer, Silt Strider, Pillarback Hauler, Basin Treader, Nestweaver Drifter, Milkbladder Herdling, Bone Lantern, Latchfin Mite, Crackrunner | ❌ no actors, no mesh refs |
| Ridge Courser, Vaultback Dray, Tetherback Packgrazer (mounts) | ❌ no mount system |
| Suture Wisp, Needle Maw, Drift Stalker, Vane Rippers, Glassjaw Cluster, Silt Hounds, Trench Diggers, Carrion Choir, Fogleech Swarm, Ironstag Stalker, Shellmaw Ambusher (predators) | ❌ no AI |
| `AQRWildlifeActor` placeholder for everything | ✅ generic single-class stub |
| `DT_Species_Wildlife.csv` with 9 placeholder rows | 🟡 legacy names |

**Elite variants** (Mawcap Bloom Prime, Pillarback Titan, Ridge Courser
Stormline, Vaultback Dray Bastion, Suture Wisp Prime, Vane Rippers
Galepack, Trench Diggers Sapper): ❌ no system.

---

## D. Player systems

### Character & input

| GDD item | Implementation | Status |
|---|---|---|
| FP character with vault / lean / sprint / crouch / interact | `AQRCharacter`, full input map | ✅ |
| ADS, sway, FOV punch, lean | `UQRFPViewComponent` | ✅ |
| Identity (name + pronouns + voice) | `FQRPlayerIdentity`, `UQRPronounLibrary` | ✅ |
| Character appearance customizer | `FQRCharacterAppearance` struct + library | 🔧 (no widget) |
| Spatial inventory (Body / ChestRig / Backpack grids) | `UQRInventoryComponent`, full spatial placement + rotate + move | ✅ |
| Inventory grid UMG | `UQRInventoryGridWidget` (I key) | ✅ Post |
| Hotbar (1–9 + scroll) | `UQRHotbarComponent` + `UQRHotbarHUDWidget` | ✅ |
| Hand slot states (Empty / Equipped / Shoulder-stacked / Bulk) | `EQRHandsSlotState` + replicated logic | ✅ |
| Encumbrance + sprint cutoff at E≥0.85 | `UQRInventoryComponent::IsOverEncumbered` | ✅ |
| Footstep cadence + surface SFX | `AQRCharacter::Tick` + `QRUISound::PlayFootstep` | ✅ Post |

### Survival vitals

| GDD item | Implementation | Status |
|---|---|---|
| Health, Hunger, Thirst, Fatigue, Oxygen, CoreTemperature | `UQRSurvivalComponent` | ✅ |
| Vitals HUD | `UQRVitalsHUDWidget` | ✅ Post |
| Eat / heal / drink / rest APIs | `ConsumeFood`, `ApplyHealing`, `DrinkWater`, `Rest` | ✅ |
| Death + respawn flow | `OnDied` → `AQRGameMode::HandlePlayerDied` → death screen → timer → `RestartPlayer` | ✅ Post |
| Injuries (Bleed / Fracture / Infection / Burn / Hypothermia / Heatstroke) | `EQRInjuryType` enum + `AddInjury / TreatInjury` | ✅ |
| Food edibility state machine | enum + `FoodOriginClass` + `bIsBulkItem` + `PackageIntegrity` | 🟡 (state advance code missing) |
| Status tags (Bleeding / Hungry / etc.) | `FGameplayTagContainer ActiveStatusTags` | ✅ |

### Combat

| GDD item | Implementation | Status |
|---|---|---|
| Fire / reload / jam / fouling / suppressor | `UQRWeaponComponent` full system | ✅ |
| Spread cone + recoil + ADS multiplier | `TryFireFromTrace` | ✅ |
| Muzzle flash + impact + tracer Niagara FX | Replicated multicast | ✅ Post |
| Weapon fire SFX | Free_Sounds_Pack Gunshot_1-1 | ✅ Post |
| Reload anim notify | `UQRReloadFinishNotify` | ✅ Post |
| Long-range scope / optics (v8 patch) | Optics calibration in encyclopedia; **no zoom-scope code** | ❌ |
| Attachments | `DT_ArmoryAttachments.csv` + `bHasSuppressor` flag; no slotting UI | 🟡 |
| Ammo types (Standard / Dirty / Match) | `DirtyAmmoFoulingMult` exists; no ammo type enum on instance | 🟡 |

### Build mode

| GDD item | Implementation | Status |
|---|---|---|
| Ghost placement + snap + rotate + cancel | `UQRBuildModeComponent` | ✅ |
| BuildPieceCatalog DataTable | `FQRBuildPieceRow`, seeder populates 19 BLD_ pieces | ✅ |
| Build piece selector widget | `UQRBuildPieceSelectorWidget` | ✅ Post |
| Buildable health / repair / decay | `FQRBuildableSaveData::Health`; **no upkeep code** | 🟡 |
| Snap profiles per piece (Wall / Floor / Door / Roof / Structural) | `EQRBuildCategory`; snap logic in build component | ✅ |
| Storage stations w/ pull radius + priority | `FQRBuildableSaveData::StoredItems` + Vault component; no auto-hauler | 🟡 |

### Vault / parkour

| `UQRVaultComponent` (vault over obstacles in jump) | ✅ |

---

## E. Crafting, stations, research

| GDD item | Implementation | Status |
|---|---|---|
| Crafting queue + ingredient consume + station tag check | `UQRCraftingComponent` | ✅ |
| Recipe DataTable | `DT_Recipes.csv` (extensive content) + `FQRRecipeTableRow` | ✅ (data) / 🟡 (some recipes not exposed via DataAssets) |
| Crafting bench actor + widget | `AQRCraftingBench` + `UQRCraftingWidget` | ✅ Post |
| Station list: Workbench, LogYard, Pantry, AnvilForge, Generator | Encyclopedia + DT_ReferenceComponents; **only Workbench-equivalent shipped via AQRCraftingBench** | 🔧 |
| Tech tree (TechNodes + unlock by Reference Components) | `UQRResearchComponent` (261 lines cpp) + `DT_TechNodes.csv` | ✅ |
| Micro-research stacking (repeatable family research) | `MicroResearchFinalScalar` math + DT_MicroResearch* CSVs; no scheduler code | 🟡 |
| Passive learning (carcass / plant observation) | Tags exist; no observer code | ❌ |
| Reference Components (haul physical objects to unlock) | concept defined; **no "I just hauled a regulator home" event** | ❌ |

---

## F. NPCs, leadership, AI

| GDD item | Implementation | Status |
|---|---|---|
| NPC base class | `AQRNPCActor` + `AQRNPCSpawner` | ✅ Post |
| NPC role component | `UQRNPCRoleComponent` | ✅ |
| Leader component (Aptitudes + Morale + Issues) | `UQRLeaderComponent` + `FQRLeaderSaveData` | ✅ |
| Faction component | `UQRFactionComponent` | ✅ |
| Dialogue | `UQRDialogueComponent` + `UQRDialogueWidget` + pronoun substitution | ✅ |
| AI Behavior Tree | None | ❌ |
| Worker / labor scheduling (assignment by role-fit) | `EQRNPCRole` enum; no scheduler | ❌ |
| Hauling / depot pull (auto-move stockpile → station) | Concept in DT_VariablesCanonical; **no implementation** | ❌ |
| Civilian raid response (panic / fight / hide) | `EFearState` (engine), no civilian code | ❌ |
| Leader directives → side missions | DT_MissionsLeaderDirectives.csv exists; no mission generator | 🟡 |

---

## G. Factions & Concordat — Master GDD §13

| GDD item | Implementation | Status |
|---|---|---|
| Vanguard Concordat as hardcoded mega-faction | `AQRVanguardColony` actor (located by class) | ✅ |
| Satellite outposts (rings of difficulty) | None — single Concordat actor only | ❌ |
| Raid scheduler (weather-aware, base-aware) | `AQRRaidScheduler` forward-decl; **no implementation** | ❌ |
| Faction contracts (DT_MissionsFactionContracts) | Data row exists; no contract system | 🟡 |
| Hostility decay + cooldowns | `VanguardConcordat->AdvanceTime` ticked from GameMode | ✅ |
| Voss as leader (Fanatic-tier guard) | Lore only | 🔧 |

---

## H. Remnants — Master GDD §13 (lower half)

| GDD item | Implementation | Status |
|---|---|---|
| Signal Spires / Power Cores / Data Archives / Resonance Chambers | DT_Items_Master items exist (REM_*); **no actor classes** | 🟡 |
| Wake states: Dormant → Stirring → Active → Hostile → Subsiding | Enum scaffolding implicit in gameplay tags; no FSM | ❌ |
| Artifacts: Data Shards / Power Cells / Signal Fragments / Memory Cores | Item definitions exist (REM_ART_*) | 🟡 |
| Codex lore unlock on Memory Core pickup | None | ❌ |

---

## I. Save / persistence — Master GDD §16

| GDD item | Implementation | Status |
|---|---|---|
| Save player vitals + inventory + identity + location | `AQRGameMode::QuickSave` | ✅ Post |
| Restore on load | `ApplyLoadedDataToPlayer` | ✅ Post |
| Periodic autosave (5 min default) | Timer in BeginPlay | ✅ Post |
| Logout / EndPlay autosave | Hooks | ✅ Post |
| Co-op save authority (listen-server / dedi) | Single slot for v1 (per-PC slots later) | 🟡 |
| Looted-container registry persistence | `UQRLootedRegistry` + `FQRGameSaveData.LootedContainerIds` field | 🟡 (no save/load glue yet) |
| Buildable persistence | `FQRBuildableSaveData` + `ColonyBuildables` array; no save/load glue | 🟡 |
| Chunk delta saves | `FQRChunkDelta` defined; no use | ❌ |
| Save migration (version → version) | `MigrateToCurrentVersion` stub | 🔧 |

---

## J. UI / UX — Master GDD §15

| GDD item | Implementation | Status |
|---|---|---|
| Hotbar + vitals HUD | both shipped | ✅ Post |
| Inventory grid (Tarkov-style) | shipped | ✅ Post |
| Pause / Main / Settings menus | shipped | ✅ Post |
| Crafting widget | shipped | ✅ Post |
| Dialogue widget | shipped | ✅ Post |
| Build piece selector | shipped | ✅ Post |
| Death screen | shipped | ✅ Post |
| Creative-mode item browser (dev) | shipped | ✅ Post |
| Codex UI (species discovery / lore) | None | ❌ |
| Codex tracking back-end | `EQREdibilityState` + Status tags; **no aggregator** | 🟡 |
| Damage / hit indicator | None (audio only) | 🟡 |
| Mini-map / compass | None | ❌ |
| Crafting requirements explanation (CanCraft reason text) | `CanCraft(OutReason)` exists; widget doesn't display | 🟡 |
| Settings: sensitivity / FOV / volume | shipped | ✅ Post |
| Co-op lobby / session UI | None | ❌ |

---

## K. Audio — Visual World Bible §2

| GDD item | Implementation | Status |
|---|---|---|
| UI feedback (click / confirm / deny) | `QRUISound` helper | ✅ Post |
| Surface-aware footstep cues (10 surfaces × gait) | `QRUISound::PlayFootstep` w/ EQRFootSurface + EQRFootGait | ✅ Post |
| Weapon fire SFX | Gunshot_1-1 wired to multicast | ✅ Post |
| Hit / death cries | PlayHitImpact + PlayDeathCry | ✅ Post |
| Ambient loops per biome (wind / rain / birds / drone) | `UQRBiomeProfile.AmbientLoop` field; no actor that plays it on biome entry | 🟡 |
| Predator audio signatures | None | ❌ |

---

## L. Procedural generation — POST-GDD addition

| Implementation feature | Notes | Status |
|---|---|---|
| `UQRWorldGenSubsystem` | Seed → 14-biome cell grid + depth bands + POI plan + minimap texture | ✅ Post |
| `AQRWorldGenSeedActor` | Editor-button driver — Generate + ExportMinimap | ✅ Post |
| `AQRWorldGenSpawner` | Spawns POIs / wrecks / caves / fauna from the POI plan | ✅ Post |
| `AQRProceduralScatterActor` | Box volume + palette + slope/trace + HISM packing; biome-aware via the subsystem | ✅ Post |
| `UQRBiomeProfile` data asset | Palette + density + landscape mat + sky + ambient | ✅ Post |
| `AQRBiomeZone` | Designer-placed biome override zone (priority over the worldgen cell) | ✅ Post |
| `qr_seed_biome_profiles.py` | Creates all 14 canonical biome profiles | ✅ Post |
| `qr_generate_heightmap.py` | Bakes heightmap + per-biome ground weightmaps from the cell grid | ✅ Post |
| `qr_create_proc_world_map.py` | Builds L_ProcTest with one scatter + ScifiJungle PCG manager | ✅ Post |
| ScifiJungle PCG integration | Coexists with scatter; works alongside | ✅ |

**Open worldgen gaps:**
- Programmatic Landscape heightmap import (currently editor-assisted)
- Landscape layer-blend material consuming the biome weightmaps
- Traversal validation PlayerStart → each depth band
- World-partition streaming + chunk delta saves

---

## M. Things in CODE that aren't yet in any GDD (post-additions)

These were built during implementation but don't yet appear in the
GDDs. Each should either get a GDD entry or be marked as
implementation-only detail:

1. **Programmatic C++ UMG pattern** — every widget is constructed in
   C++ via `WidgetTree` so the project plays without WBP authoring.
   GDD §15 should note the swap-in pattern (designer subclasses to
   replace with polished WBP_).
2. **Editor Python script pipeline** — 9 scripts under
   `Tools/EditorScripts/` automate import / material assignment /
   anim retarget / DataTable seeding / map creation / biome profiles.
   Should be acknowledged in §17 (Implementation Backbone) as the
   canonical asset-pipeline.
3. **Fab pack borrowing** — 40 Fab packs provide stand-in materials,
   sounds, anims, FX. The GDD assumes final art; we have placeholder
   art now. Worth a §17 sub-section "Asset stand-ins (pre-final-art)".
4. **`UQRBiomeProfile` data asset** — concrete vehicle for the
   biome catalog in GDD §4. Replaces / formalizes the rows in the
   biome catalog table.
5. **Footstep surface enum mapped to Project Settings Physical
   Surfaces** — implementation detail of audio §K.
6. **Reload anim notify** + **player AnimInstance C++ class** —
   bridge between Master GDD §17 (Data Asset Plan) and actual
   animation authoring. Worth a "ABP_QRPlayer parent class is
   UQRPlayerAnimInstance" line in §17.
7. **Save/load lifecycle hooks** (EndPlay / Logout / 5min timer) —
   §16 describes save data structure but not when save runs.
   Should add a "save triggers" sub-section.
8. **Manual editor tasks checklist** — `MANUAL_EDITOR_TASKS.md`
   exists as a build doc. Worth pointing at from §17.

---

## N. The biggest open gaps — priority order (reconciled 2026-07-08)

**Doc state warning:** reconciled twice — 2026-05-24 (survey) and
2026-07-08 (this section had drifted behind CLAUDE.md: the 2026-06-11
sprint closed items 2, 4-9 but only CLAUDE.md said so). CLAUDE.md's
"Active priorities" mirrors this list; both move together.

### Genuinely missing (work to do, in priority order)

1. **AI behavior trees** — 🟡 partial, biggest chunk closed 2026-07-08.
   Wildlife: `AQRWildlifeAIController` code-only FSM (4 Hz think,
   NavMesh `MoveToLocation`, role branching, herd alert, per-species
   sizing/gravity/damage — see 2026-06-05 notes in git history).
   NPC villagers: `UQRNPCBrainComponent` wander/work/sleep/socialize
   FSM + single-node anims (idle/walk/run/sleep/work/talk/death/attack).
   **NEW 2026-07-08:** colonist JOB AI — farmers harvest/replant
   `AQRFarmPlotActor` tiles + deposit to depots + feed husbandry
   animals; guards patrol build-piece waypoint loops; medics triage +
   heal the most-wounded survival component in range. Raiders: the
   brain yields movement to `UQRRaidPartyAI` (lazy re-probe — the FSM
   attaches post-spawn) while painting anims, and flashes the melee
   swing (`FlashAttack`) on each attack tick. Wildlife can now carry
   skinned bodies + single-node anims via `DefaultBodyMesh`/Idle/Walk/
   Run/DeathAnim slots on `AQRWildlifeBase` (first user:
   `AQRWildlife_ColonyDog`, fully skinned German Shepherd).
   **Still missing:** leader-level BTs, herd-route data driving,
   predator pressure-pull weighting, dog follow/guard behavior.
2. **Mission generator + RewardSourceValidation** — ✅ closed 2026-06-11.
   `InstantiateMission` runs the `MissionLocationFallbackRule` cascade;
   `GrantRewards` enforces No-Pocket-OP per `EQRRewardSource`;
   `UQRMissionHUDWidget` surfaces active missions.
3. **Hauler / depot pull logic** — ✅ v1 closed 2026-06-11
   (`GetCurrentDemandItem` drives demand). v2 open:
   `StorageDeficitMod` weighting across multiple stalled stations.
4. **Long-range optics + sniper (patch v8)** — ✅ closed 2026-06-11.
   `WPN_LONGRANGE_SNIPER` in `ConfigureForWeaponId`;
   `UQRFPViewComponent::ScopeZoomMultiplier` gives 8X/16X tiers.
5. **Cross-contamination crop mutation** — ✅ closed 2026-06-11.
   `AQRFarmPlotActor` runs `UQRMath::CrossContamExposure` per grow
   cycle, branches yield to `MUT_<original>`, propagates SporeLoad.
   2026-07-08: farmer NPCs now actually drive the loop (harvest/replant).
6. **Mount husbandry loop** — ✅ closed 2026-06-11.
   `UQRMountHusbandryComponent` (BaseTameDays, stress pool, panic);
   Courser + Dray subclassed. 2026-07-08: farmer NPCs FeedOrPet daily.
7. **Leader directive chains + Moral Compass** — ✅ closed 2026-06-11
   (chain side): `OnQuestIssued` fires at Escalating→QuestIssued;
   `IssueDirectiveMission` materializes a family-matched mission.
8. **Faction raid experience bands** — ✅ closed 2026-06-11.
   `FQRRaidPlan::Experience` via `DetermineRaidTier`; raid AI restamps
   perception/speed/damage/retreat per tier.
9. **Civilian Fight mode** — ✅ closed 2026-06-11. Fight state
   acquires nearest live raider, fires on `FireIntervalSeconds`.
10. **Codex save persistence** — 🟡 partial (2026-06-11). Save v2
    persists `UQRResearchComponent` state (tech nodes, micro-research,
    `CodexStates`) via `FQRSaveSnapshot`. Still missing: the game-
    module `UQRCodexSubsystem::Entries` map (SeenCount / FirstSeen /
    per-entry state) — it lives in the game module so QRSaveNet can't
    reference its struct; needs an export/import pair like
    `UQRLootedRegistry` uses.
11. **Co-op transaction-ID safety net** — GDD demands server-authored
    transaction IDs on every inventory mutation to prevent dupes.
    Standard UE replication is used; no transaction-ID layer yet.
12. **Programmatic Landscape import** — heightmap/weightmap bake to
    disk; importing into a Landscape actor is still editor-assisted.
13. **World partition streaming + chunk delta saves** — 🟡 the VISUAL
    side is addressed 2026-07-08 by `AQRDressingStreamer` (a moving
    (2R+1)² scatter-tile bubble that follows the player, one tile per
    0.25 s, deterministic per-tile seeds — the whole 64 km map reads
    dressed at ~9k live instances). True World Partition actor
    streaming + chunk delta SAVES remain open.

### Build-blockers (urgent — gameplay fails without these)

- **No NavMesh on any test level** — AI components exist but can't
  path. Manual editor task: drop `NavMeshBoundsVolume` on
  `L_DevTest`. Now scriptable via `qr_dev_test_dressup.py`.
- **AnimBP state machine empty** — deferred 2026-06-05 and largely
  moot since 2026-07-02: the game is first-person (own body hidden via
  `OwnerNoSee`) and the third-person body + all NPCs/wildlife now
  animate through single-node `PlayAnimation` (no AnimBP needed).
  Revisit only if co-op testing shows the TP body needs blending.
- ~~Buildable + looted-container save/load glue~~ — ✅ closed
  2026-06-11 (`UQRBuildModeComponent::RestoreFromSave` +
  `LootedContainerIds`).
- **DataTable rows seeded but empty** — `DT_BuildCatalog`,
  `DT_Recipes`, `DT_NPC_Greetings`, `DT_LootTables`. Now bulk-seeded
  by `qr_seed_starter_datatables.py`. **User note 2026-06:** CSVs may
  not be IMPORTED into the editor yet — run `qr_import_datatables.py`
  then reimport in UE.

### Already built (no longer count as gaps — fix the prior priority list)

| Was listed missing | Actually in code |
|---|---|
| Codex aggregator + UI | `UQRCodexSubsystem` + `UQRCodexWidget` (K key) |
| Mission director | `UQRMissionDirector` (template-fed, runtime-active) |
| Remnant wake-state FSM | `AQRRemnantSite` 5-state FSM (Dormant→Warm→Active→Overclock→Dead) |
| Raid scheduler | `AQRRaidScheduler` (weather + concordat + opportunity scoring) |
| Faction component + camps | `UQRFactionComponent`, `AQRFactionCamp`, `UQRCampSimComponent` |
| Civilian reaction component | `UQRCivilianReactionComponent` (Fight mode no-op flagged separately) |
| Satellite outposts | `AQRSatelliteOutpost` placed by spawner |
| Hauler component | `UQRHaulerComponent` (hardcoded-item bug flagged separately) |

### Doc maintenance rule

**When a big gap closes, update this list in the same PR/commit.**
Stale priority lists are how teams waste time fixing already-done
things. CLAUDE.md mirrors the top of this list; update both.

---

## O. Glossary deltas (legacy → v1.5 canonical)

Some terms changed across GDD revisions. Code currently mixes both.
This list is what to search-and-replace when normalizing:

| Legacy (in code/CSV) | v1.5 canonical |
|---|---|
| Smokebark | Glassbark |
| Phase / VerticalSlice | Tier / Surface,Mid,Deep |
| AlienJungle (placeholder) | BasaltShelf / WindPlains / MeltlineEdges |
| PolarTundra (placeholder) | ColdBasins / IceCaves / HighRims |
| DesertSand (placeholder) | GlassDunes / SulfurRock |
| Ridge Courser Gale (alias) | Ridge Courser Stormline |
| Vaultback Dray Stonevault (alias) | Vaultback Dray Bastion |

---

## P. What I'm about to commit (this pass)

1. This document.
2. Updated `qr_seed_biome_profiles.py` to create the **14 canonical
   biomes** with depth-band metadata, replacing the 3 placeholders.
3. New `EQRDepthBand` enum + `DepthBand` field on `UQRBiomeProfile`
   for the GDD's "trees get progressively different as you go deeper"
   thread.
4. Updated `PROCEDURAL_WORLD_PLAN.md` to reference canonical biomes
   instead of placeholders.

Anything in sections N (biggest missing) is its own follow-up pass —
each is substantial enough to be a dedicated commit.
