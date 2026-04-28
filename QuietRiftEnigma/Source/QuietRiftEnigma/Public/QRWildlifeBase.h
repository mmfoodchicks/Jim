#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AIController.h"
#include "QRTypes.h"
#include "QRWildlifeBase.generated.h"

class UQRSurvivalComponent;
class UBehaviorTree;
class UAIPerceptionComponent;

// Drop entry when the animal is harvested/killed
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRWildlifeDrop
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 MinQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 MaxQuantity = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float DropChance = 1.0f;
};

// Base class for all wildlife — prey, predators, and ambient creatures
UCLASS(Abstract, BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlifeBase : public ACharacter
{
	GENERATED_BODY()

public:
	AQRWildlifeBase();

	// ── Identity ─────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	FName SpeciesId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	FText SpeciesDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	EQRWildlifeBehaviorRole BehaviorRole = EQRWildlifeBehaviorRole::Prey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	EQRBiomeType PreferredBiome = EQRBiomeType::Forest;

	// ── Stats ─────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float MaxHealth = 80.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Wildlife")
	float CurrentHealth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float MoveSpeedWalk = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float MoveSpeedFlee = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float MoveSpeedCharge = 500.0f;

	// Detection range for threat awareness
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float ThreatDetectionRadius = 1500.0f;

	// How loud this animal is (0 = silent; affects player stealth)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float NoiseFactor = 0.3f;

	// Mass in kg (affects player carry weight of carcass)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float MassKg = 50.0f;

	// ── Behavior Tree ─────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	// ── AI State ─────────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "AI")
	EQRWildlifeAIState AIState = EQRWildlifeAIState::Idle;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "AI")
	bool bIsDead = false;

	// ── Drops ────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drops")
	TArray<FQRWildlifeDrop> DeathDrops;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drops")
	TArray<FQRWildlifeDrop> HarvestDrops;

	// Time before carcass despawns (game-hours)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drops")
	float CarcassDespawnHours = 8.0f;

	// Herd/pack grouping ID (0 = solitary)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "AI")
	int32 HerdGroupId = 0;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Wildlife")
	void TakeDamage_Wildlife(float Amount, AActor* DamageCauser);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Wildlife")
	TArray<FQRWildlifeDrop> Harvest();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Wildlife")
	void SetAIState(EQRWildlifeAIState NewState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Wildlife")
	void AlertHerd(AActor* Threat);

	UFUNCTION(BlueprintPure, Category = "Wildlife")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Wildlife")
	float GetHealthPercent() const { return MaxHealth > 0 ? CurrentHealth / MaxHealth : 0.0f; }

	UFUNCTION(BlueprintNativeEvent, Category = "Wildlife")
	void OnDied(AActor* Killer);
	virtual void OnDied_Implementation(AActor* Killer);

	UFUNCTION(BlueprintNativeEvent, Category = "Wildlife")
	void OnThreatDetected(AActor* Threat);
	virtual void OnThreatDetected_Implementation(AActor* Threat) {}

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
