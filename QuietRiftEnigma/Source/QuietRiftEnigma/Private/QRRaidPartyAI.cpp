#include "QRRaidPartyAI.h"
#include "QRSurvivalComponent.h"
#include "QRFactionCamp.h"
#include "QRCampSimComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"


UQRRaidPartyAI::UQRRaidPartyAI()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;  // 10 Hz routing
}


void UQRRaidPartyAI::BeginPlay()
{
	Super::BeginPlay();
	SetState(EQRRaidPartyState::Marching);
}


void UQRRaidPartyAI::SetState(EQRRaidPartyState NewState)
{
	State = NewState;
	StateTimer = 0.0f;
	if (NewState == EQRRaidPartyState::Dead)
	{
		PrimaryComponentTick.SetTickFunctionEnable(false);
	}
}


void UQRRaidPartyAI::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Owner = GetOwner();
	if (!Owner) return;

	StateTimer += DeltaTime;

	// Survival check — if our survival component is dead, transition.
	if (UQRSurvivalComponent* Surv = Owner->FindComponentByClass<UQRSurvivalComponent>())
	{
		if (Surv->bIsDead && State != EQRRaidPartyState::Dead)
		{
			// Report the loss back to the source camp.
			if (UWorld* W = GetWorld())
			{
				for (TActorIterator<AQRFactionCamp> It(W); It; ++It)
				{
					if (AQRFactionCamp* Camp = *It)
					{
						if (Camp->Sim && Camp->Sim->CampId == SourceCampId)
						{
							Camp->Sim->ReportRaidDefeated(1);
							break;
						}
					}
				}
			}
			SetState(EQRRaidPartyState::Dead);
			return;
		}
	}

	// Retreat takes priority over engagement when wounded.
	if (State != EQRRaidPartyState::Retreat && ShouldRetreat())
	{
		SetState(EQRRaidPartyState::Retreat);
	}

	switch (State)
	{
	case EQRRaidPartyState::Marching:
	{
		if (AActor* Hostile = ScanForHostile())
		{
			CurrentTarget = Hostile;
			SetState(EQRRaidPartyState::Engage);
			break;
		}
		MoveToward(TargetLocation, MarchSpeed, DeltaTime);
		// If we've reached the target location and no hostiles found,
		// the raid effectively wasted itself — retreat with what we have.
		if (FVector::DistSquared(Owner->GetActorLocation(), TargetLocation) < 200.0f * 200.0f)
		{
			SetState(EQRRaidPartyState::Retreat);
		}
		break;
	}
	case EQRRaidPartyState::Engage:
	{
		AActor* T = CurrentTarget.Get();
		if (!T)
		{
			T = ScanForHostile();
			CurrentTarget = T;
		}
		if (!T)
		{
			SetState(EQRRaidPartyState::Marching);
			break;
		}
		const float DistSq = FVector::DistSquared(T->GetActorLocation(), Owner->GetActorLocation());
		if (DistSq > PerceptionRadius * PerceptionRadius * 1.5f * 1.5f)
		{
			// Lost line of sight — break and march again.
			CurrentTarget = nullptr;
			SetState(EQRRaidPartyState::Marching);
			break;
		}
		if (DistSq > AttackRadius * AttackRadius)
		{
			MoveToward(T->GetActorLocation(), EngageSpeed, DeltaTime);
			break;
		}
		// In attack range.
		AttackTimer -= DeltaTime;
		if (AttackTimer <= 0.0f)
		{
			AttackTimer = AttackInterval;
			if (UQRSurvivalComponent* TSurv = T->FindComponentByClass<UQRSurvivalComponent>())
			{
				if (T->HasAuthority())
				{
					TSurv->ApplyDamage(AttackDamage);
				}
			}
		}
		// Face the target.
		const FVector ToT = (T->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
		if (!ToT.IsNearlyZero())
		{
			Owner->SetActorRotation(FRotator(0, ToT.Rotation().Yaw, 0));
		}
		break;
	}
	case EQRRaidPartyState::Retreat:
	{
		MoveToward(CampOrigin, EngageSpeed, DeltaTime);
		if (FVector::DistSquared(Owner->GetActorLocation(), CampOrigin) < 300.0f * 300.0f)
		{
			// Made it home — report success (survivors return) and despawn.
			if (UWorld* W = GetWorld())
			{
				for (TActorIterator<AQRFactionCamp> It(W); It; ++It)
				{
					if (AQRFactionCamp* Camp = *It)
					{
						if (Camp->Sim && Camp->Sim->CampId == SourceCampId)
						{
							Camp->Sim->ReportRaidSuccessful(1, 0.0f);
							break;
						}
					}
				}
			}
			Owner->Destroy();
		}
		break;
	}
	default: break;
	}
}


bool UQRRaidPartyAI::MoveToward(const FVector& Destination, float Speed, float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner) return false;

	FVector Cur = Owner->GetActorLocation();
	FVector To  = Destination - Cur;
	To.Z = 0.0f;
	if (To.SizeSquared() < 1.0f) return true;

	const FVector Dir = To.GetSafeNormal();
	FVector NewLoc = Cur + Dir * Speed * DeltaTime;
	NewLoc.Z = Cur.Z;
	Owner->SetActorLocation(NewLoc);
	if (!Dir.IsNearlyZero())
	{
		Owner->SetActorRotation(FRotator(0, Dir.Rotation().Yaw, 0));
	}
	return false;
}


AActor* UQRRaidPartyAI::ScanForHostile() const
{
	UWorld* W = GetWorld();
	if (!W) return nullptr;
	AActor* Owner = GetOwner();
	if (!Owner) return nullptr;
	const FVector Loc = Owner->GetActorLocation();
	const float R2 = PerceptionRadius * PerceptionRadius;

	// Player pawn first.
	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		if (APawn* P = PC->GetPawn())
		{
			if (FVector::DistSquared(P->GetActorLocation(), Loc) <= R2) return P;
		}
	}
	return nullptr;
}


bool UQRRaidPartyAI::ShouldRetreat() const
{
	if (AActor* Owner = GetOwner())
	{
		if (UQRSurvivalComponent* Surv = Owner->FindComponentByClass<UQRSurvivalComponent>())
		{
			if (Surv->MaxHealth > 0.0f)
			{
				return (Surv->Health / Surv->MaxHealth) < RetreatHealthFrac;
			}
		}
	}
	return false;
}
