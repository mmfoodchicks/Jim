#include "QRWeaponComponent.h"
#include "QRItemInstance.h"
#include "QRSurvivalComponent.h"
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
}

bool UQRWeaponComponent::CanFire() const
{
	return WeaponState == EQRWeaponState::Ready && CurrentAmmo > 0;
}

bool UQRWeaponComponent::TryFire(AActor* Target, UQRItemInstance* AmmoInstance)
{
	if (!CanFire()) return false;

	// Check jam
	if (FMath::FRand() < GetJamChance())
	{
		WeaponState = EQRWeaponState::Jammed;
		OnWeaponJammed.Broadcast();
		return false;
	}

	float Distance = 0.0f;
	if (Target && GetOwner())
		Distance = FVector::Dist(GetOwner()->GetActorLocation(), Target->GetActorLocation()) / 100.0f;

	float Damage = ComputeEffectiveDamage(Distance);

	// Apply damage to target's survival component
	if (Target)
	{
		if (UQRSurvivalComponent* Survival = Target->FindComponentByClass<UQRSurvivalComponent>())
			Survival->ApplyDamage(Damage, EQRInjuryType::Bleeding);
	}

	--CurrentAmmo;
	FoulingFactor = FMath::Min(1.0f, FoulingFactor + 0.01f);

	if (CurrentAmmo <= 0)
		WeaponState = EQRWeaponState::Empty;

	OnWeaponFired.Broadcast(Target, Damage);
	OnAmmoChanged.Broadcast(CurrentAmmo);

	// TODO: Broadcast noise event to nearby threat detection systems
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
	if (WeaponState == EQRWeaponState::Jammed)
	{
		WeaponState   = CurrentAmmo > 0 ? EQRWeaponState::Ready : EQRWeaponState::Empty;
		FoulingFactor = FMath::Min(1.0f, FoulingFactor + 0.05f); // Jam-clearing adds fouling
	}
}

void UQRWeaponComponent::Clean()
{
	FoulingFactor = 0.0f;
}
