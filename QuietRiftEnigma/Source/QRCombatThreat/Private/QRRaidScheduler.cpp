#include "QRRaidScheduler.h"
#include "QRMath.h"
#include "GameFramework/GameModeBase.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

AQRRaidScheduler::AQRRaidScheduler()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.0f;
	bReplicates = true;
}

void AQRRaidScheduler::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRRaidScheduler, CurrentOpportunityScore);
	DOREPLIFETIME(AQRRaidScheduler, HoursSinceLastRaid);
	DOREPLIFETIME(AQRRaidScheduler, bRaidInProgress);
	DOREPLIFETIME(AQRRaidScheduler, ActiveWave);
}

void AQRRaidScheduler::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!HasAuthority()) return;

	// One-shot latch fix: nothing in the codebase ever called EndRaid,
	// so the first TriggerRaid set bRaidInProgress forever and the
	// scheduler went silent for the rest of the session. Time the wave
	// out — attackers are dead, retreated, or despawned well within it.
	if (bRaidInProgress)
	{
		RaidElapsedSeconds += DeltaTime;
		if (RaidElapsedSeconds >= RaidAutoEndSeconds)
		{
			EndRaid(/*bDefendersWon*/ true);
		}
		return;
	}

	// Advance cooldown clock in GAME hours. Match the GameMode's day
	// length via reflection (this module can't include the game module);
	// the old hardcoded 1/60 assumed a 24-minute day vs the actual 20.
	float GameHoursPerSec = 24.0f / 1200.0f;
	if (AGameModeBase* GM = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr)
	{
		if (const FFloatProperty* P = FindFProperty<FFloatProperty>(
			GM->GetClass(), TEXT("DayLengthRealSeconds")))
		{
			const float DayLen = P->GetPropertyValue_InContainer(GM);
			if (DayLen > 1.0f) GameHoursPerSec = 24.0f / DayLen;
		}
	}
	HoursSinceLastRaid += DeltaTime * GameHoursPerSec;

	// Periodic raid viability check
	RaidCheckAccumulator += DeltaTime;
	if (RaidCheckAccumulator >= RaidCheckInterval)
	{
		RaidCheckAccumulator = 0.0f;

		if (IsRaidReady() && CurrentOpportunityScore >= RaidTriggerThreshold)
		{
			// Pick faction and tier
			FGameplayTag FactionTag; // Blueprint sets actual faction
			EQRRaidExperienceTier Tier = DetermineRaidTier();
			TriggerRaid(FactionTag, Tier);
		}
	}
}

bool AQRRaidScheduler::IsRaidReady() const
{
	return HoursSinceLastRaid >= MinRaidCooldownHours && !bRaidInProgress;
}

void AQRRaidScheduler::UpdateThreatScore(float WallHealth, float LightLevel, float LivestockValue,
	float NoiseFactor, bool bIsNight)
{
	float Score = UQRMath::ComputeRaidOpportunityScore(
		BaseRisk,
		WallHealth,
		LightLevel,
		LivestockValue,
		NoiseFactor,
		bIsNight ? NightRiskMultiplier : 1.0f
	);

	if (!FMath::IsNearlyEqual(Score, CurrentOpportunityScore, 1.0f))
	{
		CurrentOpportunityScore = Score;
		OnThreatChanged.Broadcast(Score, bIsNight);
	}
}

void AQRRaidScheduler::TriggerRaid(FGameplayTag FactionTag, EQRRaidExperienceTier Tier)
{
	if (bRaidInProgress) return;

	FQRRaidWave Wave;
	Wave.WaveNumber      = 1;
	Wave.FactionTag      = FactionTag;
	Wave.ExperienceTier  = Tier;

	switch (Tier)
	{
	case EQRRaidExperienceTier::Inexperienced:
		Wave.AttackerCount      = FMath::RandRange(3, 6);
		Wave.ApproachDelaySeconds = 60.0f;
		Wave.bCallsForRetreat   = true;
		break;
	case EQRRaidExperienceTier::Competent:
		Wave.AttackerCount      = FMath::RandRange(6, 10);
		Wave.ApproachDelaySeconds = 45.0f;
		Wave.bUsesFlankRoutes   = true;
		break;
	case EQRRaidExperienceTier::Veteran:
		Wave.AttackerCount      = FMath::RandRange(10, 16);
		Wave.bUsesFlankRoutes   = true;
		Wave.bTargetsLights     = true;
		Wave.bTargetsLivestock  = true;
		Wave.ApproachDelaySeconds = 20.0f;
		break;
	case EQRRaidExperienceTier::Fanatic:
		Wave.AttackerCount      = FMath::RandRange(14, 20);
		Wave.bUsesFlankRoutes   = true;
		Wave.bTargetsLights     = true;
		Wave.bTargetsLivestock  = true;
		Wave.bCallsForRetreat   = false;
		Wave.ApproachDelaySeconds = 10.0f;
		break;
	}

	bRaidInProgress = true;
	RaidElapsedSeconds = 0.0f;
	ActiveWave      = Wave;
	HoursSinceLastRaid = 0.0f;

	SpawnRaidWave(Wave);
	OnRaidStarted.Broadcast(Wave);
}

void AQRRaidScheduler::EndRaid(bool bDefendersWon)
{
	bRaidInProgress = false;
	OnRaidEnded.Broadcast(bDefendersWon);
}

EQRRaidExperienceTier AQRRaidScheduler::DetermineRaidTier() const
{
	// Escalate based on how many raids have occurred (tracked externally via HoursSinceLastRaid)
	float Roll = FMath::FRand();
	if (Roll < 0.4f) return EQRRaidExperienceTier::Inexperienced;
	if (Roll < 0.7f) return EQRRaidExperienceTier::Competent;
	if (Roll < 0.9f) return EQRRaidExperienceTier::Veteran;
	return EQRRaidExperienceTier::Fanatic;
}
