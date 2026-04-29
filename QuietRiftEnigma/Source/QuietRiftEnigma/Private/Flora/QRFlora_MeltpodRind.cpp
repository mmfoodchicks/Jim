#include "Flora/QRFlora_MeltpodRind.h"
#include "Net/UnrealNetwork.h"
#include "QRSurvivalComponent.h"

AQRFlora_MeltpodRind::AQRFlora_MeltpodRind()
{
	SpeciesId      = FName("PLT_MELTPOD_RIND");
	DisplayName    = FText::FromString("Meltpod Rind");
	MaxHarvestCharges = 3;
	RegrowthTimeHours = 24.0f;

	HarvestYields.Add({ FName("FOD_MELTPOD_GEL"), 1, 2, 1.0f, FGameplayTag() });
}

void AQRFlora_MeltpodRind::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRFlora_MeltpodRind, bSeamsOpen);
}
