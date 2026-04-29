#include "Flora/QRFlora_CinderThorn.h"
#include "QRSurvivalComponent.h"
#include "Kismet/GameplayStatics.h"

AQRFlora_CinderThorn::AQRFlora_CinderThorn()
{
	SpeciesId      = FName("PLT_CINDER_THORN");
	DisplayName    = FText::FromString("Cinder Thorn");
	MaxHarvestCharges = 3;
	RegrowthTimeHours = 28.0f;

	HarvestYields.Add({ FName("MAT_CINDER_THORN_ASH"),  1, 2, 1.0f,  FGameplayTag() });
	HarvestYields.Add({ FName("MAT_EMBER_THORN"),         1, 3, 0.7f,  FGameplayTag() });
}

void AQRFlora_CinderThorn::OnHarvest_Implementation(AActor* Harvester)
{
	if (!Harvester) return;
	if (bRequiresProtectiveGloves)
	{
		// Check for glove equipment tag — if absent, deal thorn pierce damage
		// This check is done via survival component
		UQRSurvivalComponent* Survival = Harvester->FindComponentByClass<UQRSurvivalComponent>();
		if (Survival)
		{
			Survival->ApplyDamage(ThornPierceDamage);
		}
	}
	Super::OnHarvest_Implementation(Harvester);
}
