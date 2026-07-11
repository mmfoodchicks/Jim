#include "QRSkyManager.h"
#include "QRGameMode.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
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

#if WITH_EDITOR
	// Label lookup is editor-only (GetActorLabel doesn't exist in
	// shipping). For dev work this is enough; in a packaged build we'd
	// switch to Actor Tags. Wrapped in WITH_EDITOR so the file still
	// compiles for cooked targets -- moons just won't orbit there.
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		const FString Label = It->GetActorLabel();
		if (Label == TEXT("QR_Jupiter"))
		{
			JupiterActor = *It;
			continue;
		}
		for (int32 i = 0; i < Moons.Num(); ++i)
		{
			if (Label == Moons[i].ActorLabel.ToString())
			{
				MoonActors[i] = *It;
				break;
			}
		}
	}
#endif

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

	// Pitch follows a clean cosine — at t=0 the sun is at -90° (mid-
	// night, below horizon), peaks at +90° at t=0.5 (noon), back at
	// t=1.0.
	const float SunPitch = -FMath::Cos(DayProgress * 2.0f * PI) * 90.0f;

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
