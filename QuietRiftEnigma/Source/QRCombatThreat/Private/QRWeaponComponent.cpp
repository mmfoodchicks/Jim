#include "QRWeaponComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "QRSurvivalComponent.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

UQRWeaponComponent::UQRWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UQRWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRWeaponComponent, WeaponState);
	DOREPLIFETIME(UQRWeaponComponent, CurrentAmmo);
	DOREPLIFETIME(UQRWeaponComponent, FoulingFactor);
	DOREPLIFETIME(UQRWeaponComponent, bIsJammed);
}

bool UQRWeaponComponent::CanFire() const
{
	return !bIsJammed && WeaponState == EQRWeaponState::Ready && CurrentAmmo > 0;
}

float UQRWeaponComponent::GetFoulingIncrement(bool bIsDirtyAmmo, bool bHasSuppressor) const
{
	float Inc = FoulingPerShot;
	if (bIsDirtyAmmo)   Inc *= DirtyAmmoFoulingMult;
	if (bHasSuppressor) Inc *= SuppressorFoulingMult;
	return FMath::Clamp(Inc, 0.0f, 1.0f);
}

bool UQRWeaponComponent::TryFire(AActor* Target, UQRItemInstance* AmmoInstance)
{
	if (!CanFire()) return false;

	// Determine ammo quality for fouling calculation
	const bool bIsDirtyAmmo = AmmoInstance && AmmoInstance->Definition &&
		AmmoInstance->Definition->ItemTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Ammo.Dirty")));

	// Check jam before firing
	if (FMath::FRand() < GetJamChance())
	{
		bIsJammed   = true;
		WeaponState = EQRWeaponState::Jammed;
		OnWeaponJammed.Broadcast();
		return false;
	}

	float Distance = 0.0f;
	if (Target && GetOwner())
		Distance = FVector::Dist(GetOwner()->GetActorLocation(), Target->GetActorLocation()) / 100.0f;

	float Damage = ComputeEffectiveDamage(Distance);

	if (Target)
	{
		if (UQRSurvivalComponent* Survival = Target->FindComponentByClass<UQRSurvivalComponent>())
			Survival->ApplyDamage(Damage, EQRInjuryType::Bleeding);
	}

	--CurrentAmmo;
	// v1.17: canonical fouling increment (dirty ammo ×5, suppressor ×1.5)
	FoulingFactor = FMath::Clamp(FoulingFactor + GetFoulingIncrement(bIsDirtyAmmo, false), 0.0f, 1.0f);

	if (CurrentAmmo <= 0)
		WeaponState = EQRWeaponState::Empty;

	OnWeaponFired.Broadcast(Target, Damage);
	OnAmmoChanged.Broadcast(CurrentAmmo);
	return true;
}

float UQRWeaponComponent::ComputeEffectiveDamage(float DistanceMeters) const
{
	if (WeaponType == EQRWeaponType::Melee) return BaseDamage;

	// Linear falloff beyond effective range
	float Falloff = FMath::Clamp(1.0f - (DistanceMeters - EffectiveRangeMeters) / EffectiveRangeMeters, 0.2f, 1.0f);
	return BaseDamage * Falloff;
}

void UQRWeaponComponent::BeginReload()
{
	if (WeaponState == EQRWeaponState::Jammed || WeaponState == EQRWeaponState::Firing) return;
	WeaponState = EQRWeaponState::Reloading;
}

void UQRWeaponComponent::FinishReload(int32 NewAmmoCount)
{
	CurrentAmmo = FMath::Min(NewAmmoCount, MagazineCapacity);
	WeaponState = EQRWeaponState::Ready;
	OnWeaponReloaded.Broadcast();
	OnAmmoChanged.Broadcast(CurrentAmmo);
}

void UQRWeaponComponent::ClearJam()
{
	if (bIsJammed || WeaponState == EQRWeaponState::Jammed)
	{
		bIsJammed     = false;
		WeaponState   = CurrentAmmo > 0 ? EQRWeaponState::Ready : EQRWeaponState::Empty;
		// Jam clearing physically removes debris — small fouling addition from the action
		FoulingFactor = FMath::Clamp(FoulingFactor + 0.05f, 0.0f, 1.0f);
	}
}

void UQRWeaponComponent::Clean()
{
	FoulingFactor = 0.0f;
	// Cleaning also clears any pre-jam condition (does not clear active jam — use ClearJam first)
}
