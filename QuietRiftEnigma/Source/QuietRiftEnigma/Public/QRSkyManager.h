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

	// Residual sun floor when the sun is below the horizon. Near-zero so
	// the NIGHT is genuinely lit by the Jovianlight (Jupiter's reflected
	// glow), not by a ghost sun shining up through the world. A tiny
	// non-zero value gives a hint of star/zodiacal light. The old 250 lux
	// "night sun" lit the ground from below and washed out the Jovianlight
	// shadow -- that was the bug.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky",
		meta = (ClampMin = "0", ClampMax = "10000"))
	float NightIntensity = 3.0f;

	// Constant intensity of the Jovianlight (the second directional light
	// labeled QR_Jovianlight). Per canon ~125 lux (500x our full moon).
	// It does NOT cycle -- Jupiter is fixed in the sky from a tidally-
	// locked moon -- so it's the steady night-shadow source. At noon the
	// ~2,800 lux sun overwhelms it (one shadow); at twilight both the low
	// sun and Jupiter cast (briefly two shadows); at night Jupiter alone
	// casts (the Jovianlight shadow).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky",
		meta = (ClampMin = "0", ClampMax = "10000"))
	float JovianIntensity = 125.0f;

	// Cream-tan tint of Jupiter's reflected light.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky")
	FLinearColor JovianColor = FLinearColor(0.95f, 0.85f, 0.65f, 1.0f);

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

	// ── Runtime celestial guarantee ─────────────────────────────────
	// If the editor-script-authored QR_Jupiter / QR_Moon_* actors are
	// NOT in the level (script never run on this map, or a cooked build
	// where label lookup can't work), spawn them at runtime so the sky
	// can never be empty. Script-authored actors always win.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky|Celestial")
	bool bAutoCreateCelestials = true;

	// Runtime deep-space starfield: instanced star dome (brightness
	// tiers + Milky Way band + planet-bright points + dim nebula
	// patches). No light pollution on a Jovian moon — nights are dense.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky|Celestial")
	bool bAutoCreateStarfield = true;

	// Star dome radius (cm). Beyond Jupiter (65 km) so bodies occlude
	// stars correctly.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky|Celestial",
		meta = (ClampMin = "1000000"))
	float StarDomeRadius = 9000000.0f;

	// Deterministic star layout seed — same sky every session.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Sky|Celestial")
	int32 StarfieldSeed = 20570;

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	/** Cached Jupiter and moon actor references resolved at BeginPlay. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> JupiterActor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> MoonActors;

	/** The second directional light (QR_Jovianlight) representing
	 *  Jupiter's reflected glow. Resolved by label at BeginPlay; the
	 *  Tick keeps it shadow-casting and aimed from Jupiter so the night
	 *  side gets a real Jovianlight shadow. */
	UPROPERTY(Transient)
	TObjectPtr<ADirectionalLight> JovianLight;

	/** Runtime starfield actor + per-tier instanced components/MIDs so
	 *  Tick can fade stars out in daylight. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> StarfieldActor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UInstancedStaticMeshComponent>> StarTiers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> StarTierMIDs;

	/** Base colors per tier, pre-fade (index-matched to StarTiers). */
	TArray<FLinearColor> StarTierBaseColors;

	void ResolveSkyActors();

	/** Spawn Jupiter + moons at runtime when the level doesn't carry
	 *  them. Uses the script's positions/scales; tags actors so future
	 *  resolution works outside the editor too. */
	void EnsureCelestialBodies();

	/** Build the instanced star dome (tiers, Milky Way, planets,
	 *  nebulae) if not already present. */
	void EnsureStarfield();

	/** Fade star tiers with sun height; hide them in full daylight. */
	void UpdateStarVisibility(float AboveHorizon);

	/** Spawn one unlit-ish celestial sphere (engine basic sphere + a
	 *  color MID). Returns the spawned actor. */
	AActor* SpawnCelestialSphere(const FString& NameTag, const FVector& Location,
		float UniformScale, const FLinearColor& Color);
};
