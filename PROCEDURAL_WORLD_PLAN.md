# Procedural World Plan — Quiet Rift: Enigma

How to build a generative world out of the 40 Fab packs already in
`Content/Fabs/`. Read top-to-bottom; cross items off as you go.

---

## 1. The pipeline (high level)

```
  ┌──────────────┐    ┌────────────────────┐    ┌─────────────────┐
  │  Landscape   │ →  │  Auto-blend mat    │ →  │  Biome zones    │
  │  (terrain)   │    │  (slope/height)    │    │  (region masks) │
  └──────────────┘    └────────────────────┘    └─────────────────┘
                                                         │
              ┌──────────────────────────────────────────┘
              ▼
  ┌──────────────────────┐     ┌─────────────────────────┐
  │  Foliage / clutter   │  +  │  POIs / structures /    │
  │  (per-biome scatter) │     │  wildlife / NPCs        │
  └──────────────────────┘     └─────────────────────────┘
              │                              │
              ▼                              ▼
        ┌─────────────────────────────────────────┐
        │  Atmosphere (sky / fog / weather / SFX) │
        └─────────────────────────────────────────┘
```

We have **all five layers** covered by the existing packs.

`UQRWorldGenSubsystem` now drives this pipeline end to end from a single
WorldSeed — see §5 for the build status.

---

## 2. Per-pack proc-gen evaluation

### Tier S — direct procedural systems (use these as the engine)

| Pack | What it does | How we use it |
|---|---|---|
| **ScifiJungle/PCG** | Full Unreal PCG system: `BP_PCG_Manager`, `BP_PCG_Biome_Bundled`, four prebuilt biome graphs (MountainCovering, Floating, Shore, Mountain), reusable subgraphs (`PCG_S_Placementer`, `PCGE_SurfaceRotation`). Drag the manager into a level, configure volume + biome, hit Generate — full forest appears. | **Use as-is for the Alien Jungle biome.** Adapt the biome graph's mesh references to swap in our other plant/rock packs for additional biomes. |
| **MWLandscapeAutoMaterial** | Master landscape material that auto-blends Grass / Dirt / Rock / Snow / Stones based on slope + altitude, plus 5 `LandscapeGrassType` assets (`LGT_MWAM_*`) that procedurally instance grass tufts on whichever layer is active. | **Default landscape material on every map.** Designer paints a heightmap; the material decides what surface shows where. |

### Tier A — biome content palettes (the "stuff to scatter")

| Pack | Best biome | Asset count |
|---|---|---|
| **OWD_Plants_Pack** | Alien Jungle, generic vegetation | 483 |
| **ScifiJungle** (non-PCG meshes) | Alien Jungle | 749 |
| **Polar** | Polar Tundra, ice caves | 301 |
| **WinterTown** | Polar Tundra, derelict cold settlements | 727 |
| **ROCKY_SAND_PACK** | Desert Sand, dry biome | 203 |
| **Rock_Collection_04** | Universal rocks | 41 |
| **WoodenProps** | Camp clutter, derelict fill | 210 |
| **Construction_VOL1** | Player-built bases (not really proc-gen) | 235 |
| **DeepWaterStation** | Sci-fi outpost POI | 400 |
| **Ruined_Modern_Buildings** | Crash-site debris POI | 300 |
| **IndustryPropsPack6** | Industrial outpost POI | 92 |

### Tier B — atmosphere + weather + FX

| Pack | Use |
|---|---|
| **Chaotic_Skies** | Per-biome sky materials (cite from `UQRBiomeProfile.SkyMaterial`) |
| **FogArea** | Drop fog volumes inside biome zones for mood |
| **LensFlareVFX** | Sun flare for outdoor scenes |
| **Polar/Particles** | Snow weather emitters |
| **WinterTown/Particles** | Winter ambient FX |
| **Vefects/Free_Fire** | Hazards, campfires, lava |
| **NiagaraExamples** | Generic FX library — pickup sparkles, footstep dust |
| **Free_Sounds_Pack** | Ambient loops (`Ambient_Wind`, `Ambient_Birds`, `Ambient_Rain`) per biome |

### Tier C — wildlife / NPCs / characters

| Pack | Use |
|---|---|
| **German_Shepherd_3D_Model** | Real wildlife — drop into AQRWildlifeActor as the Visual mesh |
| **DeadBodies_Poses_nikoff** | Crash-site corpses (death poses on Mannequin skeleton) |
| **FuturisticWarrior** | NPC mesh option |
| **QuantumCharacter** | Alt NPC mesh option |
| **MPMECH** | Optional mech actor — late-game elite enemy? |

### Tier D — animations (for whatever character ends up in the world)

| Pack | Use |
|---|---|
| **FuturisticWarrior/Animation** (43) | Player locomotion + combat (needs retarget — script ready) |
| **FreeAnimsMixPack** (115) | Mannequin-rigged combat / dance / death |
| **RamsterZ_FreeAnims_Volume1** (71) | Mannequin-rigged combat / paired interactions |
| **DynamicFalling** (10) | Roll / dodge transitions |
| **CombatMagicAnims** (35) | NOT useful — magic-themed |
| **FreeAnimationsPack** (10) | NPC ambient barks (eat / talk / rest) |

### Tier E — single-purpose helpers

| Pack | Use |
|---|---|
| **Bodycam_VHS_Effect** | Optional first-person post-process aesthetic |
| **PhoneSystem** | UI sound effects (already wired) |
| **LED_Generator** | Computer screen materials (interactive terminals later) |
| **ModernBridges** | One-off bridge meshes if a level needs them |
| **Horror_Props** | Crash-site grim props |
| **StampIt** | Decal stamping for floor / wall details |
| **SERLO_DeveloperTools** | Empty — skip |
| **MPMECH** | Empty meshes folder — skip the rig, the IKR_UE5Manny BP is useful as a retarget reference |

---

## 3. Step-by-step setup

After pulling the latest commits and running the full Phase 1 of
`MANUAL_EDITOR_TASKS.md`:

### 3a. Create the biome profiles
- [ ] In UE → Output Log → Python:
      `exec(open(r'<Project>/Tools/EditorScripts/qr_seed_biome_profiles.py').read())`
- [ ] Verify all 14 canonical assets created under
      `/Game/QuietRift/Data/Biomes/` (`BP_BasaltShelf`, `BP_WindPlains`,
      … `BP_RidgeShadows`).
- [ ] Open a few, scroll the Palette array. Trees + plants point at the
      real `SM_TRE_*` / `SM_PLT_*` meshes; rocks are still Fab
      stand-ins. The script logs any path that didn't resolve.

### 3b. Create a procedural test map
- [ ] In UE → Output Log → Python:
      `exec(open(r'<Project>/Tools/EditorScripts/qr_create_proc_world_map.py').read())`
- [ ] Open `/Game/Maps/L_ProcTest`. There's a 200m floor + lights +
      one `AQRProceduralScatterActor` named `ProcScatter_Default`.
- [ ] Select the scatter actor → Details panel → set
      **BiomeProfile = BP_AlienJungle**. Click **Generate**.
      You should see 500ish scattered plants + rocks on the floor.
- [ ] Change Seed → re-Generate → confirm world changes deterministically.

### 3c. Generate the landscape from the worldgen seed
- [ ] With `AQRWorldGenSeedActor::Generate` already run, bake the
      terrain: Output Log → Python →
      `exec(open(r'<Project>/Tools/EditorScripts/qr_generate_heightmap.py').read())`.
- [ ] The script writes to `Saved/QRWorldGen/`: a 16-bit heightmap
      (`.r16` + `.png`) and one 8-bit weightmap PNG per ground layer
      (Basalt / Slate / Moss / Mud / Glass / Ice / Hazard). It prints
      the exact Resolution + Scale to use.
- [ ] Modes → Landscape → New → **Import from File**. Feed it the
      `.r16` heightmap at the printed Resolution + Scale.
- [ ] Build a landscape **Layer Blend** master material with one layer
      per ground type, then import each `Weight_*` PNG to its paint
      layer. (`MWLandscapeAutoMaterial`'s `M_MWAM_Landscape` works as a
      single-material fallback if you skip per-biome texturing.)
- [ ] Delete any flat ProcGround floor — scatter actors trace against
      the landscape now.

### 3d. Multiple biomes in one map
- [ ] Drop a second `AQRProceduralScatterActor`. Set its
      BiomeProfile = `BP_PolarTundra`. Move its bounds to cover a
      different region.
- [ ] Drop a third for `BP_DesertSand`.
- [ ] Each scatter only fills its own bounds — overlapping volumes
      both run, giving you mixed transitions.

### 3e. Layered with ScifiJungle PCG
- [ ] Drop a `BP_PCG_Manager` from `/Game/Fabs/ScifiJungle/PCG/Bundle/Blueprints/`.
- [ ] Configure its volume + biome. Generate.
- [ ] PCG places dense thickets / mountain coverings; our scatter
      actor handles sparse rock + plant scatter on top. They
      coexist — PCG owns its own ISMs.

### 3f. Wildlife + POIs
- [ ] Drop `AQRNPCSpawner` actors at chosen POI locations
      (crash sites, abandoned outposts).
- [ ] Drop `AQRWildlifeActor` instances or use the scatter actor
      with ActorClass entries pointing at the wildlife actor for
      randomized fauna distribution.

### 3g. Atmosphere
- [ ] Drop a `BP_FogArea` from `/Game/Fabs/FogArea/Blueprints/` over
      humid biomes for a thick ground fog.
- [ ] Drop snow particle systems from `/Game/Fabs/Polar/Particles/`
      inside the polar zone.
- [ ] Add an ambient `UAudioComponent` per biome, bound to the
      biome profile's `AmbientLoop`.

---

## 4. Canonical biomes

The 14 GDD §4 biomes all ship as `UQRBiomeProfile` assets (run the
seeder in §3a). The worldgen subsystem assigns them per depth band:

| Depth band | Biomes |
|---|---|
| Surface (Tier 1) | BasaltShelf, WindPlains, MeltlineEdges, CraterFloors |
| Mid (Tier 2) | WetBasins, ShallowFens, ThermalCracks, GlassDunes, MossFields |
| Deep (Tier 3) | MagneticRidges, HighRims, ColdBasins, CanyonWebs, RidgeShadows |

The Remnant ring reuses the Deep biome pool. POI archetypes (crash
wrecks, faction outposts, caverns) are placed by `AQRWorldGenSpawner`,
not biome profiles — see `MANUAL_EDITOR_TASKS.md` Phase 7.

---

## 5. Worldgen pipeline status

Phase 1 + 2 are built:

- ✅ **`UQRWorldGenSubsystem`** — seed → 14-biome cell grid, depth
      bands, POI placement plan, minimap texture.
- ✅ **`AQRWorldGenSeedActor`** — editor-button driver (Generate +
      ExportMinimap).
- ✅ **`AQRWorldGenSpawner`** — places POIs / wrecks / caves / fauna.
- ✅ **`AQRBiomeZone`** — designer-placed biome override zone.
- ✅ **`qr_generate_heightmap.py`** — bakes a heightmap + per-biome
      ground weightmaps from the cell grid.

Open gaps:

- [ ] **Programmatic Landscape import** — the heightmap + weightmaps
      bake to disk; importing them into a Landscape actor is still a
      manual editor step (Modes → Landscape → Import from File).
- [ ] **Landscape layer-blend material** — a master material that
      blends the 7 ground layers driven by the weightmaps.
- [ ] **Traversal validation** — confirm a path PlayerStart → each
      depth band exists.

---

## 6. What you genuinely have to do by hand

Stuff that needs human eyeballs, no matter how much we automate:

- **Build the landscape layer-blend material** — a master material
  that blends the 7 ground layers (Basalt / Slate / Moss / Mud /
  Glass / Ice / Hazard) the weightmap bake produces.
- **Run the Landscape Import** — feed the baked heightmap + weightmaps
  into Modes → Landscape → Import from File. The bake script prints
  the exact resolution + scale; no script does the import itself yet.
- **Author the master scenario** — where's the crash site, where's
  the Vanguard outpost, where's the player base location.
- **Tune scatter density / spacing** per scene until it looks right.
  No script can decide what "looks right" for your specific map.
- **Pick which Fab pack mesh** is the canonical "alien tree" for
  the project — opinion call, not automation.
