#pragma once

#include "CoreMinimal.h"
#include "QRWorldItem.h"
#include "QRWildlifeActor.generated.h"

/**
 * Wildlife AI state. Drives the tick-based FSM in AQRWildlifeActor.
 */
UENUM(BlueprintType)
enum class EQRWildlifeState : uint8
{
	Idle    UMETA(DisplayName = "Idle"),
	Wander  UMETA(DisplayName = "Wander"),
	Flee    UMETA(DisplayName = "Flee (prey reacting to threat)"),
	Stalk   UMETA(DisplayName = "Stalk (predator closing on prey)"),
	Attack  UMETA(DisplayName = "Attack"),
	Dead    UMETA(DisplayName = "Dead"),
};

/**
 * Wildlife actor with a lightweight tick-driven FSM. Inherits from
 * AQRWorldItem so the carcass can still be picked up by F-interact
 * after the creature dies (Dead state stops ticking + collapses
 * collision).
 *
 *   Prey (bIsPredator = false): Idle → Wander; sees player at
 *     PerceptionRadius → Flee. Wears down to Idle/Wander when the
 *     player leaves.
 *   Predator (bIsPredator = true): Idle → Wander; sees player → Stalk
 *     (slow approach); within AttackRadius → Attack (damages
 *     UQRSurvivalComponent on a per-AttackInterval beat).
 *
 * Movement is still 2D-locked (no falling) and traceless — the actor
 * glides on the spawn Z. Real navmesh + skeletal animation are
 * follow-on work; this is the in-engine FSM scaffold so the spawned
 * world feels alive instead of stationary.
 */
UCLASS(BlueprintType)
class QUIETRIFTENIGMA_API AQRWildlifeActor : public AQRWorldItem
{
	GENERATED_BODY()

public:
	AQRWildlifeActor();

	// ── Movement ──────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Movement")
	float WanderRadius = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Movement")
	float MoveSpeed = 140.0f;

	// Speed multiplier when fleeing / stalking / attacking.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Movement")
	float FleeSpeedMult = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Movement")
	float StalkSpeedMult = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Movement")
	float TurnRateDegPerSec = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Movement")
	float MinDwellSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Movement")
	float MaxDwellSeconds = 5.0f;

	// ── Behavior ──────────────────────────────
	// True = predator (Stalk + Attack). False = prey (Flee).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Behavior")
	bool bIsPredator = false;

	// Range at which the creature notices the player (cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Behavior",
		meta = (ClampMin = "100", ClampMax = "10000"))
	float PerceptionRadius = 1800.0f;

	// Predators within this range trigger Attack.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Behavior",
		meta = (ClampMin = "50", ClampMax = "1000"))
	float AttackRadius = 200.0f;

	// Damage dealt per attack beat.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Behavior",
		meta = (ClampMin = "0", ClampMax = "500"))
	float AttackDamage = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Behavior",
		meta = (ClampMin = "0.1", ClampMax = "10"))
	float AttackInterval = 1.25f;

	// Hit points before going Dead state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife|Behavior",
		meta = (ClampMin = "1", ClampMax = "10000"))
	float Health = 60.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wildlife|State")
	EQRWildlifeState State = EQRWildlifeState::Idle;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Wildlife")
	void ReceiveDamage(float Amount, AActor* Source);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	FVector HomeLocation = FVector::ZeroVector;
	FVector CurrentTarget = FVector::ZeroVector;
	float SpawnZ = 0.0f;
	float DwellRemaining = 0.0f;
	float AttackTimer = 0.0f;
	TWeakObjectPtr<AActor> AggroTarget;

	void PickNewTarget();
	void SetState(EQRWildlifeState NewState);
	void TickMovement(float DeltaTime, float Speed, const FVector& DesiredTarget);
	AActor* ScanForPlayerInRange(float Range) const;
};
