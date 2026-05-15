#include "QRWildlifeActor.h"
#include "QRCharacter.h"
#include "QRSurvivalComponent.h"
#include "QRCodexSubsystem.h"
#include "QRItemDefinition.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AQRWildlifeActor::AQRWildlifeActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AQRWildlifeActor::BeginPlay()
{
	Super::BeginPlay();
	HomeLocation  = GetActorLocation();
	SpawnZ        = HomeLocation.Z;
	CurrentTarget = HomeLocation;
	SetState(EQRWildlifeState::Idle);
	DwellRemaining = 0.5f;
}

void AQRWildlifeActor::SetState(EQRWildlifeState NewState)
{
	if (State == NewState) return;
	State = NewState;
	// Reset transient state on transition.
	switch (State)
	{
	case EQRWildlifeState::Wander:
		PickNewTarget();
		break;
	case EQRWildlifeState::Attack:
		AttackTimer = 0.0f;
		break;
	case EQRWildlifeState::Dead:
		SetActorTickEnabled(false);
		// Collapse the AQRWorldItem sphere collision so the carcass
		// only responds to F-interact, no longer blocks the player.
		break;
	default:
		break;
	}
}

void AQRWildlifeActor::PickNewTarget()
{
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Dist  = FMath::FRandRange(WanderRadius * 0.2f, WanderRadius);
	CurrentTarget = FVector(
		HomeLocation.X + FMath::Cos(Angle) * Dist,
		HomeLocation.Y + FMath::Sin(Angle) * Dist,
		SpawnZ);
}

AActor* AQRWildlifeActor::ScanForPlayerInRange(float Range) const
{
	UWorld* W = GetWorld();
	if (!W) return nullptr;
	APlayerController* PC = W->GetFirstPlayerController();
	if (!PC) return nullptr;
	AActor* P = PC->GetPawn();
	if (!P) return nullptr;
	const float DistSq = FVector::DistSquared(P->GetActorLocation(), GetActorLocation());
	if (DistSq <= Range * Range) return P;
	return nullptr;
}

void AQRWildlifeActor::TickMovement(float DeltaTime, float Speed,
	const FVector& DesiredTarget)
{
	FVector Cur = GetActorLocation();
	FVector ToTarget = DesiredTarget - Cur;
	ToTarget.Z = 0.0f;
	const float Dist = ToTarget.Size();
	if (Dist < 1.0f) return;

	const FVector Dir = ToTarget.GetSafeNormal();
	FVector NewLoc = Cur + Dir * Speed * DeltaTime;
	NewLoc.Z = SpawnZ;
	SetActorLocation(NewLoc);

	if (!Dir.IsNearlyZero())
	{
		const FRotator TargetRot(0.0f, Dir.Rotation().Yaw, 0.0f);
		const FRotator NewRot = FMath::RInterpConstantTo(
			GetActorRotation(), TargetRot, DeltaTime, TurnRateDegPerSec);
		SetActorRotation(NewRot);
	}
}

void AQRWildlifeActor::ReceiveDamage(float Amount, AActor* Source)
{
	if (State == EQRWildlifeState::Dead) return;
	Health -= Amount;

	// Damage triggers aggression for predators, panic for prey.
	if (Source)
	{
		AggroTarget = Source;
		if (bIsPredator)
		{
			SetState(EQRWildlifeState::Stalk);
		}
		else
		{
			SetState(EQRWildlifeState::Flee);
		}
	}

	if (Health <= 0.0f)
	{
		SetState(EQRWildlifeState::Dead);
	}
}

void AQRWildlifeActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (State == EQRWildlifeState::Dead) return;

	// Perception. Skip when already mid-action to keep state changes
	// from thrashing.
	if (State == EQRWildlifeState::Idle || State == EQRWildlifeState::Wander)
	{
		if (AActor* Player = ScanForPlayerInRange(PerceptionRadius))
		{
			AggroTarget = Player;
			SetState(bIsPredator ? EQRWildlifeState::Stalk : EQRWildlifeState::Flee);

			// Codex: first sighting → record this species as Observed.
			// AQRWildlifeActor is a subclass of AQRWorldItem so it carries
			// an ItemId / Definition we can use as the codex key.
			if (UWorld* W = GetWorld())
			{
				if (UQRCodexSubsystem* Codex = W->GetSubsystem<UQRCodexSubsystem>())
				{
					const FName Id = ItemId.IsNone() && Definition
						? Definition->ItemId
						: ItemId;
					const FText DisplayName = (Definition && !Definition->DisplayName.IsEmpty())
						? Definition->DisplayName
						: FText::FromName(Id);
					Codex->Record(Id, TEXT("Fauna"), DisplayName,
						EQRCodexDiscoveryState::Observed);
				}
			}
		}
	}

	switch (State)
	{
	case EQRWildlifeState::Idle:
	{
		DwellRemaining -= DeltaTime;
		if (DwellRemaining <= 0.0f) SetState(EQRWildlifeState::Wander);
		break;
	}
	case EQRWildlifeState::Wander:
	{
		FVector ToTarget = CurrentTarget - GetActorLocation();
		ToTarget.Z = 0.0f;
		if (ToTarget.SizeSquared() < 30.0f * 30.0f)
		{
			SetState(EQRWildlifeState::Idle);
			DwellRemaining = FMath::FRandRange(MinDwellSeconds, MaxDwellSeconds);
		}
		else
		{
			TickMovement(DeltaTime, MoveSpeed, CurrentTarget);
		}
		break;
	}
	case EQRWildlifeState::Flee:
	{
		AActor* Threat = AggroTarget.Get();
		if (!Threat)
		{
			SetState(EQRWildlifeState::Idle);
			DwellRemaining = MaxDwellSeconds;
			break;
		}
		// Vector away from threat, distance-scaled.
		FVector Away = GetActorLocation() - Threat->GetActorLocation();
		Away.Z = 0.0f;
		if (Away.SizeSquared() > PerceptionRadius * PerceptionRadius * 4.0f)
		{
			// Far enough — calm down.
			AggroTarget = nullptr;
			SetState(EQRWildlifeState::Wander);
			HomeLocation = GetActorLocation();  // re-anchor where we ended up
			break;
		}
		const FVector FleeTarget = GetActorLocation() + Away.GetSafeNormal() * 1500.0f;
		TickMovement(DeltaTime, MoveSpeed * FleeSpeedMult, FleeTarget);
		break;
	}
	case EQRWildlifeState::Stalk:
	{
		AActor* Prey = AggroTarget.Get();
		if (!Prey)
		{
			SetState(EQRWildlifeState::Wander);
			break;
		}
		const float DistSq = FVector::DistSquared(Prey->GetActorLocation(), GetActorLocation());
		if (DistSq > PerceptionRadius * PerceptionRadius * 4.0f)
		{
			AggroTarget = nullptr;
			SetState(EQRWildlifeState::Wander);
			break;
		}
		if (DistSq <= AttackRadius * AttackRadius)
		{
			SetState(EQRWildlifeState::Attack);
			break;
		}
		TickMovement(DeltaTime, MoveSpeed * StalkSpeedMult, Prey->GetActorLocation());
		break;
	}
	case EQRWildlifeState::Attack:
	{
		AActor* Prey = AggroTarget.Get();
		if (!Prey)
		{
			SetState(EQRWildlifeState::Wander);
			break;
		}
		const float DistSq = FVector::DistSquared(Prey->GetActorLocation(), GetActorLocation());
		if (DistSq > AttackRadius * AttackRadius * 1.5f * 1.5f)
		{
			SetState(EQRWildlifeState::Stalk);
			break;
		}
		AttackTimer -= DeltaTime;
		if (AttackTimer <= 0.0f)
		{
			AttackTimer = AttackInterval;
			if (UQRSurvivalComponent* Surv = Prey->FindComponentByClass<UQRSurvivalComponent>())
			{
				if (Prey->HasAuthority())
				{
					Surv->ApplyDamage(AttackDamage);
				}
			}
		}
		// Continue facing prey but stop pushing into it.
		const FVector ToPrey = (Prey->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!ToPrey.IsNearlyZero())
		{
			const FRotator TargetRot(0.0f, ToPrey.Rotation().Yaw, 0.0f);
			const FRotator NewRot = FMath::RInterpConstantTo(
				GetActorRotation(), TargetRot, DeltaTime, TurnRateDegPerSec);
			SetActorRotation(NewRot);
		}
		break;
	}
	default: break;
	}
}
