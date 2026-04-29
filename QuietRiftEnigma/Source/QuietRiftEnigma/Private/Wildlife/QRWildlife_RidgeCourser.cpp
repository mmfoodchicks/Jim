#include "Wildlife/QRWildlife_RidgeCourser.h"
#include "Net/UnrealNetwork.h"

AQRWildlife_RidgeCourser::AQRWildlife_RidgeCourser()
{
	SpeciesId          = FName("ANI_RIDGE_COURSER");
	SpeciesDisplayName = FText::FromString("Ridge Courser");
	BehaviorRole       = EQRWildlifeBehaviorRole::Prey;
	MaxHealth          = 200.0f;
	MassKg             = 280.0f;
	MoveSpeedWalk      = 400.0f;
	MoveSpeedFlee      = 550.0f;
	ThreatDetectionRadius = 2200.0f;
	NoiseFactor        = 0.2f;

	DeathDrops.Add({ FName("FOD_COURSER_MEAT"),  3, 5, 1.0f });
	DeathDrops.Add({ FName("MAT_COURSER_HIDE"),  1, 2, 0.8f });
	DeathDrops.Add({ FName("MAT_VANE_QUILL"),    2, 4, 0.7f });
}

bool AQRWildlife_RidgeCourser::TryMount(AActor* Rider)
{
	if (!bIsTamed || MountedRider != nullptr || !Rider) return false;
	MountedRider = Rider;
	MoveSpeedWalk = MoveSpeedWalk * MountedSpeedMultiplier;
	return true;
}

void AQRWildlife_RidgeCourser::Dismount()
{
	if (!MountedRider) return;
	MoveSpeedWalk = MoveSpeedWalk / MountedSpeedMultiplier;
	MountedRider = nullptr;
}

void AQRWildlife_RidgeCourser::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRWildlife_RidgeCourser, bIsTamed);
	DOREPLIFETIME(AQRWildlife_RidgeCourser, MountedRider);
}
