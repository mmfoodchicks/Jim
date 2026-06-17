# Exotic Metals → Arsenal (weapons / armour / shields)

Source-of-truth for the survival crafting ladder added 2026-06-08. The
metals are the planet's canonical materials already referenced in
`DT_Items_Master.csv` and the wildlife drop tables; this doc ties them
to the melee/primitive arsenal, armour, and the shield progression.

The weapon behaviour is **name-driven** in
`UQRWeaponComponent::ConfigureForWeaponId` — an item whose id contains a
recognised token gets the right stats automatically, so adding a new
blade is just a new item id, no code change.

---

## The metals (lowest → highest tier)

| Metal token | Canonical source | Dmg ×mult | Notes |
|---|---|---|---|
| `FERRIC` / scrap / bone | `MAT_FERRIC_ANTLER`, `MAT_SCRAP_METAL` | 1.00 | the iron analogue — starter tier |
| `BLACKGLASS` | `MAT_BLACKGLASS_PLATE` (volcanic glass) | 1.25 | very sharp, brittle (low durability later) |
| `MAGNET` | `MAT_MAGNET_INGOT` | 1.15 | magnetic alloy, tough |
| `SUNWIRE` | `MAT_SUNWIRE_INGOT` | 1.25 | conductive — shock procs later |
| `FROSTSPARK` | `MAT_FROSTSPARK_CRYSTAL` | 1.40 | cryo — slow procs later |
| `SPARKSTONE` | `MAT_SPARKSTONE_CORE` | 1.55 | energy-infused, high damage |
| `REMNANT` / `EXOTIC` | Remnant-site alloys (research-gated) | 2.00 | endgame |

The `×mult` multiplies a blade's base damage in code (`MetalMult()` in
`ConfigureForWeaponId`). The same metals feed armour + shields in the
recipe data.

---

## Weapons

### Crude (no metal token → ×1.0)
`WPN_DAGGER`, `WPN_BONE_KNIFE`, `WPN_MACHETE`, `WPN_HATCHET`,
`WPN_STONE_AXE`, `WPN_WOOD_CLUB`, `WPN_STONE_SPEAR`, `WPN_PICKAXE`
(dual-use tool/weapon), `WPN_KNUCKLE_DUSTER`.

All are `EQRWeaponType::Melee`: a short-range **sphere-sweep swing**
(no pixel-perfect aim), no falloff, no recoil, swing cadence via RPM.

| Class | Base dmg | Reach | Swing RPM | Sweep cm |
|---|---|---|---|---|
| Dagger/knife | 28 | 1.8 m | 200 | 18 |
| Machete | 40 | 2.0 m | 150 | 22 |
| Sword | 55 | 2.4 m | 110 | 26 |
| Axe | 70 | 2.2 m | 85 | 24 |
| Hatchet | 45 | 1.9 m | 130 | 20 |
| Pickaxe | 38 | 2.0 m | 100 | 20 |
| Spear/javelin | 50 | 3.2 m | 100 | 16 (reach) |
| Club/mace/maul | 60 | 2.0 m | 90 | 28 |
| Fist/knuckle | 18 | 1.6 m | 260 | 16 |

### Metal-tier blades
`WPN_<METAL>_SWORD`, `WPN_<METAL>_AXE`, `WPN_<METAL>_SPEAR` for each
metal above — e.g. `WPN_SPARKSTONE_SWORD` = 55 × 1.55 ≈ 85 dmg.

### Bows / drawn (`EQRWeaponType::Bow`, precise on ADS)
`WPN_SHORTBOW` (55 dmg), `WPN_RECURVE_BOW`, `WPN_CROSSBOW` (90 dmg, slow),
`WPN_SLING` (22 dmg). Hitscan MVP — arrow-arc projectiles are a follow-up.

---

## Shields (`EQRWeaponType::Shield`)

Hold **RMB** (ADS) to raise. Blocks **frontal** damage only. Tiered:

| Shield | Id | Block | Notes |
|---|---|---|---|
| Wood buckler | `WPN_WOOD_SHIELD` | 45 % | starter |
| Scrap shield | `WPN_SCRAP_SHIELD` | 45 % | |
| Riot shield | `WPN_RIOT_SHIELD` | 70 % | ballistic |
| Plasma/energy shield | `WPN_PLASMA_SHIELD` | 92 % | Halo-style; has a 200 HP regen absorb pool (regen wiring TBD) |

Damage reduction is applied in `AQRCharacter::TakeDamage` when a shield is
equipped and raised and the hit is frontal.

---

## Armour (`Clothing` category)

`ARM_<METAL>_HELM`, `ARM_<METAL>_CHEST`, `ARM_<METAL>_LEGS` per metal.
These exist as item defs now so the metals "make armour" and recipes
resolve; the **wear-protection damage system is a follow-up** (the
clothing slots + a flat/zone damage-reduction on the survival component).

---

## How to use it (testing)

1. Compile (weapon component + character changes).
2. UE Python console:
   ```
   exec(open(r'D:\QuietRiftEnigma\Jim\QuietRiftEnigma\Tools\EditorScripts\qr_seed_crude_arsenal.py').read())
   run()
   ```
   Creates equippable item defs for the whole arsenal (held mesh empty
   until meshes are baked — the *logic* works immediately).
3. Open the creative browser (**Tab**), equip a `WPN_*`, and:
   - melee → LMB swings (watch the pink sweep tracer + crit logs)
   - bow → LMB draws + looses, tight on ADS
   - shield → hold RMB to block (watch `SHIELD blocked` logs)

---

## Follow-ups (not done this pass)

- Blender meshes for the crude weapons + shields (so they show in hand).
  Add generators to `qr_generate_weapons_assets_assets.py` + rows to a
  crude-arsenal CSV.
- Armour wear-protection: clothing slots feeding a damage-reduction on
  `UQRSurvivalComponent`.
- Energy-shield regen pool (`ShieldMaxHP` is defined but not yet drained/
  regenerated — currently shields mitigate by the flat fraction).
- Metal status procs (Sunwire shock, Frostspark slow).
- Crafting recipes (`RC_WPN_*`) wired into the research/crafting tables.
