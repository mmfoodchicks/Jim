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
	// ratios to Jupiter's body radius: Io ~6 RJ, Europa ~9.4 RJ, Ganymede
	// ~15 RJ, Callisto ~26 RJ. Jupiter's game radius is 700,000 cm
	// (scale 7000 on a 1m sphere -> 7 km radius), so orbits become:
	//   Io        4.2 Mm,  Europa     6.6 Mm,
	//   Ganymede 10.5 Mm,  Callisto  18.2 Mm.
	// Period ratios preserve real Jovian (each ~2x previous, ~2.3x for
	// Callisto) compressed to seconds so motion is visible during play:
	// Io ~2 min, Europa ~4 min, Ganymede ~8 min, Callisto ~18 min.
	// Initial phases stagger them so they don't line up at world start.
	Moons.Reset();
	Moons.Add({ TEXT("QR_Moon_Io"),         4200000.f,  120.f,  0.00f });
	Moons.Add({ TEXT("QR_Moon_Europa"),     6600000.f,  240.f,  0.25f });
	Moons.Add({ TEXT("QR_Moon_Ganymede"),  10500000.f,  480.f,  0.50f });
	Moons.Add({ TEXT("QR_Moon_Callisto"),  18200000.f, 1110.f,  0.75f });
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

	ResolveSkyActors();
}


void AQRSkyManager::ResolveSkyActors()
{
	UWorld* W = GetWorld();
	if (!W) return;

	JupiterActor = nullptr;
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
	const float HeightAlpha = FMath::Clamp((-SunPitch / 90.0f), 0.0f, 1.0f);
	const float Intensity   = FMath::Lerp(NightIntensity, DayIntensity, HeightAlpha);

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
