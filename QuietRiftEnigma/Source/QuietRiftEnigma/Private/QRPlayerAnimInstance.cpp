#include "QRPlayerAnimInstance.h"
#include "QRCharacter.h"
#include "QRSurvivalComponent.h"
#include "QRWeaponComponent.h"
#include "QRFPViewComponent.h"
#include "QRItemDefinition.h"
#include "QRItemInstance.h"
#include "QRInventoryComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void UQRPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	OwnerCharacter = Cast<AQRCharacter>(TryGetPawnOwner());
}

void UQRPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerCharacter)
	{
		OwnerCharacter = Cast<AQRCharacter>(TryGetPawnOwner());
		if (!OwnerCharacter) return;
	}

	if (!CachedWeapon) CachedWeapon = OwnerCharacter->FindComponentByClass<UQRWeaponComponent>();
	if (!CachedView)   CachedView   = OwnerCharacter->FindComponentByClass<UQRFPViewComponent>();

	// Locomotion ─────────────────────────────────
	FVector Vel = OwnerCharacter->GetVelocity();
	Vel.Z = 0.0f;
	Speed = Vel.Size();

	const FRotator Facing = OwnerCharacter->GetActorRotation();
	const FVector  Forward = FRotationMatrix(Facing).GetUnitAxis(EAxis::X);
	const FVector  VelDir  = Vel.GetSafeNormal();
	const float    Dot     = FVector::DotProduct(Forward, VelDir);
	// Signed: positive = forward, negative = backward. Strafe contributes
	// via the side cross product.
	const float    Cross   = FVector::CrossProduct(Forward, VelDir).Z;
	Direction = (Speed > 1.0f) ? FMath::Clamp(Dot * FMath::Sign(Cross != 0.0f ? Cross : 1.0f), -1.0f, 1.0f) : 0.0f;

	if (UCharacterMovementComponent* CMC = OwnerCharacter->GetCharacterMovement())
	{
		bIsFalling  = CMC->IsFalling();
		bIsCrouched = CMC->IsCrouching();
	}
	bIsSprinting = OwnerCharacter->bIsSprinting;

	// Combat ─────────────────────────────────────
	if (CachedWeapon)
	{
		bIsReloading = (CachedWeapon->WeaponState == EQRWeaponState::Reloading);
		bIsJammed    = CachedWeapon->bIsJammed;
	}
	else
	{
		bIsReloading = false;
		bIsJammed    = false;
	}

	bIsAiming = CachedView ? CachedView->IsADS() : false;

	// WeaponAlpha — 1 if the player is actually holding a ranged weapon.
	WeaponAlpha = 0.0f;
	if (OwnerCharacter->Inventory && OwnerCharacter->Inventory->HandSlot)
	{
		const UQRItemInstance*  Held = OwnerCharacter->Inventory->HandSlot;
		const UQRItemDefinition* Def = Held ? Held->Definition : nullptr;
		if (Def && Def->Category == EQRItemCategory::Weapon)
		{
			WeaponAlpha = 1.0f;
		}
	}

	// Survival ───────────────────────────────────
	if (UQRSurvivalComponent* Surv = OwnerCharacter->Survival)
	{
		bIsDead   = Surv->bIsDead;
		HealthPct = (Surv->MaxHealth > 0.0f) ? FMath::Clamp(Surv->Health / Surv->MaxHealth, 0.0f, 1.0f) : 0.0f;
	}

	// View ───────────────────────────────────────
	LeanAlpha = CachedView ? CachedView->GetCurrentLean() : 0.0f;
}
