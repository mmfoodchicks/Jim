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
- [ ] Verify three assets created under
      `/Game/QuietRift/Data/Biomes/`:
      `BP_AlienJungle`, `BP_PolarTundra`, `BP_DesertSand`.
- [ ] Open each, scroll the Palette array, fix any rows that the
      script logged as missing (the source path may have shifted
      inside the Fab pack).

### 3b. Create a procedural test map
- [ ] In UE → Output Log → Python:
      `exec(open(r'<Project>/Tools/EditorScripts/qr_create_proc_world_map.py').read())`
- [ ] Open `/Game/Maps/L_ProcTest`. There's a 200m floor + lights +
      one `AQRProceduralScatterActor` named `ProcScatter_Default`.
- [ ] Select the scatter actor → Details panel → set
      **BiomeProfile = BP_AlienJungle**. Click **Generate**.
      You should see 500ish scattered plants + rocks on the floor.
- [ ] Change Seed → re-Generate → confirm world changes deterministically.

### 3c. Use a real landscape (optional but recommended)
- [ ] Create a new Landscape actor in the level (Modes → Landscape).
- [ ] Sculpt or import a heightmap.
- [ ] In its Material slot, drop **MWLandscapeAutoMaterial's master
      material** (`/Game/Fabs/MWLandscapeAutoMaterial/Materials/M_MWAM_Landscape`).
- [ ] Apply the 5 `LGT_MWAM_*` Landscape Grass Types to its
      foliage list (Project Settings → Landscape Grass).
- [ ] Delete the ProcGround floor — the scatter actor traces against
      the landscape now.
- [ ] Drag the BiomeProfile-bound scatter actor's bounds to cover the
      landscape. Generate.

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

## 4. Pre-built biomes (current + planned)

| Biome | Profile asset | Status |
|---|---|---|
| Alien Jungle (default Tharsis IV) | `BP_AlienJungle` | ✅ ready (run the seeder) |
| Polar Tundra | `BP_PolarTundra` | ✅ ready |
| Desert Sand | `BP_DesertSand` | ✅ ready |
| Crashed Ship Interior | `BP_CrashSite` | TODO — DeepWaterStation interior + Ruined_Modern_Buildings debris |
| Industrial Ruin | `BP_IndustrialRuin` | TODO — IndustryPropsPack6 + Ruined_Modern_Buildings |
| Vanguard Outpost | `BP_VanguardOutpost` | TODO — Construction_VOL1 + WinterTown |
| Underground Cavern | `BP_Cavern` | TODO — Polar/Meshes + Rock_Collection_04 |

Add more by copying any existing biome profile and swapping its
Palette entries for the relevant Fab pack's meshes.

---

## 5. Things still to build (next passes)

In rough priority:

- [ ] **`AQRBiomeZone` actor** — tag-based region detector that
      switches active sky / fog / ambient sound when player enters,
      bound to a `UQRBiomeProfile`.
- [ ] **`UQRWorldGenSubsystem`** — game-instance subsystem that
      streams biome zones + scatter regen as the player moves
      (only one at a time loaded).
- [ ] **Heightmap generator** — Python that creates a noise-driven
      landscape heightmap and imports it (so even the terrain shape
      is procedural). UE has `unreal.LandscapeProxy.import_landscape_heightmap`.
- [ ] **POI placer** — Python that scans biome zones for flat
      enough spots and drops a random POI prefab there (crash
      site, outpost, wreck). Shareable seed → same POI layout.
- [ ] **Wildlife population script** — biome-aware density
      (jungle = dense / desert = sparse / polar = predator-only).

Each is its own commit; pick whichever you want next.

---

## 6. What you genuinely have to do by hand

Stuff that needs human eyeballs, no matter how much we automate:

- **Sculpt or source the heightmap** for any landscape that's not
  generated procedurally. World Creator / Gaea / hand sculpt.
- **Paint biome layer weights** on the landscape — designer decides
  where the jungle ends and the tundra begins.
- **Author the master scenario** — where's the crash site, where's
  the Vanguard outpost, where's the player base location.
- **Tune scatter density / spacing** per scene until it looks right.
  No script can decide what "looks right" for your specific map.
- **Pick which Fab pack mesh** is the canonical "alien tree" for
  the project — opinion call, not automation.
