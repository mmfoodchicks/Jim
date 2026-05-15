#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRBiomeZone.generated.h"

class UQRBiomeProfile;
class UBoxComponent;
class UAudioComponent;
class APostProcessVolume;
class AExponentialHeightFog;

/**
 * Designer-placed biome override zone. When the player overlaps,
 * the zone's BiomeProfile takes priority over whatever the worldgen
 * subsystem says about that cell — useful for hand-authored areas
 * (a derelict outpost interior that should always read cold + foggy
 * regardless of the surrounding macro biome).
 *
 * Zones can also drive a child PostProcessVolume / fog / point lights
 * via the Apply() callback hooked into BeginOverlap → applies the
 * biome's atmosphere preset. Falling out of every zone restores the
 * worldgen-driven default.
 *
 * For the simple case (let the worldgen biome drive everything),
 * skip this actor entirely — AQRCharacter polls the subsystem on tick
 * and swaps ambient audio automatically.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRBiomeZone : public AActor
{
	GENERATED_BODY()

public:
	AQRBiomeZone();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|BiomeZone")
	TObjectPtr<UBoxComponent> Bounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|BiomeZone")
	TObjectPtr<UQRBiomeProfile> BiomeProfile;

	// Higher priority overrides lower. Used when zones overlap.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|BiomeZone")
	int32 Priority = 0;

protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* Overlapped,
		AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* Overlapped,
		AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	virtual void BeginPlay() override;
};
