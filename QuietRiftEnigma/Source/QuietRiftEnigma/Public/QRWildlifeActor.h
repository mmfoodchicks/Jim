#pragma once

#include "CoreMinimal.h"
#include "QRWorldItem.h"
#include "QRWildlifeActor.generated.h"

/**
 * Stub wildlife actor — placeholder for proper AI animals.
 *
 * Spawned when the player drops a Wildlife-category item from the hotbar.
 * Inherits from AQRWorldItem so pickup, mesh display, and replication
 * already work; adds a simple lerp-toward-random-points wander every
 * few seconds. No nav mesh, no animations (static meshes only), no
 * pathfinding around obstacles. Players can still walk up and pick it
 * back up via the standard interact (F).
 *
 * Movement is purely 2D — Z is locked to the spawn height so the actor
 * glides on a horizontal plane rather than falling through floors.
 * This is intentionally janky-looking; the real animal AI is a separate
 * project (behavior trees, perception, locomotion, skeletal animation).
 */
UCLASS(BlueprintType)
class QUIETRIFTENIGMA_API AQRWildlifeActor : public AQRWorldItem
{
	GENERATED_BODY()

public:
	AQRWildlifeActor();

	// Max distance from spawn point the wanderer will roam to.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
	float WanderRadius = 800.0f;

	// Travel speed in cm/s.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
	float MoveSpeed = 140.0f;

	// Seconds between picking a new destination once the current one is reached.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
	float MinDwellSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
	float MaxDwellSeconds = 5.0f;

	// How quickly the actor rotates toward its movement direction (deg/s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
	float TurnRateDegPerSec = 360.0f;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	FVector HomeLocation = FVector::ZeroVector;
	FVector CurrentTarget = FVector::ZeroVector;
	float SpawnZ = 0.0f;
	float DwellRemaining = 0.0f;
	bool bDwelling = false;

	void PickNewTarget();
};
