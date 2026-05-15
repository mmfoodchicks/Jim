#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "QRProceduralScatterActor.h"
#include "QRBiomeProfile.generated.h"

class UMaterialInterface;

/**
 * Designer-authored biome description. Holds the scatter palette,
 * density rules, and the landscape material that paints the terrain
 * underneath. AQRProceduralScatterActor reads this when set so multiple
 * scatter volumes can share a single source of truth per biome.
 *
 * Save these as UDataAsset assets at /Game/QuietRift/Data/Biomes/.
 * The Python script qr_seed_biome_profiles.py creates three starter
 * profiles (Alien Jungle, Polar Tundra, Desert Sand) with palettes
 * pulled from the relevant Fab packs.
 */
UCLASS(BlueprintType)
class QUIETRIFTENIGMA_API UQRBiomeProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	// Player-readable name shown in any biome-debug UI later.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Biome")
	FText DisplayName;

	// Optional tag a designer can use to gate spawning, weather, music.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Biome")
	FName BiomeTag;

	// Scatter palette for this biome. Same struct
	// AQRProceduralScatterActor::Palette uses, so a profile is a
	// drop-in replacement.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Biome")
	TArray<FQRScatterEntry> Palette;

	// Default placements per scatter volume that uses this biome. The
	// scatter actor's TargetCount overrides this if non-zero.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Biome",
		meta = (ClampMin = "1", ClampMax = "10000"))
	int32 SuggestedTargetCount = 300;

	// Default min spacing between placements (cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Biome",
		meta = (ClampMin = "0", ClampMax = "5000"))
	float SuggestedMinSpacing = 100.0f;

	// Max slope (deg from world up) the scatter accepts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Biome",
		meta = (ClampMin = "0", ClampMax = "90"))
	float SuggestedMaxSlopeDeg = 50.0f;

	// Optional landscape material to paint on terrain inside this biome.
	// MWLandscapeAutoMaterial works as a sensible default.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Biome|Terrain")
	TSoftObjectPtr<UMaterialInterface> LandscapeMaterial;

	// Optional sky material override (Chaotic_Skies entries work).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Biome|Atmosphere")
	TSoftObjectPtr<UMaterialInterface> SkyMaterial;

	// Optional ambient sound cue (rain / wind / birds / cave drone).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Biome|Atmosphere")
	TSoftObjectPtr<class USoundBase> AmbientLoop;
};
