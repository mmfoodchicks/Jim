# Quiet Rift: Enigma — Full-Game Deep Audit (2026-08-09)

Deep static "play the whole game through the code" run. 16 subsystem
slices were audited by parallel deep-read agents; 12 completed, and the
highest-impact findings below were then **re-verified by hand** against
the cited code before any fix. Four slices (worldgen/dressing,
missions/logistics, data-table integrity, editor-scripts/smoke) died on
session limits mid-run — re-run them (§F) after the fix batches land.

Legend: ✅ = verified by hand against the code · 🔎 = agent finding,
not yet independently re-verified · severity in [brackets].

---

## A. The big picture

The codebase's subtle math is mostly fine — the serious problems are
**wiring gaps**: complete, well-written systems that nothing ever
instantiates or calls. The game compiles and walks, but entire pillars
(weather, research, NPC damage, build confirmation, single-player
interaction, save-resume worldgen) are dead ends at runtime.

---

## B. Verified majors (fix batches A–E)

### Batch A — core wiring (single-player playability)

1. ✅ [high] `QRGameMode.cpp:78-83` — `ColonyState` / `Research` /
   `Weather` are only ever *looked up* on the GameState
   (`FindComponentByClass`); nothing anywhere creates them. Weather
   sim, research/tech gating, and colony aggregation are all inert.
   **Fix:** new `AQRGameState` with the three as default subobjects;
   set `GameStateClass` in the GameMode constructor.
2. ✅ [high] `QRCharacter.cpp:712-718` — on authority (single-player!)
   `TryInteract` only broadcasts `OnInteract`, which has **zero
   subscribers**. All real interact logic (dialogue start, loot,
   world-item pickup, crash-site breach) lives only in
   `Server_Interact_Implementation`, which never runs for the host.
   **Fix:** extract `DoInteract(Target)`, call from both paths.
3. ✅ [medium] `QRCharacter.cpp:694-709` — dialogue widget is mounted
   with no mouse cursor / input mode (bench branch sets them, dialogue
   branch doesn't) → Continue button unclickable.
4. ✅ [medium] `QRCharacter.cpp` `SetupPlayerInputComponent` — no
   Crouch binding at all (C key documented in SYSTEM_COHESION_AUDIT).
5. ✅ [medium] `QRCharacter.cpp:431-433` — footstep cadence/gait uses
   `Speed / CMC->MaxWalkSpeed`; MaxWalkSpeed *is* walk speed while
   walking, so SpeedAlpha ≈ 1 and walking always plays Run-gait
   footsteps at sprint cadence. **Fix:** alpha against WalkSpeed→SprintSpeed
   constants.
6. ✅ [medium] `QRCharacter.cpp:1164-1196` — Esc opens the pause menu,
   but while paused the PauseAction won't trigger (no
   `bTriggerWhenPaused`) so Esc can't close it.
7. ✅ [low] `QRCharacter.cpp:78` — `QR_Exposure(LockedExposureEV)` runs
   only in the constructor, before property serialization → the editor
   EV knob is dead. Re-apply in BeginPlay.
8. ✅ [medium, co-op] sprint (`SetSprinting`) and ADS (`TryUseHeld`)
   mutate replicated/server state client-side only; no RPC.
9. 🔎 [medium] `QRSettingsWidget.cpp` — closing Settings from the main
   menu restores game input (soft-lock); FOV slider pins
   PlayerCameraManager FOV (kills ADS zoom); sensitivity slider inert;
   saved settings never applied at boot.
10. 🔎 [medium] `QRCheatManager` never registered on any
    PlayerController → all QR.* cheats unreachable.
11. 🔎 [high] `Config/DefaultGame.ini` — GameDefaultMap / GameMode keys
    in the wrong INI section → packaged boot may not reach L_MainMenu.
    (PIE unaffected, which is why it wasn't noticed.)

### Batch B — killability (combat actually works)

12. ✅ [high] `QRNPCActor.h` — NPCs have Capsule/Mesh/Dialogue/Faction/
    Brain/Reaction and **no health model**; no `UQRSurvivalComponent`,
    no `TakeDamage` override. `UQRWeaponComponent::ApplyPelletDamage`
    needs one of the two → player bullets no-op on every NPC; raiders
    (spawned as plain `AQRNPCActor` by `AQRFactionCamp::HandleRaidLaunched`)
    are unkillable. Medic triage ("heal most-wounded survival
    component") is equally dead.
    **Fix:** add Survival to AQRNPCActor + route TakeDamage into it +
    death flow (brain death anim, corpse cleanup, raid-party removal).
13. ✅ [high] `QRWildlifeBase.cpp` `OnDied_Implementation` — plays
    anim/ragdoll/despawn but never rolls `DeathDrops`/`Harvest()` and
    never spawns loot items; killing any animal yields nothing, corpse
    despawns in ~20 s.
14. 🔎 [medium] `QRWildlifeActor.cpp` (legacy class) — `ReceiveDamage`
    has no callers and no TakeDamage bridge → unkillable; its death
    path is also the only caller of `ReportSpeciesKilled` → KillTarget
    missions can't progress.
15. 🔎 [medium] raider melee bypasses shield block and applies no
    injury type (`QRRaidPartyAI.cpp:186`).

### Batch C — save integrity

16. ✅ [high] `QRGameMode.cpp:124` + `:462` — resuming a save skips
    world bootstrap entirely, and `Data.WorldSeed` is hardcoded 0 ("BP
    fills") → load a save = barren world (no POIs/camps/fauna).
    **Fix:** capture the real seed; on resume, regenerate the world
    from the saved seed in `HandleLoadComplete`.
17. ✅ [high] `QuickSave` writes `bIsAlive = true` unconditionally; a
    save during the death window restores an unkillable 0-HP pawn.
18. ✅ [medium] load-in-flight race: BeginPlay starts async load;
    EndPlay/Logout/autosave can QuickSave fresh-spawn state over the
    good slot before the load applies.
19. ✅ [low] `QuickLoad`/BeginPlay `OnLoadComplete.AddUObject` stacks
    duplicate bindings (the "delegate dedupes" comment is false).
20. ✅ [medium] `bHasPendingLoadedData` never cleared → every later
    authoritative pawn BeginPlay re-applies the stale snapshot.
21. 🔎 [high] colonist job roles (`AQRNPCColonist::ColonistRole`) and
    raider-ness are not in `FQRNPCSaveData` → reload kills the job AI;
    autosave during a raid resurrects raiders as friendly villagers.
    Needs save v3 + migration.
22. 🔎 [medium] hotbar slots/active index have no Capture/Apply.
23. 🔎 [medium] `QRLootContainerComponent` blank UniqueId regenerates
    per session → looted-state never persists across restarts. Fix:
    derive stable id from actor path.
24. 🔎 [medium] farm plots, husbandry progress, faction-camp strategic
    state have no Capture/Apply (documented gap; schedule for v3+).
25. 🔎 [medium] `QRInventoryComponent.cpp:572` — spoilage advances in
    real hours vs the 20-min game day (~72× too slow).
26. 🔎 [medium] weapon ammo/fouling/jam state: `FQRWeaponSaveData`
    exists but is never captured (moot while creative unlimited-ammo
    is on; note for later).

### Batch D — build mode

27. ✅ [high] `TryConfirmPlacement` / `RotateGhost` / `ExitBuildMode`
    have **zero callers** (header says "BP calls" — no BP exists; the
    project rule is plays-without-BP). G enters build mode and you're
    stuck with a ghost forever. **Fix:** contextual dispatch — in build
    mode LMB confirms, R rotates 90°, G exits.
28. 🔎 [medium] snap/overlap queries use `AllStaticObjects` but placed
    pieces are WorldDynamic → no snapping/overlap rejection vs your own
    base.
29. 🔎 [medium] loading a save with 0 pieces skips the teardown pass
    (duplication); RestoreFromSave silently drops unresolvable pieces
    and the next autosave erases them permanently.
30. 🔎 [medium] no BLD_ rows in DT_Items_Master/DT_Recipes → build mode
    unreachable in survival play (creative-only today).

### Batch E — crafting / research / data

31. 🔎 [high] `qr_import_datatables.py` columns don't match
    `FQRRecipeTableRow` → DT_Recipes imports empty (crafting has no
    recipes at runtime even though the CSV is full).
32. 🔎 [high] `QRCraftingBench` has its own `UQRCraftingComponent` but
    no bridge to the *player's* inventory → bench can't consume player
    ingredients or grant outputs.
33. 🔎 [medium] `InitializeTechNodes` zero callers → tech tree empty at
    runtime; research points have no source.
34. 🔎 [medium] cancelling an in-flight craft destroys the consumed
    ingredients (no refund).
35. 🔎 [medium] micro-research charges cost once but grants up to
    MaxStacks.
36. 🔎 [medium] ~46 wildlife drop ids + flora yields + `MUT_<crop>`
    mutation outputs don't exist in DT_Items_Master.csv → dead drops
    when B-batch makes loot real. Add rows (raw drops need no recipes;
    mirror rule N/A).

### Deferred within this pass (verified or plausible, not batched)

- Sky sun-cycle quarter-day phase vs `bIsNight` (`QRSkyManager.cpp:184`) — visual/schedule mismatch.
- `QRCampSimComponent` hostility gate unreachable / raid outcome
  accounting / dead FallbackLeadership / `FindFProperty("bEventActive")`
  on a property that doesn't exist (component is in the same module —
  include it directly).
- `QRRaidScheduler` one-shot latch (nothing calls EndRaid); hardcoded
  time scale.
- `QRRaidPartyAI` Z-locked MoveToward vs 3D arrival check → stalls on
  slopes.
- Civilian Fight/Flee unreachable (always Hide); raiders carry civilian
  Reaction components that fight the raid FSM.
- Wildlife: herd alert dead (HerdGroupId never set), injured-flee
  collapses in 0.25 s, perception only sees player 1, death not
  replicated to clients, scent system fully inert.
- Vault: accepts 140 cm obstacles but launch reaches ~117 cm.
- Runtime IMC priority inverted (50 vs 0) — authored BP context can't
  override runtime defaults.
- `UQRPlayerAnimInstance` direction sign wrong on diagonals.
- Co-op replication family: HUD-on-client creation, world-item OnRep
  mesh, sky/weather managers server-only, NPC brains simulating on
  clients, death RepNotify, client ammo HUD refresh.
- Item exploits: drop-then-pickup resets durability/spoil; equip slot
  duplication (worn+held); zero-quantity food ghost in hand.

## F. Not yet audited (agents died on session limits)

Re-run these four slices after the fix batches:
worldgen/dressing · missions/logistics · data-table cross-integrity ·
editor-scripts + smoke-sim.

## G. Fix progress (updated 2026-08-09, same day)

All five batches LANDED on `claude/unreal-cpp-blueprint-catchup-nGaRb`:

- **audit A** (`9887e1e6`) — items 1–11 ✅fixed: AQRGameState carries the
  three components; SP interact via DoInteract; dialogue cursor +
  widget toggle; crouch; footstep gait; Esc-close pause; exposure knob;
  sprint RPC + client ADS preview; settings FOV/sensitivity/boot-apply
  + main-menu soft-lock; cheat manager registration; GameMapsSettings.
- **audit B** (`405b1cf1`) — items 12–14 ✅fixed: NPC survival component
  + TakeDamage + death flow; wildlife death loot + mission report;
  legacy wildlife TakeDamage bridge. (15 raider-melee shield/injury
  still open.)
- **audit C** (`055e9712`) — items 16–23 ✅fixed: resume regenerates the
  world from the saved seed; dead/in-flight save guards; bind-once +
  pending-clear; save v3 (colonist roles, hotbar, raider/corpse skip);
  stable loot-container ids; spoil game-hours. (24 farm/husbandry/camp
  capture and 26 weapon-state capture still open.)
- **audit D** (`a4715981`) — items 27–29 ✅fixed: LMB confirm / R rotate /
  G exit; WorldStatic object type + widened overlap queries;
  unrestorable-piece carry-forward; unconditional teardown. (30 BLD_
  survival-mode rows still open — creative-only today.)
- **audit E** (`723b059a`) — items 31–32, 34–35 ✅fixed: recipe CSV
  design→struct converter (216/243 rows convert; 27 data gaps in the
  report file); bench pulls/delivers via the player inventory; cancel
  refund; micro-research pay-per-stack. (33 InitializeTechNodes
  population and 36 missing drop-item rows still open.)

**After pulling:** rerun `qr_import_datatables.py` → `run(overwrite=True)`
so DT_Recipes re-imports through the converter, then Ctrl+B — five
batches of C++ changed; paste any compile errors.

**Still open, in priority order:** items 24, 26, 30, 33; species-CSV
legacy naming (§O glossary); FindDepotWithItem priority-vs-distance
ordering; escort mission family has no progress source; depot
StoredItems subobject replication (co-op); DressingStreamer tile-Z
window; remnant wake-chain data (RiftTechNodeProgression empty);
save doesn't capture map size/cell size alongside the seed.

### Second pass, same day — batches F–I (all four remaining slices done)

- **audit F+G** (`70ded3fc`) — raids reachable end-to-end (hostility
  creep, outcome accounting, scheduler latch/timescale, ground-follow
  pathing, villager targeting, melee injury); sky phase aligned with
  the clock; wildlife herd ids assigned, alert/blind-flee fixed;
  item 15 ✅fixed (melee injury type).
- **audit H** (`7749f4c6`) — worldgen slice findings fixed: hazard-belt
  scaling (root cause of "New Game is an empty floor"), same-archetype
  POI spacing with retries (remnants + satellites were mathematically
  unplaceable), crash/hauler asset-registry item resolution (wrecks
  scattered zero loot; haulers vaporized depot stock), once-per-save
  wreck scatter, corrupt-save bootstrap fallback, seeded POI yaw,
  ConcordatCapitalClass default. Data: DT_TechNodes.csv 15 malformed
  rows repaired; qr_seed_drop_items.py creates the 112 dangling drop/
  harvest/loot item definitions — item 36 ✅fixed.
- **audit I** — missions live: MissionTemplateTable loaded from the new
  struct-format DT_MissionTemplates.csv (8 starter templates; the
  design-format CSV remains as the design doc), RollNewMission on a
  120s cadence, ranged-for mutation crashes fixed in all three
  progress handlers, NPC deaths report kills. Logistics: hauler
  component finally attached (Hauler-role colonists), bench Storage
  inventory + multi-source crafting counting/consumption so hauler
  deliveries are visible to CanCraft.
- Editor-scripts + smoke slices re-run inline: all 37 scripts parse,
  all promised entry points exist, pack paths resolve (the four
  /Game/Characters|Mannequins hits are engine-template mesh probes in
  optional scripts with fallbacks — not defects).
