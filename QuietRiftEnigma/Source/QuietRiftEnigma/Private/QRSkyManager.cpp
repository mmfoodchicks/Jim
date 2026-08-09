#include "QRSkyManager.h"
#include "QRGameMode.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Engine/World.h"


AQRSkyManager::AQRSkyManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;  // 2 Hz is plenty for sky

	// Default Galilean moons. Orbital radii (in cm) preserve the real
	// Orbit radii keep the real Galilean RATIOS (1 : 1.6 : 2.55 : 4.4)
	// but are scaled to Jupiter's DISTANCE from the player (~65 km), not
	// its body radius. The old radii (4.2-18.2 Mm = 42-182 km) were
	// larger than Jupiter's 65 km distance, so once the tick ran the
	// moons flung out to 42-182 km orbits -- Callisto swung up to 247 km
	// away and even behind the origin, scattering the moons across (and
	// off) the sky. Capping Callisto at ~11 km keeps all four within
	// ~10 deg of Jupiter -- a tight, always-visible moon cluster on
	// Jupiter's side of the dome.
	// Period ratios preserve real Jovian (each ~2x previous, ~2.3x for
	// Callisto) compressed to seconds so motion is visible during play:
	// Io ~2 min, Europa ~4 min, Ganymede ~8 min, Callisto ~18 min.
	// Initial phases stagger them so they don't line up at world start.
	Moons.Reset();
	Moons.Add({ TEXT("QR_Moon_Io"),          250000.f,  120.f,  0.00f });
	Moons.Add({ TEXT("QR_Moon_Europa"),      400000.f,  240.f,  0.25f });
	Moons.Add({ TEXT("QR_Moon_Ganymede"),    640000.f,  480.f,  0.50f });
	Moons.Add({ TEXT("QR_Moon_Callisto"),   1100000.f, 1110.f,  0.75f });
}


void AQRSkyManager::BeginPlay()
{
	Super::BeginPlay();

	if (!SunLight)
	{
		// Prefer a Movable directional light (QR_KeyLight is spawned Movable
		// by qr_setup_sky.py) over a leftover Static "DirectionalLight_1" the
		// map may have shipped with -- rotating a Static light is what spammed
		// the per-tick 'has to be Movable' warning. Fall back to the first
		// light found if none are Movable (it then gets forced Movable below).
		ADirectionalLight* Fallback = nullptr;
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
		{
			ADirectionalLight* DL = *It;
			if (!Fallback) Fallback = DL;
			if (UDirectionalLightComponent* LC = DL->FindComponentByClass<UDirectionalLightComponent>())
			{
				if (LC->Mobility == EComponentMobility::Movable)
				{
					SunLight = DL;
					break;
				}
			}
		}
		if (!SunLight) SunLight = Fallback;
	}

	// The sun is rotated every tick, which UE only permits on a Movable
	// light component. Force it Movable so a Static/Stationary map light
	// doesn't spam 'has to be Movable if you'd like to move' every frame.
	if (SunLight)
	{
		if (UDirectionalLightComponent* LC = SunLight->FindComponentByClass<UDirectionalLightComponent>())
		{
			if (LC->Mobility != EComponentMobility::Movable)
			{
				LC->SetMobility(EComponentMobility::Movable);
			}
		}
	}

	// Resolve the 'Multiple directional lights are competing to be the
	// single one used for forward shading' warning by giving the chosen
	// sun the clear-winner ForwardShadingPriority and demoting every other
	// directional light (e.g. QR_Jovianlight, or a leftover map light) so
	// the tie no longer has to be broken by brightness.
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		ADirectionalLight* DL = *It;
		if (UDirectionalLightComponent* LC = DL->FindComponentByClass<UDirectionalLightComponent>())
		{
			LC->ForwardShadingPriority = (DL == SunLight) ? 10 : 0;
			LC->MarkRenderStateDirty();
		}
	}

	ResolveSkyActors();
}


void AQRSkyManager::ResolveSkyActors()
{
	UWorld* W = GetWorld();
	if (!W) return;

	JupiterActor = nullptr;
	JovianLight  = nullptr;
	MoonActors.Reset();
	MoonActors.SetNum(Moons.Num());

	// Resolve by ACTOR TAG first (works in every build — runtime-spawned
	// and script-spawned bodies both carry tags now), then fall back to
	// the editor-only label path for maps dressed by older script runs.
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (It->ActorHasTag(TEXT("QR_Jupiter")))
		{
			JupiterActor = *It;
			continue;
		}
		for (int32 i = 0; i < Moons.Num(); ++i)
		{
			if (It->ActorHasTag(Moons[i].ActorLabel))
			{
				MoonActors[i] = *It;
				break;
			}
		}
	}
#if WITH_EDITOR
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		const FString Label = It->GetActorLabel();
		if (!JupiterActor && Label == TEXT("QR_Jupiter"))
		{
			JupiterActor = *It;
			continue;
		}
		for (int32 i = 0; i < Moons.Num(); ++i)
		{
			if (!MoonActors[i] && Label == Moons[i].ActorLabel.ToString())
			{
				MoonActors[i] = *It;
				break;
			}
		}
	}
#endif

	// Guarantee the sky: if the level doesn't carry the celestial stack
	// (script never run on this map — the "Jupiter isn't there" bug),
	// build it at runtime.
	if (bAutoCreateCelestials) EnsureCelestialBodies();
	if (bAutoCreateStarfield)  EnsureStarfield();

	// Resolve the Jovianlight (works in any build -- it's a typed cast,
	// not a label, falling back to the dimmest demoted directional light
	// if the label-spawned one isn't present).
	for (TActorIterator<ADirectionalLight> It(W); It; ++It)
	{
		ADirectionalLight* DL = *It;
		if (DL == SunLight) continue;
#if WITH_EDITOR
		if (DL->GetActorLabel() == TEXT("QR_Jovianlight"))
		{
			JovianLight = DL;
			break;
		}
#endif
		// Non-editor / unlabeled fallback: the first non-sun directional
		// light is the Jovianlight.
		if (!JovianLight) JovianLight = DL;
	}

	// One-time config so it always casts the night shadow with a soft
	// penumbra and the canon cream tint.
	if (JovianLight)
	{
		if (UDirectionalLightComponent* LC = JovianLight->FindComponentByClass<UDirectionalLightComponent>())
		{
			if (LC->Mobility != EComponentMobility::Movable)
			{
				LC->SetMobility(EComponentMobility::Movable);
			}
			LC->SetCastShadows(true);
			LC->LightSourceAngle = 2.0f;          // soft penumbra
			LC->ForwardShadingPriority = 0;        // sun is the forward winner
			LC->SetLightColor(JovianColor);
			LC->SetIntensity(JovianIntensity);
			LC->MarkRenderStateDirty();
		}
	}
}


void AQRSkyManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!SunLight) return;

	AQRGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AQRGameMode>() : nullptr;
	if (!GM) return;

	const float DayProgress = GM->GetDayProgress();  // 0..1

	// Pitch follows a clean cosine, phase-aligned with the GameMode's
	// clock (bIsNight = progress > 0.5): t=0 dawn at the horizon,
	// t=0.25 noon (pitch -90, sun overhead), t=0.5 sunset, t=0.75
	// midnight. The old un-shifted cosine put noon at t=0 — a quarter
	// day out of phase, so "night" NPC schedules ran in daylight and
	// mornings were pitch dark.
	const float SunPitch = -FMath::Cos((DayProgress - 0.25f) * 2.0f * PI) * 90.0f;

	FRotator Rot = SunLight->GetActorRotation();
	Rot.Pitch = SunPitch;
	Rot.Yaw   = 30.0f + DayProgress * 360.0f;  // slow yaw drift for shadow variety
	SunLight->SetActorRotation(Rot);

	// Intensity + color from height above horizon.
	//
	// UE convention: a DirectionalLight's forward vector is the direction
	// the light SHINES, and the sun disk renders OPPOSITE to it. So
	// Pitch=-90° means "light shines straight down, sun is overhead at
	// noon" (visible), and Pitch=+90° means "light shines straight up,
	// sun is underneath the world at midnight" (invisible). The previous
	// HeightAlpha = SunPitch/90 had the sign backwards: it lit the world
	// to DayIntensity when the sun was under the world, and dimmed it to
	// NightIntensity when the sun was overhead. Flipping the sign makes
	// overhead = bright as physics expects.
	// AboveHorizon: 1 at noon, 0 at/below the horizon. Squared so the sun
	// fades hard as it sets -- below the horizon it contributes only the
	// tiny NightIntensity floor, leaving the night to the Jovianlight.
	// This is the fix for "shadows seem off at night": a 250-lux sun
	// shining UP through the world was lighting the ground from below and
	// killing the Jupiter shadow.
	const float AboveHorizon = FMath::Clamp((-SunPitch / 90.0f), 0.0f, 1.0f);
	const float SunCurve     = AboveHorizon * AboveHorizon;
	const float Intensity    = FMath::Lerp(NightIntensity, DayIntensity, SunCurve);

	// Deep-space layer: fade the star tiers with sun height and keep the
	// slow sidereal drift running.
	UpdateStarVisibility(AboveHorizon);

	FLinearColor Color;
	if (SunPitch <= -30.0f)
	{
		// Sun high overhead — blend from horizon warm to noon white.
		const float A = FMath::Clamp((-SunPitch - 30.0f) / 60.0f, 0.0f, 1.0f);
		Color = FMath::Lerp(HorizonColor, NoonColor, A);
	}
	else if (SunPitch <= 0.0f)
	{
		// Sun above horizon but low — hold the warm horizon tint.
		Color = HorizonColor;
	}
	else
	{
		// Sun below horizon — blend from horizon warm down into midnight blue.
		const float A = FMath::Clamp(SunPitch / 90.0f, 0.0f, 1.0f);
		Color = FMath::Lerp(HorizonColor, MidnightColor, A);
	}

	if (UDirectionalLightComponent* LC = SunLight->FindComponentByClass<UDirectionalLightComponent>())
	{
		LC->SetIntensity(Intensity);
		LC->SetLightColor(Color);
	}

	// Aim the Jovianlight FROM Jupiter so the night-side shadow points
	// correctly away from where Jupiter renders in the sky. Constant
	// intensity (Jupiter is fixed for a tidally-locked moon) -- only the
	// direction is kept in sync with the visible Jupiter actor. With the
	// sun faded out below the horizon, this is the dominant night light,
	// so it carves the single soft "Jovianlight" shadow. At twilight the
	// low sun and this both cast -> the brief two-shadow window.
	if (JovianLight)
	{
		if (UDirectionalLightComponent* JLC = JovianLight->FindComponentByClass<UDirectionalLightComponent>())
		{
			if (JupiterActor)
			{
				// Light shines from Jupiter toward the play area (origin).
				const FVector ShineDir = (-JupiterActor->GetActorLocation()).GetSafeNormal();
				if (!ShineDir.IsNearlyZero())
				{
					JovianLight->SetActorRotation(ShineDir.Rotation());
				}
			}
			JLC->SetIntensity(JovianIntensity);
			JLC->SetLightColor(JovianColor);
		}
	}

	// Orbit moons around Jupiter. Each moon's world position is
	// recomputed analytically from current time so missed ticks (we run
	// at 2 Hz, plus PIE pauses) never accumulate drift. Flat XY-plane
	// orbit -- the real Galileans are coplanar within a few degrees, so
	// this reads correctly from any moon's surface.
	if (JupiterActor)
	{
		const FVector JLoc = JupiterActor->GetActorLocation();
		const double Time = GetWorld()->GetTimeSeconds();
		for (int32 i = 0; i < Moons.Num() && i < MoonActors.Num(); ++i)
		{
			AActor* Moon = MoonActors[i];
			if (!Moon) continue;
			const float Period = FMath::Max(Moons[i].PeriodSeconds, 1.0f);
			const float Angle  = (float)((Time / Period + Moons[i].InitialPhase) * 2.0 * PI);
			const FVector Offset(
				FMath::Cos(Angle) * Moons[i].OrbitRadius,
				FMath::Sin(Angle) * Moons[i].OrbitRadius,
				0.0f);
			Moon->SetActorLocation(JLoc + Offset);
		}
	}
}


// ─── Runtime celestial guarantee ─────────────────────────────────────

AActor* AQRSkyManager::SpawnCelestialSphere(const FString& NameTag,
	const FVector& Location, float UniformScale, const FLinearColor& Color)
{
	UWorld* W = GetWorld();
	if (!W) return nullptr;

	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!Sphere) return nullptr;

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Body = W->SpawnActor<AActor>(AActor::StaticClass(), Location,
		FRotator::ZeroRotator, SP);
	if (!Body) return nullptr;

	UStaticMeshComponent* SMC = NewObject<UStaticMeshComponent>(Body);
	SMC->RegisterComponent();
	SMC->SetStaticMesh(Sphere);
	SMC->SetMobility(EComponentMobility::Movable);
	SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SMC->SetCastShadow(false);
	Body->SetRootComponent(SMC);
	Body->SetActorLocation(Location);
	Body->SetActorScale3D(FVector(UniformScale));

	// Prefer the script-authored flat-emissive material when the editor
	// pipeline has run; otherwise tint the engine basic-shape material.
	UMaterialInterface* Authored = LoadObject<UMaterialInterface>(nullptr,
		*FString::Printf(TEXT("/Game/QuietRift/Materials/M_%s.M_%s"), *NameTag, *NameTag));
	if (Authored)
	{
		SMC->SetMaterial(0, Authored);
	}
	else if (UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Basic, Body);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		SMC->SetMaterial(0, MID);
	}

	Body->Tags.Add(FName(*NameTag));
#if WITH_EDITOR
	Body->SetActorLabel(NameTag);
#endif
	return Body;
}

void AQRSkyManager::EnsureCelestialBodies()
{
	// Positions/scales mirror qr_setup_sky.py: Jupiter ~65 km out at
	// scale 4500 ≈ 4° of sky (canon: 8× our Moon, cream-tan banded).
	if (!JupiterActor)
	{
		JupiterActor = SpawnCelestialSphere(TEXT("QR_Jupiter"),
			FVector(5000000.0f, 1000000.0f, 4000000.0f), 4500.0f,
			FLinearColor(0.87f, 0.75f, 0.55f, 1.0f));
		if (JupiterActor)
		{
			UE_LOG(LogTemp, Log, TEXT("[QRSky] runtime-spawned QR_Jupiter (level had none)"));
		}
	}

	// Real-proportion Galilean scales (Io ≈ 2.6% of Jupiter, etc).
	static const float MoonScales[4] = { 117.0f, 100.0f, 169.0f, 155.0f };
	static const FLinearColor MoonColors[4] = {
		FLinearColor(0.85f, 0.78f, 0.45f, 1.0f),   // Io — sulfur yellow
		FLinearColor(0.80f, 0.78f, 0.72f, 1.0f),   // Europa — icy tan
		FLinearColor(0.55f, 0.50f, 0.45f, 1.0f),   // Ganymede — grey-brown
		FLinearColor(0.40f, 0.37f, 0.33f, 1.0f),   // Callisto — dark grey
	};
	const FVector JLoc = JupiterActor
		? JupiterActor->GetActorLocation()
		: FVector(5000000.0f, 1000000.0f, 4000000.0f);
	for (int32 i = 0; i < Moons.Num() && i < MoonActors.Num(); ++i)
	{
		if (MoonActors[i]) continue;
		MoonActors[i] = SpawnCelestialSphere(Moons[i].ActorLabel.ToString(),
			JLoc + FVector(Moons[i].OrbitRadius, 0, 0),
			MoonScales[FMath::Min(i, 3)], MoonColors[FMath::Min(i, 3)]);
	}
}

void AQRSkyManager::EnsureStarfield()
{
	if (StarfieldActor) return;
	UWorld* W = GetWorld();
	if (!W) return;

	// Reuse one authored by a previous run of this code in a saved map.
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (It->ActorHasTag(TEXT("QR_Starfield"))) { StarfieldActor = *It; break; }
	}
	if (StarfieldActor) return;

	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Sphere || !Basic) return;

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	StarfieldActor = W->SpawnActor<AActor>(AActor::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator, SP);
	if (!StarfieldActor) return;
	StarfieldActor->Tags.Add(TEXT("QR_Starfield"));
#if WITH_EDITOR
	StarfieldActor->SetActorLabel(TEXT("QR_Starfield"));
#endif
	USceneComponent* Root = NewObject<USceneComponent>(StarfieldActor);
	Root->RegisterComponent();
	StarfieldActor->SetRootComponent(Root);

	StarTiers.Reset();
	StarTierMIDs.Reset();
	StarTierBaseColors.Reset();

	FRandomStream Rng(StarfieldSeed);

	// Random point on the visible dome (slightly below horizon so the
	// sky reads full to the edges).
	auto DomeDir = [&Rng]() -> FVector
	{
		const float Az = Rng.FRandRange(0.0f, 2.0f * PI);
		const float El = FMath::Asin(Rng.FRandRange(-0.06f, 1.0f)); // uniform-ish over dome
		return FVector(FMath::Cos(El) * FMath::Cos(Az),
		               FMath::Cos(El) * FMath::Sin(Az),
		               FMath::Sin(El));
	};

	// The Milky Way band: a tilted great circle. Points sample along it
	// with gaussian scatter off-plane.
	const FVector BandNormal = FVector(0.35f, 0.2f, 1.0f).GetSafeNormal();
	auto BandDir = [&]() -> FVector
	{
		const float T = Rng.FRandRange(0.0f, 2.0f * PI);
		const FVector U = FVector::CrossProduct(BandNormal, FVector::UpVector).GetSafeNormal();
		const FVector V = FVector::CrossProduct(BandNormal, U);
		FVector Dir = (U * FMath::Cos(T) + V * FMath::Sin(T));
		// Gaussian-ish off-plane scatter (sum of two uniforms).
		const float Off = (Rng.FRand() + Rng.FRand() - 1.0f) * 0.12f;
		Dir = (Dir + BandNormal * Off).GetSafeNormal();
		return Dir;
	};

	auto MakeTier = [&](const TCHAR* Name, int32 Count, float MinScale, float MaxScale,
		const FLinearColor& Color, bool bBand) -> void
	{
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(StarfieldActor, FName(Name));
		ISM->RegisterComponent();
		ISM->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		ISM->SetStaticMesh(Sphere);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCastShadow(false);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Basic, StarfieldActor);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		ISM->SetMaterial(0, MID);
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector Dir = bBand ? BandDir() : DomeDir();
			if (Dir.Z < -0.08f) continue;
			const float S = Rng.FRandRange(MinScale, MaxScale) / 100.0f; // basic sphere = 100cm
			FTransform T(FRotator::ZeroRotator, Dir * StarDomeRadius, FVector(S));
			ISM->AddInstance(T);
		}
		StarTiers.Add(ISM);
		StarTierMIDs.Add(MID);
		StarTierBaseColors.Add(Color);
	};

	// No light pollution: a dense, layered night sky.
	MakeTier(TEXT("Stars_Bright"),  180,  260.0f, 380.0f, FLinearColor(1.0f, 1.0f, 0.98f), false);
	MakeTier(TEXT("Stars_Mid"),     700,  150.0f, 240.0f, FLinearColor(0.85f, 0.88f, 1.0f), false);
	MakeTier(TEXT("Stars_Dim"),    2200,   80.0f, 140.0f, FLinearColor(0.55f, 0.58f, 0.70f), false);
	MakeTier(TEXT("MilkyWay"),     2600,   70.0f, 150.0f, FLinearColor(0.60f, 0.60f, 0.72f), true);
	MakeTier(TEXT("Nebulae"),        14,  900.0f, 1600.0f, FLinearColor(0.28f, 0.20f, 0.34f), true);
	// Sister planets read as extra-bright tinted stars from 5.2 AU.
	MakeTier(TEXT("Planet_Saturn"),   1,  420.0f, 420.0f, FLinearColor(1.0f, 0.92f, 0.70f), false);
	MakeTier(TEXT("Planet_Inner"),    3,  350.0f, 420.0f, FLinearColor(1.0f, 0.85f, 0.80f), false);

	UE_LOG(LogTemp, Log, TEXT("[QRSky] runtime starfield built (%d tiers)"), StarTiers.Num());
}

void AQRSkyManager::UpdateStarVisibility(float AboveHorizon)
{
	if (StarTiers.Num() == 0) return;

	// Stars wash out as the (dim, 1/27-Earth) sun climbs. Fully hidden
	// only near noon; the fade preserves bright stars into twilight.
	const float StarAlpha = FMath::Clamp(1.0f - AboveHorizon * 1.6f, 0.0f, 1.0f);
	const bool bVisible = StarAlpha > 0.02f;
	for (int32 i = 0; i < StarTiers.Num(); ++i)
	{
		if (!StarTiers[i]) continue;
		StarTiers[i]->SetVisibility(bVisible);
		if (bVisible && StarTierMIDs.IsValidIndex(i) && StarTierMIDs[i] &&
			StarTierBaseColors.IsValidIndex(i))
		{
			StarTierMIDs[i]->SetVectorParameterValue(TEXT("Color"),
				StarTierBaseColors[i] * StarAlpha);
		}
	}

	// Slow sidereal drift so the night sky isn't a static painting.
	if (StarfieldActor)
	{
		AQRGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AQRGameMode>() : nullptr;
		if (GM)
		{
			FRotator R = StarfieldActor->GetActorRotation();
			R.Yaw = GM->GetDayProgress() * 360.0f;
			StarfieldActor->SetActorRotation(R);
		}
	}
}
