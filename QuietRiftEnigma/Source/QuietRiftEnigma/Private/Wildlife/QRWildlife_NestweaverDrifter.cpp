#include "Wildlife/QRWildlife_NestweaverDrifter.h"
#include "Net/UnrealNetwork.h"

AQRWildlife_NestweaverDrifter::AQRWildlife_NestweaverDrifter()
{
	SpeciesId          = FName("ANI_NESTWEAVER_DRIFTER");
	SpeciesDisplayName = FText::FromString("Nestweaver Drifter");
	BehaviorRole       = EQRWildlifeBehaviorRole::Prey;
	MaxHealth          = 80.0f;
	MassKg             = 12.0f;
	MoveSpeedWalk      = 120.0f;
	MoveSpeedFlee      = 280.0f;
	ThreatDetectionRadius = 1200.0f;
	NoiseFactor        = 0.1f;

	DeathDrops.Add({ FName("MAT_MEMBRANE_FIN"),  2, 4, 1.0f });
	DeathDrops.Add({ FName("FOD_DRIFTER_MEAT"),  1, 2, 1.0f });
}

bool AQRWildlife_NestweaverDrifter::CollectEgg(TArray<FQRWildlifeDrop>& OutEggs)
{
	if (!bIsPenned || HoursUntilNextEgg > 0.0f) return false;
	OutEggs.Add({ EggItemId, 1, 1, 1.0f });
	HoursUntilNextEgg = EggLayIntervalHours;
	return true;
}

void AQRWildlife_NestweaverDrifter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRWildlife_NestweaverDrifter, bIsPenned);
	DOREPLIFETIME(AQRWildlife_NestweaverDrifter, HoursUntilNextEgg);
}
