#include "Wildlife/QRWildlife_SutureWisp.h"
#include "Kismet/GameplayStatics.h"

AQRWildlife_SutureWisp::AQRWildlife_SutureWisp()
{
	SpeciesId          = FName("PRD_SUTURE_WISP");
	SpeciesDisplayName = FText::FromString("Suture Wisp");
	BehaviorRole       = EQRWildlifeBehaviorRole::Predator;
	MaxHealth          = bIsPrimeElite ? 350.0f : 180.0f;
	MassKg             = 90.0f;
	MoveSpeedWalk      = 300.0f;
	MoveSpeedFlee      = 500.0f;
	ThreatDetectionRadius = 1800.0f;
	NoiseFactor        = 0.15f;

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
