#include "Wildlife/QRWildlife_PillarbackHauler.h"
#include "Kismet/GameplayStatics.h"

AQRWildlife_PillarbackHauler::AQRWildlife_PillarbackHauler()
{
	SpeciesId          = FName("ANI_PILLARBACK_HAULER");
	SpeciesDisplayName = FText::FromString("Pillarback Hauler");
	BehaviorRole       = EQRWildlifeBehaviorRole::Prey;
	MaxHealth          = 800.0f;
	MassKg             = 2400.0f;
	MoveSpeedWalk      = 200.0f;
	MoveSpeedFlee      = 350.0f;
	ThreatDetectionRadius = 1200.0f;
	NoiseFactor        = 0.8f;

	DeathDrops.Add({ FName("FOD_HAULER_MEAT_LARGE"), 8, 16, 1.0f });
	DeathDrops.Add({ FName("MAT_PILLAR_BONE"),        4,  8, 0.9f });
	DeathDrops.Add({ FName("MAT_HAULER_HIDE_HEAVY"),  2,  4, 0.8f });

	DorsalMineralDrops.Add({ FName("MAT_MINERAL_SHARD"), 2, 6, 0.75f });
	DorsalMineralDrops.Add({ FName("MAT_BONE_DENSE"),    1, 3, 0.6f });
}

void AQRWildlife_PillarbackHauler::OnThreatDetected_Implementation(AActor* Threat)
{
	// Megafauna is slow to panic but delivers devastating stomps if cornered
	SetAIState(EQRWildlifeAIState::Fleeing);
}

void AQRWildlife_PillarbackHauler::OnDeath_Implementation()
{
	Super::OnDeath_Implementation();
	// Release dorsal mineral cache on death
	for (const FQRWildlifeDrop& Drop : DorsalMineralDrops)
	{
		// Spawn drop items at dorsal socket location
		// Blueprint can override to play VFX/audio
	}
}
