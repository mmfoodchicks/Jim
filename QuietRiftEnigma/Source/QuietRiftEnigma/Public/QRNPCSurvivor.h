#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "QRTypes.h"
#include "QRNPCSurvivor.generated.h"

class UQRInventoryComponent;
class UQRSurvivalComponent;
class UQRNPCRoleComponent;
class UQRLeaderComponent;
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

	// ── Night Panic ───────────────────────────
	// NPC may panic at night if morale is very low
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Colony")
	bool bHasNightPanic = false;

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
