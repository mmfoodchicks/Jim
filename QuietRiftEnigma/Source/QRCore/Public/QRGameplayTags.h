#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

// UE_DECLARE_GAMEPLAY_TAG_EXTERN expands to a plain `extern FNativeGameplayTag`
// which has no DLL export specifier. Consumers in other modules then fail to
// link to symbols defined in QRCore.dll. This wrapper adds QRCORE_API so the
// linker can resolve the tags across module boundaries.
#define QR_DECLARE_GAMEPLAY_TAG_EXTERN(TagName) QRCORE_API extern FNativeGameplayTag TagName;

namespace QRGameplayTags
{
	// ── Item Category Tags ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Food)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Tool)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Weapon)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Ammo)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Component)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_ReferenceComponent)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Resource)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Medicine)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Clothing)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_ChestRig)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Backpack)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Cosmetic)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Seed)

	// ── Equipment Slot Tags ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Equipment_Slot_ChestRig)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Equipment_Slot_Backpack)

	// ── Food State Tags ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Food_Edibility_Unknown)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Food_Edibility_Safe)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Food_Edibility_Toxic)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Food_Edibility_MustCook)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Food_Spoil_Fresh)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Food_Spoil_Aging)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Food_Spoil_Spoiled)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Food_Spoil_Preserved)

	// ── Research Tags ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Research_Family_Food)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Research_Family_Water)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Research_Family_Medicine)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Research_Family_Materials)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Research_Family_Metalwork)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Research_Family_Electronics)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Research_Family_Ecology)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Research_Family_Remnant)

	// ── Survival Status Tags ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Bleeding)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Fracture)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Infection)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Concussion)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Toxin)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Exhausted)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Hypothermia)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Hungry)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Starving)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Thirsty)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Dehydrated)

	// ── NPC Role Tags ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Gatherer)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Hauler)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Cook)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Builder)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Miner)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Guard)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Researcher)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Medic)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Scout)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Farmer)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Engineer)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Leader)

	// ── Faction Tags ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Faction_Player)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Faction_EngineerCompact)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Faction_Remnant)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Faction_Hostile_Generic)
	// The Vanguard Concordat — the hardcoded antagonist faction controlling the Rift approaches
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Faction_Vanguard)
	// Satellite outposts that ring the Concordat; difficulty scales with distance
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Faction_Vanguard_Satellite)
	// Threat tag used by the raid scheduler to classify Vanguard-sourced raids
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Threat_Vanguard)

	// ── Biome Tags (v1.5 canonical) ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_BasaltShelf)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_WindPlains)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_WetBasins)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_ColdBasins)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_CraterFloors)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_CraterWalls)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_ThermalCracks)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_GlassDunes)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_MagneticRidges)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_MossFields)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_HighRims)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_MeltlineEdges)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_ShallowFens)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_RidgeShadows)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_CanyonWebs)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_VentRims)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_IronBasalt)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_IceCaves)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_ShadowFens)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_RemnantSite)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_AbandonedStructures)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Biome_Caves)

	// ── Flora Entity Tags (v1.5 canonical) ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_LatticeBulb)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_SpiralReed)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_MeltpodRind)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_EmberLace)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_NullmintNodes)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_KnifeleafFan)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_ResinChimney)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_SilkCystVine)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_MawcapBloom)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_ThreadmoldSheet)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_FerricBloom)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_CrystalLichen)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_CinderThorn)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_IronbrineCups)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_GlasbarkTree)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_SlagrootTree)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_VelvetspineTree)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Flora_AsterbarkTree)

	// ── Fauna Entity Tags — Animals (v1.5 canonical) ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_PebbleSkitter)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_GleamLarver)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_ShardbackGrazer)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_SiltStrider)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_PillarbackHauler)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_BasinTreader)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_NestweaverDrifter)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_MilkbladderHerdling)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_BoneLantern)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_LatchfinMite)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_Crackrunner)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_RidgeCourser)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_VaultbackDray)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_TetherbackPackgrazer)

	// ── Fauna Entity Tags — Predators (v1.5 canonical) ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_SutureWisp)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_NeedleMaw)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_DriftStalker)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_VaneRippers)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_GlassjawCluster)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_SiltHounds)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_TrenchDiggers)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_CarrionChoir)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_FogleechSwarm)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_IronstagStalker)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Fauna_ShellmawAmbusher)

	// ── Station / Depot Tags ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Station_Workbench)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Station_AnvilForge)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Station_Kiln)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Station_LogYard)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Station_StonePile)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Station_Pantry)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Station_ScrapHeap)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Station_CartographyBoard)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Station_WeaponRack)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Station_Generator)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Depot_Category_Wood)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Depot_Category_Stone)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Depot_Category_Food)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Depot_Category_Scrap)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Depot_Category_Fuel)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Depot_Category_Component)

	// ── Threat Tags ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Threat_Predator)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Threat_Raid)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Threat_Remnant)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Threat_Environmental)

	// ── Ending Tags ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Ending_Rescue)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Ending_Escape)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Ending_Settlement)

	// ── Tool Tags (for harvest gating) ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Knife)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Axe)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_HeavyAxe)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Gloves)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Pickaxe)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Sickle)

	// ── Ammo Quality Tags ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Ammo_Dirty)       // Improvised / dirty ammo (×5 fouling)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Ammo_Subsonic)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Ammo_AP)          // Armor-piercing

	// ── Weather Event Tags ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Weather_DustStorm)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Weather_AcidRain)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Weather_VentEruption)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Weather_HeatWave)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Weather_IceFog)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Weather_MagneticStorm)

	// ── Scent Tags ──
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Scent_Meat)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Scent_Blood)
	QR_DECLARE_GAMEPLAY_TAG_EXTERN(Scent_Carrion)
}
