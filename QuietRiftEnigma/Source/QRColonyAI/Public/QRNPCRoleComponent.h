#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRTypes.h"
#include "QRNPCRoleComponent.generated.h"

class AQRStationBase;

// A single task in an NPC's fallback priority array
USTRUCT(BlueprintType)
struct QRCOLONYAI_API FQRNPCTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName TaskId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	// Scoring priority: higher = preferred when needs match
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Priority = 1.0f;

	// Tags that enable/block this task
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer RequiredConditionTags;

	// True if this task requires a station to be available
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bRequiresStation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag RequiredStationTag;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNPCRoleChanged, EQRNPCRole, OldRole, EQRNPCRole, NewRole);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCTaskChanged, FName, NewTaskId);

// Component that manages an NPC's assigned role, task queue, and fallback logic
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QRCOLONYAI_API UQRNPCRoleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRNPCRoleComponent();

	// ── Config ───────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role")
	EQRNPCRole PrimaryRole = EQRNPCRole::Unassigned;

	// Ordered fallback task list — NPC attempts from top to bottom
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role")
	TArray<FQRNPCTask> FallbackTaskArray;

	// ── Skill Levels ─────────────────────────
	// 0..100 skill in each role category. Server-authoritative; not replicated
	// (UE 5.7+ disallows replicated TMaps — clients query via RPC if needed).
	UPROPERTY(BlueprintReadOnly, Category = "Skills")
	TMap<EQRNPCRole, float> RoleSkillLevels;

	// ── Runtime State ─────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Role")
	EQRNPCRole CurrentRole = EQRNPCRole::Unassigned;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Role")
	FName CurrentTaskId;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Role")
	bool bIsWorking = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Role")
	EQRCivilianRaidState RaidState = EQRCivilianRaidState::Normal;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Role|Events")
	FOnNPCRoleChanged OnRoleChanged;

	UPROPERTY(BlueprintAssignable, Category = "Role|Events")
	FOnNPCTaskChanged OnTaskChanged;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Role")
	void AssignRole(EQRNPCRole NewRole);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Role")
	void SetRaidState(EQRCivilianRaidState NewRaidState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Role")
	void SelectBestTask(const FGameplayTagContainer& WorldConditions);

	UFUNCTION(BlueprintPure, Category = "Role")
	float GetSkillLevel(EQRNPCRole Role) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Role")
	void GainSkillXP(EQRNPCRole Role, float XP);

	// Copy a fraction of Mentor's skill levels into this component.
	// InheritanceFraction [0..1]: 0.33 for die-and-replace lineage; 0.15 for active apprenticeship.
	// Skill never decreases — only the gap between mentor and self is inherited.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Skills")
	void InheritSkillsFromMentor(const UQRNPCRoleComponent* Mentor, float InheritanceFraction = 0.33f);

	// Work efficiency multiplier based on skill [0.5 .. 2.0]
	UFUNCTION(BlueprintPure, Category = "Role")
	float GetEfficiencyMultiplier() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void SetCurrentTask(FName TaskId);
};
