#include "Wildlife/QRWildlife_SiltStrider.h"

AQRWildlife_SiltStrider::AQRWildlife_SiltStrider()
{
	SpeciesId          = FName("ANI_SILT_STRIDER");
	SpeciesDisplayName = FText::FromString("Silt Strider");
	BehaviorRole       = EQRWildlifeBehaviorRole::Prey;
	MaxHealth          = 120.0f;
	MassKg             = 140.0f;
	MoveSpeedWalk      = 250.0f;
	MoveSpeedFlee      = BurstFleeSpeed;
	ThreatDetectionRadius = 1500.0f;
	NoiseFactor        = 0.3f;

	DeathDrops.Add({ FName("FOD_STRIDER_MEAT"),   2, 4, 1.0f });
	DeathDrops.Add({ FName("MAT_BUOYANT_SAC"),    1, 1, 0.6f });
}

void AQRWildlife_SiltStrider::OnThreatDetected_Implementation(AActor* Threat)
{
	SetAIState(EQRWildlifeAIState::Fleeing);
	AlertHerd(Threat);
}
