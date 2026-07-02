#include "QRNPCBrainComponent.h"
#include "QRCivilianReactionComponent.h"
#include "QRRaidPartyAI.h"
#include "QRGameMode.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "EngineUtils.h"


UQRNPCBrainComponent::UQRNPCBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Per-frame movement, 2 Hz planner via accumulator.
	PrimaryComponentTick.TickInterval = 0.0f;
}


void UQRNPCBrainComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		if (bUseSpawnLocationAsHome && HomePosition.IsZero())
		{
			HomePosition = Owner->GetActorLocation();
		}
		if (AssignedWorkPost.IsZero()) AssignedWorkPost = HomePosition;
		if (AssignedBed.IsZero())      AssignedBed      = HomePosition;

		Reaction = Owner->FindComponentByClass<UQRCivilianReactionComponent>();

		// Raiders spawn as AQRNPCActor too -- if a raid FSM is attached
		// the brain yields all movement to it (but keeps driving anims).
		bYieldToRaidAI = Owner->FindComponentByClass<UQRRaidPartyAI>() != nullptr;

		// Force the owner's skeletal mesh into single-node anim mode so
		// PlayAnimation works without an AnimBP. If the mesh slot is
		// empty (designer didn't pick one) we leave it alone -- the
		// caller is responsible; otherwise we'd mask the bug.
		if (USkeletalMeshComponent* SMC = Owner->FindComponentByClass<USkeletalMeshComponent>())
		{
			CachedMesh = SMC;
			SMC->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			if (UAnimSequence* Idle = IdleAnim.LoadSynchronous())
			{
				SMC->PlayAnimation(Idle, /*bLooping*/ true);
			}
		}
		LastFrameLocation = Owner->GetActorLocation();
	}

	CurrentTarget = HomePosition;
}


void UQRNPCBrainComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	StateTimer += DeltaTime;
	if (FlashTimeRemaining > 0.0f)
	{
		FlashTimeRemaining -= DeltaTime;
		if (FlashTimeRemaining <= 0.0f)
		{
			// Overlay finished -- clear the token so the state loop
			// re-selects and resumes its looping sequence.
			LastPlayedAnim = nullptr;
		}
	}

	// Raid FSM owns the body entirely; the brain only paints anims.
	if (bYieldToRaidAI)
	{
		ApplyAnimForState(DeltaTime);
		return;
	}

	ThinkAccumulator += DeltaTime;
	if (ThinkAccumulator >= 0.5f)
	{
		ThinkAccumulator = 0.0f;
		Think();
	}

	// Reaction component owns the body during Flee/Fight/Hide -- skip
	// our movement so we don't fight it for actor transform. We still
	// drive the anim swap so a fleeing NPC plays Walk while the
	// reaction component moves the capsule.
	if (State != EQRNPCBrainState::Reacting)
	{
		StepTowardTarget(DeltaTime);
	}
	ApplyAnimForState(DeltaTime);
}


void UQRNPCBrainComponent::FlashAnim(UAnimSequence* Seq, float DurationSec)
{
	if (!Seq) return;
	USkeletalMeshComponent* SMC = CachedMesh.Get();
	if (!SMC) return;
	SMC->PlayAnimation(Seq, /*bLooping*/ false);
	LastPlayedAnim = Seq;
	FlashTimeRemaining = FMath::Max(0.1f, DurationSec);
}


void UQRNPCBrainComponent::FlashAttack()
{
	if (UAnimSequence* Seq = AttackAnim.LoadSynchronous())
	{
		FlashAnim(Seq, FMath::Max(0.5f, Seq->GetPlayLength()));
	}
}


void UQRNPCBrainComponent::PlayIfDifferent(UAnimSequence* Seq)
{
	if (!Seq) return;
	USkeletalMeshComponent* SMC = CachedMesh.Get();
	if (!SMC) return;
	if (LastPlayedAnim.Get() == Seq) return;   // already playing -- don't restart
	SMC->PlayAnimation(Seq, /*bLooping*/ true);
	LastPlayedAnim = Seq;
}


void UQRNPCBrainComponent::ApplyAnimForState(float DeltaSeconds)
{
	USkeletalMeshComponent* SMC = CachedMesh.Get();
	AActor* Owner = GetOwner();
	if (!SMC || !Owner || DeltaSeconds <= 0.0f) return;

	const FVector Now = Owner->GetActorLocation();
	const float Speed = (Now - LastFrameLocation).Size2D() / DeltaSeconds;
	LastFrameLocation = Now;

	// A one-shot overlay (FlashAnim) owns the mesh until its countdown
	// expires; swapping mid-swing would clip the attack pose.
	if (FlashTimeRemaining > 0.0f) return;

	// Resolve target anim from State + Speed. Each lookup falls back
	// to a sensible parent so a sparsely-authored NPC still animates.
	auto Pick = [this](TSoftObjectPtr<UAnimSequence>& Primary,
	                   TSoftObjectPtr<UAnimSequence>& Fallback) -> UAnimSequence*
	{
		if (UAnimSequence* S = Primary.LoadSynchronous()) return S;
		return Fallback.LoadSynchronous();
	};

	UAnimSequence* Target = nullptr;

	// Movement wins over state-loop -- a walking sleeper isn't a thing,
	// but the brain may decide the NPC needs to walk somewhere during
	// the Sleep window if they got displaced. Velocity is the truth.
	if (Speed >= RunSpeedThreshold)
	{
		Target = Pick(RunAnim, WalkAnim);
		if (!Target) Target = IdleAnim.LoadSynchronous();
	}
	else if (Speed >= WalkSpeedThreshold)
	{
		Target = WalkAnim.LoadSynchronous();
		if (!Target) Target = IdleAnim.LoadSynchronous();
	}
	else
	{
		// Stationary: state-loop drives the pose.
		switch (State)
		{
		case EQRNPCBrainState::Sleep:     Target = Pick(SleepAnim, IdleAnim); break;
		case EQRNPCBrainState::Work:      Target = Pick(WorkAnim,  IdleAnim); break;
		case EQRNPCBrainState::Socialize: Target = Pick(TalkAnim,  IdleAnim); break;
		default:                          Target = IdleAnim.LoadSynchronous(); break;
		}
	}

	bAnimIsWalking = Speed >= WalkSpeedThreshold;
	PlayIfDifferent(Target);
}


void UQRNPCBrainComponent::Think()
{
	if (!GetOwner()) return;

	// Raid FSMs attach AFTER spawn (HandleRaidLaunched registers the
	// component post-SpawnActor, i.e. post-BeginPlay), so the BeginPlay
	// probe misses them. Re-probe at think rate until one shows up;
	// raiders never lose the component, so this is a one-way latch.
	if (!bYieldToRaidAI)
	{
		bYieldToRaidAI = GetOwner()->FindComponentByClass<UQRRaidPartyAI>() != nullptr;
		if (bYieldToRaidAI) return;
	}

	// Yield to the civilian-reaction FSM whenever it's active.
	if (UQRCivilianReactionComponent* R = Reaction.Get())
	{
		if (R->Mode != EQRCivilianMode::Normal &&
			R->Mode != EQRCivilianMode::AlertCalm)
		{
			if (State != EQRNPCBrainState::Reacting)
			{
				State = EQRNPCBrainState::Reacting;
				StateTimer = 0.0f;
			}
			return;
		}
	}

	// Out of reaction? Default state stays Idle/Wander; Schedule
	// promotes to Work / Sleep when the hour matches.
	const bool bNight = IsNightHour();
	const bool bWork  = IsWorkHour();

	if (bNight)
	{
		State = EQRNPCBrainState::Sleep;
		CurrentTarget = AssignedBed;
		return;
	}
	if (bWork)
	{
		State = EQRNPCBrainState::Work;
		CurrentTarget = AssignedWorkPost;
		return;
	}

	// Day, no schedule slot: idle / wander / socialize loop. Switch
	// states only when arrived (so the NPC actually finishes a leg
	// before re-rolling).
	const float Dist = (CurrentTarget - GetOwner()->GetActorLocation()).Size2D();
	if (Dist <= ArriveRadius || StateTimer > 8.0f)
	{
		const float Roll = FMath::FRand();
		if (Roll < SocializeChance)
		{
			if (AActor* Partner = FindSocialPartner())
			{
				State = EQRNPCBrainState::Socialize;
				CurrentTarget = Partner->GetActorLocation();
				StateTimer = 0.0f;
				return;
			}
		}
		State = EQRNPCBrainState::Wander;
		CurrentTarget = PickWanderPoint();
		StateTimer = 0.0f;
	}
}


bool UQRNPCBrainComponent::StepTowardTarget(float DeltaSeconds)
{
	AActor* Owner = GetOwner();
	if (!Owner) return false;

	const FVector Loc = Owner->GetActorLocation();
	FVector To = CurrentTarget - Loc;
	To.Z = 0.0f;
	const float Dist = To.Size();
	if (Dist <= ArriveRadius) return true;

	const FVector Dir = To.GetSafeNormal();
	FVector NewLoc = Loc + Dir * WalkSpeed * DeltaSeconds;
	NewLoc.Z = Loc.Z;   // 2D step — wildlife slope conformance handles Z elsewhere
	Owner->SetActorLocation(NewLoc, /*bSweep*/ true);

	const FRotator Look = Dir.Rotation();
	Owner->SetActorRotation(FRotator(0.0f, Look.Yaw, 0.0f));
	return false;
}


bool UQRNPCBrainComponent::IsWorkHour() const
{
	if (UWorld* W = GetWorld())
	{
		if (AQRGameMode* GM = W->GetAuthGameMode<AQRGameMode>())
		{
			const float P = GM->GetDayProgress();
			return P >= WorkStartFrac && P < WorkEndFrac;
		}
	}
	return false;
}


bool UQRNPCBrainComponent::IsNightHour() const
{
	if (UWorld* W = GetWorld())
	{
		if (AQRGameMode* GM = W->GetAuthGameMode<AQRGameMode>())
		{
			return GM->IsCurrentlyNight();
		}
	}
	return false;
}


FVector UQRNPCBrainComponent::PickWanderPoint() const
{
	const float Ang  = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Dist = FMath::FRandRange(WanderRadius * 0.2f, WanderRadius);
	return HomePosition + FVector(FMath::Cos(Ang) * Dist, FMath::Sin(Ang) * Dist, 0.0f);
}


AActor* UQRNPCBrainComponent::FindSocialPartner() const
{
	UWorld* W = GetWorld();
	AActor* Owner = GetOwner();
	if (!W || !Owner) return nullptr;

	AActor* Best = nullptr;
	float BestDistSq = SocialRangeCm * SocialRangeCm;
	const FVector MyLoc = Owner->GetActorLocation();

	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A || A == Owner) continue;
		if (!A->FindComponentByClass<UQRNPCBrainComponent>()) continue;
		const float D2 = FVector::DistSquared(MyLoc, A->GetActorLocation());
		if (D2 < BestDistSq)
		{
			BestDistSq = D2;
			Best = A;
		}
	}
	return Best;
}
