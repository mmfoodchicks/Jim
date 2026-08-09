#include "QRCampSimComponent.h"
#include "QRLeaderComponent.h"
#include "QRGameMode.h"
#include "QRWeatherComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"


UQRCampSimComponent::UQRCampSimComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


void UQRCampSimComponent::AdvanceGameHours(float DeltaGameHours)
{
	if (DeltaGameHours <= 0.0f) return;

	const float DeltaDays = DeltaGameHours / 24.0f;

	// Resource income — scaled by current population (workers).
	const float PopFactor = FMath::Clamp(State.Population / 10.0f, 0.5f, 3.0f);
	State.Resources += ResourceIncomePerDay * DeltaDays * PopFactor;

	// Population growth — scaled by resources available.
	const float StarvedScalar = (State.Resources > 0.0f) ? 1.0f : 0.0f;
	const float PopDelta = BasePopGrowthPerDay * DeltaDays * StarvedScalar;
	if (State.Population < PopulationCap)
	{
		// Accumulate fractional pop until it ticks an integer up.
		// (Member accumulator — the old function-static map keyed by raw
		// `this` leaked entries and could collide across PIE worlds.)
		PopulationFraction += PopDelta;
		while (PopulationFraction >= 1.0f && State.Population < PopulationCap)
		{
			State.Population += 1;
			PopulationFraction -= 1.0f;
		}
	}

	// Hostility creep — camps heat up over time until something (a
	// defeat, leader death) cools them off. Without this nothing ever
	// RAISED hostility, so the 0.55 raid gate was unreachable from the
	// 0.4 starting value and no camp could ever launch its first raid.
	State.Hostility = FMath::Min(1.0f,
		State.Hostility + HostilityGrowthPerDay * DeltaDays);

	// Military recruitment — convert resources into soldiers when we
	// have spare population to train.
	const int32 SpareCivilians = State.Population - State.MilitaryStrength;
	if (State.Resources >= ResourcesPerSoldier && SpareCivilians > 0)
	{
		const int32 NewSoldiers = FMath::Min(
			static_cast<int32>(State.Resources / ResourcesPerSoldier),
			SpareCivilians);
		State.MilitaryStrength += NewSoldiers;
		State.Resources        -= NewSoldiers * ResourcesPerSoldier;
	}

	State.HoursSinceLastRaid    += DeltaGameHours;
	State.HoursSinceLastDefeat  += DeltaGameHours;

	TryDecideRaid();
}


void UQRCampSimComponent::ModifyHostility(float Delta)
{
	State.Hostility = FMath::Clamp(State.Hostility + Delta, 0.0f, 1.0f);
}


void UQRCampSimComponent::ReportRaidDefeated(int32 PartySize)
{
	// NOTE: the whole party was already deducted from MilitaryStrength at
	// launch (TryDecideRaid), and this is called once PER DEAD RAIDER by
	// the raid FSM / NPC death flow — the old version subtracted the
	// party AGAIN on every call, double-charging the garrison.
	State.HoursSinceLastRaid = -DefeatExtraCooldownHours;  // negative pushes cooldown out
	State.HoursSinceLastDefeat = 0.0f;
	// Losses discourage further hostility — scaled per raider lost so
	// per-death reporting doesn't crater hostility (-0.10 apiece did).
	State.Hostility = FMath::Max(0.0f,
		State.Hostility - 0.02f * FMath::Max(1, PartySize));
}


void UQRCampSimComponent::ReportRaidSuccessful(int32 SurvivingMilitary, float LootedResources)
{
	// Survivors REJOIN the garrison. This is also called once per
	// returning raider — the old assignment replaced the entire home
	// garrison with the survivor count (one survivor home = army of 1).
	State.MilitaryStrength += FMath::Max(0, SurvivingMilitary);
	State.Resources       += LootedResources;
	State.HoursSinceLastRaid = 0.0f;
	State.SuccessfulRaids++;
	// Success emboldens the camp.
	State.Hostility = FMath::Min(1.0f, State.Hostility + 0.05f);
}


EQRRaidExperienceTier UQRCampSimComponent::DetermineRaidTier() const
{
	if (bForceFanaticTier) return EQRRaidExperienceTier::Fanatic;

	// Veteran teams come either from sustained success OR from a
	// well-led camp that's currently picking its moment (high
	// leadership + favorable conditions = veteran cadre even on the
	// first raid).
	const float Leadership = GetEffectiveLeadership();
	if (State.SuccessfulRaids >= 6) return EQRRaidExperienceTier::Veteran;
	if (Leadership >= 8.0f && AreConditionsFavorable())
	{
		return EQRRaidExperienceTier::Veteran;
	}
	if (State.SuccessfulRaids >= 2) return EQRRaidExperienceTier::Competent;
	return EQRRaidExperienceTier::Inexperienced;
}


// ─── Raid decision ──────────────────────────────────────────────────

void UQRCampSimComponent::TryDecideRaid()
{
	// Cooldown — never launches inside a cooldown window.
	if (State.HoursSinceLastRaid < RaidCooldownHours) return;

	const float Leadership = GetEffectiveLeadership();

	// Leadership modifies thresholds. High-skill leaders prepare more
	// thoroughly; low-skill rush an under-staffed force.
	const float Scalar = FMath::Lerp(
		1.0f - 0.5f * LeadershipThresholdInfluence,   // L=0 → 0.5×
		1.0f + 0.5f * LeadershipThresholdInfluence,   // L=10 → 1.5×
		Leadership / 10.0f);

	const int32 EffectiveMilThreshold      = FMath::CeilToInt(RaidMilitaryThreshold * Scalar);
	const float EffectiveHostilityThreshold = RaidHostilityThreshold * Scalar;

	if (State.MilitaryStrength < EffectiveMilThreshold) return;
	if (State.Hostility        < EffectiveHostilityThreshold) return;

	// Competent leaders gate on favorable conditions; rash ones don't.
	if (Leadership >= ConditionGatedSkillFloor && !AreConditionsFavorable())
	{
		// Hold this tick — wait for night / weather / vulnerability.
		return;
	}

	// All gates passed — commit a fraction of military to a raid.
	const int32 PartySize = FMath::Max(1,
		FMath::FloorToInt(State.MilitaryStrength * MilitaryRaidCommitment));
	State.MilitaryStrength -= PartySize;
	State.HoursSinceLastRaid = 0.0f;

	FQRRaidPlan Plan;
	Plan.SourceCampId      = CampId;
	Plan.OriginLocation    = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	Plan.TargetLocation    = FindRaidTargetLocation();
	Plan.PartySize         = PartySize;
	Plan.HostilityAtLaunch = State.Hostility;
	Plan.Experience        = DetermineRaidTier();
	OnRaidLaunched.Broadcast(Plan);

	UE_LOG(LogTemp, Log,
		TEXT("[QRCamp %s] launching raid — party=%d hostility=%.2f leadership=%.1f"),
		*CampId.ToString(), PartySize, State.Hostility, Leadership);
}


FVector UQRCampSimComponent::FindRaidTargetLocation() const
{
	UWorld* W = GetWorld();
	if (!W) return FVector::ZeroVector;
	AActor* Owner = GetOwner();
	const FVector OwnerLoc = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;

	// Score candidates by:
	//   • Relative weakness  (target weaker than us = preferred)
	//   • Distance           (closer = preferred)
	//   • Hostility relative (high-hostility target = preferred)
	// Player gets a base hostility-bonus equal to this camp's Hostility.
	// Other camps contribute their own hostility differential.
	FVector BestLoc = OwnerLoc;
	float BestScore = -1.0f;

	auto Score = [&](const FVector& Loc, float TargetMilitary, float HostilityToTarget) -> float
	{
		const float DistKm = FVector::Distance(OwnerLoc, Loc) / 100000.0f;
		// Prefer closer + weaker + hostile targets.
		const float DistScore     = FMath::Clamp(1.0f - (DistKm / 25.0f), 0.0f, 1.0f);
		const float WeaknessScore = FMath::Clamp(1.0f - (TargetMilitary / FMath::Max(1.0f, (float)State.MilitaryStrength)), 0.0f, 1.5f);
		const float HostScore     = FMath::Clamp(HostilityToTarget, 0.0f, 1.0f);
		return DistScore * 0.35f + WeaknessScore * 0.30f + HostScore * 0.35f;
	};

	// 1. Player pawn — treated as a single "target" with unknown
	//    military but high importance. Use a soft military stand-in of 5.
	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		if (APawn* P = PC->GetPawn())
		{
			const float S = Score(P->GetActorLocation(), 5.0f, State.Hostility);
			if (S > BestScore) { BestScore = S; BestLoc = P->GetActorLocation(); }
		}
	}

	// 2. Other camps. We compare faction tags via UQRFactionComponent
	//    if both sides carry one. For v1 we use a simple "different
	//    actor + different camp id" heuristic — any other AQRFactionCamp
	//    is a potential rival.
	{
		// Walk actors via the world's actor list.
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			AActor* A = *It;
			if (!A || A == Owner) continue;
			UQRCampSimComponent* Other = A->FindComponentByClass<UQRCampSimComponent>();
			if (!Other) continue;
			if (Other == this) continue;
			// Don't attack camps with same-faction id (treat as allies).
			if (Other->CampId == CampId) continue;
			// Camps only consider each other when both above mid hostility.
			if (State.Hostility < 0.55f) continue;

			const float S = Score(A->GetActorLocation(),
				static_cast<float>(Other->State.MilitaryStrength),
				State.Hostility);
			if (S > BestScore) { BestScore = S; BestLoc = A->GetActorLocation(); }
		}
	}

	return BestLoc;
}


float UQRCampSimComponent::GetEffectiveLeadership() const
{
	if (AActor* Owner = GetOwner())
	{
		if (UQRLeaderComponent* L = Owner->FindComponentByClass<UQRLeaderComponent>())
		{
			// Worldgen stamps per-camp variation onto FallbackLeadership
			// (it can't reach the leader component pre-BeginPlay). The camp
			// always carries a leader component, so the fallback was dead:
			// prefer the leader's aptitude once anything actually changed
			// it; an untouched 5.0 default defers to a stamped fallback.
			const bool bLeaderTouched   = !FMath::IsNearlyEqual(L->LeadershipAptitude, 5.0f);
			const bool bFallbackStamped = !FMath::IsNearlyEqual(FallbackLeadership, 5.0f);
			if (bLeaderTouched || !bFallbackStamped)
			{
				return FMath::Clamp(L->LeadershipAptitude, 0.0f, 10.0f);
			}
		}
	}
	return FMath::Clamp(FallbackLeadership, 0.0f, 10.0f);
}


bool UQRCampSimComponent::AreConditionsFavorable() const
{
	UWorld* W = GetWorld();
	if (!W) return false;

	AQRGameMode* GM = W->GetAuthGameMode<AQRGameMode>();
	if (!GM) return false;

	int32 Score = 0;

	// Night ≈ +1
	if (GM->bIsNight) ++Score;

	// Active weather event ≈ +1. Direct call — the old reflective lookup
	// searched for a property named "bEventActive" that has never existed
	// on UQRWeatherComponent, so weather NEVER counted toward raid
	// conditions. Both classes live in this module; no reflection needed.
	if (UQRWeatherComponent* Weather = GM->Weather)
	{
		if (Weather->HasActiveEvent()) ++Score;
	}

	// Player far from this camp (out exploring, undefended base)
	// ≈ +1. "Far" = more than 8 km away.
	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		APawn* P = PC->GetPawn();
		if (P && GetOwner())
		{
			const float DistKm = FVector::Distance(P->GetActorLocation(),
				GetOwner()->GetActorLocation()) / 100000.0f;
			if (DistKm > 8.0f) ++Score;
		}
	}

	// 2+ favorable conditions = launch.
	return Score >= 2;
}
