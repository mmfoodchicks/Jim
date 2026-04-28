#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRTypes.h"
#include "QRWeaponComponent.generated.h"

class UQRItemInstance;

UENUM(BlueprintType)
enum class EQRWeaponType : uint8
{
	Melee       UMETA(DisplayName = "Melee"),
	Ranged      UMETA(DisplayName = "Ranged"),
	Thrown      UMETA(DisplayName = "Thrown"),
	Improvised  UMETA(DisplayName = "Improvised"),
};

UENUM(BlueprintType)
enum class EQRWeaponState : uint8
{
	Holstered   UMETA(DisplayName = "Holstered"),
	Ready       UMETA(DisplayName = "Ready"),
	Firing      UMETA(DisplayName = "Firing"),
	Reloading   UMETA(DisplayName = "Reloading"),
	Jammed      UMETA(DisplayName = "Jammed"),
	Empty       UMETA(DisplayName = "Empty"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponFired,   AActor*, Target, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponJammed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponReloaded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAmmoChanged, int32, Remaining);

// Handles weapon logic: firing, jamming, fouling, noise generation, reloading
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QRCOMBATTHREAT_API UQRWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRWeaponComponent();

	// ── Config ───────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName WeaponItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	EQRWeaponType WeaponType = EQRWeaponType::Ranged;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float BaseDamage = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float EffectiveRangeMeters = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	int32 MagazineCapacity = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float ReloadTimeSeconds = 3.0f;

	// Noise radius in meters when fired (affects wildlife flee and enemy detection)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float FireNoiseRadiusMeters = 200.0f;

	// Jamming probability per shot when fouling is high (0..1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float BaseJamChance = 0.02f;

	// Durability damage per shot
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float WearPerShot = 0.5f;

	// ── Runtime State ─────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Weapon")
	EQRWeaponState WeaponState = EQRWeaponState::Holstered;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Weapon")
	int32 CurrentAmmo = 0;

	// Fouling factor [0..1] — increases jam chance over time
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Weapon")
	float FoulingFactor = 0.0f;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponFired OnWeaponFired;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponJammed OnWeaponJammed;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponReloaded OnWeaponReloaded;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnAmmoChanged OnAmmoChanged;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	bool TryFire(AActor* Target, UQRItemInstance* AmmoInstance);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	void BeginReload();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	void FinishReload(int32 NewAmmoCount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	void ClearJam();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	void Clean(); // Remove fouling

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool CanFire() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	float ComputeEffectiveDamage(float DistanceMeters) const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetJamChance() const { return FMath::Clamp(BaseJamChance + FoulingFactor * 0.2f, 0.0f, 0.8f); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
