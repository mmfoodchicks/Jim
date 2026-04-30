#include "QRRemnantStructure.h"
#include "Net/UnrealNetwork.h"

AQRRemnantStructure::AQRRemnantStructure()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

void AQRRemnantStructure::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRRemnantStructure, CurrentWakeState);
	DOREPLIFETIME(AQRRemnantStructure, HostileTimeRemaining);
}

void AQRRemnantStructure::BeginPlay()
{
	Super::BeginPlay();
}

void AQRRemnantStructure::NotifyRiftResearchProgress(float Progress)
{
	if (!HasAuthority()) return;

	switch (CurrentWakeState)
	{
	case EQRRemnantWakeState::Dormant:
		if (Progress >= StirThreshold)
			SetWakeState(EQRRemnantWakeState::Stirring);
		break;
	case EQRRemnantWakeState::Stirring:
		if (Progress >= ActiveThreshold)
			SetWakeState(EQRRemnantWakeState::Active);
		break;
	default:
		break;
	}
}

void AQRRemnantStructure::AdvanceWakeState()
{
	if (!HasAuthority()) return;

	switch (CurrentWakeState)
	{
	case EQRRemnantWakeState::Dormant:    SetWakeState(EQRRemnantWakeState::Stirring);  break;
	case EQRRemnantWakeState::Stirring:   SetWakeState(EQRRemnantWakeState::Active);    break;
	case EQRRemnantWakeState::Active:     SetWakeState(EQRRemnantWakeState::Hostile);   break;
	case EQRRemnantWakeState::Hostile:    SetWakeState(EQRRemnantWakeState::Subsiding); break;
	case EQRRemnantWakeState::Subsiding:  SetWakeState(EQRRemnantWakeState::Active);    break;
	}
}

float AQRRemnantStructure::TickStudy(float DeltaTime)
{
	if (!HasAuthority()) return 0.0f;
	if (!CanBeStudied())  return -1.0f;
	if (bStudyComplete)   return 0.0f;

	StudyAccumulator += DeltaTime;
	if (StudyAccumulator >= StudyDurationSeconds)
	{
		bStudyComplete = true;
		StudyAccumulator = 0.0f;
		return GetStudyYield();
	}
	return 0.0f;
}

float AQRRemnantStructure::GetStudyYield() const
{
	switch (CurrentWakeState)
	{
	case EQRRemnantWakeState::Active:   return BaseResearchPointsOnStudy * 1.5f;
	case EQRRemnantWakeState::Hostile:  return BaseResearchPointsOnStudy * 0.5f;
	default:                            return BaseResearchPointsOnStudy;
	}
}

void AQRRemnantStructure::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority()) return;

	if (CurrentWakeState == EQRRemnantWakeState::Hostile)
	{
		TickDefensiveDamage(DeltaTime);

		HostileTimeRemaining -= DeltaTime;
		if (HostileTimeRemaining <= 0.0f)
		{
			// Transition to Subsiding — reuse HostileTimeRemaining as the cooldown clock.
			// Structures wind down at 1/3 the speed they wound up.
			SetWakeState(EQRRemnantWakeState::Subsiding);
			HostileTimeRemaining = HostileDurationSeconds * 0.333f;
		}
	}
	else if (CurrentWakeState == EQRRemnantWakeState::Subsiding)
	{
		HostileTimeRemaining -= DeltaTime;
		if (HostileTimeRemaining <= 0.0f)
		{
			HostileTimeRemaining = 0.0f;
			SetWakeState(EQRRemnantWakeState::Active);
		}
	}
}

void AQRRemnantStructure::SetWakeState(EQRRemnantWakeState NewState)
{
	if (CurrentWakeState == NewState) return;

	CurrentWakeState = NewState;

	if (NewState == EQRRemnantWakeState::Hostile)
		HostileTimeRemaining = HostileDurationSeconds;

	OnWakeStateChanged.Broadcast(this, NewState);
	OnWakeStateEntered(NewState);
}

void AQRRemnantStructure::TickDefensiveDamage(float DeltaTime)
{
	if (!bHasDefensiveSystems) return;

	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere  = FCollisionShape::MakeSphere(DefensiveRadiusCm);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	GetWorld()->OverlapMultiByChannel(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		ECC_Pawn,
		Sphere,
		Params);

	const float DamageThisTick = DefensiveDamagePerSecond * DeltaTime;
	for (const FOverlapResult& Hit : Overlaps)
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			HitActor->TakeDamage(DamageThisTick, FDamageEvent(), nullptr, this);
		}
	}
}

void AQRRemnantStructure::OnRep_WakeState()
{
	OnWakeStateEntered(CurrentWakeState);
}
