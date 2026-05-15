#include "QRCampSimComponent.h"
#include "QRLeaderComponent.h"
#include "QRGameMode.h"
#include "QRWeatherComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"


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
		static thread_local TMap<UQRCampSimComponent*, float> PopAccum;
		float& Acc = PopAccum.FindOrAdd(this);
		Acc += PopDelta;
		while (Acc >= 1.0f && State.Population < PopulationCap)
		{
			State.Population += 1;
			Acc -= 1.0f;
		}
	}

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
	// Camp loses the committed force entirely and absorbs a long cooldown.
	State.MilitaryStrength = FMath::Max(0, State.MilitaryStrength - PartySize);
	State.HoursSinceLastRaid = -DefeatExtraCooldownHours;  // negative pushes cooldown out
	State.HoursSinceLastDefeat = 0.0f;
	// Defeats discourage further hostility short-term.
	State.Hostility = FMath::Max(0.0f, State.Hostility - 0.10f);
}


void UQRCampSimComponent::ReportRaidSuccessful(int32 SurvivingMilitary, float LootedResources)
{
	State.MilitaryStrength = FMath::Max(0, SurvivingMilitary);
	State.Resources       += LootedResources;
	State.HoursSinceLastRaid = 0.0f;
	// Success emboldens the camp.
	State.Hostility = FMath::Min(1.0f, State.Hostility + 0.05f);
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
	OnRaidLaunched.Broadcast(Plan);

	UE_LOG(LogTemp, Log,
		TEXT("[QRCamp %s] launching raid — party=%d hostility=%.2f leadership=%.1f"),
		*CampId.ToString(), PartySize, State.Hostility, Leadership);
}


FVector UQRCampSimComponent::FindRaidTargetLocation() const
{
	// v1 targets the local player. Smarter target selection (player
	// outposts / undefended POIs / supply lines) is a follow-up once
	// the player-camp actor exists.
	if (UWorld* W = GetWorld())
	{
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			if (APawn* P = PC->GetPawn()) return P->GetActorLocation();
		}
	}
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}


float UQRCampSimComponent::GetEffectiveLeadership() const
{
	if (AActor* Owner = GetOwner())
	{
		if (UQRLeaderComponent* L = Owner->FindComponentByClass<UQRLeaderComponent>())
		{
			return FMath::Clamp(L->LeadershipAptitude, 0.0f, 10.0f);
		}
	}
	return FallbackLeadership;
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

	// Active weather event ≈ +1 (reflective lookup so we don't bind to
	// the enum value here).
	if (UQRWeatherComponent* Weather = GM->Weather)
	{
		const FBoolProperty* P = FindFProperty<FBoolProperty>(Weather->GetClass(), TEXT("bEventActive"));
		if (P && P->GetPropertyValue_InContainer(Weather)) ++Score;
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
