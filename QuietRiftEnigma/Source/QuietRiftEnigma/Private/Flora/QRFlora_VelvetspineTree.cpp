#include "Flora/QRFlora_VelvetspineTree.h"
#include "Net/UnrealNetwork.h"

AQRFlora_VelvetspineTree::AQRFlora_VelvetspineTree()
{
	SpeciesId      = FName("TRE_VELVETSPINE");
	DisplayName    = FText::FromString("Velvetspine");
	MaxHarvestCharges = 3;
	RegrowthTimeHours = 300.0f;

	HarvestYields.Add({ FName("MAT_VELVETSPINE_LOG"),   2, 5, 1.0f,  FGameplayTag::RequestGameplayTag("Tool.Axe") });
	HarvestYields.Add({ FName("MAT_VELVETSPINE_SPINE"), 1, 3, 0.8f,  FGameplayTag::RequestGameplayTag("Tool.Gloves") });
	HarvestYields.Add({ FName("MAT_FIBER_LONG"),         1, 4, 0.7f,  FGameplayTag() });
}

void AQRFlora_VelvetspineTree::Fell()
{
	if (bIsFelled || bIsDepleted) return;
	bIsFelled = true;
	RemainingCharges = FMath::Max(RemainingCharges, 5);
}

void AQRFlora_VelvetspineTree::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRFlora_VelvetspineTree, bIsFelled);
}
