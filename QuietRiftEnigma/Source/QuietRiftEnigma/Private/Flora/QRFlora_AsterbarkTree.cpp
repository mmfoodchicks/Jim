#include "Flora/QRFlora_AsterbarkTree.h"
#include "Net/UnrealNetwork.h"

AQRFlora_AsterbarkTree::AQRFlora_AsterbarkTree()
{
	SpeciesId      = FName("TRE_ASTERBARK");
	DisplayName    = FText::FromString("Asterbark");
	MaxHarvestCharges = 3;
	RegrowthTimeHours = 480.0f;

	HarvestYields.Add({ FName("MAT_ASTERBARK_LOG"),   2, 4, 1.0f,  FGameplayTag::RequestGameplayTag("Tool.Axe") });
	HarvestYields.Add({ FName("MAT_ASTERBARK_FLECK"), 2, 6, 0.9f,  FGameplayTag() });
}

void AQRFlora_AsterbarkTree::Fell()
{
	if (bIsFelled || bIsDepleted) return;
	bIsFelled = true;
	RemainingCharges = FMath::Max(RemainingCharges, 5);
}

void AQRFlora_AsterbarkTree::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRFlora_AsterbarkTree, bIsFelled);
}
