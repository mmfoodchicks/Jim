#include "QRWeaponComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "QRSurvivalComponent.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"

UQRWeaponComponent::UQRWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// FX assets (MuzzleFlashFX / ImpactFX / TracerFX / FireSound) intentionally
	// default to nullptr -- the firing code null-checks every use. Wire them
	// per-weapon in BP defaults, or set them at runtime via LoadObject.
	//
	// Previous behaviour used ConstructorHelpers::FObjectFinder against
	// /Game/Fabs/NiagaraExamples/... Those Niagara systems have internal
	// references to /Game/NiagaraExamples/... (without /Fabs/) which don't
	// exist in this checkout, and FObjectFinder logs LoadErrors warnings
	// for every missing dependent package at CDO load time. Skipping the
	// auto-wire silences that wall of noise without losing any function:
	// when the assets exist the BP can wire them, and when they don't the
	// firing code already degrades gracefully.
}

void UQRWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRWeaponComponent, WeaponState);
	DOREPLIFETIME(UQRWeaponComponent, CurrentAmmo);
	DOREPLIFETIME(UQRWeaponComponent, FoulingFactor);
	DOREPLIFETIME(UQRWeaponComponent, bIsJammed);
	DOREPLIFETIME(UQRWeaponComponent, bHasSuppressor);
	DOREPLIFETIME(UQRWeaponComponent, FireMode);
	DOREPLIFETIME(UQRWeaponComponent, RoundsPerMinute);
	DOREPLIFETIME(UQRWeaponComponent, bUnlimitedAmmo);
}

bool UQRWeaponComponent::CanFire() const
{
	if (bIsJammed) return false;
	if (WeaponState == EQRWeaponState::Holstered || WeaponState == EQRWeaponState::Reloading)
		return false;
	// Unlimited-ammo (testing) ignores the magazine entirely.
	if (!bUnlimitedAmmo && CurrentAmmo <= 0) return false;
	return true;
}

bool UQRWeaponComponent::IsFireCadenceReady() const
{
	const UWorld* W = GetWorld();
	if (!W) return true;
	return (W->GetTimeSeconds() - LastFireTimeSeconds) >= GetFireIntervalSeconds();
}

void UQRWeaponComponent::ConfigureForWeaponId(FName WeaponId)
{
	WeaponItemId = WeaponId;
	const FString S = WeaponId.ToString().ToUpper();

	auto Apply = [this](EQRFireMode Mode, float Rpm)
	{
		FireMode = Mode;
		RoundsPerMinute = Rpm;
	};

	// Name-based, matching DT_ArmoryWeapons RPM values. Order matters:
	// check the more specific tokens before generic ones.
	if      (S.Contains(TEXT("SMG")))        Apply(EQRFireMode::FullAuto,   750.0f);
	else if (S.Contains(TEXT("CARBINE")))    Apply(EQRFireMode::FullAuto,   650.0f);
	else if (S.Contains(TEXT("LONGRANGE")))  Apply(EQRFireMode::SingleShot,  35.0f);
	else if (S.Contains(TEXT("BOLT")))       Apply(EQRFireMode::SingleShot,  40.0f);
	else if (S.Contains(TEXT("PUMP")) || S.Contains(TEXT("SHOTGUN")))
	                                         Apply(EQRFireMode::SingleShot,  90.0f);
	else if (S.Contains(TEXT("DMR")))        Apply(EQRFireMode::SemiAuto,   240.0f);
	else if (S.Contains(TEXT("SNIPER")))     Apply(EQRFireMode::SingleShot,  40.0f);
	else if (S.Contains(TEXT("PISTOL")))     Apply(EQRFireMode::SemiAuto,   360.0f);
	else                                     Apply(EQRFireMode::SemiAuto,   360.0f);
}

float UQRWeaponComponent::GetFoulingIncrement(bool bIsDirtyAmmo, bool bUseSuppressor) const
{
	float Inc = FoulingPerShot;
	if (bIsDirtyAmmo)   Inc *= DirtyAmmoFoulingMult;
	if (bUseSuppressor) Inc *= SuppressorFoulingMult;
	return FMath::Clamp(Inc, 0.0f, 1.0f);
}

bool UQRWeaponComponent::TryFire(AActor* Target, UQRItemInstance* AmmoInstance)
{
	if (!CanFire()) return false;

	// Rate-of-fire gate. Authoritative pacing for every fire mode — a
	// full-auto weapon held down only lands shots at its RPM, and a
	// bolt/pump gun is forced to wait out its (slow) cycle delay before
	// the next round. Blocks spam-clicking past the cyclic rate too.
	if (!IsFireCadenceReady()) return false;
	if (const UWorld* W = GetWorld())
	{
		LastFireTimeSeconds = W->GetTimeSeconds();
	}

	// Determine ammo quality for fouling calculation
	const bool bIsDirtyAmmo = AmmoInstance && AmmoInstance->Definition &&
		AmmoInstance->Definition->ItemTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Ammo.Dirty")));

	// Check jam before firing. Skipped entirely in unlimited-ammo (test)
	// mode so the gun never jams to a stop on the range -- the prior
	// behaviour accumulated fouling -> rising jam chance -> a permanent
	// Jammed state that read as "the gun stopped working".
	if (!bUnlimitedAmmo && FMath::FRand() < GetJamChance())
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
		{
			Survival->ApplyDamage(Damage, EQRInjuryType::Bleeding);
		}
		else
		{
			// No survival component (e.g. wildlife, which model health on
			// AQRWildlifeBase). Route through the engine damage pipeline so
			// the target's TakeDamage override handles it. Keeps this lower
			// module free of any game-module type dependency.
			AController* InstigatorController = nullptr;
			if (AActor* MyOwner = GetOwner())
			{
				InstigatorController = MyOwner->GetInstigatorController();
			}
			UGameplayStatics::ApplyDamage(Target, Damage, InstigatorController, GetOwner(), nullptr);
		}
	}

	// v1.17: canonical fouling increment (dirty ammo ×5, suppressor ×1.5).
	// Unlimited-ammo (test) mode keeps the bore clean so accuracy + jam
	// chance never degrade during a long range session.
	if (!bUnlimitedAmmo)
	{
		FoulingFactor = FMath::Clamp(FoulingFactor + GetFoulingIncrement(bIsDirtyAmmo, bHasSuppressor), 0.0f, 1.0f);

		--CurrentAmmo;
		if (CurrentAmmo <= 0)
			WeaponState = EQRWeaponState::Empty;
	}

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
	// Cannot reload while jammed (WeaponState check alone is insufficient — bIsJammed can be
	// true while state is Empty, which would let FinishReload silently set state=Ready while
	// the jam persists, creating a soft-lock where CanFire() is permanently false).
	if (bIsJammed) return;
	if (WeaponState == EQRWeaponState::Firing || WeaponState == EQRWeaponState::Reloading) return;
	WeaponState = EQRWeaponState::Reloading;

	// Complete the reload after ReloadTimeSeconds. A reload-animation
	// notify may call FinishReload sooner; if so this timer fires into a
	// no-op, since FinishReload only acts while the state is Reloading.
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(ReloadTimerHandle, this,
			&UQRWeaponComponent::HandleReloadTimerElapsed,
			FMath::Max(0.05f, ReloadTimeSeconds), false);
	}
	else
	{
		HandleReloadTimerElapsed();
	}
}

void UQRWeaponComponent::HandleReloadTimerElapsed()
{
	// Refill the magazine to capacity. Pulling rounds from the player's
	// inventory and enforcing per-magazine ammo types is the upcoming
	// magazine system — for now the reload simply tops the mag off.
	FinishReload(MagazineCapacity);
}

void UQRWeaponComponent::FinishReload(int32 NewAmmoCount)
{
	// Only accept FinishReload if we actually initiated a reload — prevents ammo refill exploit
	if (WeaponState != EQRWeaponState::Reloading) return;

	// Clamp to [0, MagazineCapacity]; negative NewAmmoCount would otherwise soft-lock CanFire()
	CurrentAmmo = FMath::Clamp(NewAmmoCount, 0, MagazineCapacity);
	WeaponState = CurrentAmmo > 0 ? EQRWeaponState::Ready : EQRWeaponState::Empty;
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

float UQRWeaponComponent::GetEffectiveSpreadDegrees(bool bIsAimed, bool bIsMoving) const
{
	float Spread = BaseSpreadDegrees;
	if (!bIsAimed) Spread *= HipFireSpreadMult;
	if (bIsMoving) Spread *= MovingSpreadMult;
	// Fouling lerps spread up to FoulingSpreadMult at full fouling.
	const float FoulMult = FMath::Lerp(1.0f, FoulingSpreadMult, FoulingFactor);
	Spread *= FoulMult;
	return Spread;
}

FQRFireResult UQRWeaponComponent::TryFireFromTrace(FVector TraceStart, FVector TraceForward,
	bool bIsAimed, bool bIsMoving, UQRItemInstance* AmmoInstance)
{
	FQRFireResult Result;
	if (!CanFire()) return Result;

	UWorld* W = GetWorld();
	if (!W) return Result;
	if (!TraceForward.Normalize()) return Result;

	// Compute spread-adjusted direction. RandPointInCircle-like approach
	// using a small uniform offset in the plane perpendicular to forward.
	const float SpreadDeg = GetEffectiveSpreadDegrees(bIsAimed, bIsMoving);
	const float SpreadRad = FMath::DegreesToRadians(SpreadDeg);
	// Sample a random direction inside the spread cone.
	const float Theta = FMath::FRandRange(0.0f, 2.0f * PI);
	// sin(spread) for the cone radius at a unit distance, then a random
	// 0..1 sqrt-distributed magnitude so spread is uniform over area.
	const float Magnitude = FMath::Sqrt(FMath::FRand()) * FMath::Tan(SpreadRad);
	// Build orthonormal basis around TraceForward.
	FVector Right = FVector::CrossProduct(FVector::UpVector, TraceForward);
	if (!Right.Normalize()) Right = FVector::RightVector;
	const FVector Up = FVector::CrossProduct(TraceForward, Right);
	const FVector Offset = (Right * FMath::Cos(Theta) + Up * FMath::Sin(Theta)) * Magnitude;
	const FVector FinalDir = (TraceForward + Offset).GetSafeNormal();

	const float RangeCm = MaxRangeMeters * 100.0f;
	const FVector TraceEnd = TraceStart + FinalDir * RangeCm;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(QRWeaponFire), /*bTraceComplex*/ false);
	// Ignore the weapon's owner so the player doesn't shoot themselves.
	if (AActor* MyOwner = GetOwner()) Params.AddIgnoredActor(MyOwner);
	const bool bHit = W->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);

	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;

	// Delegate to the standard TryFire path for damage / fouling / state
	// transitions. We pass HitActor so it applies damage via the survival
	// component as it already does; if HitActor is null this is just a
	// "fire into the air" which still consumes ammo + ages the weapon.
	const bool bFired = TryFire(HitActor, AmmoInstance);

	Result.bFired = bFired;
	if (!bFired)
	{
		// TryFire might have produced a jam — recoil shouldn't apply then.
		return Result;
	}

	Result.bHitSomething = bHit;
	Result.HitActor      = HitActor;
	Result.HitLocation   = bHit ? Hit.ImpactPoint  : TraceEnd;
	Result.HitNormal     = bHit ? Hit.ImpactNormal : -FinalDir;
	Result.DistanceMeters = bHit ? (Hit.Distance / 100.0f) : MaxRangeMeters;
	Result.Damage         = ComputeEffectiveDamage(Result.DistanceMeters);

	// Recoil — aimed shots get reduced kick. Standard FPS feel.
	const float AimMult = bIsAimed ? 0.5f : 1.0f;
	Result.RecoilPitch = RecoilPitch * AimMult;
	Result.RecoilYaw   = FMath::FRandRange(-RecoilYawRandomRange, RecoilYawRandomRange) * AimMult;

	// Replicated cosmetic FX. Muzzle origin defaults to TraceStart — a
	// real socket lookup belongs once the held weapon mesh carries a
	// "Muzzle" socket; until then this anchors the flash to the camera.
	Multicast_PlayFireFX(TraceStart, Result.HitLocation, Result.HitNormal, bHit);

	return Result;
}

void UQRWeaponComponent::Multicast_PlayFireFX_Implementation(
	FVector MuzzleLoc, FVector HitLoc, FVector HitNormal, bool bHit)
{
	UWorld* W = GetWorld();
	if (!W) return;

	// Muzzle flash. Prefer attaching to the owner's mesh at MuzzleSocketName
	// when both are present so the flash follows weapon motion; otherwise
	// just spawn at the supplied muzzle location.
	if (MuzzleFlashFX)
	{
		USceneComponent* AttachMesh = nullptr;
		if (AActor* OwnerActor = GetOwner())
		{
			AttachMesh = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
			if (!AttachMesh) AttachMesh = OwnerActor->FindComponentByClass<UStaticMeshComponent>();
		}

		if (AttachMesh && MuzzleSocketName != NAME_None && AttachMesh->DoesSocketExist(MuzzleSocketName))
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(
				MuzzleFlashFX, AttachMesh, MuzzleSocketName,
				FVector::ZeroVector, FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget, /*bAutoDestroy*/ true);
		}
		else
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				W, MuzzleFlashFX, MuzzleLoc, FRotator::ZeroRotator);
		}
	}

	// Impact FX at the hit point, oriented to the hit normal.
	if (bHit && ImpactFX)
	{
		const FRotator ImpactRot = HitNormal.Rotation();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(W, ImpactFX, HitLoc, ImpactRot);
	}

	// Tracer ribbon from muzzle to hit point (or trace end if miss).
	if (TracerFX)
	{
		const FVector Delta = HitLoc - MuzzleLoc;
		const FRotator TracerRot = Delta.Rotation();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(W, TracerFX, MuzzleLoc, TracerRot);
	}

	// Weapon fire SFX at the muzzle. If no FireSound is wired on this
	// weapon, lazy-load the bundled Gunshot cue on first fire and cache
	// it process-wide. This replaces the prior CDO-time auto-wire
	// (removed because the Niagara siblings cascaded into LoadErrors)
	// while keeping the audio default working when the Fab pack is
	// present. LoadObject simply returns null when the asset is missing
	// and we play nothing in that case.
	USoundBase* SoundToPlay = FireSound;
	if (!SoundToPlay)
	{
		static USoundBase* CachedDefault = nullptr;
		static bool bTriedLoad = false;
		if (!bTriedLoad)
		{
			bTriedLoad = true;
			CachedDefault = LoadObject<USoundBase>(nullptr,
				TEXT("/Game/Fabs/Free_Sounds_Pack/cue/Gunshot_1-1_Cue.Gunshot_1-1_Cue"));
		}
		SoundToPlay = CachedDefault;
	}
	if (SoundToPlay)
	{
		UGameplayStatics::PlaySoundAtLocation(W, SoundToPlay, MuzzleLoc, FireSoundVolume);
	}
}
