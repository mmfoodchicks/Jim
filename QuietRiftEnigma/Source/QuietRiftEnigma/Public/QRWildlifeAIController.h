#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "QRTypes.h"
#include "QRWildlifeAIController.generated.h"

class AQRWildlifeBase;

/**
 * Code-only AI controller for AQRWildlifeBase pawns. Drives a small
 * state machine via a 4Hz think timer plus AAIController::MoveToLocation
 * for navmesh-aware pathing.
 *
 * Why no UBehaviorTree asset: the BT graph (composites, decorators,
 * task nodes, connections) isn't authorable via Python in UE 5.x, and
 * the wildlife behaviour we need is simple enough that a C++ FSM is
 * cheaper to maintain than 27 BT assets. Designer can still author a
 * UBehaviorTree later and assign it to the pawn's BehaviorTree
 * property — AQRWildlifeBase::BeginPlay will RunBehaviorTree on it
 * and our controller will respect that (we no-op think while a BT is
 * driving).
 *
 * State machine (per pawn->BehaviorRole):
 *   Prey:      Idle -> Wandering <-> Grazing; Alert/Flee on threat.
 *   Predator:  Idle -> Wandering; Stalking/Charging/Attacking on prey.
 *   Scavenger: Idle -> Wandering; Scavenging on nearby corpse (TBD).
 *   Ambient:   Idle -> Wandering forever, no perception reaction.
 *   Hazard:    Static, no movement.
 */
UCLASS(BlueprintType)
class QUIETRIFTENIGMA_API AQRWildlifeAIController : public AAIController
{
	GENERATED_BODY()

public:
	AQRWildlifeAIController();

	// Distance in cm at which this animal sees a threat (prey) or prey
	// (predator). 1500 cm = 15 m by default.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QR|Wildlife",
		meta = (ClampMin = "100.0"))
	float PerceptionRadius = 1500.0f;

	// Wander radius around the pawn's spawn-time home location.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QR|Wildlife",
		meta = (ClampMin = "100.0"))
	float WanderRadius = 800.0f;

	// Once fleeing, the animal stops fleeing once this far from the
	// threat. Should be >= PerceptionRadius * 2 so it doesn't immediately
	// re-perceive and re-flee.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QR|Wildlife",
		meta = (ClampMin = "500.0"))
	float SafeFleeDistance = 3500.0f;

	// Distance at which a predator can start an attack swing.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QR|Wildlife",
		meta = (ClampMin = "50.0"))
	float AttackRange = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QR|Wildlife",
		meta = (ClampMin = "0.0"))
	float AttackDamage = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QR|Wildlife",
		meta = (ClampMin = "0.25"))
	float AttackIntervalSeconds = 1.5f;

	// Frequency of the AI think tick. 0.25s = 4Hz, plenty for wildlife.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QR|Wildlife",
		meta = (ClampMin = "0.05", ClampMax = "5.0"))
	float ThinkIntervalSeconds = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QR|Wildlife",
		meta = (ClampMin = "0.0"))
	float IdleDwellMin = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QR|Wildlife",
		meta = (ClampMin = "0.5"))
	float IdleDwellMax = 6.0f;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AQRWildlifeBase> WildlifePawn;

	// Threat (prey side) or target prey (predator side). Same field reused.
	UPROPERTY(Transient)
	TObjectPtr<AActor> CurrentTarget;

	FVector HomeLocation = FVector::ZeroVector;
	float DwellUntilSec = 0.0f;
	float NextAttackAtSec = 0.0f;
	bool bMoveOutstanding = false;

	FTimerHandle ThinkTimerHandle;

	/** Periodic state-machine update. */
	void Think();

	/** Apply the pawn's current state-driven speed and broadcast. */
	void SetState(EQRWildlifeAIState NewState);

	bool IsPredatorRole() const;
	bool IsAmbientRole() const;
	bool IsHazardRole() const;

	/** Within PerceptionRadius — player counts; in future, other
	 *  predators could too. Returns nullptr if nothing detected. */
	AActor* ScanForThreat() const;

	/** Within PerceptionRadius — looks for prey-role wildlife. */
	AActor* ScanForPrey() const;

	/** Pick a navmesh-projected wander destination around HomeLocation. */
	FVector PickWanderTarget() const;

	/** Vector-away-from-threat destination, projected to navmesh. */
	FVector PickFleeTarget(const AActor* Threat) const;

	/** Apply an attack swing if cooldown ready and target alive. */
	void TryAttack(AActor* Target);

	/** True if AQRWildlifeBase::BehaviorTree is set on the pawn -- in
	 *  that case the BT is in charge and our state machine should
	 *  step aside. */
	bool IsPawnBTDriven() const;
};
