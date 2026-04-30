#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "QRTypes.h"
#include "QRColonyStateComponent.generated.h"

// Snapshot of one NPC's needs summary for the colony report
USTRUCT(BlueprintType)
struct QRCOLONYAI_API FQRSurvivorRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName SurvivorId;
	UPROPERTY(BlueprintReadOnly) FText DisplayName;
	UPROPERTY(BlueprintReadOnly) EQRNPCRole CurrentRole = EQRNPCRole::Unassigned;
	UPROPERTY(BlueprintReadOnly) float Health = 100.0f;
	UPROPERTY(BlueprintReadOnly) float Morale = 50.0f;
	UPROPERTY(BlueprintReadOnly) EQRNPCMoodState Mood = EQRNPCMoodState::Stable;
	UPROPERTY(BlueprintReadOnly) bool bIsLeader = false;
	UPROPERTY(BlueprintReadOnly) EQRLeaderType LeaderType = EQRLeaderType::None;
	UPROPERTY(BlueprintReadOnly) bool bIsAlive = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnColonyReportReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSurvivorDied, FName, SurvivorId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnColonyMoraleChanged, float, NewMorale);

// Central colony-level aggregation component (attached to the GameState or ColonyActor)
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QRCOLONYAI_API UQRColonyStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRColonyStateComponent();

	// ── Survivor Registry ─────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	TArray<FQRSurvivorRecord> SurvivorRecords;

	// ── Aggregate Colony Stats ────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	float ColonyMorale = 50.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	int32 PopulationCount = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	int32 WorkerCount = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	float FoodSupplyDays = 0.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	float WaterSupplyDays = 0.0f;

	// ── Camp Style ────────────────────────────
	// Tags representing the camp's culture/philosophy
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	FGameplayTagContainer CampStyleTags;

	// ── Leadership Stability ──────────────────
	// Accumulates each time a leader is promoted or replaced. Decays over time.
	// High instability drags colony morale down and is visible in the colony report UI.
	// Range [0..100]. Each leadership swap adds to this; it decays ~5 pts per game-minute.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	float LeadershipInstabilityScore = 0.0f;

	// ── Ending State ─────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	EQREndingPath ActiveEndingPath = EQREndingPath::None;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Colony|Events")
	FOnColonyReportReady OnColonyReportReady;

	UPROPERTY(BlueprintAssignable, Category = "Colony|Events")
	FOnSurvivorDied OnSurvivorDied;

	UPROPERTY(BlueprintAssignable, Category = "Colony|Events")
	FOnColonyMoraleChanged OnColonyMoraleChanged;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Colony")
	void RegisterSurvivor(FQRSurvivorRecord Record);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Colony")
	void UpdateSurvivorRecord(FName SurvivorId, float NewHealth, float NewMorale, EQRNPCMoodState Mood);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Colony")
	void MarkSurvivorDead(FName SurvivorId);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Colony")
	void RecalculateColonyStats();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Colony")
	void ApplyColonyMoraleEvent(float Delta);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Colony")
	void SetEndingPath(EQREndingPath Path);

	// Call whenever a leader is promoted, demoted, or replaced.
	// Severity [0..50]: 0 = new leader first promotion, higher = repeat swap on same role.
	// World-found leaders should pass severity 0 — the colony respects their background.
	// Applies an immediate morale hit and raises LeadershipInstabilityScore.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Colony")
	void ApplyLeadershipChurn(float Severity);

	UFUNCTION(BlueprintPure, Category = "Colony")
	int32 CountSurvivorsWithRole(EQRNPCRole Role) const;

	UFUNCTION(BlueprintPure, Category = "Colony")
	bool HasLeaderOfType(EQRLeaderType Type) const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
