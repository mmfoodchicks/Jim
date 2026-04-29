#include "Flora/QRFlora_FerricBloom.h"

AQRFlora_FerricBloom::AQRFlora_FerricBloom()
{
	SpeciesId      = FName("PLT_FERRIC_BLOOM");
	DisplayName    = FText::FromString("Ferric Bloom");
	MaxHarvestCharges = 2;
	RegrowthTimeHours = 40.0f;

	HarvestYields.Add({ FName("MAT_FERRIC_PETAL"),   1, 3, 1.0f, FGameplayTag() });
	HarvestYields.Add({ FName("MAT_MAGNETIC_DUST"),  0, 2, 0.6f, FGameplayTag() });
}
