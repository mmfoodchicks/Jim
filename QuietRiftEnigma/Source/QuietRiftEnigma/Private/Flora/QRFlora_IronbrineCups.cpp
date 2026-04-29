#include "Flora/QRFlora_IronbrineCups.h"
#include "Net/UnrealNetwork.h"

AQRFlora_IronbrineCups::AQRFlora_IronbrineCups()
{
	SpeciesId      = FName("PLT_IRONBRINE_CUPS");
	DisplayName    = FText::FromString("Ironbrine Cups");
	MaxHarvestCharges = 4;
	RegrowthTimeHours = 6.0f;

	HarvestYields.Add({ FName("MAT_IRONBRINE"), 1, 2, 1.0f, FGameplayTag() });
}

void AQRFlora_IronbrineCups::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRFlora_IronbrineCups, BrineFillLevel);
}
