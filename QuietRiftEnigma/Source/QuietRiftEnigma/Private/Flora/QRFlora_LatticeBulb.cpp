#include "Flora/QRFlora_LatticeBulb.h"

AQRFlora_LatticeBulb::AQRFlora_LatticeBulb()
{
	SpeciesId      = FName("PLT_LATTICE_BULB");
	DisplayName    = FText::FromString("Lattice Bulb");
	MaxHarvestCharges = BulbCount;
	RegrowthTimeHours = 18.0f;

	HarvestYields.Add({ FName("FOD_LATTICE_TUBER"), 1, 2, 1.0f,  FGameplayTag() });
	HarvestYields.Add({ FName("MAT_HOLLOW_SHELL"),   0, 1, 0.4f,  FGameplayTag() });
}
