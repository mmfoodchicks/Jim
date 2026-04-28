#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "Net/UnrealNetwork.h"
#include "QRMath.h"

void UQRItemInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRItemInstance, Quantity);
	DOREPLIFETIME(UQRItemInstance, Durability);
	DOREPLIFETIME(UQRItemInstance, SpoilProgress);
}

void UQRItemInstance::Initialize(const UQRItemDefinition* InDefinition, int32 InQuantity)
{
	Definition     = InDefinition;
	Quantity       = InQuantity;
	InstanceGuid   = FGuid::NewGuid();
	SpoilProgress  = 0.0f;
	SpoilState     = EQRSpoilState::Fresh;

	if (Definition)
	{
		Durability     = Definition->MaxDurability > 0.0f ? Definition->MaxDurability : -1.0f;
		EdibilityState = Definition->FoodStats.DefaultEdibility;
	}
}

void UQRItemInstance::AdvanceSpoilByHours(float GameHoursElapsed)
{
	if (!Definition || Definition->FoodStats.SpoilRatePerHour <= 0.0f)
		return;

	SpoilProgress = UQRMath::AdvanceSpoil(SpoilProgress, Definition->FoodStats.SpoilRatePerHour, GameHoursElapsed);
	RefreshSpoilState();
}

void UQRItemInstance::ApplyDurabilityDamage(float Amount)
{
	if (Durability < 0.0f)
		return;
	Durability = FMath::Max(0.0f, Durability - Amount);
}

void UQRItemInstance::RefreshSpoilState()
{
	if (SpoilProgress >= 1.0f)
		SpoilState = EQRSpoilState::Rotten;
	else if (SpoilProgress >= 0.75f)
		SpoilState = EQRSpoilState::Spoiled;
	else if (SpoilProgress >= 0.4f)
		SpoilState = EQRSpoilState::Aging;
	else
		SpoilState = EQRSpoilState::Fresh;
}

void UQRItemInstance::OnRep_Quantity()  {}
void UQRItemInstance::OnRep_Durability() {}
void UQRItemInstance::OnRep_Spoil()
{
	RefreshSpoilState();
}
