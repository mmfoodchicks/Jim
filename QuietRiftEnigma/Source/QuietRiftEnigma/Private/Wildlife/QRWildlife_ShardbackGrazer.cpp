#include "Wildlife/QRWildlife_ShardbackGrazer.h"

AQRWildlife_ShardbackGrazer::AQRWildlife_ShardbackGrazer()
{
	SpeciesId          = FName("ANI_SHARDBACK_GRAZER");
	SpeciesDisplayName = FText::FromString("Shardback Grazer");
	BehaviorRole       = EQRWildlifeBehaviorRole::Prey;
	MaxHealth          = 150.0f;
	MassKg             = 200.0f;
	MoveSpeedWalk      = 280.0f;
	MoveSpeedFlee      = 520.0f;
	ThreatDetectionRadius = 1800.0f;
	NoiseFactor        = 0.35f;

	DeathDrops.Add({ FName("FOD_SHARDBACK_MEAT"),  3, 6, 1.0f });
	DeathDrops.Add({ FName("MAT_CERAMIC_PLATE"),   2, 4, 0.85f });
	DeathDrops.Add({ FName("MAT_BONE_MEDIUM"),     1, 2, 0.7f });
}

void AQRWildlife_ShardbackGrazer::OnThreatDetected_Implementation(AActor* Threat)
{
	SetAIState(EQRWildlifeAIState::Fleeing);
	AlertHerd(Threat);
}
