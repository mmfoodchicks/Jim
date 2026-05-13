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

	// ── Oxygen ─────────────────────────────────
	// Game premise: limited oxygen on the moon surface. Suit/Mask refills
	// Oxygen; consumption is constant when outside breathable air. Pause
	// drain by setting OxygenDrainPerSecond to 0 indoors / in safe zones.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config")
	float MaxOxygen = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config")
	float OxygenDrainPerSecond = 0.0f; // 0 by default — outdoor zones set this on entry

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config")
	float LowOxygenThreshold = 0.30f;  // tag warning fires below this fraction

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config")
	float SuffocationThreshold = 0.05f; // health drain starts below this

	// ── Sprint locomotion ──────────────────────
	// Multiplier applied to FatigueDrainPerSecond when the owning character
	// has bIsSprinting==true (read reflectively so non-AQRCharacter owners
	// just behave neutrally). 3x is enough for a noticeable burn without
	// making sprint useless.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config",
		meta = (ClampMin = "1.0", ClampMax = "10.0"))
	float SprintFatigueDrainMultiplier = 3.0f;

	// ── Hyperthermia ───────────────────────────
	// Symmetric with the existing hypothermia path (CoreTemperature < 35).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Config")
	float HyperthermiaThreshold = 39.0f;

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

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Oxygen, Category = "Survival|Vitals")
	float Oxygen = 100.0f;

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

	// Applies nutrition from FoodItem and decrements its Quantity by 1.
	// Caller must remove the item instance from inventory when Quantity reaches 0.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	void ConsumeFood(UQRItemInstance* FoodItem);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	void DrinkWater(float WaterML, bool bIsPurified = false);

	// Attempt to spread a disease to survivors within RadiusCm (shared-meal / shared-source contamination).
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	void SpreadInfectionNearby(float RadiusCm, EQRInjuryType Type, float Chance);

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
	bool IsLowOxygen() const  { return Oxygen / MaxOxygen < LowOxygenThreshold; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	bool IsSuffocating() const { return Oxygen / MaxOxygen < SuffocationThreshold; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	bool IsOverheating() const { return CoreTemperature > HyperthermiaThreshold; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetHealthPercent() const { return Health / MaxHealth; }

	// Composite movement penalty from injuries + survival state. Returns a
	// multiplier in [0.5, 1.0] — 1.0 = no penalty, 0.5 = halved speed.
	// Fracture: 0.6x. Severe bleeding: 0.85x. Exhausted/Suffocating: 0.7x.
	// Multipliers stack multiplicatively, then clamp to 0.5 floor so the
	// player still has some control even when wrecked.
	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetMovementSpeedMultiplier() const;

	// True when sprint should be denied — exhausted OR suffocating OR
	// severe-fracture. Called by AQRCharacter::CanSprint via reflection.
	UFUNCTION(BlueprintPure, Category = "Survival")
	bool IsSprintBlockedByCondition() const;

	// Refill oxygen — usually called by a respirator / suit when in a
	// pressurized zone, or by triggering an indoor volume.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	void RefillOxygen(float Amount);

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
	UFUNCTION() void OnRep_Oxygen();

	// Reflectively look up the owner's bIsSprinting flag.
	bool QueryOwnerIsSprinting() const;
};
