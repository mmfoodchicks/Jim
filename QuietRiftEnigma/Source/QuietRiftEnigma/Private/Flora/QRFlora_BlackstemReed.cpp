#include "Flora/QRFlora_BlackstemReed.h"

AQRFlora_BlackstemReed::AQRFlora_BlackstemReed()
{
	SpeciesId      = FName("PLT_BlackstemReed");
	DisplayName    = FText::FromString("Blackstem Reed");
	PreferredBiome = EQRBiomeType::RiverFlat;
	MaxHarvestCharges = 8;
	RegrowthTimeHours = 12.0f;

	HarvestYields.Add({ FName("MAT_REED_FIBER"),   3, 6, 1.0f,   FGameplayTag() });
	HarvestYields.Add({ FName("MAT_REED_THATCH"),  2, 4, 0.9f,   FGameplayTag() });
	HarvestYields.Add({ FName("MAT_REED_STALK"),   1, 3, 0.8f,   FGameplayTag() });
	HarvestYields.Add({ FName("FOD_REED_PORRIDGE"), 0, 1, 0.3f,  FGameplayTag() }); // Edible pith
}
