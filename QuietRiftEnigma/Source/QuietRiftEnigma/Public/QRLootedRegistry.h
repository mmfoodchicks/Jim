#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "QRLootedRegistry.generated.h"

/**
 * World-scoped registry of looted container GUIDs. Survives level
 * streaming (it's a world subsystem, lives for the duration of the
 * UWorld) and gets serialized into the save state alongside the rest
 * of the game data.
 *
 * UQRLootContainerComponent consults this on BeginPlay to decide
 * whether to mark itself empty, and updates it whenever a container
 * is looted so subsequent reloads see the updated set.
 *
 * Save integration: UQRSaveGameSystem reads/writes
 * FQRGameSaveData::LootedContainerIds to/from this subsystem's set.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRLootedRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Loot|Registry")
	bool HasBeenLooted(const FGuid& ContainerId) const;

	UFUNCTION(BlueprintCallable, Category = "Loot|Registry")
	void MarkLooted(const FGuid& ContainerId);

	UFUNCTION(BlueprintCallable, Category = "Loot|Registry")
	void Clear();

	// Save integration helpers — these copy in/out of the save struct.
	UFUNCTION(BlueprintCallable, Category = "Loot|Registry")
	TArray<FGuid> ExportLootedIds() const;

	UFUNCTION(BlueprintCallable, Category = "Loot|Registry")
	void ImportLootedIds(const TArray<FGuid>& InIds);

private:
	UPROPERTY()
	TSet<FGuid> LootedIds;
};
