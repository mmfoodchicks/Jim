#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "QRTypes.h"
#include "QRNPCSurvivor.generated.h"

class UQRInventoryComponent;
class UQRSurvivalComponent;
class UQRNPCRoleComponent;
class UQRLeaderComponent;
class UQRColonyStateComponent;
class UBehaviorTree;

// Full NPC survivor with survival needs, role task system, and optional leadership
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRNPCSurvivor : public ACharacter
{
	GENERATED_BODY()

public:
	AQRNPCSurvivor();

	// ── Components ───────────────────────────
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UQRInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UQRSurvivalComponent> Survival;

	// Named RoleComp (not Role) to avoid shadowing the deprecated AActor::Role network role member.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UQRNPCRoleComponent> RoleComp;

	// Only present if this NPC is a leader
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UQRLeaderComponent> LeaderComp;

	// ── Identity ─────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", Replicated)
	FName SurvivorId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", Replicated)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", Replicated)
	bool bIsLeader = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", Replicated)
	EQRLeaderType LeaderType = EQRLeaderType::None;

	// Morale of this individual NPC [0..100]
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	float IndividualMorale = 50.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	EQRNPCMoodState Mood = EQRNPCMoodState::Stable;

	// ── Behavior ──────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> SurvivorBehaviorTree;

	// ── Innate Moral Compass ─────────────────
	// Per-axis values [-1..1] matching the 8 EQRMoralCompassAxis slots.
	// Set at spawn; compared against camp's CampPolicyVector to produce alignment score.
	// High misalignment causes morale drain; extreme misalignment causes the NPC to leave.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	TArray<float> InnateCompassVector;

	// Cached alignment score with current camp policy [-1..1]. Recalculated when camp
	// policy changes or this NPC's morale is evaluated.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	float CampAlignmentScore = 0.0f;

	// ── Night Panic ───────────────────────────
	// NPC may panic at night if morale is very low
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	bool bHasNightPanic = false;

	// How many times this NPC has been promoted to any leader role.
	// Used to scale churn severity: first promotion = no churn, each swap after = escalating cost.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	int32 LeaderPromotionCount = 0;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NPC")
	void PromoteToLeader(EQRLeaderType Type);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NPC")
	void DemoteFromLeader();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NPC")
	void SetMorale(float NewMorale);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NPC")
	void ApplyMoraleEvent(float Delta, FText EventDescription);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NPC")
	void SetRaidState(EQRCivilianRaidState NewState);

	// Picks a raid response based on this NPC's individual morale:
	//   >= 65 → Defending  (high morale: stand and fight back)
	//   >= 30 → Hiding     (mid morale: shelter in place, stay quiet)
	//   <  30 → Fleeing    (low morale: run, abandon the camp if needed)
	UFUNCTION(BlueprintPure, Category = "NPC|Raid")
	EQRCivilianRaidState DetermineRaidResponse() const;

	// Convenience wrapper — picks a response and applies it as the new raid state.
	// Call from the GameMode/raid scheduler when a raid begins for each civilian.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NPC|Raid")
	void RespondToRaid();

	// Recompute CampAlignmentScore by dot-product of InnateCompassVector vs camp policy.
	// Call this when the camp's policy vector changes or when evaluating whether an NPC stays.
	// Returns the new score; Blueprint can check against a leave threshold (e.g. < -0.6).
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NPC")
	float UpdateCampAlignment(const TArray<float>& CampPolicyVector);

	UFUNCTION(BlueprintPure, Category = "NPC")
	bool IsAlive() const;

	UFUNCTION(BlueprintNativeEvent, Category = "NPC")
	void OnMoraleCollapsed();
	virtual void OnMoraleCollapsed_Implementation() {}

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnSurvivalDeath();
};
