#include "QRWildlifeSpawner.h"
#include "QRWildlifeBase.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "TimerManager.h"

AQRWildlifeSpawner::AQRWildlifeSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AQRWildlifeSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) return;

	for (int32 i = 0; i < InitialBurst; ++i)
	{
		TrySpawnOne();
	}

	if (bAutoStart)
	{
		StartSpawning();
	}
}

void AQRWildlifeSpawner::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TopUpTimerHandle);
	}
	Super::EndPlay(Reason);
}

void AQRWildlifeSpawner::StartSpawning()
{
	if (!GetWorld()) return;
	GetWorld()->GetTimerManager().SetTimer(
		TopUpTimerHandle, this, &AQRWildlifeSpawner::TopUpNow,
		FMath::Max(SpawnIntervalSeconds, 0.25f), /*loop*/ true);
}

void AQRWildlifeSpawner::StopSpawning()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TopUpTimerHandle);
	}
}

void AQRWildlifeSpawner::TopUpNow()
{
	if (!HasAuthority()) return;

	// Counting pass picks a single budget to refill toward, so a long
	// timer interval can still place several animals at once if a lot
	// of carcasses were cleared between ticks.
	int32 Live = CountLiveTrackedWildlife();
	int32 ToSpawn = FMath::Clamp(MaxAlive - Live, 0, 8);
	for (int32 i = 0; i < ToSpawn; ++i)
	{
		if (!TrySpawnOne()) break;
	}
}

int32 AQRWildlifeSpawner::CountLiveTrackedWildlife() const
{
	int32 N = 0;
	if (bGlobalCap)
	{
		if (UWorld* W = GetWorld())
		{
			for (TActorIterator<AQRWildlifeBase> It(W); It; ++It)
			{
				AQRWildlifeBase* A = *It;
				if (A && !A->IsDead()) ++N;
			}
		}
	}
	else
	{
		for (const TWeakObjectPtr<AQRWildlifeBase>& W : SpawnedAnimals)
		{
			AQRWildlifeBase* A = W.Get();
			if (A && !A->IsDead()) ++N;
		}
	}
	return N;
}

void AQRWildlifeSpawner::ClearMyWildlife()
{
	for (TWeakObjectPtr<AQRWildlifeBase>& W : SpawnedAnimals)
	{
		if (AQRWildlifeBase* A = W.Get())
		{
			A->Destroy();
		}
	}
	SpawnedAnimals.Reset();
}

bool AQRWildlifeSpawner::PickSpawnLocation(FVector& OutLocation) const
{
	UWorld* W = GetWorld();
	if (!W) return false;

	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(W);
	if (!Nav) return false;

	const FVector Origin = GetActorLocation();
	for (int32 attempt = 0; attempt < 12; ++attempt)
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float R     = FMath::FRandRange(SpawnRadiusMin, SpawnRadiusMax);
		const FVector Raw = Origin + FVector(FMath::Cos(Angle) * R,
		                                     FMath::Sin(Angle) * R,
		                                     0.0f);
		FNavLocation Out;
		if (Nav->ProjectPointToNavigation(Raw, Out, FVector(400.0f, 400.0f, 800.0f)))
		{
			OutLocation = Out.Location + FVector(0.0f, 0.0f, 100.0f);
			return true;
		}
	}
	return false;
}

bool AQRWildlifeSpawner::TrySpawnOne()
{
	if (!HasAuthority() || SpeciesPool.Num() == 0) return false;
	if (CountLiveTrackedWildlife() >= MaxAlive) return false;

	UWorld* W = GetWorld();
	if (!W) return false;

	const int32 Idx = FMath::RandRange(0, SpeciesPool.Num() - 1);
	TSubclassOf<AQRWildlifeBase> Cls = SpeciesPool[Idx];
	if (!*Cls) return false;

	FVector Loc;
	if (!PickSpawnLocation(Loc)) return false;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AQRWildlifeBase* A = W->SpawnActor<AQRWildlifeBase>(
		*Cls, Loc, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Params);
	if (!A) return false;

	SpawnedAnimals.Add(A);
	return true;
}
