#include "QRCivilianReactionComponent.h"
#include "QRFactionCamp.h"
#include "QRCampSimComponent.h"
#include "QRSurvivalComponent.h"
#include "QRInventoryComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "QRLeaderComponent.h"
#include "QRRaidPartyAI.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"


UQRCivilianReactionComponent::UQRCivilianReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;  // 4 Hz is plenty
}


void UQRCivilianReactionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Subscribe to every existing AQRFactionCamp's Sim — they're spawned
	// by worldgen before player NPCs typically, so this catches the
	// strategy layer cleanly.
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<AQRFactionCamp> It(W); It; ++It)
		{
			if (AQRFactionCamp* Camp = *It)
			{
				if (Camp->Sim)
				{
					Camp->Sim->OnRaidLaunched.AddDynamic(this, &UQRCivilianReactionComponent::HandleRaidLaunched);
				}
			}
		}
	}
}


void UQRCivilianReactionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<AQRFactionCamp> It(W); It; ++It)
		{
			if (AQRFactionCamp* Camp = *It)
			{
				if (Camp->Sim)
				{
					Camp->Sim->OnRaidLaunched.RemoveDynamic(this, &UQRCivilianReactionComponent::HandleRaidLaunched);
				}
			}
		}
	}
	Super::EndPlay(Reason);
}


void UQRCivilianReactionComponent::HandleRaidLaunched(FQRRaidPlan Plan)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;
	// Only react if the raid target is close enough to us.
	const float DistSq = FVector::DistSquared(Plan.TargetLocation, Owner->GetActorLocation());
	if (DistSq > TriggerRadiusCm * TriggerRadiusCm) return;

	// Decide mode based on personal state.
	const float Morale = GetMorale();
	const float HP     = GetHealthFraction();
	EQRCivilianMode Choice = EQRCivilianMode::AlertCalm;

	if (Morale < FleeMoraleThreshold || HP < FleeHealthThreshold)
	{
		Choice = EQRCivilianMode::Flee;
	}
	else if (IsArmed())
	{
		Choice = EQRCivilianMode::Fight;
	}
	else
	{
		// Unarmed but composed — hide.
		Choice = EQRCivilianMode::Hide;
	}
	EnterMode(Choice, Plan.OriginLocation);
}


void UQRCivilianReactionComponent::EnterMode(EQRCivilianMode NewMode, FVector ThreatLocation)
{
	Mode = NewMode;
	LastThreatLocation = ThreatLocation;
	StateTimer = 0.0f;
}


void UQRCivilianReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	StateTimer += DeltaTime;

	AActor* Owner = GetOwner();
	if (!Owner) return;

	switch (Mode)
	{
	case EQRCivilianMode::Flee:
	{
		// Run directly away from the threat. After a while, exit.
		FVector Away = Owner->GetActorLocation() - LastThreatLocation;
		Away.Z = 0.0f;
		if (!Away.IsNearlyZero())
		{
			const FVector Dir = Away.GetSafeNormal();
			Owner->SetActorLocation(Owner->GetActorLocation() + Dir * FleeSpeed * DeltaTime);
			Owner->SetActorRotation(FRotator(0, Dir.Rotation().Yaw, 0));
		}
		if (StateTimer > 30.0f) EnterMode(EQRCivilianMode::Normal, LastThreatLocation);
		break;
	}
	case EQRCivilianMode::Hide:
	{
		// v1 hide behavior — back away from threat at half flee speed.
		// Real hide-in-cover finds nearby AbandonedStructure cells; that's
		// a follow-up.
		FVector Away = Owner->GetActorLocation() - LastThreatLocation;
		Away.Z = 0.0f;
		if (!Away.IsNearlyZero())
		{
			const FVector Dir = Away.GetSafeNormal();
			Owner->SetActorLocation(Owner->GetActorLocation() + Dir * (FleeSpeed * 0.4f) * DeltaTime);
		}
		if (StateTimer > 60.0f) EnterMode(EQRCivilianMode::Normal, LastThreatLocation);
		break;
	}
	case EQRCivilianMode::Fight:
	{
		// Face the live threat and fire on an interval. Militia-grade
		// behavior, not soldier AI: stand ground, slow aimed shots, lots
		// of misses under stress.
		FireCooldown -= DeltaTime;

		AActor* Target = AcquireFightTarget();
		const FVector FaceAt = Target ? Target->GetActorLocation() : LastThreatLocation;
		FVector To = (FaceAt - Owner->GetActorLocation()).GetSafeNormal2D();
		if (!To.IsNearlyZero())
		{
			Owner->SetActorRotation(FRotator(0, To.Rotation().Yaw, 0));
		}

		if (Target && FireCooldown <= 0.0f)
		{
			FireAtTarget(Target);
			FireCooldown = FMath::Max(0.3f, FireIntervalSeconds);
		}

		// Stand down when no raider has been visible for a while.
		if (Target)      StateTimer = 0.0f;
		if (StateTimer > 20.0f) EnterMode(EQRCivilianMode::Normal, LastThreatLocation);
		break;
	}
	default: break;
	}
}


AActor* UQRCivilianReactionComponent::AcquireFightTarget()
{
	AActor* Owner = GetOwner();
	UWorld* W = GetWorld();
	if (!Owner || !W) return nullptr;

	const FVector MyLoc = Owner->GetActorLocation();
	AActor* Best = nullptr;
	float BestDistSq = EngageRangeCm * EngageRangeCm;

	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A || A == Owner) continue;
		UQRRaidPartyAI* Raid = A->FindComponentByClass<UQRRaidPartyAI>();
		if (!Raid || Raid->State == EQRRaidPartyState::Dead) continue;
		const float DistSq = FVector::DistSquared(MyLoc, A->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = A;
		}
	}
	if (Best)
	{
		LastThreatLocation = Best->GetActorLocation();
	}
	return Best;
}


void UQRCivilianReactionComponent::FireAtTarget(AActor* Target)
{
	AActor* Owner = GetOwner();
	UWorld* W = GetWorld();
	if (!Owner || !W || !Target) return;

	// Stress miss roll first — most civilian shots go wide.
	if (FMath::FRand() > FightHitChance) return;

	// Line-of-sight check from chest height; a wall between us blocks it.
	const FVector From = Owner->GetActorLocation() + FVector(0, 0, 60.0f);
	const FVector To   = Target->GetActorLocation() + FVector(0, 0, 40.0f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(QRCivilianFire), false, Owner);
	const bool bHit = W->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params);
	if (bHit && Hit.GetActor() != Target) return;   // something in the way

	// A real weapon in the hand slot hits harder than improvised swings.
	float Damage = FightDamage;
	if (UQRInventoryComponent* Inv = Owner->FindComponentByClass<UQRInventoryComponent>())
	{
		if (Inv->HandSlot && Inv->HandSlot->Definition &&
			Inv->HandSlot->Definition->Category == EQRItemCategory::Weapon)
		{
			Damage *= 1.5f;
		}
	}

	UGameplayStatics::ApplyDamage(Target, Damage,
		Owner->GetInstigatorController(), Owner, nullptr);
}


bool UQRCivilianReactionComponent::IsArmed() const
{
	AActor* Owner = GetOwner();
	if (!Owner) return false;
	UQRInventoryComponent* Inv = Owner->FindComponentByClass<UQRInventoryComponent>();
	if (!Inv) return false;
	// Hand-slot weapon counts as armed.
	if (UQRItemInstance* H = Inv->HandSlot)
	{
		if (H->Definition && H->Definition->Category == EQRItemCategory::Weapon)
		{
			return true;
		}
	}
	// Or any weapon in the spatial grid.
	for (UQRItemInstance* I : Inv->Items)
	{
		if (I && I->Definition && I->Definition->Category == EQRItemCategory::Weapon)
		{
			return true;
		}
	}
	return false;
}


float UQRCivilianReactionComponent::GetMorale() const
{
	AActor* Owner = GetOwner();
	if (!Owner) return 50.0f;
	if (UQRLeaderComponent* L = Owner->FindComponentByClass<UQRLeaderComponent>())
	{
		return L->MoraleIndex;
	}
	return 50.0f;
}


float UQRCivilianReactionComponent::GetHealthFraction() const
{
	AActor* Owner = GetOwner();
	if (!Owner) return 1.0f;
	if (UQRSurvivalComponent* Surv = Owner->FindComponentByClass<UQRSurvivalComponent>())
	{
		if (Surv->MaxHealth > 0.0f) return Surv->Health / Surv->MaxHealth;
	}
	return 1.0f;
}
