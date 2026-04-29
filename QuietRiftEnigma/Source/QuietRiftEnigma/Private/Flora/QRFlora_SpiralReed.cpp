#include "Flora/QRFlora_SpiralReed.h"

AQRFlora_SpiralReed::AQRFlora_SpiralReed()
{
	SpeciesId      = FName("PLT_SPIRAL_REED");
	DisplayName    = FText::FromString("Spiral Reed");
	MaxHarvestCharges = ReedCount / 3;
	RegrowthTimeHours = 12.0f;

	HarvestYields.Add({ FName("MAT_SPIRAL_FIBER"), 2, 4, 1.0f,  FGameplayTag::RequestGameplayTag("Tool.Knife") });
	HarvestYields.Add({ FName("FOD_REED_GRAIN"),   0, 2, 0.5f,  FGameplayTag() });
}
