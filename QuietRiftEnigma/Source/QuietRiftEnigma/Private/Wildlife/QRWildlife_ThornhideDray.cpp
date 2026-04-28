#include "Wildlife/QRWildlife_ThornhideDray.h"

AQRWildlife_ThornhideDray::AQRWildlife_ThornhideDray()
{
	SpeciesId          = FName("ANM_ThornhideDray");
	SpeciesDisplayName = FText::FromString("Thornhide Dray");
	BehaviorRole       = EQRWildlifeBehaviorRole::Scavenger;
	PreferredBiome     = EQRBiomeType::Scrubland;
	MaxHealth          = 55.0f;
	MassKg             = 30.0f;
	MoveSpeedWalk      = 240.0f;
	MoveSpeedFlee      = 500.0f;
	NoiseFactor        = 0.45f;

	DeathDrops.Add({ FName("FOD_DRAY_MEAT"),    1, 3, 1.0f });
	DeathDrops.Add({ FName("MAT_THORN_QUILL"),  2, 5, 0.8f }); // Crafting ammo/trap
	DeathDrops.Add({ FName("MAT_DRAY_HIDE"),    0, 1, 0.7f });
}

void AQRWildlife_ThornhideDray::OnThreatDetected_Implementation(AActor* Threat)
{
	// Drays flee but regroup; they harass rather than fight directly
	SetAIState(EQRWildlifeAIState::Fleeing);
	AlertHerd(Threat);
}
