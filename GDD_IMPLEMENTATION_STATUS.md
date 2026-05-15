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
| 64 km finite world + hazard belt + hard wall | `DT_VariablesWorldgen.csv` defines tunables; no worldgen actor | 🔧 |
| WorldSeed → BiomeSeed / POISeed / EcologySeed / FactionSeed / LootSeed | `AQRProceduralScatterActor::Seed` field (single seed only) | 🟡 |
| Macro terrain generation pipeline | None — relies on hand-painted Landscape + MWLandscapeAutoMaterial | ❌ |
| Macro / Micro / Habitat / Context biome tag layering | `UQRBiomeProfile.BiomeTag` exists (one tag per profile) | 🟡 |
| Canonical biome list (BasaltShelf, WindPlains, WetBasins, etc.) | **Mismatch** — our profiles use placeholder names (AlienJungle, PolarTundra, DesertSand) | ❌ |
| Traversal validation from PlayerStart to each depth band | None | ❌ |
| Depth-band gating (surface → mid → deep → remnant) | None | ❌ |
| Place order: Remnant → faction capital → wrecks → minor POIs → flora → fauna → predators | None — scatter actor is flat random | ❌ |
| Navmesh + chunk IDs + discovery + save deltas | World partition / save deltas: missing | ❌ |

**This is the largest gap.** GDD §4 describes a full worldgen pipeline.
We have one stochastic scatter actor + biome data assets — nothing that
generates terrain shape, places POIs in depth bands, or validates
connectivity.

### Canonical biome list (GDD §4 table) — implementation alignment

| GDD biome | Our biome profile asset | Status |
|---|---|---|
| BasaltShelf | (placeholder `BP_AlienJungle`) | ❌ rename |
| WindPlains | none | ❌ |
| MeltlineEdges | none | ❌ |
| WetBasins / ShadowFens | none | ❌ |
| GlassDunes | none | ❌ |
| ThermalCracks / SteamVents | none | ❌ |
| RazorstoneRidge / MagneticRidges | none | ❌ |
| CanyonWebs / IceCaves | none | ❌ |
| ColdBasins | (placeholder `BP_PolarTundra`) | ❌ rename |
| CraterFloors / CraterWalls | none | ❌ |
| HighRims | none | ❌ |
| MossFields | none | ❌ |
| RidgeShadows | none | ❌ |
| ShallowFens | none | ❌ |

**Next pass:** rewrite `qr_seed_biome_profiles.py` to create the 14
canonical biomes with their GDD flora/fauna/predator pools.

---

## C. Flora & fauna — Visual World Bible §5–6 + GDD §14

### Trees (depth-progressed wood sources)

| GDD species | Biome | Tier / role | Implementation |
|---|---|---|---|
| Glassbark | BasaltShelf, MeltlineEdges, WindPlains | Surface / general wood | ❌ no mesh, no item |
| Velvetspine | WindPlains, RidgeShadows | Mid / fibrous long-grain | ❌ |
| Slagroot | ThermalCracks, CraterFloors | Mid / dense structural | ❌ |
| Asterbark | MagneticRidges, HighRims | **Deep / premium** | ❌ |

The "trees get progressively different as you go deeper" thread —
canonically Glassbark→Velvetspine→Slagroot→Asterbark with palette
shift visible at distance. **Currently we have no tree art for any of
these.** The Fab packs we have are stand-ins (Smokebark from
DT_Species_Flora is the legacy name, replaced by Glassbark in v1.5).

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

| Implementation feature | Notes |
|---|---|
| `AQRProceduralScatterActor` | Box volume + palette + slope check + ground trace + HISM packing | ✅ Post |
| `UQRBiomeProfile` data asset | Palette + density + landscape mat + sky + ambient | ✅ Post |
| `qr_create_proc_world_map.py` | Builds L_ProcTest with one scatter + ScifiJungle PCG manager | ✅ Post |
| `qr_seed_biome_profiles.py` | Creates 3 placeholder biome profiles | 🟡 wrong names — refactor next pass |
| ScifiJungle PCG integration | Coexists with scatter; works alongside | ✅ |

**Not yet built (Phase 7 of MANUAL_EDITOR_TASKS.md):**
- AQRBiomeZone — tag region detector switching sky / fog / ambient on entry
- UQRWorldGenSubsystem — biome streaming + per-band gating
- Heightmap procedural import — make terrain shape generative
- POI placer — flat-spot scan + prefab drop
- Wildlife density per biome — biome-aware spawn budget

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

## N. The biggest missing categories — priority order

1. **Worldgen pipeline (GDD §4)** — we have data tunables, no actor
   that actually generates a 64km biome-tagged world. Without this,
   every map is hand-painted.
2. **Biome catalog alignment** — code uses placeholder biomes; GDD
   defines 14 canonical biomes. Trivial to fix; pending next pass.
3. **POI placement system** — DT_POIArchetypes has 16 archetypes; no
   placer reads them.
4. **AI behavior trees** — NPC + wildlife + predator AI is entirely
   absent. AQRWildlifeActor wanders in 2D; AQRNPCActor stands still.
5. **Hauler / depot pull logic** — central economic loop of the
   colony; not implemented.
6. **Civilian raid response + emergency armory** — Master GDD §12.
7. **Long-range scope / optics** — added in patch v8; not in code.
8. **Codex aggregator + UI** — discovery system is half-coded
   (state tags exist), no central tracker or screen.
9. **Mission generator** — DT_ProceduralMissionTemplates exists,
   no generator code.
10. **Remnant wake-state FSM** — five-state system per remnant site.

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
