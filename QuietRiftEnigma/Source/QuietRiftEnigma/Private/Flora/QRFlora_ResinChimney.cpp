#include "Flora/QRFlora_ResinChimney.h"
#include "Net/UnrealNetwork.h"

AQRFlora_ResinChimney::AQRFlora_ResinChimney()
{
	SpeciesId      = FName("PLT_RESIN_CHIMNEY");
	DisplayName    = FText::FromString("Resin Chimney");
	MaxHarvestCharges = 4;
	RegrowthTimeHours = 16.0f;

	HarvestYields.Add({ FName("MAT_RESIN_BLOCK"), 1, 3, 1.0f, FGameplayTag::RequestGameplayTag("Tool.Knife") });
}

void AQRFlora_ResinChimney::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRFlora_ResinChimney, CollarThickness);
}
