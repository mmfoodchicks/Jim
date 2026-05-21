# Quiet Rift: Enigma — Claude session pointer

UE 5.7 first-person survival sim. Solo/co-op, 64km finite world on a Jovian
moon (2057 crash). C++ heavy — Blueprints are subclassable but the project
plays without WBP authoring because every widget is built in C++.

**If you're a fresh session: read this file, then `SYSTEM_COHESION_AUDIT.md`
and `GDD_IMPLEMENTATION_STATUS.md` before touching code.** Don't make the
user re-explain the project.

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

---

## Don't touch

- `Content/Fabs/**` and `FabsHierarchy.txt` — borrowed/generated Fab pack
  content. The `qr_audit_fabs.py` / `qr_wire_fab_packs.py` /
  `qr_repoint_fab_packs.py` scripts manage it.
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

## Active priorities (snapshot — update as work lands)

From `GDD_IMPLEMENTATION_STATUS.md` §N, biggest open gaps in priority order:

1. Worldgen pipeline (GDD §4) — data tunables exist, no actor generates
   a 64km biome-tagged world.
2. Biome catalog rename — code uses 3 placeholders, GDD defines 14
   canonical biomes (BasaltShelf, WindPlains, MeltlineEdges, etc.).
3. POI placement system — 16 archetypes in DT_POIArchetypes, no placer.
4. AI behavior trees — NPCs + wildlife + predators have no BT.
5. Hauler / depot pull logic — central economic loop.
6. Civilian raid response + emergency armory.
7. Long-range scope / optics (patch v8) not in code.
8. Codex aggregator + UI.
9. Mission generator from DT_ProceduralMissionTemplates.
10. Remnant wake-state FSM (Dormant→Stirring→Active→Hostile→Subsiding).
