#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "QRTypes.h"
#include "QRMissionDirector.generated.h"


/**
 * Mission template family — drives objective tracking.
 *
 *   FetchItem    : pick up N of an item id
 *   KillTarget   : kill N wildlife of a species (or NPC of a faction)
 *   ScoutPOI     : reach within radius of a POI
 *   EscortNPC    : protect an NPC while they move to a POI
 *   ResearchItem : study an item / species enough to advance Codex
 */
UENUM(BlueprintType)
enum class EQRMissionFamily : uint8
{
	FetchItem    UMETA(DisplayName = "Fetch Item"),
	KillTarget   UMETA(DisplayName = "Kill Target"),
	ScoutPOI     UMETA(DisplayName = "Scout POI"),
	EscortNPC    UMETA(DisplayName = "Escort NPC"),
	ResearchItem UMETA(DisplayName = "Research Item"),
};


/**
 * Where a mission's rewards come from — the GDD's No-Pocket-OP law
 * (RewardSourceValidation, Master GDD v1.4 Mission & Leadership pass):
 * every reward must have a believable survival source. The director
 * enforces each source's rules at grant time:
 *
 *   NPCPersonal     : a person hands you a few things from their own
 *                     pack — item quantities capped (no one carries 40
 *                     rifles), XP allowed.
 *   FactionStockpile: items are WITHDRAWN from a real depot that
 *                     actually has them. Stockpile empty = no reward,
 *                     and the log says so. No materializing.
 *   SiteContainer   : items spawn as world pickups at the mission site
 *                     ("recovered from the wreck"), never into pocket.
 *   Infrastructure  : the reward is the structure/system itself — no
 *                     pocket items granted.
 *   Knowledge       : codex/research advancement only — items stripped.
 *   Morale          : colony morale bump only — items stripped.
 */
UENUM(BlueprintType)
enum class EQRRewardSource : uint8
{
	NPCPersonal      UMETA(DisplayName = "NPC Personal"),
	FactionStockpile UMETA(DisplayName = "Faction Stockpile"),
	SiteContainer    UMETA(DisplayName = "Site Container"),
	Infrastructure   UMETA(DisplayName = "Infrastructure"),
	Knowledge        UMETA(DisplayName = "Knowledge"),
	Morale           UMETA(DisplayName = "Morale"),
};


/**
 * MissionLocationFallbackRule — controls placement when a named POI
 * didn't spawn in this world. Resolution cascades from the authored
 * rule down to GenerateMinorPOI, which always produces a point.
 */
UENUM(BlueprintType)
enum class EQRMissionLocationRule : uint8
{
	ExactPOI         UMETA(DisplayName = "Exact POI"),
	RegionHint       UMETA(DisplayName = "Region Hint"),
	BiomeHint        UMETA(DisplayName = "Biome Hint"),
	FactionOwned     UMETA(DisplayName = "Faction Owned"),
	GenerateMinorPOI UMETA(DisplayName = "Generate Minor POI"),
};


/**
 * DataTable row for procedural mission templates. RowName is the
 * template's MissionId. Designer authors a pool of these and the
 * director rolls one when the player has bandwidth (no active mission
 * of this family, conditions met).
 *
 * Master GDD §1 design law: rewards must come from believable survival
 * sources — recovered items, faction stockpiles, leader trust, etc.
 * Don't author rewards here that contradict that.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRMissionTemplateRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EQRMissionFamily Family = EQRMissionFamily::FetchItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MultiLine = true))
	FText Description;

	// FetchItem: the ItemId to gather. KillTarget: the SpeciesId.
	// ScoutPOI: the POI archetype id. EscortNPC: the destination POI.
	// ResearchItem: the species/item id to Research.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 TargetQuantity = 1;

	// Reward XP applied to the originating leader / faction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 RewardXP = 50;

	// Reward items (ItemId → Quantity). Comes out of a real stockpile
	// per GDD design law; designer should match this to an NPC's
	// realistic inventory.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, int32> RewardItems;

	// 0..1 weight when the director rolls a new mission. Higher = more
	// likely. Set to 0 to take a template out of rotation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
	float RollWeight = 1.0f;

	// No-Pocket-OP: where the reward comes from. Validated at grant time
	// — see EQRRewardSource. Defaults to the strictest common case.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EQRRewardSource RewardSource = EQRRewardSource::FactionStockpile;

	// Placement rule for location-bearing families (ScoutPOI, EscortNPC,
	// and any fetch whose target is site-bound).
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EQRMissionLocationRule LocationRule = EQRMissionLocationRule::RegionHint;

	// ScoutPOI: how close (meters) the player must get to complete.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "5"))
	float ScoutRadiusMeters = 30.0f;
};


/**
 * One live mission instance derived from a template.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRActiveMission
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName MissionId;     // matches template RowName
	UPROPERTY(BlueprintReadOnly) FName TargetId;
	UPROPERTY(BlueprintReadOnly) int32 TargetQuantity = 1;
	UPROPERTY(BlueprintReadOnly) int32 CurrentProgress = 0;
	UPROPERTY(BlueprintReadOnly) EQRMissionFamily Family = EQRMissionFamily::FetchItem;
	UPROPERTY(BlueprintReadOnly) FDateTime IssuedAt;

	// Resolved world location for ScoutPOI / EscortNPC / site missions.
	// Set at issue time via the template's MissionLocationFallbackRule.
	UPROPERTY(BlueprintReadOnly) FVector TargetLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) bool bHasTargetLocation = false;
	UPROPERTY(BlueprintReadOnly) float ScoutRadiusMeters = 30.0f;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMissionIssued,    FName, MissionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMissionCompleted, FName, MissionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMissionProgress, FName, MissionId, int32, NewProgress);


/**
 * Lives on AQRGameMode (or any persistent actor). Owns the active-
 * mission list + handles roll / progress / complete transitions.
 *
 * Listens to gameplay events:
 *   • UQRInventoryComponent::OnItemAdded     → FetchItem progress
 *   • AQRWildlifeActor death                  → KillTarget progress
 *   • AQRCharacter location overlap with POI  → ScoutPOI progress
 *   • UQRCodexSubsystem::OnEntryUpdated      → ResearchItem progress
 *
 * For v1 we ship the data model + roll + manual ReportProgress API;
 * the auto-listening hooks wire in a later pass when each system has
 * a stable event surface.
 */
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRMissionDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRMissionDirector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Missions")
	TObjectPtr<UDataTable> MissionTemplateTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Missions",
		meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxConcurrentMissions = 3;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Missions")
	TArray<FQRActiveMission> ActiveMissions;

	UPROPERTY(BlueprintAssignable, Category = "QR|Missions|Events")
	FOnMissionIssued    OnMissionIssued;

	UPROPERTY(BlueprintAssignable, Category = "QR|Missions|Events")
	FOnMissionCompleted OnMissionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "QR|Missions|Events")
	FOnMissionProgress  OnMissionProgress;

	// Roll a new mission from the template table. Returns the chosen
	// MissionId, or NAME_None if the table is empty or all templates
	// are filtered out. Limits to MaxConcurrentMissions; idempotent
	// against duplicates (won't roll the same id twice).
	UFUNCTION(BlueprintCallable, Category = "QR|Missions")
	FName RollNewMission();

	// Start a specific template (bypasses the weighted roll). Used by
	// save-restore and scripted mission chains. Returns false when the
	// id is unknown, already active, or the concurrency cap is hit.
	UFUNCTION(BlueprintCallable, Category = "QR|Missions")
	bool StartMissionById(FName MissionId);

	// Report progress against an active mission. Family-aware: for
	// FetchItem you'd pass the item count delta; for KillTarget the
	// kill count delta; etc. Completes the mission when CurrentProgress
	// >= TargetQuantity.
	UFUNCTION(BlueprintCallable, Category = "QR|Missions")
	void ReportProgress(FName MissionId, int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "QR|Missions")
	void CompleteMission(FName MissionId);

	UFUNCTION(BlueprintCallable, Category = "QR|Missions")
	void AbandonMission(FName MissionId);

	UFUNCTION(BlueprintPure, Category = "QR|Missions")
	bool IsMissionActive(FName MissionId) const;

	// Subscribe to gameplay events on the local player's components.
	// Called from BeginPlay; safe to re-call when the player respawns.
	UFUNCTION(BlueprintCallable, Category = "QR|Missions")
	void HookPlayerEvents(class AQRCharacter* Player);

	UFUNCTION(BlueprintCallable, Category = "QR|Missions")
	void UnhookPlayerEvents();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<class AQRCharacter> HookedPlayer;

	UFUNCTION()
	void HandleItemAdded(class UQRItemInstance* Item, int32 SlotIndex);

	UFUNCTION()
	void HandleCodexUpdated(FName EntryId, EQRCodexDiscoveryState NewState);

	// ── Instantiation, rewards, location (v2) ─────────────────────

	// Build the live instance from a template row: resolves the target
	// location through the MissionLocationFallbackRule cascade.
	void InstantiateMission(FName Id, const FQRMissionTemplateRow& Row);

	// No-Pocket-OP reward grant. Looks the template back up and routes
	// XP + items through the RewardSource rules (see EQRRewardSource).
	void GrantRewards(const FQRActiveMission& Mission);

	// MissionLocationFallbackRule cascade. Always succeeds by the time
	// it reaches GenerateMinorPOI (random navigable ring point).
	bool ResolveMissionLocation(const FQRMissionTemplateRow& Row,
		FName TargetId, FVector& OutLocation) const;

	// Reward helpers, one per source.
	int32 WithdrawFromStockpile(FName ItemId, int32 Quantity);   // returns granted
	void  SpawnSiteCache(const FQRActiveMission& Mission,
		const TMap<FName, int32>& Items);

	// 1 Hz scout check against HookedPlayer's location.
	FTimerHandle ScoutTimerHandle;
	void TickScoutCheck();

public:
	// Called by AQRWildlifeActor when a tracked species dies. Static
	// helper iterates every active director (single director in v1)
	// and reports a KillTarget delta.
	static void ReportSpeciesKilled(class UWorld* World, FName SpeciesId, int32 Delta);
};
