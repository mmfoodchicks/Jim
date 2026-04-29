#include "Wildlife/QRWildlife_TrenchDiggers.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

AQRWildlife_TrenchDiggers::AQRWildlife_TrenchDiggers()
{
	SpeciesId          = FName("PRD_TRENCH_DIGGERS");
	SpeciesDisplayName = FText::FromString("Trench Digger");
	BehaviorRole       = EQRWildlifeBehaviorRole::Predator;
	MaxHealth          = 200.0f;
	MassKg             = 250.0f;
	MoveSpeedWalk      = 220.0f;
	MoveSpeedFlee      = 300.0f;
	ThreatDetectionRadius = 2000.0f;
	NoiseFactor        = 0.6f;

	DeathDrops.Add({ FName("MAT_MINERAL_TOOTH"),   2, 4, 1.0f });
	DeathDrops.Add({ FName("MAT_DIGGER_CARAPACE"), 1, 2, 0.8f });
}

void AQRWildlife_TrenchDiggers::StartDigging(AActor* TargetStructure)
{
	if (!TargetStructure) return;
	bIsTunneling = true;
	// BT will tick wall damage via WallDamagePerCycle / DigCycleSeconds
}

void AQRWildlife_TrenchDiggers::SurfaceErupt()
{
	bIsTunneling = false;
	// Spawn ground-burst VFX at current location from Blueprint
}

void AQRWildlife_TrenchDiggers::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRWildlife_TrenchDiggers, bIsTunneling);
}
