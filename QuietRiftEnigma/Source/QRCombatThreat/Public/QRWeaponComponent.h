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

	// v1.17: fouling added per shot (canonical name from v1.17 patch)
	// Dirty/primitive ammo multiplies this by 5; suppressor multiplies by 1.5
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon",
		meta = (ClampMin = "0", ClampMax = "1"))
	float FoulingPerShot = 0.005f;

	// v1.17: multiplier applied to FoulingPerShot when dirty/improvised ammo is used
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float DirtyAmmoFoulingMult = 5.0f;

	// v1.17: additional fouling multiplier when a suppressor attachment is fitted
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float SuppressorFoulingMult = 1.5f;

	// v1.17: seconds required to clear a jam (ClearJam action duration)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon",
		meta = (ClampMin = "0"))
	float MalfunctionClearSeconds = 4.0f;

	// ── Runtime State ─────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Weapon")
	EQRWeaponState WeaponState = EQRWeaponState::Holstered;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Weapon")
	int32 CurrentAmmo = 0;

	// Fouling accumulator [0..1] — drives jam chance and accuracy penalty
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Weapon")
	float FoulingFactor = 0.0f;

	// v1.17: explicit jam flag, separate from WeaponState so Blueprints can query it directly
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Weapon")
	bool bIsJammed = false;

	// Set to true by the attachment system when a suppressor is fitted
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Weapon")
	bool bHasSuppressor = false;

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

	// v1.17: compute per-shot fouling increment accounting for ammo type and suppressor
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetFoulingIncrement(bool bIsDirtyAmmo, bool bHasSuppressor) const;

	// v1.17: True when jammed or state is Empty/Reloading
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReadyToFire() const { return !bIsJammed && WeaponState == EQRWeaponState::Ready && CurrentAmmo > 0; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
