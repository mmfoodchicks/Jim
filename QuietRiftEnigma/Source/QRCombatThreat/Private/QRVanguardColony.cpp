#include "QRVanguardColony.h"
#include "QRSatelliteOutpost.h"
#include "QRFactionComponent.h"
#include "QRRaidScheduler.h"
#include "QRGameplayTags.h"
#include "Net/UnrealNetwork.h"

AQRVanguardColony::AQRVanguardColony()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	FactionComp = CreateDefaultSubobject<UQRFactionComponent>(TEXT("FactionComp"));
}

void AQRVanguardColony::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRVanguardColony, ActiveOutpostCount);
	DOREPLIFETIME(AQRVanguardColony, TotalOutpostCount);
	DOREPLIFETIME(AQRVanguardColony, HostilityScore);
	DOREPLIFETIME(AQRVanguardColony, HoursSinceLastDirectRaid);
}

void AQRVanguardColony::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority()) return;

	FactionComp->FactionTag        = QRGameplayTags::Faction_Vanguard;
	FactionComp->FactionDisplayName = ColonyName;
	FactionComp->InitRelation(QRGameplayTags::Faction_Player, EQRFactionStance::AtWar, -100.0f);
}

void AQRVanguardColony::RegisterOutpost(AQRSatelliteOutpost* Outpost)
{
	if (!Outpost || RegisteredOutposts.Contains(Outpost)) return;
	RegisteredOutposts.Add(Outpost);
	++TotalOutpostCount;
	++ActiveOutpostCount;
	OnRaidCapacityChanged.Broadcast();
}

void AQRVanguardColony::OnOutpostLost(AQRSatelliteOutpost* Outpost)
{
	ActiveOutpostCount = FMath::Max(0, ActiveOutpostCount - 1);
	// Losing an outpost angers Voss — he interprets it as a declaration of open war
	RaiseHostility(15.0f);
	OnRaidCapacityChanged.Broadcast();
}

void AQRVanguardColony::RaiseHostility(float Amount)
{
	HostilityScore = FMath::Clamp(HostilityScore + Amount, 0.0f, 100.0f);
}

int32 AQRVanguardColony::GetTotalRaidCapacity() const
{
	return ConcordatBaseRaidSlots + ActiveOutpostCount;
}

float AQRVanguardColony::GetOutpostIntegrityRatio() const
{
	if (TotalOutpostCount <= 0) return 1.0f;
	return (float)ActiveOutpostCount / (float)TotalOutpostCount;
}

void AQRVanguardColony::IssueDirectRaid(AQRRaidScheduler* Scheduler)
{
	if (!Scheduler) return;
	if (HoursSinceLastDirectRaid < DirectRaidCooldownHours) return;

	HoursSinceLastDirectRaid = 0.0f;
	// Direct raids are always Fanatic-tier — these are Voss's personal guard
	Scheduler->TriggerRaid(QRGameplayTags::Faction_Vanguard, EQRRaidExperienceTier::Fanatic);
}

void AQRVanguardColony::AdvanceTime(float GameHoursElapsed)
{
	HoursSinceLastDirectRaid += GameHoursElapsed;

	// High hostility causes the Concordat to issue unsolicited direct raids
	if (HostilityScore >= 75.0f && HoursSinceLastDirectRaid >= DirectRaidCooldownHours)
	{
		// Blueprint connects a RaidScheduler reference and calls IssueDirectRaid when ready
		// Hostility decays slowly so sustained pressure doesn't permanently lock raid cadence
		HostilityScore = FMath::Max(0.0f, HostilityScore - 2.0f * GameHoursElapsed);
	}
}
