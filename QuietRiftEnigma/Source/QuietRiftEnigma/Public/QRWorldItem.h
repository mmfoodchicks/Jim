#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRWorldItem.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UQRItemDefinition;
class UQRItemInstance;

/**
 * Pickupable item in the world. Spawned by:
 *   - UQRHotbarComponent::DropActiveItem (the drop button)
 *   - Authored Blueprints / level placement (loose loot)
 *   - Loot tables that scatter physical items
 *
 * Pickup runs through AQRCharacter's existing interaction trace: when the
 * player presses interact while this actor is the focused target, the
 * character's Server_Interact looks for AQRWorldItem and calls TryPickup
 * (same pattern as the dialogue / loot container auto-handlers).
 *
 * The instance data is intentionally minimal — ItemId + Quantity — so the
 * actor can replicate cheaply without sending the full UQRItemInstance.
 * Pickup reconstitutes a fresh instance from the definition on the server
 * side, identical to TryAddByDefinition's normal path.
 */
UCLASS(BlueprintType)
class QUIETRIFTENIGMA_API AQRWorldItem : public AActor
{
	GENERATED_BODY()

public:
	AQRWorldItem();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Item")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// Sphere used for the interact trace to easily land on small items.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Item")
	TObjectPtr<USphereComponent> InteractionVolume;

	// What this pickup represents. Set via InitializeFrom or in the editor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "World Item")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "World Item")
	int32 Quantity = 1;

	// Optional editor-time def pointer — if set in a placed actor, BeginPlay
	// applies its mesh + ids automatically so designers don't have to keep
	// the mesh in sync with the item id manually.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Item")
	TSoftObjectPtr<UQRItemDefinition> DefinitionForEditorPlacement;

	// Configure this actor from a definition. Sets the mesh, ids, and any
	// other visuals derived from the def. Safe to call on either authority
	// or simulated proxy — mesh setting is local.
	UFUNCTION(BlueprintCallable, Category = "World Item")
	void InitializeFrom(const UQRItemDefinition* Definition, int32 InQuantity);

	// Server-side pickup: adds to picker's inventory, destroys this actor.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "World Item")
	bool TryPickup(class AActor* Picker);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
