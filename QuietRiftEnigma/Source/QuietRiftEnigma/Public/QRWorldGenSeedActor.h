#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRWorldGenSeedActor.generated.h"

class UTexture2D;

/**
 * Designer-placed driver for UQRWorldGenSubsystem. Drop one in a
 * level, set WorldSeed + map size, click Generate in Details, then
 * use the subsystem queries (GetBiomeAt / GetDepthBandAt) from any
 * scatter actor or BP that needs to know what biome a position is in.
 *
 * Also includes an OnBeginPlay auto-generate flag so the world
 * regenerates per session if the level designer wants a different
 * world every time (or set bAutoGenerateOnBeginPlay = false to
 * persist the same world across sessions — same seed = same world).
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWorldGenSeedActor : public AActor
{
	GENERATED_BODY()

public:
	AQRWorldGenSeedActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|WorldGen")
	int32 WorldSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|WorldGen",
		meta = (ClampMin = "16", ClampMax = "256"))
	float WorldMapSizeKm = 64.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|WorldGen",
		meta = (ClampMin = "32", ClampMax = "1024"))
	float CellSizeMeters = 128.0f;

	// If true, regenerates on BeginPlay (game launch). Off by default
	// so a saved seed produces a stable world.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|WorldGen")
	bool bAutoGenerateOnBeginPlay = false;

	// Resolution multiplier when exporting the minimap PNG.
	// Larger = bigger image, slower export. 1 = one pixel per cell
	// (500×500 for a 64 km world at 128 m cells).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|WorldGen",
		meta = (ClampMin = "1", ClampMax = "8"))
	int32 MinimapPixelsPerCell = 2;

	// Run the worldgen pipeline now. Button in the Details panel.
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "QR|WorldGen")
	void Generate();

	// Build a minimap texture and save it to disk under
	// /Game/QuietRift/Data/Worldgen/ for inspection. Button.
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "QR|WorldGen")
	void ExportMinimap();

	virtual void BeginPlay() override;
};
