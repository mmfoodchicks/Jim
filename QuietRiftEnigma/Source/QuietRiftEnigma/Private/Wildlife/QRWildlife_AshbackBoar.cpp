#include "Wildlife/QRWildlife_AshbackBoar.h"
#include "QRDepotComponent.h"

AQRWildlife_AshbackBoar::AQRWildlife_AshbackBoar()
{
	SpeciesId          = FName("ANM_AshbackBoar");
	SpeciesDisplayName = FText::FromString("Ashback Boar");
	BehaviorRole       = EQRWildlifeBehaviorRole::Scavenger;
	PreferredBiome     = EQRBiomeType::BurnZone;
	MaxHealth          = 90.0f;
	MassKg             = 80.0f;
	MoveSpeedWalk      = 260.0f;
	MoveSpeedCharge    = 550.0f;
	MoveSpeedFlee      = 420.0f;
	ThreatDetectionRadius = 1200.0f;
	NoiseFactor        = 0.6f;

	DeathDrops.Add({ FName("FOD_BOAR_MEAT"),    3, 6, 1.0f });
	DeathDrops.Add({ FName("MAT_BOAR_HIDE"),    1, 2, 0.9f });
	DeathDrops.Add({ FName("MAT_BOAR_TUSK"),    1, 2, 0.7f });
	DeathDrops.Add({ FName("MAT_BONE_MEDIUM"),  1, 2, 0.75f });
}

void AQRWildlife_AshbackBoar::OnThreatDetected_Implementation(AActor* Threat)
{
	// Boars charge when threatened if health is above 50%; flee below
	if (GetHealthPercent() > 0.5f)
		SetAIState(EQRWildlifeAIState::Charging);
	else
		SetAIState(EQRWildlifeAIState::Fleeing);
}

bool AQRWildlife_AshbackBoar::TryStealFood(AActor* FoodDepot)
{
	if (!FoodDepot) return false;

	UQRDepotComponent* Depot = FoodDepot->FindComponentByClass<UQRDepotComponent>();
	if (!Depot || Depot->GetTotalStoredCount() == 0) return false;

	// Steal a small amount from the nearest food depot
	UQRItemInstance* Stolen = Depot->WithdrawItem(FName("FOD_FIELD_RATION"), 1);
	return Stolen != nullptr;
}
