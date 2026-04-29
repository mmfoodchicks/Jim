#include "Flora/QRFlora_GlasbarkTree.h"
#include "Net/UnrealNetwork.h"

AQRFlora_GlasbarkTree::AQRFlora_GlasbarkTree()
{
	SpeciesId      = FName("TRE_GLASSBARK");
	DisplayName    = FText::FromString("Glassbark");
	MaxHarvestCharges = 3;
	RegrowthTimeHours = 240.0f;

	HarvestYields.Add({ FName("MAT_GLASSBARK_LOG"),   2, 4, 1.0f,  FGameplayTag::RequestGameplayTag("Tool.Axe") });
	HarvestYields.Add({ FName("MAT_GLASSBARK_FLAKE"), 2, 5, 0.8f,  FGameplayTag() });
	HarvestYields.Add({ FName("MAT_BRANCH"),           1, 3, 1.0f,  FGameplayTag() });
}

void AQRFlora_GlasbarkTree::Fell()
{
	if (bIsFelled || bIsDepleted) return;
	bIsFelled = true;
	RemainingCharges = FMath::Max(RemainingCharges, 5);
}

void AQRFlora_GlasbarkTree::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRFlora_GlasbarkTree, bIsFelled);
}
