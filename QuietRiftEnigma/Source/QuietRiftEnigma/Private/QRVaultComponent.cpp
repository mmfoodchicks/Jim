#include "QRVaultComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UQRVaultComponent::UQRVaultComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UQRVaultComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ACharacter>(GetOwner());
}

bool UQRVaultComponent::IsOnCooldown() const
{
	const UWorld* W = GetWorld();
	if (!W) return false;
	return (W->GetTimeSeconds() - LastVaultTimeSeconds) < VaultCooldown;
}

bool UQRVaultComponent::TryVault()
{
	if (!OwnerCharacter || IsOnCooldown()) return false;

	UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	if (!Move) return false;

	// Falling/swimming/flying — don't vault.
	if (Move->IsFalling() || Move->IsSwimming() || Move->IsFlying()) return false;

	if (bBlockWhileSprinting)
	{
		const FBoolProperty* Prop = FindFProperty<FBoolProperty>(OwnerCharacter->GetClass(), TEXT("bIsSprinting"));
		if (Prop && Prop->GetPropertyValue_InContainer(OwnerCharacter)) return false;
	}

	FVector LandSpot;
	if (!DetectVault(LandSpot)) return false;

	const FVector Forward = OwnerCharacter->GetActorForwardVector();
	const FVector LaunchVel = Forward * VaultLaunchForward + FVector(0, 0, VaultLaunchUp);
	OwnerCharacter->LaunchCharacter(LaunchVel, /*bXYOverride*/ true, /*bZOverride*/ true);

	LastVaultTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	OnVaultStarted.Broadcast();
	return true;
}

bool UQRVaultComponent::DetectVault(FVector& OutLandSpot) const
{
	UWorld* World = GetWorld();
	if (!World || !OwnerCharacter) return false;

	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if (!Capsule) return false;

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius     = Capsule->GetScaledCapsuleRadius();
	const FVector ActorLoc = OwnerCharacter->GetActorLocation();
	// Feet Z = actor center - half height (since capsule center is actor location).
	const float FeetZ      = ActorLoc.Z - HalfHeight;
	const FVector Forward  = OwnerCharacter->GetActorForwardVector();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(QRVaultDetect), false);
	Params.AddIgnoredActor(OwnerCharacter);

	// ── Step 1: forward trace at chest height ──────────
	const FVector ChestStart = ActorLoc; // capsule center ≈ chest
	const FVector ChestEnd   = ChestStart + Forward * VaultForwardReach;

	FHitResult ChestHit;
	const bool bHitChest = World->LineTraceSingleByChannel(
		ChestHit, ChestStart, ChestEnd, ECC_Visibility, Params);

	if (bDrawDebug)
	{
		DrawDebugLine(World, ChestStart, ChestEnd,
			bHitChest ? FColor::Red : FColor::Green, false, 1.5f, 0, 1.0f);
	}
	if (!bHitChest) return false;

	// ── Step 2: trace down from above the hit to find obstacle top ──
	const FVector HitPoint = ChestHit.ImpactPoint;
	// Push the start a little past the face so we hit the TOP not the front.
	const FVector OverheadStart(
		HitPoint.X + Forward.X * (Radius + 4.0f),
		HitPoint.Y + Forward.Y * (Radius + 4.0f),
		FeetZ + VaultMaxObstacleHeight + 10.0f);
	const FVector OverheadEnd(OverheadStart.X, OverheadStart.Y, FeetZ);

	FHitResult TopHit;
	const bool bHitTop = World->LineTraceSingleByChannel(
		TopHit, OverheadStart, OverheadEnd, ECC_Visibility, Params);

	if (bDrawDebug)
	{
		DrawDebugLine(World, OverheadStart, OverheadEnd,
			bHitTop ? FColor::Yellow : FColor::Magenta, false, 1.5f, 0, 1.0f);
	}
	if (!bHitTop) return false; // open void past the wall — too tall to vault

	const float ObstacleHeight = TopHit.ImpactPoint.Z - FeetZ;
	if (ObstacleHeight < VaultMinObstacleHeight) return false;
	if (ObstacleHeight > VaultMaxObstacleHeight) return false;

	// ── Step 3: capsule sweep at landing spot to confirm we fit ──────
	const FVector LandSpot(
		TopHit.ImpactPoint.X + Forward.X * VaultLandOffsetForward,
		TopHit.ImpactPoint.Y + Forward.Y * VaultLandOffsetForward,
		TopHit.ImpactPoint.Z + HalfHeight + 2.0f);

	FCollisionShape CapShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FHitResult ClearHit;
	const bool bBlocked = World->SweepSingleByChannel(
		ClearHit, LandSpot, LandSpot + FVector(0, 0, 1), FQuat::Identity,
		ECC_Visibility, CapShape, Params);

	if (bDrawDebug)
	{
		DrawDebugCapsule(World, LandSpot, HalfHeight, Radius, FQuat::Identity,
			bBlocked ? FColor::Red : FColor::Green, false, 1.5f, 0, 1.0f);
	}
	if (bBlocked) return false;

	// Also need a bit of vertical clearance above the obstacle top.
	if (VaultRequiredClearance > 0.0f)
	{
		const FVector ClearStart(TopHit.ImpactPoint.X, TopHit.ImpactPoint.Y,
			TopHit.ImpactPoint.Z + 2.0f);
		const FVector ClearEnd(ClearStart.X, ClearStart.Y, ClearStart.Z + VaultRequiredClearance);
		FHitResult OverheadObstruction;
		if (World->LineTraceSingleByChannel(OverheadObstruction, ClearStart, ClearEnd,
			ECC_Visibility, Params))
		{
			return false;
		}
	}

	OutLandSpot = LandSpot;
	return true;
}
