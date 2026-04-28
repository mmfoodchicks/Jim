#include "Wildlife/QRWildlife_HookjawStalker.h"
#include "Net/UnrealNetwork.h"

AQRWildlife_HookjawStalker::AQRWildlife_HookjawStalker()
{
	SpeciesId          = FName("ANM_HookjawStalker");
	SpeciesDisplayName = FText::FromString("Hookjaw Stalker");
	BehaviorRole       = EQRWildlifeBehaviorRole::Predator;
	PreferredBiome     = EQRBiomeType::Forest;
	MaxHealth          = 120.0f;
	MassKg             = 90.0f;
	MoveSpeedWalk      = 200.0f;  // Slow stalk
	MoveSpeedCharge    = 650.0f;  // Explosive burst speed
	MoveSpeedFlee      = 480.0f;
	ThreatDetectionRadius = 2500.0f;
	NoiseFactor        = 0.05f;   // Nearly silent

	DeathDrops.Add({ FName("FOD_STALKER_MEAT"),     2, 4, 1.0f });
	DeathDrops.Add({ FName("MAT_STALKER_HIDE"),     1, 2, 0.9f });
	DeathDrops.Add({ FName("MAT_HOOKJAW_FANG"),     1, 2, 0.75f }); // Crafting component
	DeathDrops.Add({ FName("MAT_PREDATOR_GLAND"),   0, 1, 0.5f });  // Rare medicine drop
}

void AQRWildlife_HookjawStalker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRWildlife_HookjawStalker, bIsStalking);
}

void AQRWildlife_HookjawStalker::OnThreatDetected_Implementation(AActor* Threat)
{
	// Stalkers begin stalking, then transition to attack when close enough
	bIsStalking = true;
	SetAIState(EQRWildlifeAIState::Stalking);

	// Pack hunters alert their pack
	if (bPackHunter) AlertHerd(Threat);
}
