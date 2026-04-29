#include "Flora/QRFlora_NullmintNodes.h"

AQRFlora_NullmintNodes::AQRFlora_NullmintNodes()
{
	SpeciesId      = FName("PLT_NULLMINT_NODES");
	DisplayName    = FText::FromString("Nullmint Nodes");
	MaxHarvestCharges = NodeCount;
	RegrowthTimeHours = 20.0f;

	HarvestYields.Add({ FName("MED_NULLMINT"), 1, 1, 1.0f, FGameplayTag() });
}
