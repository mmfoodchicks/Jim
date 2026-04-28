#include "Wildlife/QRWildlife_GlasshornRunner.h"

AQRWildlife_GlasshornRunner::AQRWildlife_GlasshornRunner()
{
	SpeciesId          = FName("ANM_GlasshornRunner");
	SpeciesDisplayName = FText::FromString("Glasshorn Runner");
	BehaviorRole       = EQRWildlifeBehaviorRole::Prey;
	PreferredBiome     = EQRBiomeType::OpenField;
	MaxHealth          = 40.0f;
	MassKg             = 25.0f;
	MoveSpeedWalk      = 320.0f;
	MoveSpeedFlee      = 780.0f; // Very fast — requires stealth approach
	ThreatDetectionRadius = 3000.0f;
	NoiseFactor        = 0.1f;

	// Optics resource from horns + food
	DeathDrops.Add({ FName("FOD_RUNNER_MEAT"),   2, 4, 1.0f });
	DeathDrops.Add({ FName("MAT_GLASSHORN"),     1, 2, 0.85f }); // Optics crafting
	DeathDrops.Add({ FName("MAT_RUNNER_HIDE"),   1, 1, 0.9f });
}

void AQRWildlife_GlasshornRunner::OnThreatDetected_Implementation(AActor* Threat)
{
	// Extreme flee response — no alert, purely evasion
	SetAIState(EQRWildlifeAIState::Fleeing);
}
