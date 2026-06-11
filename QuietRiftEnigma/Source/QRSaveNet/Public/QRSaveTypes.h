#pragma once

#include "CoreMinimal.h"
#include "QRTypes.h"
#include "QRTechNode.h"
#include "QRMicroResearch.h"
#include "QRSurvivalComponent.h"   // FQRInjury
#include "QRSaveTypes.generated.h"

// Where an item was equipped when saved. Mirrors the paper-doll slots.
// SaveVersion 2+; v1 saves leave everything at None and rely on the
// legacy HandSlot/bHasHandSlot fields below.
UENUM()
enum class EQRSavedEquipSlot : uint8
{
	None = 0,
	Helm,
	ChestArmour,
	LegsArmour,
	ChestRig,
	Backpack,
	Hand,
	Offhand,
};

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

	// SaveVersion 2: spatial placement, so the player's hand-arranged grid
	// layout survives reload instead of being re-packed at random.
	UPROPERTY() EQRContainerKind ContainerKind = EQRContainerKind::None;
	UPROPERTY() int32 GridX = -1;
	UPROPERTY() int32 GridY = -1;
	UPROPERTY() bool bRotated = false;

	// SaveVersion 2: non-None means this entry was equipped, not loose in
	// the grid. Restored via the matching TryEquip* path on load.
	UPROPERTY() EQRSavedEquipSlot EquippedSlot = EQRSavedEquipSlot::None;
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

	// SaveVersion 2: active injuries persist — a fracture survives a reload
	// instead of healing for free, and oxygen/temperature resume where they
	// were instead of resetting to defaults.
	UPROPERTY() TArray<FQRInjury> ActiveInjuries;
	UPROPERTY() float Oxygen = 100.0f;
	UPROPERTY() float CoreTemperature = 37.0f;
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

	// SaveVersion 2: row id into DT_BuildCatalog — what kind of piece this
	// is, so load can respawn the right mesh. Matches UQRBuildPieceTag::PieceId.
	UPROPERTY() FName PieceId;
};

// One codex entry snapshot — mirrors the game module's FQRCodexEntry,
// which can't live here (the game module depends on QRSaveNet, not the
// reverse). Conversion happens in UQRCodexSubsystem::Export/ImportEntries.
USTRUCT()
struct QRSAVENET_API FQRCodexEntrySaveData
{
	GENERATED_BODY()

	UPROPERTY() FName Id;
	UPROPERTY() FName Category;
	UPROPERTY() EQRCodexDiscoveryState State = EQRCodexDiscoveryState::Undiscovered;
	UPROPERTY() FText DisplayName;
	UPROPERTY() FText Description;
	UPROPERTY() int32 SeenCount = 0;
	UPROPERTY() FDateTime FirstSeen;
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

	UPROPERTY() FIntVector ChunkCoord = FIntVector::ZeroValue;
	UPROPERTY() TArray<FQRHarvestNodeSaveData> HarvestNodes;
	UPROPERTY() TArray<FQRBuildableSaveData> Buildables;
	UPROPERTY() TArray<FName> DestroyedActorIds;
};

// One NPC's mid-game snapshot. AQRNPCActor instances aren't level-
// authored so saving the class + location means load can respawn them
// where they walked off to instead of reverting to the spawner ring.
USTRUCT()
struct QRSAVENET_API FQRNPCSaveData
{
	GENERATED_BODY()

	UPROPERTY() FString ActorLabel;          // QR_Village_03_Theo_Beckford etc.
	UPROPERTY() FString NPCClassPath;        // soft path to subclass; resolved on load
	UPROPERTY() FText   DisplayName;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;

	// Brain memory -- so a colonist who'd walked to their work post
	// at the moment of save is still standing there on reload.
	UPROPERTY() FVector HomePosition       = FVector::ZeroVector;
	UPROPERTY() FVector AssignedWorkPost   = FVector::ZeroVector;
	UPROPERTY() FVector AssignedBed        = FVector::ZeroVector;
	UPROPERTY() uint8   BrainState         = 0;   // EQRNPCBrainState
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

	// Player identity (name, pronouns, voice profile reference). Defaulted
	// by UQRPronounLibrary::MakeDefaultIdentity for legacy saves.
	UPROPERTY() FQRPlayerIdentity PlayerIdentity;

	// Player character appearance (body sliders, face shape, skin / hair /
	// eye colors, hair / facial-hair indices). Defaulted by
	// UQRCharacterCustomizationLibrary::MakeDefaultAppearance for legacy
	// saves. Decoupled from PlayerIdentity so body type and pronouns stay
	// orthogonal — a Feminine-bodied character with He pronouns is valid.
	UPROPERTY() FQRCharacterAppearance PlayerAppearance;

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

	// SaveVersion 2: the mission director's live instances (template id →
	// current progress), so in-flight procedural missions resume mid-count.
	UPROPERTY() TMap<FName, int32> DirectorMissionProgress;

	// World delta
	UPROPERTY() TArray<FQRChunkDelta> ChunkDeltas;

	// Loot persistence — GUIDs of containers the player has already looted.
	// UQRLootedRegistry exports/imports this set on save / load so emptied
	// containers stay empty across reloads.
	UPROPERTY() TArray<FGuid> LootedContainerIds;

	// SaveVersion 2: full codex (SeenCount / FirstSeen / per-entry state).
	// The research component's CodexStates map only carries discovery
	// states; this is the K-key codex widget's data.
	UPROPERTY() TArray<FQRCodexEntrySaveData> CodexEntries;

	// SaveVersion 2: every brain-carrying AQRNPCActor in the world. Lets
	// a saved village survive reload with each colonist where they walked
	// to, not back at the spawner ring.
	UPROPERTY() TArray<FQRNPCSaveData> NPCActors;

	// Faction data
	UPROPERTY() TMap<FName, float> FactionTrustScores;
};
