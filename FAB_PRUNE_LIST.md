# Fab Pack Prune / Keep List

Generated from `qr_fab_audit.md` + grep of `Source/` and `Tools/` for
each pack name. Three categories:

- **KEEP — core**: code or seeder script directly references the pack.
  Deleting breaks compile or PIE.
- **KEEP — future**: no current code reference, but the pack maps to
  a GDD-defined system that isn't built yet (POI archetype, optics,
  late-game enemy, etc.). Keep until the corresponding system ships
  or is explicitly cut.
- **DELETE**: empty, broken beyond repair, or scope-irrelevant. No
  code refs anywhere.

The decision rule for "broken beyond repair" is: every asset in the
pack has a broken dependency the repoint pass can't fix (because the
referenced asset doesn't exist anywhere in the project, not just at
the wrong path).

---

## Summary

| Category | Packs | Notes |
|---|---:|---|
| KEEP — core (referenced in code) | 12 | See below |
| KEEP — future (maps to GDD system) | 16 | See below |
| DELETE | 6 | See below |
| **Total** | **34** | |

---

## DELETE — 6 packs

| Pack | Reason | Broken deps in audit |
|---|---|---:|
| CombatMagicAnims | 1 anim, references missing Quinn mesh. No SK, no code use. Not in GDD. | 1 |
| FreeAnimationsPack | 10 anims, every one references missing `SK_Mannequin` inside the pack itself. No skeleton, so anims can't even play if repointed. | 10 |
| LED_Generator | 28 materials, 0 meshes. Materials all broken (missing textures). Nothing to apply them to. | 28 |
| PhoneSystem | Cell-phone simulator (73 BPs, GPS navigation, selfie poses). Game is set on a Jovian moon in 2057 — phones aren't in scope. Largest broken-dep contributor outside ScifiJungle/NiagaraExamples. | 119 |
| SERLO_DeveloperTools | 1 blueprint referencing itself (broken). Editor tool that we don't use. | 1 |
| StampIt | 52 textures, nothing else. Stamp decals for a painting workflow we don't have. | 0 |

Deleting these clears ~159 broken deps and removes ~75 BP / 38 mat /
52 tex of dead weight from the project.

**How to delete safely:**
```
1. Right-click each pack folder in Content Browser -> Delete
2. UE asks "this asset has references" — accept (the refs are
   redirectors to deleted Fab paths or self-references)
3. After all six are deleted, right-click /Game/Fabs -> Fix Up
   Redirectors In Folder.
```

---

## KEEP — core (12 packs)

| Pack | Wired in | Why core |
|---|---|---|
| Essential_Foosteps_SK | `QRUISound.cpp` line ~82 | Footstep cue paths hardcoded; surface-aware playback |
| FreeAnimsMixPack | `qr_create_player_blueprint.py`, `qr_retarget_anims_to_mannequin.py` | UE5 SK_Mannequin + 38 anims for the player |
| Free_Sounds_Pack | `QRUISound.cpp` (click/confirm/deny/fire/impact/death), `qr_seed_biome_profiles.py` (ambient loops) | UI feedback + weapon SFX + biome ambient |
| FuturisticWarrior | `qr_create_player_blueprint.py`, `qr_retarget_anims_to_mannequin.py` | Player character costume layer + 43 anim source |
| MWLandscapeAutoMaterial | `qr_seed_biome_profiles.py` line ~48 | Default landscape material on every biome |
| NiagaraExamples | `QRWeaponComponent.cpp` (muzzle / impact / tracer FX), `qr_assign_fab_materials.py` | Weapon-fire VFX |
| OWD_Plants_Pack | `qr_seed_biome_profiles.py` (palette source) | Flora pool for biome scatter |
| Polar | `qr_seed_biome_profiles.py` | ColdBasins / IceCaves biome props |
| RamsterZ_FreeAnims_Volume1 | `qr_create_player_blueprint.py`, `qr_retarget_anims_to_mannequin.py` | **Cleanest pack in audit** — 44 player anims |
| Rock_Collection_04 | `qr_seed_biome_profiles.py` (ROCK_1/2/3) | Universal rocks across biomes |
| ScifiJungle | `qr_seed_biome_profiles.py` (SCIFI_TREE) + PCG manager per `PROCEDURAL_WORLD_PLAN.md` | Alien Jungle / BasaltShelf biome — heaviest by asset count |
| WeaponSniper | `qr_assign_fab_materials.py` | Sniper rifle mesh — needed for the unbuilt long-range scope (GDD §combat patch v8) |

---

## KEEP — future (16 packs)

Mapped to a GDD-defined system that isn't built yet. Don't delete
until either the system ships or it gets cut from scope.

| Pack | Maps to (in GDD / status doc) |
|---|---|
| Construction_VOL1 | POI props (`DT_POIArchetypes`: Construction Yard archetype) |
| DeadBodies_Poses_nikoff | Crash-site corpse decoration (`GAME_OVERVIEW.md` crash lore + `MissionsMainQuestline` MQ_000) |
| DeepWaterStation | Sci-fi outpost POI (`DT_POIArchetypes` archetype) |
| DynamicFalling | Vault landing / parkour anim source (`UQRVaultComponent` exists, lacks landing roll) |
| FogArea | Biome zone atmosphere volumes (`AQRBiomeZone` + `UQRBiomeProfile`, `PROCEDURAL_WORLD_PLAN.md` Tier B) |
| German_Shepherd_3D_Model | Canine predator mesh swap-in (Silt Hounds / Trench Diggers in GDD `Visual World Bible §6`) |
| Horror_Props | Wreck / crash POI decoration (corpse + bag + cloth variants) |
| IndustryPropsPack6 | Industrial outpost POI archetype |
| LensFlareVFX | Outdoor atmosphere (sun flare for `AQRSkyManager`) |
| ModernBridges | POI placement across canyons / cracks (BasaltShelf, ThermalCracks biomes) |
| MPMECH | Late-game elite enemy candidate (`PROCEDURAL_WORLD_PLAN.md` Tier C note) |
| QuantumCharacter | NPC modular-mesh option (`CLAUDE.md` notes as alt to FuturisticWarrior) |
| ROCKY_SAND_PACK | GlassDunes / SulfurRock biome (`PROCEDURAL_WORLD_PLAN.md` mapping) |
| Ruined_Modern_Buildings | Wreck POI archetype (`DT_POIArchetypes`) |
| Vefects | Impact-frame VFX + shockwave SFX layer (118 Niagara systems unused; large library) |
| WinterTown | Derelict cold settlement POI (WinterTown furniture + buildings) |
| WoodenProps | Camp clutter / derelict fill, build piece source |

---

## Borderline calls (worth flagging)

- **Vefects** is 0 code refs today but has the biggest VFX library
  outside NiagaraExamples (118 Niagara systems). Worth wiring its
  shockwave + impact frames into combat before culling.
- **DynamicFalling** has only 10 anims but they're all root-motion
  rolls — useful if we ever build a proper vault-landing system.
  Tiny pack (10 anims + 2 SK) — cheap to keep.
- **QuantumCharacter** has 10 modular SK pieces (arms, body, jacket
  variants) but 0 anims. Useless as a player without an anim source,
  but as an NPC outfit kit it's a strong candidate. Keep until the
  identity/appearance system needs more mesh variety.

---

## After pruning — expected broken-dep delta

Per the audit, the 6 delete packs contribute **159 broken refs**
between them (1 + 10 + 28 + 119 + 1 + 0). After deletion + `Fix Up
Redirectors`, those drop to zero.

The remaining ~1,920 broken refs across kept packs are addressable by
the extended `qr_repoint_fab_packs.py` (commit before this one) —
each one is the `/Game/<Pack>/` → `/Game/Fabs/<Pack>/` shape that the
repoint pass resolves directly.

Run the prune + repoint in this order:

```python
# 1. Delete the six packs above via the Content Browser.
# 2. Right-click /Game/Fabs -> Fix Up Redirectors In Folder.
# 3. In Python console:
exec(open(r'<Project>/Tools/EditorScripts/qr_repoint_fab_packs.py').read())
# Output's [verify] tally should drop dramatically.
```
