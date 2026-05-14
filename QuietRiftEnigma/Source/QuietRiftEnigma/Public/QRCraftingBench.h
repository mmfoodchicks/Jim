#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRCraftingBench.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UQRCraftingComponent;

/**
 * Crafting workbench — placeable in level, interactable via F. Hosts a
 * UQRCraftingComponent that drives the actual queue, and a static mesh
 * for visuals. The character's interact path on the client opens the
 * crafting widget bound to this bench's component; the server side of
 * Server_Interact has no extra work to do because the widget queues
 * recipes through the component which RPCs internally.
 *
 * Designer drops one of these in the level, sets the mesh, picks a
 * RecipeTable + StationTag on the Crafting component, and that's it —
 * the character can walk up, press F, queue + cancel recipes.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRCraftingBench : public AActor
{
	GENERATED_BODY()

public:
	AQRCraftingBench();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Bench")
	TObjectPtr<USphereComponent> InteractSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Bench")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Bench")
	TObjectPtr<UQRCraftingComponent> Crafting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Bench")
	FText DisplayName;
};
