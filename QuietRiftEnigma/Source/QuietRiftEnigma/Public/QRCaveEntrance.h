#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRCaveEntrance.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UPointLightComponent;

/**
 * Cave-mouth marker placed by the worldgen pipeline wherever a cell
 * has the Caves habitat flag set. v1 is a visual marker — a cave-
 * mouth mesh + dim point light + interact volume. The actual sub-
 * level streaming for interior caves comes later; for now interacting
 * spawns a configurable "Cave Interior" sub-actor or opens a
 * streamed level by name.
 *
 * Designer can subclass and override the mesh / light / DestinationLevel
 * fields per-biome (basalt caves vs ice caves vs canyon webs).
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRCaveEntrance : public AActor
{
	GENERATED_BODY()

public:
	AQRCaveEntrance();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Cave")
	TObjectPtr<USphereComponent> InteractSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Cave")
	TObjectPtr<UStaticMeshComponent> EntranceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Cave")
	TObjectPtr<UPointLightComponent> InteriorGlow;

	// Optional level name to stream in on enter. Empty = marker only.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Cave")
	FName DestinationLevel;

	// Player-readable name shown on interact prompt.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Cave")
	FText DisplayName;
};
