#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRBuildPieceTag.generated.h"

/**
 * Tiny marker component the build system attaches to every placed
 * piece. Holds the piece's catalog id + a stable GUID for save state.
 *
 * Why a tag component instead of a custom AActor subclass: build pieces
 * are just static-mesh actors with snap-socket empties baked into the
 * mesh; they don't need bespoke behavior. The tag is what makes the
 * build mode component able to find them via FindComponentByClass for
 * overlap / snap queries.
 */
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRBuildPieceTag : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRBuildPieceTag();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Build")
	FName PieceId;

	// Stable id for save-state binding (mirrors UQRLootContainerComponent
	// — auto-assigned on BeginPlay if blank).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Build")
	FGuid PieceGuid;

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
