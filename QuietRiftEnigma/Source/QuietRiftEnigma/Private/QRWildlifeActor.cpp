#include "QRWildlifeActor.h"
#include "Engine/World.h"

AQRWildlifeActor::AQRWildlifeActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AQRWildlifeActor::BeginPlay()
{
	Super::BeginPlay();
	HomeLocation = GetActorLocation();
	SpawnZ       = HomeLocation.Z;
	CurrentTarget = HomeLocation;
	bDwelling    = true;
	DwellRemaining = 0.5f;
}

void AQRWildlifeActor::PickNewTarget()
{
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Dist  = FMath::FRandRange(WanderRadius * 0.2f, WanderRadius);
	CurrentTarget = FVector(
		HomeLocation.X + FMath::Cos(Angle) * Dist,
		HomeLocation.Y + FMath::Sin(Angle) * Dist,
		SpawnZ);
	bDwelling = false;
}

void AQRWildlifeActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bDwelling)
	{
		DwellRemaining -= DeltaTime;
		if (DwellRemaining <= 0.0f) PickNewTarget();
		return;
	}

	FVector Cur = GetActorLocation();
	FVector ToTarget = CurrentTarget - Cur;
	ToTarget.Z = 0.0f;
	const float Dist = ToTarget.Size();

	if (Dist < 30.0f)
	{
		// Arrived — dwell for a randomized interval before moving again.
		bDwelling = true;
		DwellRemaining = FMath::FRandRange(MinDwellSeconds, MaxDwellSeconds);
		return;
	}

	const FVector Dir = ToTarget.GetSafeNormal();
	FVector NewLoc = Cur + Dir * MoveSpeed * DeltaTime;
	NewLoc.Z = SpawnZ;  // 2D wander — no falling
	SetActorLocation(NewLoc);

	// Smoothly turn the actor's yaw toward movement direction.
	if (!Dir.IsNearlyZero())
	{
		const FRotator TargetRot(0.0f, Dir.Rotation().Yaw, 0.0f);
		const FRotator NewRot = FMath::RInterpConstantTo(
			GetActorRotation(), TargetRot, DeltaTime, TurnRateDegPerSec);
		SetActorRotation(NewRot);
	}
}
