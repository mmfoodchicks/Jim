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
}


void AQRSkyManager::BeginPlay()
{
	Super::BeginPlay();

	if (!SunLight)
	{
		// Find the first ADirectionalLight in the level.
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
		{
			SunLight = *It;
			break;
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
	const float HeightAlpha = FMath::Clamp((SunPitch / 90.0f), 0.0f, 1.0f);
	const float Intensity   = FMath::Lerp(NightIntensity, DayIntensity, HeightAlpha);

	FLinearColor Color;
	if (SunPitch >= 30.0f)
	{
		const float A = FMath::Clamp((SunPitch - 30.0f) / 60.0f, 0.0f, 1.0f);
		Color = FMath::Lerp(HorizonColor, NoonColor, A);
	}
	else if (SunPitch >= 0.0f)
	{
		const float A = FMath::Clamp(SunPitch / 30.0f, 0.0f, 1.0f);
		Color = FMath::Lerp(HorizonColor, HorizonColor, A);  // hold horizon color
	}
	else
	{
		const float A = FMath::Clamp((SunPitch + 90.0f) / 90.0f, 0.0f, 1.0f);
		Color = FMath::Lerp(MidnightColor, HorizonColor, A);
	}

	if (UDirectionalLightComponent* LC = SunLight->FindComponentByClass<UDirectionalLightComponent>())
	{
		LC->SetIntensity(Intensity);
		LC->SetLightColor(Color);
	}
}
