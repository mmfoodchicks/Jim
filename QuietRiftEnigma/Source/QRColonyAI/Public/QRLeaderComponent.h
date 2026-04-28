#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRTypes.h"
#include "QRLeaderComponent.generated.h"

// A leader complaint/directive mission generated from the system
USTRUCT(BlueprintType)
struct QRCOLONYAI_API FQRLeaderDirective
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName DirectiveId;

	UPROPERTY(BlueprintReadOnly)
	FText Title;

	UPROPERTY(BlueprintReadOnly)
	FText Description;

	UPROPERTY(BlueprintReadOnly)
	EQRLeaderType LeaderType = EQRLeaderType::None;

	// What stat is this directive trying to fix
	UPROPERTY(BlueprintReadOnly)
	FName AffectedStat;

	// Severity (0..1) drives how urgently the leader escalates
	UPROPERTY(BlueprintReadOnly)
	float Severity = 0.0f;

	// How long until this directive escalates to a debuff
	UPROPERTY(BlueprintReadOnly)
	float EscalationTimeHours = 48.0f;

	UPROPERTY(BlueprintReadOnly)
	bool bIsResolved = false;
};

// A leader condition / debuff state
USTRUCT(BlueprintType)
struct QRCOLONYAI_API FQRLeaderCondition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName ConditionId;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	// Which stat this debuffs and by how much
	UPROPERTY(BlueprintReadOnly)
	FName AffectedStat;

	UPROPERTY(BlueprintReadOnly)
	float DebuffAmount = 0.0f;

	// Healed by successful relevant leadership events
	UPROPERTY(BlueprintReadOnly)
	float RemainingHours = 24.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeaderDirectiveAdded, FQRLeaderDirective, Directive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeaderDirectiveResolved, FName, DirectiveId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeaderConditionAdded, FQRLeaderCondition, Condition);

// Manages a colony leader's directives, conditions, XP, and moral compass
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QRCOLONYAI_API UQRLeaderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRLeaderComponent();

	// ── Identity ─────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leader")
	EQRLeaderType LeaderType = EQRLeaderType::None;

	// ── Stats (replicated) ───────────────────
	// Morale Index (MI): 0..100
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Stats")
	float MoraleIndex = 50.0f;

	// Morale Resilience (MR): how much MI buffers before cracking
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Stats")
	float MoraleResilience = 30.0f;

	// Morale Gradient (MG): recent trend in MI (+/- per game-day)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Stats")
	float MoraleGradient = 0.0f;

	// Leader XP (gained from successful events)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Stats")
	float LeaderXP = 0.0f;

	// Defection risk [0..1] — high when MI is chronically low
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Stats")
	float DefectionRisk = 0.0f;

	// Moral compass vector [-1..1] (negative = ruthless, positive = humane)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Stats")
	float MoralCompassVector = 0.0f;

	// ── Active Directives / Conditions ───────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Directives")
	TArray<FQRLeaderDirective> ActiveDirectives;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Conditions")
	TArray<FQRLeaderCondition> ActiveConditions;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Leader|Events")
	FOnLeaderDirectiveAdded OnDirectiveAdded;

	UPROPERTY(BlueprintAssignable, Category = "Leader|Events")
	FOnLeaderDirectiveResolved OnDirectiveResolved;

	UPROPERTY(BlueprintAssignable, Category = "Leader|Events")
	FOnLeaderConditionAdded OnConditionAdded;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void AddDirective(FQRLeaderDirective Directive);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void ResolveDirective(FName DirectiveId, float MoralBias = 0.0f);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void AddCondition(FQRLeaderCondition Condition);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void HealCondition(FName ConditionId);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void ApplyMoraleChange(float Delta, bool bIsEvent = false);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void GainLeaderXP(float XP);

	UFUNCTION(BlueprintPure, Category = "Leader")
	float GetTotalDebuff(FName StatName) const;

	UFUNCTION(BlueprintPure, Category = "Leader")
	bool IsDefecting() const { return DefectionRisk >= 0.85f; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void TickDirectiveEscalation(float DeltaTime);
	void TickConditionRecovery(float DeltaTime);
	void RecalculateDefectionRisk();
};
