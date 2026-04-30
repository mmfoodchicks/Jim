#include "QRSatelliteOutpost.h"
#include "QRVanguardColony.h"
#include "QRFactionComponent.h"
#include "QRRaidScheduler.h"
#include "QRGameplayTags.h"
#include "Net/UnrealNetwork.h"

AQRSatelliteOutpost::AQRSatelliteOutpost()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	FactionComp = CreateDefaultSubobject<UQRFactionComponent>(TEXT("FactionComp"));
}

void AQRSatelliteOutpost::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRSatelliteOutpost, bIsDestroyed);
	DOREPLIFETIME(AQRSatelliteOutpost, ControlState);
}

void AQRSatelliteOutpost::BeginPlay()
{
	Super::BeginPlay();

	if (FactionComp)
	{
		FactionComp->FactionTag = QRGameplayTags::Faction_Vanguard_Satellite;
		FactionComp->InitRelation(QRGameplayTags::Faction_Player, EQRFactionStance::AtWar, -100.0f);
	}

	// Register with parent colony and compute distance-based tier
	if (HasAuthority() && ParentColony)
	{
		DistanceToConcordatM = FVector::Dist(GetActorLocation(), ParentColony->GetActorLocation()) / 100.0f;
		ParentColony->RegisterOutpost(this);

		if (!bOverrideTierManually)
		{
			HardpointTier = ComputeTierFromDistance(DistanceToConcordatM);
		}
	}
}

EQRVanguardHardpointTier AQRSatelliteOutpost::ComputeTierFromDistance(float DistanceM) const
{
	if (DistanceM > ListeningPostMaxDistM) return EQRVanguardHardpointTier::ListeningPost;
	if (DistanceM > ForwardPostMaxDistM)   return EQRVanguardHardpointTier::ForwardPost;
	if (DistanceM > HardpointMaxDistM)     return EQRVanguardHardpointTier::Hardpoint;
	if (DistanceM > InnerSanctumMaxDistM)  return EQRVanguardHardpointTier::InnerSanctum;
	return EQRVanguardHardpointTier::Concordat;
}

EQRRaidExperienceTier AQRSatelliteOutpost::GetRaidTier() const
{
	switch (HardpointTier)
	{
	case EQRVanguardHardpointTier::ListeningPost:  return EQRRaidExperienceTier::Inexperienced;
	case EQRVanguardHardpointTier::ForwardPost:    return EQRRaidExperienceTier::Competent;
	case EQRVanguardHardpointTier::Hardpoint:      return EQRRaidExperienceTier::Veteran;
	case EQRVanguardHardpointTier::InnerSanctum:
	case EQRVanguardHardpointTier::Concordat:      return EQRRaidExperienceTier::Fanatic;
	default:                                       return EQRRaidExperienceTier::Inexperienced;
	}
}

void AQRSatelliteOutpost::SetDestroyed()
{
	if (bIsDestroyed) return;
	bIsDestroyed = true;
	ControlState = EQRControlState::PlayerControlled;

	if (ParentColony)
		ParentColony->OnOutpostLost(this);

	OnOutpostDestroyed.Broadcast(this);
}

void AQRSatelliteOutpost::IssueRaid(AQRRaidScheduler* Scheduler)
{
	if (!Scheduler || bIsDestroyed) return;
	Scheduler->TriggerRaid(QRGameplayTags::Faction_Vanguard_Satellite, GetRaidTier());
}
