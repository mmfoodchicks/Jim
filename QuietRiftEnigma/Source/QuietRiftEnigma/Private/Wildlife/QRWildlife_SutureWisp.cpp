#include "Wildlife/QRWildlife_SutureWisp.h"
#include "Kismet/GameplayStatics.h"

AQRWildlife_SutureWisp::AQRWildlife_SutureWisp()
{
	SpeciesId          = FName("PRD_SUTURE_WISP");
	SpeciesDisplayName = FText::FromString("Suture Wisp");
	BehaviorRole       = EQRWildlifeBehaviorRole::Predator;
	// Canon floater: hovers above the ground -- don't drop the mesh
	// to the capsule bottom.
	bGroundBodyToFeet = false;
	MaxHealth          = bIsPrimeElite ? 350.0f : 180.0f;
	MassKg             = 90.0f;
	MoveSpeedWalk      = 300.0f;
	MoveSpeedFlee      = 500.0f;
	ThreatDetectionRadius = 1800.0f;
	NoiseFactor        = 0.15f;

	// Real-world size + attack tuning (drifting filament predator)
	BodyLengthMeters   = 1.6f;
	BodyHeightMeters   = 1.4f;
	AttackDamage       = 20.0f;

	DeathDrops.Add({ FName("MAT_WISP_RIBBON"),   2, 4, 1.0f });
	DeathDrops.Add({ FName("MAT_FILAMENT_CORD"),  1, 3, 0.7f });
}

void AQRWildlife_SutureWisp::TriggerFilamentBind(AActor* Target)
{
	if (!Target) return;
	// Bind handled via status effect tag applied in Blueprint
	UGameplayStatics::ApplyDamage(Target, SlashDamage, GetController(), this, nullptr);
}

void AQRWildlife_SutureWisp::OnThreatDetected_Implementation(AActor* Threat)
{
	SetAIState(EQRWildlifeAIState::Attacking);
	TriggerFilamentBind(Threat);
}
