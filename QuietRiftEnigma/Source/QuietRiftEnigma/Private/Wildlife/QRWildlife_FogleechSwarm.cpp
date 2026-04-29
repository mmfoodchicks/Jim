#include "Wildlife/QRWildlife_FogleechSwarm.h"
#include "QRSurvivalComponent.h"

AQRWildlife_FogleechSwarm::AQRWildlife_FogleechSwarm()
{
	SpeciesId          = FName("PRD_FOGLEECH_SWARM");
	SpeciesDisplayName = FText::FromString("Fogleech Swarm");
	BehaviorRole       = EQRWildlifeBehaviorRole::Predator;
	MaxHealth          = 10.0f; // Each leech is nearly trivial alone
	MassKg             = 2.0f;
	MoveSpeedWalk      = 200.0f;
	MoveSpeedFlee      = 100.0f;
	ThreatDetectionRadius = 800.0f;
	NoiseFactor        = 0.1f;

	DeathDrops.Add({ FName("MAT_LEECH_MEMBRANE"), 1, 3, 0.4f });
}

void AQRWildlife_FogleechSwarm::ApplySwarmEffects(AActor* Target, float DeltaTime)
{
	if (!Target) return;
	UQRSurvivalComponent* Survival = Target->FindComponentByClass<UQRSurvivalComponent>();
	if (Survival)
	{
		// Apply bleed damage
		Survival->ApplyDamage(BleedDamagePerSecond * DeltaTime);
		// Composure drain is handled via tag application in Blueprint
	}
}

void AQRWildlife_FogleechSwarm::OnThreatDetected_Implementation(AActor* Threat)
{
	// Swarms aggregate toward warm bodies, not flee
	SetAIState(EQRWildlifeAIState::Attacking);
}
