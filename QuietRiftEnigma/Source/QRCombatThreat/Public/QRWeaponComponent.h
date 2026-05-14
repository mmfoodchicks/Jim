#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRTypes.h"
#include "QRWeaponComponent.generated.h"

class UQRItemInstance;
class UNiagaraSystem;
class USoundBase;

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

// Result of a trace-fire pass. Tells the firer how to apply the recoil.
USTRUCT(BlueprintType)
struct QRCOMBATTHREAT_API FQRFireResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bFired = false;
	UPROPERTY(BlueprintReadOnly) bool bHitSomething = false;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> HitActor = nullptr;
	UPROPERTY(BlueprintReadOnly) FVector HitLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector HitNormal = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float Damage = 0.0f;
	UPROPERTY(BlueprintReadOnly) float DistanceMeters = 0.0f;

	// How much pitch / yaw to apply to the firer's view as kick.
	UPROPERTY(BlueprintReadOnly) float RecoilPitch = 0.0f;
	UPROPERTY(BlueprintReadOnly) float RecoilYaw = 0.0f;
};

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

	// ── Spread + recoil (trace-fire tuning) ─
	// Base cone half-angle in degrees applied to every shot. Aim-down-
	// sights cuts spread; hip-fire / moving / fouling multiply it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Aiming",
		meta = (ClampMin = "0", ClampMax = "30"))
	float BaseSpreadDegrees = 1.0f;

	// Spread multiplier when NOT aiming down sights (hip fire).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Aiming",
		meta = (ClampMin = "1", ClampMax = "10"))
	float HipFireSpreadMult = 3.0f;

	// Additional spread multiplier while the firer is moving.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Aiming",
		meta = (ClampMin = "1", ClampMax = "5"))
	float MovingSpreadMult = 1.8f;

	// Additional spread multiplier at full fouling (FoulingFactor == 1.0).
	// Linearly interpolated between 1.0 and this value by FoulingFactor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Aiming",
		meta = (ClampMin = "1", ClampMax = "5"))
	float FoulingSpreadMult = 2.0f;

	// Trace range in meters before the shot is considered a miss into
	// the void. 500m covers all in-game engagement distances; raise for
	// the long-range sniper, lower for shotgun.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Aiming",
		meta = (ClampMin = "10", ClampMax = "2000"))
	float MaxRangeMeters = 500.0f;

	// Recoil applied to the firer's controller (degrees).
	// Pitch is always up. Yaw is randomly +/-.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Aiming",
		meta = (ClampMin = "0", ClampMax = "15"))
	float RecoilPitch = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Aiming",
		meta = (ClampMin = "0", ClampMax = "15"))
	float RecoilYawRandomRange = 0.5f;

	// ── FX ───────────────────────────────────
	// Spawned at the muzzle on each successful fire. Defaults to the
	// NiagaraExamples MuzzleFlash if the Fab pack is present; designer
	// can override per-weapon.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX")
	TObjectPtr<UNiagaraSystem> MuzzleFlashFX;

	// Spawned at the trace's impact point on a hit, oriented to the hit
	// normal. NS_Impact_Metal by default — surface-specific impacts
	// (concrete / wood / glass) can be wired by inspecting the hit
	// physical material later.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX")
	TObjectPtr<UNiagaraSystem> ImpactFX;

	// Optional tracer ribbon spawned from muzzle toward the hit point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX")
	TObjectPtr<UNiagaraSystem> TracerFX;

	// Socket name on the held weapon mesh used as the muzzle origin.
	// If empty, the trace's start point is used as a fallback.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX")
	FName MuzzleSocketName;

	// Fire SFX played at the muzzle. Defaults to the Free_Sounds_Pack
	// Gunshot_1-1; designer can override per-weapon.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX")
	TObjectPtr<USoundBase> FireSound;

	// Volume multiplier on FireSound (1.0 = pack default).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX",
		meta = (ClampMin = "0", ClampMax = "4"))
	float FireSoundVolume = 1.0f;

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

	// Trace-fire from a ray. Used by the player's FP camera path —
	// caller passes the camera's location + forward, and we run a
	// line trace through the world with spread offset applied, then
	// (on hit) call the standard TryFire path with the hit actor.
	// Returns a result struct with hit info and recoil deltas the
	// caller should apply to its controller.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	FQRFireResult TryFireFromTrace(FVector TraceStart, FVector TraceForward,
		bool bIsAimed, bool bIsMoving, UQRItemInstance* AmmoInstance);

	// Compute the current spread cone half-angle in degrees, taking ADS,
	// movement, and fouling into account.
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetEffectiveSpreadDegrees(bool bIsAimed, bool bIsMoving) const;

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
	float GetFoulingIncrement(bool bIsDirtyAmmo, bool bUseSuppressor) const;

	// v1.17: True when jammed or state is Empty/Reloading
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReadyToFire() const { return !bIsJammed && WeaponState == EQRWeaponState::Ready && CurrentAmmo > 0; }

	// Plays the muzzle / impact / tracer FX on every client. Called from
	// TryFireFromTrace on the authority. Unreliable — losing one cosmetic
	// burst is fine.
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFireFX(FVector MuzzleLoc, FVector HitLoc, FVector HitNormal, bool bHit);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
