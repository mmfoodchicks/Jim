#include "QRWildlifeAIController.h"
#include "QRWildlifeBase.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"

AQRWildlifeAIController::AQRWildlifeAIController()
{
	PrimaryActorTick.bCanEverTick = false;  // we run on a timer
	bWantsPlayerState = false;
}


void AQRWildlifeAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	WildlifePawn = Cast<AQRWildlifeBase>(InPawn);
	if (!WildlifePawn || !GetWorld()) return;

	HomeLocation = WildlifePawn->GetActorLocation();

	// Adopt the species' per-pawn combat tuning so each animal hits for its
	// own damage / reach / cadence rather than the controller's generic
	// defaults. Perception scales with the pawn's authored detection radius.
	if (WildlifePawn->AttackDamage > 0.0f)          AttackDamage = WildlifePawn->AttackDamage;
	if (WildlifePawn->AttackRange >= 50.0f)         AttackRange = WildlifePawn->AttackRange;
	if (WildlifePawn->AttackIntervalSeconds >= 0.25f) AttackIntervalSeconds = WildlifePawn->AttackIntervalSeconds;
	if (WildlifePawn->ThreatDetectionRadius >= 100.0f) PerceptionRadius = WildlifePawn->ThreatDetectionRadius;

	// Stagger initial dwell so a freshly-spawned herd doesn't all step
	// off in the same tick.
	const float Now = GetWorld()->GetTimeSeconds();
	DwellUntilSec = Now + FMath::FRandRange(IdleDwellMin, IdleDwellMax);

	SetState(IsHazardRole()
		? EQRWildlifeAIState::Idle    // hazards never move
		: EQRWildlifeAIState::Idle);  // everyone else starts idle then wanders

	GetWorld()->GetTimerManager().SetTimer(
		ThinkTimerHandle, this, &AQRWildlifeAIController::Think,
		ThinkIntervalSeconds, /*loop*/ true,
		/*first fire delay*/ FMath::FRandRange(0.0f, ThinkIntervalSeconds));
}


void AQRWildlifeAIController::OnUnPossess()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ThinkTimerHandle);
	}
	WildlifePawn = nullptr;
	CurrentTarget = nullptr;
	bMoveOutstanding = false;
	Super::OnUnPossess();
}


void AQRWildlifeAIController::SetState(EQRWildlifeAIState NewState)
{
	if (!WildlifePawn) return;
	// SetAIState on the pawn already handles speed change + blackboard
	// propagation. Reuse rather than duplicate that logic.
	WildlifePawn->SetAIState(NewState);
}


bool AQRWildlifeAIController::IsPredatorRole() const
{
	return WildlifePawn &&
		WildlifePawn->BehaviorRole == EQRWildlifeBehaviorRole::Predator;
}


bool AQRWildlifeAIController::IsAmbientRole() const
{
	return WildlifePawn &&
		WildlifePawn->BehaviorRole == EQRWildlifeBehaviorRole::Ambient;
}


bool AQRWildlifeAIController::IsHazardRole() const
{
	return WildlifePawn &&
		WildlifePawn->BehaviorRole == EQRWildlifeBehaviorRole::Hazard;
}


bool AQRWildlifeAIController::IsPawnBTDriven() const
{
	// AQRWildlifeBase::BeginPlay calls RunBehaviorTree if BehaviorTree
	// is set. If designer wires one up later, this controller stays
	// quiet and lets the BT drive.
	return WildlifePawn && WildlifePawn->BehaviorTree != nullptr;
}


AActor* AQRWildlifeAIController::ScanForThreat() const
{
	if (!WildlifePawn || !GetWorld()) return nullptr;

	const FVector Origin = WildlifePawn->GetActorLocation();
	const float PerceptionSq = PerceptionRadius * PerceptionRadius;

	AActor* Best = nullptr;
	float BestDistSq = FLT_MAX;

	// Nearby predators count as threats for everyone -- prey runs from any
	// predator; a small predator runs from a noticeably bigger predator
	// ("there's always a bigger fish": the bigger-MassKg rule below).
	const bool bSelfIsPredator =
		WildlifePawn->BehaviorRole == EQRWildlifeBehaviorRole::Predator;
	for (TActorIterator<AQRWildlifeBase> It(GetWorld()); It; ++It)
	{
		AQRWildlifeBase* Other = *It;
		if (!Other || Other == WildlifePawn || Other->IsDead()) continue;
		if (Other->BehaviorRole != EQRWildlifeBehaviorRole::Predator) continue;
		if (bSelfIsPredator)
		{
			// Predator-on-predator: only meaningfully bigger predators
			// (60%+ heavier) are threatening. A pack of equal-mass wolves
			// shouldn't all flee each other.
			if (Other->MassKg < WildlifePawn->MassKg * 1.6f) continue;
		}

		const float D2 = FVector::DistSquared(Other->GetActorLocation(), Origin);
		if (D2 <= PerceptionSq && D2 < BestDistSq)
		{
			Best = Other;
			BestDistSq = D2;
		}
	}

	// The player counts as a threat too (closest wins).
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (APawn* PlayerPawn = PC->GetPawn())
		{
			const float D2 = FVector::DistSquared(PlayerPawn->GetActorLocation(), Origin);
			if (D2 <= PerceptionSq && D2 < BestDistSq)
			{
				Best = PlayerPawn;
				BestDistSq = D2;
			}
		}
	}

	return Best;
}


AActor* AQRWildlifeAIController::ScanForPrey() const
{
	if (!WildlifePawn || !GetWorld()) return nullptr;

	const FVector Origin = WildlifePawn->GetActorLocation();
	const float PerceptionSq = PerceptionRadius * PerceptionRadius;

	AActor* Best = nullptr;
	float BestDistSq = FLT_MAX;

	// Huntable = prey/scavenger/ambient, OR a noticeably smaller predator
	// (the "bigger fish" rule -- a pack predator gladly takes a smaller
	// scavenger but not an equal-mass rival).
	for (TActorIterator<AQRWildlifeBase> It(GetWorld()); It; ++It)
	{
		AQRWildlifeBase* Other = *It;
		if (!Other || Other == WildlifePawn || Other->IsDead()) continue;

		bool bHuntable = false;
		switch (Other->BehaviorRole)
		{
		case EQRWildlifeBehaviorRole::Prey:
		case EQRWildlifeBehaviorRole::Scavenger:
		case EQRWildlifeBehaviorRole::Ambient:
			bHuntable = true;
			break;
		case EQRWildlifeBehaviorRole::Predator:
			// Smaller predators are fair game. 0.6x threshold mirrors the
			// 1.6x threshold in ScanForThreat: A hunts B iff B hunts no A.
			bHuntable = Other->MassKg < WildlifePawn->MassKg * 0.6f;
			break;
		default:
			break;
		}
		if (!bHuntable) continue;

		const float D2 = FVector::DistSquared(Other->GetActorLocation(), Origin);
		if (D2 <= PerceptionSq && D2 < BestDistSq)
		{
			Best = Other;
			BestDistSq = D2;
		}
	}

	if (Best) return Best;

	// Player as a last-resort target only when very close (20% of
	// perception). Without this clamp predators path-followed the player
	// across the map even when wildlife targets were available; with it
	// they hunt the ecosystem and only engage the player on contact.
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (APawn* PlayerPawn = PC->GetPawn())
		{
			const float CloseRadius = PerceptionRadius * 0.20f;
			const float D2 = FVector::DistSquared(PlayerPawn->GetActorLocation(), Origin);
			if (D2 <= CloseRadius * CloseRadius)
			{
				return PlayerPawn;
			}
		}
	}

	return nullptr;
}


FVector AQRWildlifeAIController::PickWanderTarget() const
{
	if (!WildlifePawn) return HomeLocation;

	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Dist  = FMath::FRandRange(WanderRadius * 0.3f, WanderRadius);
	FVector Raw = HomeLocation + FVector(
		FMath::Cos(Angle) * Dist,
		FMath::Sin(Angle) * Dist,
		0.0f);

	// Project to navmesh -- if the point lands on a hill or off-mesh,
	// we get a reachable nearest neighbour instead.
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	if (Nav)
	{
		FNavLocation Out;
		if (Nav->ProjectPointToNavigation(Raw, Out, FVector(200.0f, 200.0f, 400.0f)))
		{
			return Out.Location;
		}
	}
	return Raw;
}


FVector AQRWildlifeAIController::PickFleeTarget(const AActor* Threat) const
{
	if (!WildlifePawn || !Threat) return HomeLocation;

	FVector Away = WildlifePawn->GetActorLocation() - Threat->GetActorLocation();
	Away.Z = 0.0f;
	if (Away.IsNearlyZero())
	{
		// Threat is exactly on top of us -- pick a random direction.
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		Away = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
	}
	FVector Raw = WildlifePawn->GetActorLocation() + Away.GetSafeNormal() * SafeFleeDistance;

	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	if (Nav)
	{
		FNavLocation Out;
		if (Nav->ProjectPointToNavigation(Raw, Out, FVector(400.0f, 400.0f, 400.0f)))
		{
			return Out.Location;
		}
	}
	return Raw;
}


void AQRWildlifeAIController::TryAttack(AActor* Target)
{
	if (!WildlifePawn || !Target) return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextAttackAtSec) return;
	NextAttackAtSec = Now + AttackIntervalSeconds;

	// Apply damage. If the target is a wildlife pawn, route through its
	// damage handler so death drops + state propagation happens; if it's
	// the player or anything else, use generic UGameplayStatics.
	if (AQRWildlifeBase* OtherWildlife = Cast<AQRWildlifeBase>(Target))
	{
		OtherWildlife->TakeDamage_Wildlife(AttackDamage, WildlifePawn);
	}
	else
	{
		// Player or NPC -- standard UE damage pipeline.
		FDamageEvent DamageEvent;
		Target->TakeDamage(AttackDamage, DamageEvent, this, WildlifePawn);
	}
}


void AQRWildlifeAIController::Think()
{
	// Pre-flight: pawn dead / hazard / BT-driven -- nothing to do.
	if (!WildlifePawn || WildlifePawn->IsDead()) return;
	if (IsHazardRole()) return;
	if (IsPawnBTDriven())
	{
		// A designer-supplied BT is active. Don't fight it.
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const EQRWildlifeAIState CurState = WildlifePawn->AIState;
	const bool bAmbient = IsAmbientRole();

	// Drop a target the instant it dies, otherwise the predator stands on
	// the corpse swinging forever (the "animal won't die" log spam was a
	// dead target still being attacked). Works for wildlife prey and the
	// player alike.
	if (CurrentTarget)
	{
		bool bTargetDead = false;
		if (const AQRWildlifeBase* TargetWildlife = Cast<AQRWildlifeBase>(CurrentTarget))
		{
			bTargetDead = TargetWildlife->IsDead();
		}
		if (bTargetDead)
		{
			CurrentTarget = nullptr;
			StopMovement();
			SetState(EQRWildlifeAIState::Idle);
			DwellUntilSec = Now + FMath::FRandRange(IdleDwellMin, IdleDwellMax);
			return;
		}
	}

	// Perception step: refresh CurrentTarget for non-ambient roles.
	// Ambient animals (bone lantern drifters, lantern mite swarms) just
	// wander; they don't react to anything.
	bool bFleeingBiggerFish = false;
	if (!bAmbient)
	{
		if (IsPredatorRole())
		{
			// Bigger-fish check FIRST: even a predator runs from a
			// noticeably bigger predator. ScanForThreat with a predator
			// pawn only returns 1.6x-heavier hunters (and the player when
			// they're closer than that). If something scary is around,
			// switch to flee mode regardless of what we were doing.
			if (AActor* Hunter = ScanForThreat())
			{
				CurrentTarget = Hunter;
				bFleeingBiggerFish = true;
			}
			else if (CurState != EQRWildlifeAIState::Stalking &&
			         CurState != EQRWildlifeAIState::Charging &&
			         CurState != EQRWildlifeAIState::Attacking)
			{
				CurrentTarget = ScanForPrey();
			}
		}
		else
		{
			// Prey & scavenger -- look for threats. Don't repeatedly
			// reset CurrentTarget while already fleeing.
			if (CurState != EQRWildlifeAIState::Fleeing &&
				CurState != EQRWildlifeAIState::Fleeing_Injured)
			{
				CurrentTarget = ScanForThreat();
			}
		}
	}

	// A predator that just spotted a bigger predator drops everything and
	// bolts -- it's the same behaviour prey runs when threatened.
	if (bFleeingBiggerFish && CurrentTarget)
	{
		StopMovement();
		MoveToLocation(PickFleeTarget(CurrentTarget));
		SetState(EQRWildlifeAIState::Fleeing);
		bMoveOutstanding = true;
		return;
	}

	// Apply state. Each branch is a single tick of the FSM; movement is
	// driven via MoveToLocation which itself does the navmesh pathing
	// asynchronously over many frames -- we don't tick the actor per
	// frame, just kick a move and re-evaluate on the next think.

	switch (CurState)
	{
	case EQRWildlifeAIState::Idle:
	{
		if (CurrentTarget && !bAmbient)
		{
			SetState(IsPredatorRole()
				? EQRWildlifeAIState::Stalking
				: EQRWildlifeAIState::Alert);
			break;
		}
		if (Now >= DwellUntilSec)
		{
			MoveToLocation(PickWanderTarget());
			SetState(EQRWildlifeAIState::Wandering);
			bMoveOutstanding = true;
		}
		break;
	}

	case EQRWildlifeAIState::Wandering:
	{
		if (CurrentTarget && !bAmbient)
		{
			StopMovement();
			SetState(IsPredatorRole()
				? EQRWildlifeAIState::Stalking
				: EQRWildlifeAIState::Alert);
			bMoveOutstanding = false;
			break;
		}
		// If we arrived (PathFollowing reports idle), drop to Idle for
		// a dwell.
		if (GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			bMoveOutstanding = false;
			SetState(EQRWildlifeAIState::Idle);
			DwellUntilSec = Now + FMath::FRandRange(IdleDwellMin, IdleDwellMax);
		}
		break;
	}

	case EQRWildlifeAIState::Alert:
	{
		if (!CurrentTarget)
		{
			AlertStartSec = 0.0f;
			SetState(EQRWildlifeAIState::Wandering);
			break;
		}
		// Brief alert before transitioning to flee -- gives the player
		// a beat to read what's happening. ~0.5s from ENTERING the state.
		// (The old check reused the stale idle DwellUntilSec, which could
		// freeze prey ~6.5s or skip the beat entirely.)
		if (AlertStartSec <= 0.0f) AlertStartSec = Now;
		if (Now >= AlertStartSec + 0.5f)
		{
			AlertStartSec = 0.0f;
			MoveToLocation(PickFleeTarget(CurrentTarget));
			SetState(EQRWildlifeAIState::Fleeing);
			bMoveOutstanding = true;

			// Alert the herd if this pawn is part of one.
			if (WildlifePawn->HerdGroupId > 0)
			{
				WildlifePawn->AlertHerd(CurrentTarget);
			}
		}
		break;
	}

	case EQRWildlifeAIState::Fleeing:
	case EQRWildlifeAIState::Fleeing_Injured:
	{
		// Hit from OUTSIDE perception (sniped): no CurrentTarget. Flee
		// blind for a fixed window — the old code stood down on the very
		// next 0.25s think, so long-range shots barely moved the animal.
		if (!CurrentTarget)
		{
			if (NoTargetFleeUntilSec <= 0.0f)
			{
				NoTargetFleeUntilSec = Now + BlindFleeSeconds;
				MoveToLocation(PickWanderTarget());
				bMoveOutstanding = true;
			}
			if (Now < NoTargetFleeUntilSec)
			{
				if (GetMoveStatus() == EPathFollowingStatus::Idle)
				{
					MoveToLocation(PickWanderTarget());
					bMoveOutstanding = true;
				}
				break;
			}
			NoTargetFleeUntilSec = 0.0f;
			// fall through into the stand-down below
		}
		else
		{
			NoTargetFleeUntilSec = 0.0f;
		}

		// Reached safe distance? Stand down.
		if (!CurrentTarget ||
			FVector::DistSquared(CurrentTarget->GetActorLocation(),
			                     WildlifePawn->GetActorLocation()) >
			SafeFleeDistance * SafeFleeDistance)
		{
			// Re-anchor home wherever we ended up so wandering resumes
			// here rather than dragging us back into the threat zone.
			HomeLocation = WildlifePawn->GetActorLocation();
			CurrentTarget = nullptr;
			SetState(EQRWildlifeAIState::Wandering);
			MoveToLocation(PickWanderTarget());
			bMoveOutstanding = true;
			break;
		}
		// Still in danger -- repick flee target periodically.
		if (GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			MoveToLocation(PickFleeTarget(CurrentTarget));
			bMoveOutstanding = true;
		}
		break;
	}

	case EQRWildlifeAIState::Stalking:
	{
		if (!CurrentTarget)
		{
			SetState(EQRWildlifeAIState::Wandering);
			break;
		}
		const float DistSq = FVector::DistSquared(
			CurrentTarget->GetActorLocation(), WildlifePawn->GetActorLocation());

		if (DistSq <= AttackRange * AttackRange)
		{
			StopMovement();
			SetState(EQRWildlifeAIState::Attacking);
			break;
		}
		// Lost line of sight / too far -- give up after the perception
		// 4x buffer.
		if (DistSq > PerceptionRadius * PerceptionRadius * 16.0f)
		{
			CurrentTarget = nullptr;
			SetState(EQRWildlifeAIState::Wandering);
			break;
		}
		// If close enough to commit, switch to a faster charge.
		if (DistSq <= (PerceptionRadius * 0.5f) * (PerceptionRadius * 0.5f))
		{
			SetState(EQRWildlifeAIState::Charging);
			MoveToActor(CurrentTarget, /*acceptance*/ AttackRange * 0.5f);
			break;
		}
		if (GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			MoveToActor(CurrentTarget, /*acceptance*/ PerceptionRadius * 0.3f);
		}
		break;
	}

	case EQRWildlifeAIState::Charging:
	{
		if (!CurrentTarget)
		{
			SetState(EQRWildlifeAIState::Wandering);
			break;
		}
		const float DistSq = FVector::DistSquared(
			CurrentTarget->GetActorLocation(), WildlifePawn->GetActorLocation());
		if (DistSq <= AttackRange * AttackRange)
		{
			StopMovement();
			SetState(EQRWildlifeAIState::Attacking);
			break;
		}
		if (DistSq > PerceptionRadius * PerceptionRadius * 16.0f)
		{
			CurrentTarget = nullptr;
			SetState(EQRWildlifeAIState::Wandering);
			break;
		}
		if (GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			MoveToActor(CurrentTarget, /*acceptance*/ AttackRange * 0.5f);
		}
		break;
	}

	case EQRWildlifeAIState::Attacking:
	{
		if (!CurrentTarget)
		{
			SetState(EQRWildlifeAIState::Wandering);
			break;
		}
		const float DistSq = FVector::DistSquared(
			CurrentTarget->GetActorLocation(), WildlifePawn->GetActorLocation());
		if (DistSq > (AttackRange * 1.5f) * (AttackRange * 1.5f))
		{
			// Drifted out -- chase again.
			SetState(EQRWildlifeAIState::Charging);
			break;
		}
		TryAttack(CurrentTarget);
		break;
	}

	case EQRWildlifeAIState::Dead:
	{
		// Nothing to do; pawn's OnDied detaches the controller, but if
		// for some reason we're still possessing, just stop.
		StopMovement();
		break;
	}

	case EQRWildlifeAIState::Grazing:
	case EQRWildlifeAIState::Scavenging:
	{
		// Future: hold position, play feeding anim, increment a tag on
		// the food source. For now treat as a longer dwell.
		if (Now >= DwellUntilSec)
		{
			SetState(EQRWildlifeAIState::Idle);
			DwellUntilSec = Now;  // immediate transition into next wander
		}
		// Bail to flee if threat appears.
		if (CurrentTarget && !bAmbient && !IsPredatorRole())
		{
			SetState(EQRWildlifeAIState::Alert);
			DwellUntilSec = Now;
		}
		break;
	}

	default:
		break;
	}
}
