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

public:
	// Called by AQRWildlifeActor when a tracked species dies. Static
	// helper iterates every active director (single director in v1)
	// and reports a KillTarget delta.
	static void ReportSpeciesKilled(class UWorld* World, FName SpeciesId, int32 Delta);
};
