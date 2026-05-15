#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRSkyManager.generated.h"

class ADirectionalLight;
class USkyLightComponent;


/**
 * Day/night cycle driver. Locates the level's primary
 * ADirectionalLight (sun) and rotates it based on
 * AQRGameMode::GetDayProgress() (0..1 across the game-day).
 *
 * Pitch maps -90° at midnight → 0° at sunrise → +90° at noon →
 * 0° at sunset → -90° at midnight. Color shifts warm-cool-cool-warm
 * across the same arc.
 *
 * Drop one in any gameplay level. If no DirectionalLight is found
 * it does nothing (safe no-op on menu maps).
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRSkyManager : public AActor
{
	GENERATED_BODY()

public:
	AQRSkyManager();

	// Override the auto-found DirectionalLight by dragging one in here.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky")
	TObjectPtr<ADirectionalLight> SunLight;

	// Daylight intensity (lux). Sun goes from this at noon down to
	// NightIntensity at midnight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky",
		meta = (ClampMin = "0", ClampMax = "200000"))
	float DayIntensity = 75000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky",
		meta = (ClampMin = "0", ClampMax = "10000"))
	float NightIntensity = 200.0f;

	// Sun color at noon vs at horizon (sunrise/sunset).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky")
	FLinearColor NoonColor = FLinearColor(1.0f, 0.97f, 0.93f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky")
	FLinearColor HorizonColor = FLinearColor(1.0f, 0.55f, 0.30f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky")
	FLinearColor MidnightColor = FLinearColor(0.10f, 0.18f, 0.45f, 1.0f);

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
};
