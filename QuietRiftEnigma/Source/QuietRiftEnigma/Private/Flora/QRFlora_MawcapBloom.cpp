#include "Flora/QRFlora_MawcapBloom.h"
#include "Kismet/GameplayStatics.h"

AQRFlora_MawcapBloom::AQRFlora_MawcapBloom()
{
	SpeciesId      = FName("PLT_MAWCAP_BLOOM");
	DisplayName    = FText::FromString("Mawcap Bloom");
	MaxHarvestCharges = 2;
	RegrowthTimeHours = 48.0f;

	HarvestYields.Add({ FName("MAT_MAWCAP_CAP"),   1, 1, 1.0f,  FGameplayTag::RequestGameplayTag("Tool.Knife") });
	HarvestYields.Add({ FName("MAT_SPORE_DUST"),    1, 3, 0.9f,  FGameplayTag::RequestGameplayTag("Tool.Knife") });
}

void AQRFlora_MawcapBloom::TriggerSporeBurst()
{
	// Blueprint handles VFX and the spore cloud actor spawn
	// C++ signals it by calling OnHarvest or a proximity overlap
}

void AQRFlora_MawcapBloom::OnHarvest_Implementation(AActor* Harvester)
{
	Super::OnHarvest_Implementation(Harvester);
	TriggerSporeBurst();
}
