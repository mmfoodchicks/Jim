#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "QRScentComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnScentChanged, float, NewIntensity, FGameplayTag, DominantTag);

// Tracks the scent signature of the owning actor.
// Attach to characters/NPCs. Updated by inventory changes when meat or bloody items are carried.
// Predator AI reads GetDetectionRadiusMultiplier() to scale their detection sphere.
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRScentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRScentComponent();

	// ── Config ───────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scent|Config", meta = (ClampMin = "0", ClampMax = "1"))
	float ScentDecayRatePerSecond = 0.01f;

	// Scent intensity contributed per kg of raw meat in inventory
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scent|Config", meta = (ClampMin = "0"))
	float MeatIntensityPerKg = 0.20f;

	// Flat intensity bonus when the actor has fresh blood on them
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scent|Config", meta = (ClampMin = "0", ClampMax = "1"))
	float BloodScentBonus = 0.40f;

	// Maximum detection-radius multiplier at ScentIntensity == 1.0
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scent|Config", meta = (ClampMin = "1"))
	float MaxDetectionRadiusMult = 3.0f;

	// ── Runtime State (Replicated) ───────────
	// Normalized scent intensity [0..1]; 0 = no scent, 1 = maximum
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Scent")
	float ScentIntensity = 0.0f;

	// Tag describing what the dominant scent is (Scent.Meat, Scent.Blood, Scent.Carrion)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Scent")
	FGameplayTag DominantScentTag;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Scent|Events")
	FOnScentChanged OnScentChanged;

	// ── Interface ────────────────────────────
	// Add a scent contribution directly (e.g. from a butchering event)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Scent")
	void AddScent(float Intensity, FGameplayTag ScentTag);

	// Zero out all scent (bathing, washing, etc.)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Scent")
	void ClearScent();

	// Call when inventory contents change. MeatMassKg = total raw meat kg carried.
	// bHasBlood = true if inventory contains blood-covered items or actor just butchered.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Scent")
	void RefreshFromInventory(float MeatMassKg, bool bHasBlood);

	// Detection-radius multiplier for predator AI (1.0 = no scent, up to MaxDetectionRadiusMult)
	UFUNCTION(BlueprintPure, Category = "Scent")
	float GetDetectionRadiusMultiplier() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
