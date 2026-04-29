#include "Flora/QRFlora_SlagrootTree.h"
#include "Net/UnrealNetwork.h"

AQRFlora_SlagrootTree::AQRFlora_SlagrootTree()
{
	SpeciesId      = FName("TRE_SLAGROOT");
	DisplayName    = FText::FromString("Slagroot");
	MaxHarvestCharges = 4;
	RegrowthTimeHours = 360.0f;

	HarvestYields.Add({ FName("MAT_SLAGROOT_LOG"),     3, 6, 1.0f,  FGameplayTag::RequestGameplayTag("Tool.HeavyAxe") });
	HarvestYields.Add({ FName("MAT_SLAGROOT_CINDER"),  1, 3, 0.6f,  FGameplayTag() });
}

void AQRFlora_SlagrootTree::Fell()
{
	if (bIsFelled || bIsDepleted) return;
	bIsFelled = true;
	RemainingCharges = FMath::Max(RemainingCharges, 6);
}

void AQRFlora_SlagrootTree::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRFlora_SlagrootTree, bIsFelled);
}
