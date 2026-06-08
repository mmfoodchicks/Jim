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
#include "Engine/DamageEvents.h"

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
	DOREPLIFETIME(UQRWeaponComponent, PelletsPerShot);
	DOREPLIFETIME(UQRWeaponComponent, PelletConeDegrees);
	DOREPLIFETIME(UQRWeaponComponent, bIsPrecisionWeapon);
	DOREPLIFETIME(UQRWeaponComponent, MeleeSweepRadius);
	DOREPLIFETIME(UQRWeaponComponent, WeaponType);
	DOREPLIFETIME(UQRWeaponComponent, bIsShield);
	DOREPLIFETIME(UQRWeaponComponent, ShieldDamageReduction);
	DOREPLIFETIME(UQRWeaponComponent, ShieldMaxHP);
	DOREPLIFETIME(UQRWeaponComponent, EquippedAmmoItemId);
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

	// Reset per-weapon fields by default; the branch below sets them.
	PelletsPerShot = 1;
	PelletConeDegrees = 0.0f;
	bIsPrecisionWeapon = false;
	MeleeSweepRadius = 0.0f;
	bIsShield = false;
	ShieldDamageReduction = 0.0f;
	ShieldMaxHP = 0.0f;
	WeaponType = EQRWeaponType::Ranged;

	// One place to set every per-weapon stat. EffRange = the distance out
	// to which damage is full; past it ComputeEffectiveDamage falls off.
	// Snipers/DMR get huge EffRange so there's no falloff at normal combat
	// distances (only "mega far"). Recoil is the per-shot view kick.
	auto Apply = [this](EQRFireMode Mode, float Rpm, float Dmg,
	                    float EffRangeM, float MaxRangeM, float Recoil)
	{
		FireMode = Mode;
		RoundsPerMinute = Rpm;
		BaseDamage = Dmg;
		EffectiveRangeMeters = EffRangeM;
		MaxRangeMeters = MaxRangeM;
		RecoilPitch = Recoil;
	};

	// Melee helper: short-range sphere-sweep "swing", no falloff, no recoil.
	// RangeM is the reach; SwingRPM is the swing cadence; Sweep is the hit
	// forgiveness radius (cm).
	auto Melee = [this, &Apply](float Dmg, float RangeM, float SwingRPM, float Sweep)
	{
		Apply(EQRFireMode::SemiAuto, SwingRPM, Dmg, 9999.f, RangeM, 0.0f);
		WeaponType = EQRWeaponType::Melee;
		MeleeSweepRadius = Sweep;
	};

	// Exotic-metal damage multiplier, parsed from the id suffix. The
	// planet's metals (see EXOTIC_METALS_ARSENAL.md) ladder up a blade's
	// damage; the same metals feed armour + shields in the recipe data.
	auto MetalMult = [&S]() -> float
	{
		if (S.Contains(TEXT("REMNANT")) || S.Contains(TEXT("EXOTIC"))) return 2.0f;
		if (S.Contains(TEXT("SPARKSTONE")))                            return 1.55f;
		if (S.Contains(TEXT("FROSTSPARK")))                            return 1.40f;
		if (S.Contains(TEXT("SUNWIRE")))                               return 1.25f;
		if (S.Contains(TEXT("BLACKGLASS")))                            return 1.25f;
		if (S.Contains(TEXT("MAGNET")))                                return 1.15f;
		return 1.0f; // ferric / scrap / bone / unspecified
	};

	//        mode                    RPM    dmg  eff   max   recoil
	// ── Firearms ─────────────────────────────
	if      (S.Contains(TEXT("SMG")))
		Apply(EQRFireMode::FullAuto,  900.f, 28.f, 120.f, 300.f, 0.8f);
	else if (S.Contains(TEXT("CARBINE")))
		Apply(EQRFireMode::FullAuto,  500.f, 44.f, 200.f, 400.f, 1.4f);
	else if (S.Contains(TEXT("LONGRANGE")))
	{
		Apply(EQRFireMode::SingleShot, 35.f, 150.f, 1200.f, 2000.f, 6.5f);
		bIsPrecisionWeapon = true;
	}
	else if (S.Contains(TEXT("BOLT")) && S.Contains(TEXT("SNIPER")))
	{
		Apply(EQRFireMode::SingleShot, 40.f, 120.f, 900.f, 1500.f, 5.5f);
		bIsPrecisionWeapon = true;
	}
	else if (S.Contains(TEXT("PUMP")) || S.Contains(TEXT("SHOTGUN")))
	{
		Apply(EQRFireMode::SingleShot, 90.f, 12.f, 15.f, 60.f, 4.0f);
		PelletsPerShot = 8;
		PelletConeDegrees = 6.0f;
	}
	else if (S.Contains(TEXT("DMR")))
	{
		Apply(EQRFireMode::SemiAuto,  240.f, 72.f, 600.f, 1200.f, 3.0f);
		bIsPrecisionWeapon = true;
	}
	else if (S.Contains(TEXT("SNIPER")))
	{
		Apply(EQRFireMode::SingleShot, 40.f, 120.f, 900.f, 1500.f, 5.5f);
		bIsPrecisionWeapon = true;
	}
	else if (S.Contains(TEXT("PISTOL")))
		Apply(EQRFireMode::SemiAuto,  360.f, 38.f, 80.f, 200.f, 1.0f);

	// ── Bows / drawn ─────────────────────────
	// Single-shot, slow cadence (the "draw"), precise on ADS, hitscan MVP
	// (arrow-arc projectiles are a follow-up). Crossbow hits harder + slower.
	else if (S.Contains(TEXT("CROSSBOW")))
	{
		Apply(EQRFireMode::SingleShot, 50.f, 90.f, 120.f, 400.f, 2.5f);
		WeaponType = EQRWeaponType::Bow;
		bIsPrecisionWeapon = true;
	}
	else if (S.Contains(TEXT("BOW")))
	{
		Apply(EQRFireMode::SingleShot, 70.f, 55.f, 100.f, 350.f, 2.0f);
		WeaponType = EQRWeaponType::Bow;
		bIsPrecisionWeapon = true;
	}
	else if (S.Contains(TEXT("SLING")))
	{
		Apply(EQRFireMode::SingleShot, 80.f, 22.f, 40.f, 150.f, 1.5f);
		WeaponType = EQRWeaponType::Bow;
	}

	// ── Shields ──────────────────────────────
	else if (S.Contains(TEXT("SHIELD")))
	{
		WeaponType = EQRWeaponType::Shield;
		bIsShield = true;
		MaxRangeMeters = 2.0f;
		BaseDamage = 8.0f;             // a shove / bash if you "attack"
		RoundsPerMinute = 60.f;
		if (S.Contains(TEXT("PLASMA")) || S.Contains(TEXT("ENERGY")) || S.Contains(TEXT("REMNANT")))
		{
			ShieldDamageReduction = 0.92f;   // Halo-style energy shield
			ShieldMaxHP = 200.f;             // regenerating absorb pool (wiring TBD)
		}
		else if (S.Contains(TEXT("RIOT")))
			ShieldDamageReduction = 0.70f;   // ballistic riot shield
		else
			ShieldDamageReduction = 0.45f;   // wood / scrap buckler
	}

	// ── Melee: crude + metal-tier blades ─────
	else if (S.Contains(TEXT("DAGGER")) || S.Contains(TEXT("KNIFE")))
		Melee(28.f  * MetalMult(), 1.8f, 200.f, 18.f);
	else if (S.Contains(TEXT("MACHETE")))
		Melee(40.f  * MetalMult(), 2.0f, 150.f, 22.f);
	else if (S.Contains(TEXT("SWORD")))
		Melee(55.f  * MetalMult(), 2.4f, 110.f, 26.f);
	else if (S.Contains(TEXT("AXE")) && !S.Contains(TEXT("PICK")))
		Melee(70.f  * MetalMult(), 2.2f,  85.f, 24.f);
	else if (S.Contains(TEXT("HATCHET")))
		Melee(45.f  * MetalMult(), 1.9f, 130.f, 20.f);
	else if (S.Contains(TEXT("PICKAXE")) || S.Contains(TEXT("PICK")))
		Melee(38.f  * MetalMult(), 2.0f, 100.f, 20.f);   // dual-use tool/weapon
	else if (S.Contains(TEXT("SPEAR")) || S.Contains(TEXT("JAVELIN")))
		Melee(50.f  * MetalMult(), 3.2f, 100.f, 16.f);   // reach
	else if (S.Contains(TEXT("CLUB")) || S.Contains(TEXT("MACE")) || S.Contains(TEXT("MAUL")))
		Melee(60.f  * MetalMult(), 2.0f,  90.f, 28.f);
	else if (S.Contains(TEXT("FIST")) || S.Contains(TEXT("KNUCKLE")))
		Melee(18.f  * MetalMult(), 1.6f, 260.f, 16.f);

	else
		Apply(EQRFireMode::SemiAuto,  360.f, 30.f, 150.f, 300.f, 1.2f);
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

void UQRWeaponComponent::ResolveAmmoEffect(EQRInjuryType& OutInjury, float& OutDmgMult) const
{
	OutInjury = EQRInjuryType::Bleeding;
	OutDmgMult = 1.0f;
	if (EquippedAmmoItemId.IsNone()) return;

	const FString S = EquippedAmmoItemId.ToString().ToUpper();

	// Damage-payload arrows.
	if      (S.Contains(TEXT("POISON")))     { OutInjury = EQRInjuryType::Toxin;        OutDmgMult = 0.6f; }
	else if (S.Contains(TEXT("TOXIN")))      { OutInjury = EQRInjuryType::Toxin;        OutDmgMult = 0.6f; }
	else if (S.Contains(TEXT("TRANQ")) || S.Contains(TEXT("SLEEP")) || S.Contains(TEXT("SEDAT")))
	                                          { OutInjury = EQRInjuryType::Sedated;     OutDmgMult = 0.25f; }
	else if (S.Contains(TEXT("CRYO")) || S.Contains(TEXT("FROST")) || S.Contains(TEXT("ICE")))
	                                          { OutInjury = EQRInjuryType::Frostbite;   OutDmgMult = 0.7f; }
	else if (S.Contains(TEXT("EMP")) || S.Contains(TEXT("SHOCK")) || S.Contains(TEXT("ELEC")))
	                                          { OutInjury = EQRInjuryType::Shock;       OutDmgMult = 0.5f; }
	else if (S.Contains(TEXT("SMOKE")) || S.Contains(TEXT("GAS")))
	                                          { OutInjury = EQRInjuryType::Suffocation; OutDmgMult = 0.1f; }
	else if (S.Contains(TEXT("TRACKER")) || S.Contains(TEXT("MARK")))
	                                          { OutInjury = EQRInjuryType::Marked;      OutDmgMult = 0.1f; }
	else if (S.Contains(TEXT("FIRE")) || S.Contains(TEXT("INCEND")))
	                                          { OutInjury = EQRInjuryType::Burn;        OutDmgMult = 0.85f; }
	else if (S.Contains(TEXT("EXPLOS")) || S.Contains(TEXT("FRAG")))
	                                          { OutInjury = EQRInjuryType::Concussion;  OutDmgMult = 1.6f; }
	else if (S.Contains(TEXT("BLEED")) || S.Contains(TEXT("BROADHEAD")))
	                                          { OutInjury = EQRInjuryType::Bleeding;    OutDmgMult = 1.2f; }
	else if (S.Contains(TEXT("ARMOR")) || S.Contains(TEXT("PIERC")))
	                                          { OutInjury = EQRInjuryType::Bleeding;    OutDmgMult = 1.3f; }
}

void UQRWeaponComponent::ApplyPelletDamage(AActor* HitActor, const FHitResult& Hit)
{
	if (!HitActor) return;

	EQRInjuryType Injury;
	float AmmoMult;
	ResolveAmmoEffect(Injury, AmmoMult);

	const float DistanceMeters = Hit.Distance / 100.0f;
	const float Damage = ComputeEffectiveDamage(DistanceMeters) * AmmoMult;

	if (UQRSurvivalComponent* Survival = HitActor->FindComponentByClass<UQRSurvivalComponent>())
	{
		Survival->ApplyDamage(Damage, Injury);
	}
	else
	{
		AController* InstigatorController = nullptr;
		if (AActor* MyOwner = GetOwner())
		{
			InstigatorController = MyOwner->GetInstigatorController();
		}
		// Point damage so the hit LOCATION flows to the target's TakeDamage
		// override -- AQRWildlifeBase uses it to detect headshots / crit
		// zones. ShotDir is just the trace direction for impulse / FX.
		const FVector ShotDir = Hit.TraceEnd != Hit.TraceStart
			? (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal()
			: FVector::ForwardVector;
		UGameplayStatics::ApplyPointDamage(HitActor, Damage, ShotDir, Hit,
			InstigatorController, GetOwner(), nullptr);
	}
}

float UQRWeaponComponent::GetEffectiveSpreadDegrees(bool bIsAimed, bool bIsMoving) const
{
	// Precision weapons (DMR + the two snipers) are tack-drivers on ADS:
	// zero spread, no fouling effect, no movement penalty. The trade is
	// they still have the per-weapon slow RPM and a full hip-fire penalty
	// when not aimed, so they're only accurate at the cost of being slow
	// and requiring a stop-aim discipline.
	if (bIsPrecisionWeapon && bIsAimed)
	{
		return 0.0f;
	}

	float Spread = BaseSpreadDegrees;
	if (!bIsAimed) Spread *= HipFireSpreadMult;
	if (bIsMoving) Spread *= MovingSpreadMult;
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

	const float SpreadDeg = GetEffectiveSpreadDegrees(bIsAimed, bIsMoving);
	const float ConeDeg   = SpreadDeg + PelletConeDegrees;
	const float ConeRad   = FMath::DegreesToRadians(ConeDeg);
	const float TanCone   = FMath::Tan(ConeRad);
	const float RangeCm   = MaxRangeMeters * 100.0f;

	// Orthonormal basis around TraceForward, computed once and reused per
	// pellet so the cone math doesn't drift.
	FVector Right = FVector::CrossProduct(FVector::UpVector, TraceForward);
	if (!Right.Normalize()) Right = FVector::RightVector;
	const FVector Up = FVector::CrossProduct(TraceForward, Right);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(QRWeaponFire), /*bTraceComplex*/ false);
	if (AActor* MyOwner = GetOwner()) Params.AddIgnoredActor(MyOwner);

	const int32 NumPellets = FMath::Clamp(PelletsPerShot, 1, 20);

	bool bAnyHit = false;
	FHitResult BestHit;          // for the cosmetic FX origin
	FVector    BestDir = TraceForward;
	int32      Hits    = 0;
	Result.PelletEnds.Reserve(NumPellets);

	for (int32 p = 0; p < NumPellets; ++p)
	{
		// Sample a random direction inside the (spread + pellet) cone.
		// sqrt-distributed magnitude keeps the spray uniform over area.
		const float Theta     = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Magnitude = FMath::Sqrt(FMath::FRand()) * TanCone;
		const FVector Offset  = (Right * FMath::Cos(Theta) + Up * FMath::Sin(Theta)) * Magnitude;
		const FVector PelletDir = (TraceForward + Offset).GetSafeNormal();
		const FVector TraceEnd  = TraceStart + PelletDir * RangeCm;

		// Melee weapons sweep a sphere instead of a thin line so a swing
		// connects without pixel-perfect aim -- you're hitting with an axe,
		// not threading a needle. Ranged weapons keep the precise line trace.
		FHitResult Hit;
		bool bHit = false;
		if (WeaponType == EQRWeaponType::Melee && MeleeSweepRadius > 1.0f)
		{
			bHit = W->SweepSingleByChannel(Hit, TraceStart, TraceEnd, FQuat::Identity,
				ECC_Visibility, FCollisionShape::MakeSphere(MeleeSweepRadius), Params);
		}
		else
		{
			bHit = W->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);
		}
		AActor* HitActor = bHit ? Hit.GetActor() : nullptr;

		// First pellet runs the gun mechanics (cadence, jam, ammo, fouling)
		// with a null target -- damage for EVERY pellet (including this one)
		// is applied below through ApplyPelletDamage, which carries the hit
		// location so the target can resolve a headshot / crit zone.
		if (p == 0)
		{
			if (!TryFire(nullptr, AmmoInstance))
			{
				return Result;   // jam / out-of-ammo / cadence: bail
			}
		}
		ApplyPelletDamage(HitActor, Hit);

		// Record this pellet's endpoint (impact if it hit, otherwise the
		// far end of the trace) so the firer can draw N tracer lines.
		Result.PelletEnds.Add(bHit ? Hit.ImpactPoint : TraceEnd);

		if (bHit)
		{
			++Hits;
			if (!bAnyHit)
			{
				bAnyHit = true;
				BestHit = Hit;
				BestDir = PelletDir;
			}
		}
	}

	Result.bFired = true;
	Result.bHitSomething = bAnyHit;
	Result.HitActor      = bAnyHit ? BestHit.GetActor() : nullptr;
	Result.HitLocation   = bAnyHit ? BestHit.ImpactPoint  : (TraceStart + TraceForward * RangeCm);
	Result.HitNormal     = bAnyHit ? BestHit.ImpactNormal : -TraceForward;
	Result.DistanceMeters = bAnyHit ? (BestHit.Distance / 100.0f) : MaxRangeMeters;
	Result.Damage        = ComputeEffectiveDamage(Result.DistanceMeters) * FMath::Max(Hits, 1);

	// Recoil — aimed shots get reduced kick. Standard FPS feel.
	const float AimMult = bIsAimed ? 0.5f : 1.0f;
	Result.RecoilPitch = RecoilPitch * AimMult;
	Result.RecoilYaw   = FMath::FRandRange(-RecoilYawRandomRange, RecoilYawRandomRange) * AimMult;

	// Replicated cosmetic FX. Muzzle origin defaults to TraceStart — a
	// real socket lookup belongs once the held weapon mesh carries a
	// "Muzzle" socket; until then this anchors the flash to the camera.
	Multicast_PlayFireFX(TraceStart, Result.HitLocation, Result.HitNormal, bAnyHit);

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
