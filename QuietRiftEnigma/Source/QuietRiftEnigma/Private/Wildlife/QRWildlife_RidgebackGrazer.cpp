#include "Wildlife/QRWildlife_RidgebackGrazer.h"

AQRWildlife_RidgebackGrazer::AQRWildlife_RidgebackGrazer()
{
	SpeciesId          = FName("ANM_RidgebackGrazer");
	SpeciesDisplayName = FText::FromString("Ridgeback Grazer");
	BehaviorRole       = EQRWildlifeBehaviorRole::Prey;
	PreferredBiome     = EQRBiomeType::Grassland;
	MaxHealth          = 150.0f;
	MassKg             = 180.0f;
	MoveSpeedWalk      = 280.0f;
	MoveSpeedFlee      = 560.0f;
	ThreatDetectionRadius = 2000.0f;
	NoiseFactor        = 0.4f;

	// Death drops — large food source
	DeathDrops.Add({ FName("FOD_GRAZER_MEAT_LARGE"), 4, 8, 1.0f });
	DeathDrops.Add({ FName("MAT_GRAZER_HIDE"),       1, 2, 0.9f });
	DeathDrops.Add({ FName("MAT_BONE_LARGE"),         1, 3, 0.8f });
}

void AQRWildlife_RidgebackGrazer::OnThreatDetected_Implementation(AActor* Threat)
{
	// Grazers flee and alert their herd
	SetAIState(EQRWildlifeAIState::Fleeing);
	AlertHerd(Threat);
}
