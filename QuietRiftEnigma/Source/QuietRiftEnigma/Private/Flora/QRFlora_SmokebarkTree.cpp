#include "Flora/QRFlora_SmokebarkTree.h"
#include "Net/UnrealNetwork.h"

AQRFlora_SmokebarkTree::AQRFlora_SmokebarkTree()
{
	SpeciesId      = FName("PLT_SmokebarkTree");
	DisplayName    = FText::FromString("Smokebark Tree");
	PreferredBiome = EQRBiomeType::Forest;
	MaxHarvestCharges = 3;
	RegrowthTimeHours = 72.0f;

	// Chop yields — axe required for logs; hand-pick for bark/resin
	HarvestYields.Add({ FName("MAT_SMOKEBARK_LOG"),    2, 5, 1.0f,   FGameplayTag::RequestGameplayTag("Tool.Axe") });
	HarvestYields.Add({ FName("MAT_SMOKEBARK_PLANK"),  0, 2, 0.6f,   FGameplayTag::RequestGameplayTag("Tool.Axe") });
	HarvestYields.Add({ FName("MAT_SMOKEBARK_RESIN"),  1, 3, 0.9f,   FGameplayTag() }); // No tool needed
	HarvestYields.Add({ FName("MAT_SMOKEBARK_BARK"),   1, 2, 0.8f,   FGameplayTag() });
	HarvestYields.Add({ FName("MAT_BRANCH"),           2, 6, 1.0f,   FGameplayTag() });
}

void AQRFlora_SmokebarkTree::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRFlora_SmokebarkTree, bIsFelled);
}

void AQRFlora_SmokebarkTree::Fell()
{
	if (bIsFelled || bIsDepleted) return;
	bIsFelled = true;
	// Felling dramatically increases remaining charges and log yield
	RemainingCharges = FMath::Max(RemainingCharges, 5);
	// Blueprint handles the tree-fall physics/animation trigger
}
