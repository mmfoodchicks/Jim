#include "QRScentComponent.h"
#include "QRGameplayTags.h"
#include "Net/UnrealNetwork.h"

UQRScentComponent::UQRScentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1.0f;
	SetIsReplicatedByDefault(true);
}

void UQRScentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRScentComponent, ScentIntensity);
	DOREPLIFETIME(UQRScentComponent, DominantScentTag);
}

void UQRScentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()->HasAuthority() || ScentIntensity <= 0.0f) return;

	const float NewIntensity = FMath::Max(0.0f, ScentIntensity - ScentDecayRatePerSecond * DeltaTime);
	if (NewIntensity != ScentIntensity)
	{
		ScentIntensity = NewIntensity;
		if (ScentIntensity <= 0.0f)
		{
			ScentIntensity  = 0.0f;
			DominantScentTag = FGameplayTag();
		}
		OnScentChanged.Broadcast(ScentIntensity, DominantScentTag);
	}
}

void UQRScentComponent::AddScent(float Intensity, FGameplayTag ScentTag)
{
	const float NewIntensity = FMath::Clamp(ScentIntensity + Intensity, 0.0f, 1.0f);
	if (NewIntensity != ScentIntensity || ScentTag != DominantScentTag)
	{
		ScentIntensity   = NewIntensity;
		DominantScentTag = ScentTag;
		OnScentChanged.Broadcast(ScentIntensity, DominantScentTag);
	}
}

void UQRScentComponent::ClearScent()
{
	if (ScentIntensity <= 0.0f) return;
	ScentIntensity   = 0.0f;
	DominantScentTag = FGameplayTag();
	OnScentChanged.Broadcast(0.0f, FGameplayTag());
}

void UQRScentComponent::RefreshFromInventory(float MeatMassKg, bool bHasBlood)
{
	float NewIntensity = FMath::Clamp(FMath::Max(MeatMassKg, 0.0f) * MeatIntensityPerKg, 0.0f, 1.0f);
	FGameplayTag Tag = QRGameplayTags::Scent_Meat;

	if (bHasBlood)
	{
		NewIntensity = FMath::Clamp(NewIntensity + BloodScentBonus, 0.0f, 1.0f);
		Tag          = QRGameplayTags::Scent_Blood;
	}

	// Rotten-meat carried at max intensity → carrion tag
	if (NewIntensity >= 1.0f)
		Tag = QRGameplayTags::Scent_Carrion;

	if (NewIntensity != ScentIntensity || Tag != DominantScentTag)
	{
		ScentIntensity   = NewIntensity;
		DominantScentTag = Tag;
		OnScentChanged.Broadcast(ScentIntensity, DominantScentTag);
	}
}

float UQRScentComponent::GetDetectionRadiusMultiplier() const
{
	// Linear scale: 0 intensity = 1.0x, full intensity = MaxDetectionRadiusMult
	return FMath::Lerp(1.0f, MaxDetectionRadiusMult, ScentIntensity);
}
