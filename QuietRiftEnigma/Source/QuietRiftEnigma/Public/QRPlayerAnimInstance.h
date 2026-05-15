#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "QRPlayerAnimInstance.generated.h"

class AQRCharacter;
class UQRWeaponComponent;
class UQRFPViewComponent;

/**
 * Bridge between AQRCharacter's runtime state and an AnimBP state
 * machine. Designer builds ABP_QRPlayer with AnimInstance class set to
 * this and drags the public floats / bools into nodes (Speed, Direction,
 * IsFalling, IsCrouched, IsAiming, IsReloading, IsDead, WeaponAlpha).
 *
 * NativeUpdateAnimation polls the owning character + components each
 * frame and writes the values; no events, all push-pull. Cheap.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// ── Locomotion ────────────────────────────
	UPROPERTY(BlueprintReadOnly, Category = "QR|Anim")
	float Speed = 0.0f;

	// Signed velocity along the facing direction in [-1..1] (for strafe
	// blend spaces).
	UPROPERTY(BlueprintReadOnly, Category = "QR|Anim")
	float Direction = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Anim")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Anim")
	bool bIsCrouched = false;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Anim")
	bool bIsSprinting = false;

	// ── Combat ────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Category = "QR|Anim")
	bool bIsAiming = false;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Anim")
	bool bIsReloading = false;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Anim")
	bool bIsJammed = false;

	// 0 = unarmed, 1 = holding a ranged weapon. Designer can branch
	// blend spaces on this for armed-vs-unarmed locomotion.
	UPROPERTY(BlueprintReadOnly, Category = "QR|Anim")
	float WeaponAlpha = 0.0f;

	// ── Survival ──────────────────────────────
	UPROPERTY(BlueprintReadOnly, Category = "QR|Anim")
	bool bIsDead = false;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Anim")
	float HealthPct = 1.0f;

	// ── View ──────────────────────────────────
	// Lean factor [-1..1] for additive lean poses.
	UPROPERTY(BlueprintReadOnly, Category = "QR|Anim")
	float LeanAlpha = 0.0f;

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AQRCharacter> OwnerCharacter = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UQRWeaponComponent> CachedWeapon = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UQRFPViewComponent> CachedView = nullptr;
};
