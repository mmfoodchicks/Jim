#pragma once

#include "CoreMinimal.h"
#include "QRTypes.h"
#include "QRTechNode.h"
#include "QRMicroResearch.h"
#include "QRSaveTypes.generated.h"

// Serializable item snapshot for save/load
USTRUCT()
struct QRSAVENET_API FQRItemSaveData
{
	GENERATED_BODY()

	UPROPERTY() FName ItemId;
	UPROPERTY() int32 Quantity = 1;
	UPROPERTY() float Durability = -1.0f;
	UPROPERTY() float SpoilProgress = 0.0f;
	UPROPERTY() EQREdibilityState EdibilityState = EQREdibilityState::Unknown;
	UPROPERTY() FGuid InstanceGuid;

	// v1.17 fields — must be persisted so food safety and bulk rules survive reload
	UPROPERTY() EQRFoodOriginClass FoodOriginClass = EQRFoodOriginClass::Unknown;
	UPROPERTY() float PackageIntegrity = 1.0f;
	UPROPERTY() bool bIsBulkItem = false;
};

// Serializable weapon runtime state (per-equipped weapon on a survivor)
USTRUCT()
struct QRSAVENET_API FQRWeaponSaveData
{
	GENERATED_BODY()

	UPROPERTY() FName WeaponItemId;
	UPROPERTY() int32 CurrentAmmo = 0;
	UPROPERTY() float FoulingFactor = 0.0f;
	UPROPERTY() bool bIsJammed = false;
};

// Serializable inventory snapshot
USTRUCT()
struct QRSAVENET_API FQRInventorySaveData
{
	GENERATED_BODY()

	UPROPERTY() TArray<FQRItemSaveData> Items;
	UPROPERTY() FQRItemSaveData HandSlot;
	UPROPERTY() bool bHasHandSlot = false;              // distinguishes "empty hand slot" from "no save data"
	UPROPERTY() EQRHandsSlotState HandsSlotState = EQRHandsSlotState::Empty;
};

// Serializable leader component state (v1.4 / v1.17)
USTRUCT()
struct QRSAVENET_API FQRLeaderSaveData
{
	GENERATED_BODY()

	// Aptitude axes
	UPROPERTY() float LeadershipAptitude = 5.0f;
	UPROPERTY() float SkillAptitude = 5.0f;
	UPROPERTY() float Composure = 5.0f;

	// Morale
	UPROPERTY() float MoraleIndex = 50.0f;
	UPROPERTY() float MoraleResilience = 30.0f;
	UPROPERTY() float MoraleGradient = 0.0f;
	UPROPERTY() float LeaderXP = 0.0f;
	UPROPERTY() float DefectionRisk = 0.0f;
	UPROPERTY() float MoralCompassVector = 0.0f;

	// Issue escalation pipeline
	UPROPERTY() EQRLeaderIssueState IssueState = EQRLeaderIssueState::None;
	UPROPERTY() float IssueEscalationScore = 0.0f;
	UPROPERTY() float BlockerDurationHours = 0.0f;

	// Camp alignment (8-axis; padded/truncated to current axis count on load)
	UPROPERTY() TArray<float> CampPolicyVector;
	UPROPERTY() float CampAlignmentScore = 0.0f;
};

// Save data for a single survivor
USTRUCT()
struct QRSAVENET_API FQRSurvivorSaveData
{
	GENERATED_BODY()

	UPROPERTY() FName SurvivorId;
	UPROPERTY() FText DisplayName;
	UPROPERTY() FVector WorldLocation = FVector::ZeroVector;
	UPROPERTY() float Health = 100.0f;
	UPROPERTY() float Hunger = 80.0f;
	UPROPERTY() float Thirst = 80.0f;
	UPROPERTY() float Fatigue = 100.0f;
	UPROPERTY() EQRNPCRole Role = EQRNPCRole::Unassigned;
	UPROPERTY() float MoraleIndex = 50.0f;
	UPROPERTY() bool bIsAlive = true;
	UPROPERTY() FQRInventorySaveData Inventory;
	UPROPERTY() TMap<EQRNPCRole, float> SkillLevels;
	UPROPERTY() FQRWeaponSaveData EquippedWeapon;    // v1.17: persists weapon fouling/jam/ammo
};

// Save data for a harvestable node (tree, rock, etc.)
USTRUCT()
struct QRSAVENET_API FQRHarvestNodeSaveData
{
	GENERATED_BODY()

	UPROPERTY() FGuid NodeGuid;
	UPROPERTY() float RemainingYield = 1.0f;
	UPROPERTY() float RegrowthTimeHours = 0.0f;
};

// Save data for a placed buildable/station
USTRUCT()
struct QRSAVENET_API FQRBuildableSaveData
{
	GENERATED_BODY()

	UPROPERTY() FGuid BuildableGuid;
	UPROPERTY() FName StationTag;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY() float Health = 1.0f;
	UPROPERTY() TArray<FQRItemSaveData> StoredItems;
};

// Research / Tech Node save state
USTRUCT()
struct QRSAVENET_API FQRResearchSaveData
{
	GENERATED_BODY()

	UPROPERTY() TArray<FQRTechNodeRuntime> TechNodeStates;
	UPROPERTY() TArray<FQRMicroResearchRuntime> MicroResearchStates;
	UPROPERTY() TArray<FName> MicroResearchQueue;
	UPROPERTY() TMap<FName, EQRCodexDiscoveryState> CodexStates;
};

// World chunk delta — only stores what changed from worldgen baseline
USTRUCT()
struct QRSAVENET_API FQRChunkDelta
{
	GENERATED_BODY()

	UPROPERTY() FIntVector ChunkCoord;
	UPROPERTY() TArray<FQRHarvestNodeSaveData> HarvestNodes;
	UPROPERTY() TArray<FQRBuildableSaveData> Buildables;
	UPROPERTY() TArray<FName> DestroyedActorIds;
};

// Top-level save game structure
USTRUCT()
struct QRSAVENET_API FQRGameSaveData
{
	GENERATED_BODY()

	// Incremented whenever the save layout changes (see UQRSaveGameSystem::MigrateToCurrentVersion)
	UPROPERTY() int32 SaveVersion = 0;

	// Save slot metadata
	UPROPERTY() FString SaveSlotName;
	UPROPERTY() FDateTime SaveTimestamp;
	UPROPERTY() int32 WorldSeed = 0;
	UPROPERTY() float WorldTimeSeconds = 0.0f;
	UPROPERTY() int32 DayNumber = 0;

	// Player data
	UPROPERTY() FQRSurvivorSaveData PlayerData;
	UPROPERTY() FQRInventorySaveData PlayerInventory;

	// Colony data
	UPROPERTY() TArray<FQRSurvivorSaveData> SurvivorData;
	UPROPERTY() FQRResearchSaveData ResearchData;
	UPROPERTY() TArray<FQRBuildableSaveData> ColonyBuildables;
	UPROPERTY() float ColonyMorale = 50.0f;
	UPROPERTY() EQREndingPath EndingPath = EQREndingPath::None;

	// Leader state per leader type (keyed by EQRLeaderType cast to uint8 for TMap serialization)
	UPROPERTY() TMap<uint8, FQRLeaderSaveData> LeaderStates;

	// Quest/Mission data
	UPROPERTY() TArray<FName> CompletedMissionIds;
	UPROPERTY() TArray<FName> ActiveMissionIds;

	// World delta
	UPROPERTY() TArray<FQRChunkDelta> ChunkDeltas;

	// Faction data
	UPROPERTY() TMap<FName, float> FactionTrustScores;
};
