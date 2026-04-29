#include "Wildlife/QRWildlife_VaneRippers.h"
#include "Kismet/GameplayStatics.h"

AQRWildlife_VaneRippers::AQRWildlife_VaneRippers()
{
	SpeciesId          = FName("PRD_VANE_RIPPERS");
	SpeciesDisplayName = FText::FromString("Vane Rippers");
	BehaviorRole       = EQRWildlifeBehaviorRole::Predator;
	MaxHealth          = 90.0f;
	MassKg             = 55.0f;
	MoveSpeedWalk      = 380.0f;
	MoveSpeedFlee      = 600.0f;
	ThreatDetectionRadius = 1600.0f;
	NoiseFactor        = 0.2f;

	DeathDrops.Add({ FName("MAT_VANE_BLADE"),   1, 2, 0.9f });
	DeathDrops.Add({ FName("FOD_RIPPER_MEAT"),  1, 2, 1.0f });
}

void AQRWildlife_VaneRippers::OnThreatDetected_Implementation(AActor* Threat)
{
	SetAIState(EQRWildlifeAIState::Attacking);
}
