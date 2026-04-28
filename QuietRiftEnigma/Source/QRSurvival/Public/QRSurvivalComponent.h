#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "QRTypes.h"
#include "QRSurvivalComponent.generated.h"

class UQRItemInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSurvivalStatusChanged, FGameplayTagContainer, ActiveStatuses);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthChanged, float, NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

// One active injury record
USTRUCT(BlueprintType)
struct QRSURVIVAL_API FQRInjury
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EQRInjuryType Type = EQRInjuryType::None;

	UPROPERTY(BlueprintReadOnly)
	EQRInjurySeverity Severity = EQRInjurySeverity::Minor;

	// Remaining duration in game-hours (negative = permanent until treated)
	UPROPERTY(BlueprintReadOnly)
	float RemainingHours = 24.0f;

	// DPS applied to the character per real-second
	UPROPERTY(BlueprintReadOnly)
	float DamagePerSecond = 0.0f;

	// Health per real-second restored if treated correctly
	UPROPERTY(BlueprintReadOnly)
	float HealRateIfTreated = 0.0f;

	bool operator==(const FQRInjury& Other) const
	{
		return Type == Other.Type && Severity == Other.Severity;
	}
};

// Manages all survival vitals: health, hunger, thirst, fatigue, temperature, and injuries
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QRSURVIVAL_API UQRSurvivalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRSurvivalComponent();

	// ── Config ───────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config")
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config")
	float MaxHunger = 100.0f;   // 0 = starving, 100 = full

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config")
	float MaxThirst = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config")
	float MaxFatigue = 100.0f;  // 100 = fully rested

	// Calories per real-second drain (tuned to game-day simulation)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config")
	float HungerDrainPerSecond = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config")
	float ThirstDrainPerSecond = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config")
	float FatigueDrainPerSecond = 0.005f;

	// Below this hunger %, health starts draining
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config")
	float StarvationThreshold = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config")
	float DehydrationThreshold = 0.1f;

	// ── Vitals (Replicated) ───────────────────
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Survival|Vitals")
	float Health = 100.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Hunger, Category = "Survival|Vitals")
	float Hunger = 80.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Thirst, Category = "Survival|Vitals")
	float Thirst = 80.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Fatigue, Category = "Survival|Vitals")
	float Fatigue = 100.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Survival|Vitals")
	float CoreTemperature = 37.0f; // Celsius

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Survival|Vitals")
	TArray<FQRInjury> ActiveInjuries;

	// Currently active status tags (e.g. Status.Bleeding, Status.Need.Hungry)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Survival|Vitals")
	FGameplayTagContainer ActiveStatusTags;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Survival|Vitals")
	bool bIsDead = false;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Survival|Events")
	FOnSurvivalStatusChanged OnStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Survival|Events")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Survival|Events")
	FOnDeath OnDeath;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	void ApplyDamage(float Amount, EQRInjuryType InjuryType = EQRInjuryType::None);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	void ApplyHealing(float Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	void ConsumeFood(const UQRItemInstance* FoodItem);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	void DrinkWater(float WaterML, bool bIsPurified = false);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	void Rest(float GameHours);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	void AddInjury(EQRInjuryType Type, EQRInjurySeverity Severity);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	bool TreatInjury(EQRInjuryType Type, float HealBoost = 0.0f);

	UFUNCTION(BlueprintPure, Category = "Survival")
	bool HasInjury(EQRInjuryType Type) const;

	UFUNCTION(BlueprintPure, Category = "Survival")
	bool IsStarving() const  { return Hunger / MaxHunger < StarvationThreshold; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	bool IsDehydrated() const { return Thirst / MaxThirst < DehydrationThreshold; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	bool IsExhausted() const  { return Fatigue <= 10.0f; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetHealthPercent() const { return Health / MaxHealth; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void DrainNeeds(float DeltaTime);
	void ApplyNeedDamage(float DeltaTime);
	void TickInjuries(float DeltaTime);
	void RefreshStatusTags();
	void TriggerDeath();

	UFUNCTION() void OnRep_Health();
	UFUNCTION() void OnRep_Hunger();
	UFUNCTION() void OnRep_Thirst();
	UFUNCTION() void OnRep_Fatigue();
};
