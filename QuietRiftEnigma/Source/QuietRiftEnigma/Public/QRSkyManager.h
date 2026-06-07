#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRSkyManager.generated.h"

class ADirectionalLight;
class USkyLightComponent;


/**
 * One Galilean-style moon orbiting QR_Jupiter. The orbital math is
 * deliberately a flat circular orbit in the XY plane around Jupiter's
 * world location -- the real Galileans are coplanar within a few
 * degrees so this reads correctly without a full ecliptic sim. Period
 * is in seconds (game-scale, not real Jovian months) and InitialPhase
 * staggers the moons so they don't all line up at t=0.
 */
USTRUCT(BlueprintType)
struct FQRMoonConfig
{
	GENERATED_BODY()

	/** Actor label of the placeholder StaticMeshActor in the level
	 *  (e.g. QR_Moon_Io). Spawn it via qr_setup_sky.py. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky")
	FName ActorLabel = NAME_None;

	/** Distance from QR_Jupiter, centimetres. Game-scale, not real. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky",
		meta = (ClampMin = "1000"))
	float OrbitRadius = 200000.0f;

	/** Orbital period in seconds. Real Galileans range from Io (1.77
	 *  days) to Callisto (16.7 days); we keep the same ratio (~2x each
	 *  step) but compress totals so motion is visible during play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky",
		meta = (ClampMin = "1.0"))
	float PeriodSeconds = 120.0f;

	/** Starting orbital phase, fraction of a full revolution (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky",
		meta = (ClampMin = "0", ClampMax = "1"))
	float InitialPhase = 0.0f;
};


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
	// NightIntensity at midnight. Default ~2,800 lux is the physically
	// accurate solar irradiance at Jupiter's orbit (~5.2 AU) -- roughly
	// 1/27 of Earth's noon (~75,000 lux). Bounded auto-exposure on the
	// QR_Exposure PostProcessVolume handles the dynamic range so the
	// scene reads correctly without going pitch black.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky",
		meta = (ClampMin = "0", ClampMax = "200000"))
	float DayIntensity = 2800.0f;

	// "Jovianlight" floor -- Jupiter reflects substantial sunlight onto
	// the moon during the moon's nightside, ~500x brighter than our full
	// moon. 800 lux is a brighter soft-twilight floor so the world stays
	// readable at the camera's locked EV without auto-exposure adapting.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky",
		meta = (ClampMin = "0", ClampMax = "10000"))
	float NightIntensity = 800.0f;

	// Sun color at noon vs at horizon (sunrise/sunset).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky")
	FLinearColor NoonColor = FLinearColor(1.0f, 0.97f, 0.93f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky")
	FLinearColor HorizonColor = FLinearColor(1.0f, 0.55f, 0.30f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky")
	FLinearColor MidnightColor = FLinearColor(0.10f, 0.18f, 0.45f, 1.0f);

	/** Galilean moons orbiting QR_Jupiter. Spawn the placeholder mesh
	 *  actors via qr_setup_sky.py; QRSkyManager finds them by ActorLabel
	 *  at BeginPlay and updates their world positions every tick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky")
	TArray<FQRMoonConfig> Moons;

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	/** Cached Jupiter and moon actor references resolved at BeginPlay. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> JupiterActor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> MoonActors;

	void ResolveSkyActors();
};
