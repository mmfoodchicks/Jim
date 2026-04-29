#include "Wildlife/QRWildlife_VaultbackDray.h"
#include "Net/UnrealNetwork.h"

AQRWildlife_VaultbackDray::AQRWildlife_VaultbackDray()
{
	SpeciesId          = FName("ANI_VAULTBACK_DRAY");
	SpeciesDisplayName = FText::FromString("Vaultback Dray");
	BehaviorRole       = EQRWildlifeBehaviorRole::Prey;
	MaxHealth          = 350.0f;
	MassKg             = 900.0f;
	MoveSpeedWalk      = 180.0f;
	MoveSpeedFlee      = 260.0f;
	ThreatDetectionRadius = 1000.0f;
	NoiseFactor        = 0.5f;

	DeathDrops.Add({ FName("FOD_DRAY_MEAT_LARGE"), 6, 10, 1.0f });
	DeathDrops.Add({ FName("MAT_VAULT_HIDE"),       3,  5, 0.85f });
	DeathDrops.Add({ FName("MAT_BONE_HEAVY"),        2,  4, 0.75f });
}

bool AQRWildlife_VaultbackDray::LoadCargo(float WeightKg)
{
	if (CurrentCargoWeightKg + WeightKg > MaxCargoWeightKg) return false;
	CurrentCargoWeightKg += WeightKg;
	return true;
}

void AQRWildlife_VaultbackDray::UnloadCargo()
{
	CurrentCargoWeightKg = 0.0f;
}

void AQRWildlife_VaultbackDray::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRWildlife_VaultbackDray, CurrentCargoWeightKg);
	DOREPLIFETIME(AQRWildlife_VaultbackDray, bIsTamed);
}
